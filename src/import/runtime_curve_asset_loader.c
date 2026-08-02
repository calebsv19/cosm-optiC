#include "import/runtime_curve_asset_loader.h"

#include "app/ray_tracing_sha256.h"
#include "core_io.h"
#include "import/runtime_mesh_asset_loader_internal.h"
#include "render/runtime_curve_blas_3d.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CURVE_ASSET_MAX_STRANDS 16384u
#define CURVE_ASSET_MAX_POINTS_PER_STRAND 128u
#define CURVE_ASSET_MAX_PRIMITIVES 1048576u

static RayTracingRuntimeCurveAssetSet g_last_curve_assets;
static bool g_last_curve_assets_initialized;
static char g_last_curve_scene_path[RAY_TRACING_RUNTIME_CURVE_ASSET_PATH_MAX];

static void curve_diag(char *out, size_t size, const char *message) {
    if (out && size > 0u) snprintf(out, size, "%s", message);
}

static const char *curve_string(json_object *object, const char *key) {
    json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

static bool curve_number(json_object *object, const char *key, double *out) {
    json_object *value = NULL;
    if (!object || !out ||
        !json_object_object_get_ex(object, key, &value) ||
        !(json_object_is_type(value, json_type_double) ||
          json_object_is_type(value, json_type_int))) {
        return false;
    }
    *out = json_object_get_double(value);
    return isfinite(*out);
}

static bool curve_vec3(json_object *object, const char *key, Vec3 *out) {
    json_object *value = NULL;
    if (!object || !out ||
        !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_object)) {
        return false;
    }
    return curve_number(value, "x", &out->x) &&
           curve_number(value, "y", &out->y) &&
           curve_number(value, "z", &out->z);
}

static bool curve_read_text(const char *path, char **out_text) {
    CoreBuffer buffer = {0};
    CoreResult result;
    char *text = NULL;
    if (!path || !out_text) return false;
    result = core_io_read_all(path, &buffer);
    if (result.code != CORE_OK || !buffer.data || buffer.size == 0u) {
        core_io_buffer_free(&buffer);
        return false;
    }
    text = malloc(buffer.size + 1u);
    if (!text) {
        core_io_buffer_free(&buffer);
        return false;
    }
    memcpy(text, buffer.data, buffer.size);
    text[buffer.size] = '\0';
    core_io_buffer_free(&buffer);
    *out_text = text;
    return true;
}

static bool curve_resolve_path(const char *scene_path,
                               const char *asset_path,
                               char out[RAY_TRACING_RUNTIME_CURVE_ASSET_PATH_MAX]) {
    const char *slash = NULL;
    int written = 0;
    if (!scene_path || !asset_path || !asset_path[0]) return false;
    if (asset_path[0] == '/') {
        written = snprintf(out, RAY_TRACING_RUNTIME_CURVE_ASSET_PATH_MAX,
                           "%s", asset_path);
    } else {
        slash = strrchr(scene_path, '/');
        if (!slash) {
            written = snprintf(out, RAY_TRACING_RUNTIME_CURVE_ASSET_PATH_MAX,
                               "%s", asset_path);
        } else {
            written = snprintf(out, RAY_TRACING_RUNTIME_CURVE_ASSET_PATH_MAX,
                               "%.*s/%s", (int)(slash - scene_path),
                               scene_path, asset_path);
        }
    }
    return written > 0 &&
           written < RAY_TRACING_RUNTIME_CURVE_ASSET_PATH_MAX;
}

void ray_tracing_runtime_curve_asset_set_init(
    RayTracingRuntimeCurveAssetSet *set) {
    if (!set) return;
    memset(set, 0, sizeof(*set));
    for (int i = 0; i < RAY_TRACING_RUNTIME_CURVE_ASSET_MAX_ASSETS; ++i) {
        RuntimeCurveAsset3D_Init(&set->assets[i].asset);
    }
}

void ray_tracing_runtime_curve_asset_set_free(
    RayTracingRuntimeCurveAssetSet *set) {
    if (!set) return;
    for (int i = 0; i < RAY_TRACING_RUNTIME_CURVE_ASSET_MAX_ASSETS; ++i) {
        RuntimeCurveAsset3D_Free(&set->assets[i].asset);
    }
    memset(set, 0, sizeof(*set));
}

static bool curve_load_asset_document(
    const char *path,
    const char *expected_id,
    const char *expected_sha,
    RayTracingRuntimeCurveAsset *out,
    char *diag,
    size_t diag_size) {
    char *text = NULL;
    char actual_sha[65] = {0};
    json_object *root = NULL;
    json_object *schema_version = NULL;
    json_object *strands = NULL;
    const char *family = NULL;
    const char *variant = NULL;
    const char *asset_id = NULL;
    size_t strand_count = 0u;
    size_t points_per_strand = 0u;
    Vec3 *points = NULL;
    double *radii = NULL;
    bool ok = false;
    if (!curve_read_text(path, &text) ||
        !ray_tracing_sha256_file(path, actual_sha)) {
        curve_diag(diag, diag_size, "failed to read curve asset");
        goto cleanup;
    }
    if (!expected_sha || strlen(expected_sha) != 64u ||
        strcmp(expected_sha, actual_sha) != 0) {
        curve_diag(diag, diag_size, "curve asset sha256 mismatch");
        goto cleanup;
    }
    root = json_tokener_parse(text);
    if (!root || !json_object_is_type(root, json_type_object)) {
        curve_diag(diag, diag_size, "invalid curve asset json");
        goto cleanup;
    }
    family = curve_string(root, "schema_family");
    variant = curve_string(root, "schema_variant");
    asset_id = curve_string(root, "asset_id");
    if (!family || strcmp(family, "codework_curve_asset") != 0 ||
        !variant || strcmp(variant, "curve_asset_runtime_v1") != 0 ||
        !json_object_object_get_ex(root, "schema_version", &schema_version) ||
        !json_object_is_type(schema_version, json_type_int) ||
        json_object_get_int(schema_version) != 1 ||
        !asset_id || strcmp(asset_id, expected_id) != 0 ||
        strlen(asset_id) >= sizeof(out->asset_id) ||
        strlen(path) >= sizeof(out->path)) {
        curve_diag(diag, diag_size, "curve asset identity/schema mismatch");
        goto cleanup;
    }
    if (!json_object_object_get_ex(root, "strands", &strands) ||
        !json_object_is_type(strands, json_type_array)) {
        curve_diag(diag, diag_size, "curve asset strands missing");
        goto cleanup;
    }
    strand_count = json_object_array_length(strands);
    if (strand_count == 0u || strand_count > CURVE_ASSET_MAX_STRANDS) {
        curve_diag(diag, diag_size, "curve asset strand count invalid");
        goto cleanup;
    }
    for (size_t strand_index = 0u; strand_index < strand_count; ++strand_index) {
        json_object *strand = json_object_array_get_idx(strands, strand_index);
        json_object *point_array = NULL;
        json_object *index_object = NULL;
        size_t count = 0u;
        if (!strand || !json_object_is_type(strand, json_type_object) ||
            !json_object_object_get_ex(strand, "strand_index", &index_object) ||
            !json_object_is_type(index_object, json_type_int) ||
            json_object_get_int64(index_object) != (int64_t)strand_index ||
            !json_object_object_get_ex(strand, "points", &point_array) ||
            !json_object_is_type(point_array, json_type_array)) {
            curve_diag(diag, diag_size, "curve strand structure/order invalid");
            goto cleanup;
        }
        count = json_object_array_length(point_array);
        if (count < 2u || count > CURVE_ASSET_MAX_POINTS_PER_STRAND ||
            (points_per_strand != 0u && count != points_per_strand)) {
            curve_diag(diag, diag_size, "curve strand point count invalid");
            goto cleanup;
        }
        points_per_strand = count;
    }
    if (strand_count * (points_per_strand - 1u) >
        CURVE_ASSET_MAX_PRIMITIVES) {
        curve_diag(diag, diag_size, "curve asset primitive limit exceeded");
        goto cleanup;
    }
    points = calloc(strand_count * points_per_strand, sizeof(*points));
    radii = calloc(strand_count * points_per_strand, sizeof(*radii));
    if (!points || !radii) {
        curve_diag(diag, diag_size, "out of memory loading curve asset");
        goto cleanup;
    }
    for (size_t strand_index = 0u; strand_index < strand_count; ++strand_index) {
        json_object *strand = json_object_array_get_idx(strands, strand_index);
        json_object *point_array = NULL;
        json_object_object_get_ex(strand, "points", &point_array);
        for (size_t point_index = 0u;
             point_index < points_per_strand; ++point_index) {
            json_object *point =
                json_object_array_get_idx(point_array, point_index);
            size_t flat = strand_index * points_per_strand + point_index;
            if (!point || !json_object_is_type(point, json_type_object) ||
                !curve_vec3(point, "position", &points[flat]) ||
                !curve_number(point, "radius", &radii[flat]) ||
                !(radii[flat] > 0.0)) {
                curve_diag(diag, diag_size,
                           "curve point position/radius invalid");
                goto cleanup;
            }
            if (point_index > 0u) {
                Vec3 delta = vec3_sub(points[flat], points[flat - 1u]);
                if (!(vec3_length(delta) > 1.0e-9)) {
                    curve_diag(diag, diag_size,
                               "curve strand contains zero-length segment");
                    goto cleanup;
                }
            }
        }
    }
    if (!RuntimeCurveAsset3D_BuildFromPolylineStrands(
            &out->asset, points, radii, strand_count, points_per_strand) ||
        !RuntimeCurveAsset3D_BuildBLAS(&out->asset)) {
        curve_diag(diag, diag_size, "curve primitive/BLAS build failed");
        goto cleanup;
    }
    snprintf(out->asset_id, sizeof(out->asset_id), "%s", asset_id);
    snprintf(out->path, sizeof(out->path), "%s", path);
    snprintf(out->sha256, sizeof(out->sha256), "%s", actual_sha);
    out->strand_count = strand_count;
    out->points_per_strand = points_per_strand;
    ok = true;
cleanup:
    free(points);
    free(radii);
    if (root) json_object_put(root);
    free(text);
    return ok;
}

static int curve_find_asset(const RayTracingRuntimeCurveAssetSet *set,
                            const char *asset_id) {
    for (int i = 0; i < set->asset_count; ++i) {
        if (strcmp(set->assets[i].asset_id, asset_id) == 0) return i;
    }
    return -1;
}

static bool curve_parse_instance_transform(
    json_object *object,
    double world_scale,
    RayTracingRuntimeCurveAssetInstance *instance) {
    json_object *transform = NULL;
    Vec3 position = vec3(0.0, 0.0, 0.0);
    Vec3 rotation = vec3(0.0, 0.0, 0.0);
    Vec3 scale = vec3(1.0, 1.0, 1.0);
    if (json_object_object_get_ex(object, "transform", &transform)) {
        if (!json_object_is_type(transform, json_type_object)) return false;
        if (!curve_vec3(transform, "position", &position) ||
            !curve_vec3(transform, "rotation", &rotation) ||
            !curve_vec3(transform, "scale", &scale)) {
            return false;
        }
    }
    if (!isfinite(world_scale) || !(world_scale > 0.0) ||
        !(scale.x > 0.0) || fabs(scale.x - scale.y) > 1.0e-12 ||
        fabs(scale.x - scale.z) > 1.0e-12) {
        return false;
    }
    instance->position_x = position.x * world_scale;
    instance->position_y = position.y * world_scale;
    instance->position_z = position.z * world_scale;
    instance->rotation_x = rotation.x;
    instance->rotation_y = rotation.y;
    instance->rotation_z = rotation.z;
    instance->uniform_scale = scale.x * world_scale;
    return true;
}

bool ray_tracing_runtime_curve_assets_load_scene_file(
    const char *runtime_scene_path,
    RayTracingRuntimeCurveAssetSet *out_set,
    char *diag,
    size_t diag_size) {
    char *text = NULL;
    json_object *root = NULL;
    json_object *objects = NULL;
    json_object *world_scale_object = NULL;
    double world_scale = 1.0;
    int runtime_object_index = 0;
    if (!runtime_scene_path || !out_set) return false;
    ray_tracing_runtime_curve_asset_set_free(out_set);
    ray_tracing_runtime_curve_asset_set_init(out_set);
    if (!curve_read_text(runtime_scene_path, &text)) {
        curve_diag(diag, diag_size, "failed to read runtime scene");
        return false;
    }
    root = json_tokener_parse(text);
    free(text);
    if (!root || !json_object_is_type(root, json_type_object) ||
        !json_object_object_get_ex(root, "objects", &objects) ||
        !json_object_is_type(objects, json_type_array)) {
        curve_diag(diag, diag_size, "runtime scene objects missing");
        if (root) json_object_put(root);
        return false;
    }
    if (json_object_object_get_ex(root, "world_scale", &world_scale_object)) {
        world_scale = json_object_get_double(world_scale_object);
    }
    for (size_t i = 0u; i < json_object_array_length(objects); ++i) {
        json_object *object = json_object_array_get_idx(objects, i);
        json_object *geometry = NULL;
        const char *type = curve_string(object, "object_type");
        const char *object_id = NULL;
        const char *kind = NULL;
        const char *asset_id = NULL;
        const char *runtime_path = NULL;
        const char *sha = NULL;
        int asset_index = -1;
        char resolved[RAY_TRACING_RUNTIME_CURVE_ASSET_PATH_MAX] = {0};
        if (runtime_mesh_asset_is_authoring_helper_object_type(type)) {
            continue;
        }
        if (!type || strcmp(type, "curve_asset_instance") != 0) {
            runtime_object_index += 1;
            continue;
        }
        object_id = curve_string(object, "object_id");
        if (!object_id ||
            !json_object_object_get_ex(object, "geometry_ref", &geometry) ||
            !json_object_is_type(geometry, json_type_object)) {
            curve_diag(diag, diag_size, "curve instance identity/ref missing");
            goto fail;
        }
        kind = curve_string(geometry, "kind");
        asset_id = curve_string(geometry, "id");
        runtime_path = curve_string(geometry, "runtime_path");
        sha = curve_string(geometry, "sha256");
        if (!kind || strcmp(kind, "curve_asset") != 0 || !asset_id ||
            !runtime_path || !sha ||
            strlen(object_id) >= sizeof(out_set->instances[0].object_id) ||
            strlen(asset_id) >= sizeof(out_set->instances[0].asset_id) ||
            strlen(asset_id) >= sizeof(out_set->assets[0].asset_id) ||
            !curve_resolve_path(runtime_scene_path, runtime_path, resolved)) {
            curve_diag(diag, diag_size, "curve instance geometry_ref invalid");
            goto fail;
        }
        asset_index = curve_find_asset(out_set, asset_id);
        if (asset_index < 0) {
            if (out_set->asset_count >=
                RAY_TRACING_RUNTIME_CURVE_ASSET_MAX_ASSETS) {
                curve_diag(diag, diag_size, "curve asset limit exceeded");
                goto fail;
            }
            asset_index = out_set->asset_count++;
            if (!curve_load_asset_document(
                    resolved, asset_id, sha, &out_set->assets[asset_index],
                    diag, diag_size)) {
                goto fail;
            }
        } else if (strcmp(out_set->assets[asset_index].sha256, sha) != 0 ||
                   strcmp(out_set->assets[asset_index].path, resolved) != 0) {
            curve_diag(diag, diag_size,
                       "curve asset id reused with different binding");
            goto fail;
        }
        if (out_set->instance_count >=
            RAY_TRACING_RUNTIME_CURVE_ASSET_MAX_INSTANCES) {
            curve_diag(diag, diag_size, "curve instance limit exceeded");
            goto fail;
        }
        RayTracingRuntimeCurveAssetInstance *instance =
            &out_set->instances[out_set->instance_count];
        snprintf(instance->object_id, sizeof(instance->object_id), "%s",
                 object_id);
        snprintf(instance->asset_id, sizeof(instance->asset_id), "%s",
                 asset_id);
        instance->asset_index = asset_index;
        instance->scene_object_index = runtime_object_index;
        if (!curve_parse_instance_transform(
                object, world_scale, instance)) {
            curve_diag(diag, diag_size,
                       "curve instance transform must be finite uniform scale");
            goto fail;
        }
        out_set->instance_count += 1;
        runtime_object_index += 1;
    }
    json_object_put(root);
    curve_diag(diag, diag_size, "ok");
    return true;
fail:
    json_object_put(root);
    ray_tracing_runtime_curve_asset_set_free(out_set);
    ray_tracing_runtime_curve_asset_set_init(out_set);
    return false;
}

void ray_tracing_runtime_curve_assets_reset_last(void) {
    if (g_last_curve_assets_initialized) {
        ray_tracing_runtime_curve_asset_set_free(&g_last_curve_assets);
    }
    ray_tracing_runtime_curve_asset_set_init(&g_last_curve_assets);
    g_last_curve_assets_initialized = true;
    g_last_curve_scene_path[0] = '\0';
}

void ray_tracing_runtime_curve_assets_take_last_for_scene(
    const char *runtime_scene_path,
    RayTracingRuntimeCurveAssetSet *set) {
    if (!set) return;
    ray_tracing_runtime_curve_assets_reset_last();
    g_last_curve_assets = *set;
    memset(set, 0, sizeof(*set));
    snprintf(g_last_curve_scene_path, sizeof(g_last_curve_scene_path), "%s",
             runtime_scene_path ? runtime_scene_path : "");
}

const RayTracingRuntimeCurveAssetSet *
ray_tracing_runtime_curve_assets_last(void) {
    if (!g_last_curve_assets_initialized) {
        ray_tracing_runtime_curve_assets_reset_last();
    }
    return &g_last_curve_assets;
}

bool ray_tracing_runtime_curve_assets_last_matches_scene_file(
    const char *runtime_scene_path) {
    return runtime_scene_path && g_last_curve_scene_path[0] &&
           strcmp(runtime_scene_path, g_last_curve_scene_path) == 0;
}
