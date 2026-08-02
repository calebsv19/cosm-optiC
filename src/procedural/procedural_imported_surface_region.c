#include "procedural/procedural_imported_surface_region.h"

#include "app/ray_tracing_sha256.h"
#include "core_io.h"
#include "procedural/procedural_solid_mesh.h"

#include <json-c/json.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report_set(
    ProceduralImportedSurfaceRegionReport *report,
    bool ok,
    const char *field,
    const char *message) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->ok = ok;
    snprintf(report->field, sizeof(report->field), "%s", field ? field : "");
    snprintf(report->message, sizeof(report->message), "%s",
             message ? message : "");
}

static bool stable_id(const char *text, size_t capacity) {
    size_t length;
    if (!text || !text[0] || (length = strlen(text)) >= capacity) return false;
    for (size_t i = 0u; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
    }
    return true;
}

static double clamp01(double value) {
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

static double smooth01(double value) {
    double t = clamp01(value);
    return t * t * (3.0 - (2.0 * t));
}

static bool json_number(json_object *root, const char *key, double *out) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(root, key, &value) ||
        !(json_object_is_type(value, json_type_double) ||
          json_object_is_type(value, json_type_int))) return false;
    *out = json_object_get_double(value);
    return isfinite(*out);
}

static bool json_vec3(json_object *root, const char *key, double out[3]) {
    json_object *array = NULL;
    if (!json_object_object_get_ex(root, key, &array) ||
        !json_object_is_type(array, json_type_array) ||
        json_object_array_length(array) != 3u) return false;
    for (size_t i = 0u; i < 3u; ++i) {
        json_object *value = json_object_array_get_idx(array, i);
        if (!value ||
            !(json_object_is_type(value, json_type_double) ||
              json_object_is_type(value, json_type_int))) return false;
        out[i] = json_object_get_double(value);
        if (!isfinite(out[i])) return false;
    }
    return true;
}

static json_object *vec3_json(const double value[3]) {
    json_object *array = json_object_new_array();
    if (!array) return NULL;
    for (size_t i = 0u; i < 3u; ++i)
        json_object_array_add(array, json_object_new_double(value[i]));
    return array;
}

static bool recipe_digest(
    const ProceduralImportedSurfaceRegionRecipeV1 *recipe,
    char out_digest[PROCEDURAL_IMPORTED_SURFACE_REGION_DIGEST_CAPACITY]) {
    json_object *root = json_object_new_object();
    json_object *patches = json_object_new_array();
    const char *text;
    bool ok = false;
    if (!root || !patches) goto done;
    json_object_object_add(root, "schema",
        json_object_new_string(PROCEDURAL_IMPORTED_SURFACE_REGION_RECIPE_SCHEMA));
    json_object_object_add(root, "schema_version",
        json_object_new_int((int)recipe->schema_version));
    json_object_object_add(root, "region_id",
        json_object_new_string(recipe->region_id));
    json_object_object_add(root, "source_asset_id",
        json_object_new_string(recipe->source_asset_id));
    for (size_t i = 0u; i < recipe->patch_count; ++i) {
        const ProceduralImportedSurfaceRegionPatchV1 *patch =
            &recipe->patches[i];
        json_object *entry = json_object_new_object();
        json_object_object_add(entry, "center", vec3_json(patch->center));
        json_object_object_add(entry, "radius", vec3_json(patch->radius));
        json_object_object_add(entry, "feather",
            json_object_new_double(patch->feather));
        json_object_object_add(entry, "noise_scale",
            json_object_new_double(patch->noise_scale));
        json_object_object_add(entry, "noise_strength",
            json_object_new_double(patch->noise_strength));
        json_object_object_add(entry, "strength",
            json_object_new_double(patch->strength));
        json_object_object_add(entry, "seed", json_object_new_int(patch->seed));
        json_object_array_add(patches, entry);
    }
    json_object_object_add(root, "patches", patches);
    patches = NULL;
    text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    ok = text && ray_tracing_sha256_bytes(text, strlen(text), out_digest);
done:
    if (patches) json_object_put(patches);
    if (root) json_object_put(root);
    return ok;
}

static bool values_digest(
    const double *weights,
    size_t count,
    char out_digest[PROCEDURAL_IMPORTED_SURFACE_REGION_DIGEST_CAPACITY]) {
    char *text;
    size_t capacity;
    size_t used = 0u;
    bool ok;
    if (count > (SIZE_MAX - 1u) / 25u) return false;
    capacity = (count * 25u) + 1u;
    text = malloc(capacity);
    if (!text && capacity > 0u) return false;
    for (size_t i = 0u; i < count; ++i) {
        int written = snprintf(text + used, capacity - used, "%.17g;",
                               weights[i]);
        if (written < 0 || (size_t)written >= capacity - used) {
            free(text);
            return false;
        }
        used += (size_t)written;
    }
    ok = ray_tracing_sha256_bytes(text, used, out_digest);
    free(text);
    return ok;
}

void ProceduralImportedSurfaceRegionV1_Init(
    ProceduralImportedSurfaceRegionV1 *region) {
    if (region) memset(region, 0, sizeof(*region));
}

void ProceduralImportedSurfaceRegionV1_Free(
    ProceduralImportedSurfaceRegionV1 *region) {
    if (!region) return;
    free(region->vertex_weights);
    memset(region, 0, sizeof(*region));
}

bool ProceduralImportedSurfaceRegionV1_RefreshValues(
    ProceduralImportedSurfaceRegionV1 *region) {
    double sum = 0.0;
    if (!region || !region->vertex_weights || region->vertex_count == 0u)
        return false;
    region->minimum = 1.0;
    region->maximum = 0.0;
    region->transition_vertex_count = 0u;
    for (size_t i = 0u; i < region->vertex_count; ++i) {
        double value = region->vertex_weights[i];
        if (!isfinite(value) || value < 0.0 || value > 1.0) return false;
        if (value < region->minimum) region->minimum = value;
        if (value > region->maximum) region->maximum = value;
        if (value > 1e-6 && value < 1.0 - 1e-6)
            ++region->transition_vertex_count;
        sum += value;
    }
    region->mean = sum / (double)region->vertex_count;
    return values_digest(region->vertex_weights, region->vertex_count,
                         region->value_digest_sha256);
}

bool ProceduralImportedSurfaceRegionRecipeV1_LoadJsonFile(
    const char *path,
    ProceduralImportedSurfaceRegionRecipeV1 *out_recipe,
    ProceduralImportedSurfaceRegionReport *report) {
    json_object *root = NULL;
    json_object *schema = NULL;
    json_object *version = NULL;
    json_object *region_id = NULL;
    json_object *source_asset_id = NULL;
    json_object *patches = NULL;
    ProceduralImportedSurfaceRegionRecipeV1 recipe;
    memset(&recipe, 0, sizeof(recipe));
    report_set(report, false, "recipe", "invalid region recipe");
    if (!path || !out_recipe ||
        !(root = json_object_from_file(path)) ||
        !json_object_object_get_ex(root, "schema", &schema) ||
        strcmp(json_object_get_string(schema),
               PROCEDURAL_IMPORTED_SURFACE_REGION_RECIPE_SCHEMA) != 0 ||
        !json_object_object_get_ex(root, "schema_version", &version) ||
        json_object_get_int(version) !=
            (int)PROCEDURAL_IMPORTED_SURFACE_REGION_RECIPE_SCHEMA_VERSION ||
        !json_object_object_get_ex(root, "region_id", &region_id) ||
        !json_object_object_get_ex(root, "source_asset_id", &source_asset_id) ||
        !json_object_object_get_ex(root, "patches", &patches) ||
        !json_object_is_type(patches, json_type_array)) goto fail;
    recipe.schema_version =
        PROCEDURAL_IMPORTED_SURFACE_REGION_RECIPE_SCHEMA_VERSION;
    snprintf(recipe.region_id, sizeof(recipe.region_id), "%s",
             json_object_get_string(region_id));
    snprintf(recipe.source_asset_id, sizeof(recipe.source_asset_id), "%s",
             json_object_get_string(source_asset_id));
    recipe.patch_count = json_object_array_length(patches);
    if (!stable_id(recipe.region_id, sizeof(recipe.region_id)) ||
        !stable_id(recipe.source_asset_id, sizeof(recipe.source_asset_id)) ||
        recipe.patch_count == 0u ||
        recipe.patch_count > PROCEDURAL_IMPORTED_SURFACE_REGION_MAX_PATCHES)
        goto fail;
    for (size_t i = 0u; i < recipe.patch_count; ++i) {
        json_object *entry = json_object_array_get_idx(patches, i);
        json_object *seed = NULL;
        ProceduralImportedSurfaceRegionPatchV1 *patch = &recipe.patches[i];
        if (!entry || !json_vec3(entry, "center", patch->center) ||
            !json_vec3(entry, "radius", patch->radius) ||
            !json_number(entry, "feather", &patch->feather) ||
            !json_number(entry, "noise_scale", &patch->noise_scale) ||
            !json_number(entry, "noise_strength", &patch->noise_strength) ||
            !json_number(entry, "strength", &patch->strength) ||
            !json_object_object_get_ex(entry, "seed", &seed)) goto fail;
        patch->seed = json_object_get_int(seed);
        for (size_t axis = 0u; axis < 3u; ++axis) {
            if (patch->center[axis] < 0.0 || patch->center[axis] > 1.0 ||
                patch->radius[axis] <= 0.0 || patch->radius[axis] > 2.0)
                goto fail;
        }
        if (patch->feather <= 0.0 || patch->feather > 1.0 ||
            patch->noise_scale < 0.0 || patch->noise_scale > 128.0 ||
            patch->noise_strength < 0.0 || patch->noise_strength > 1.0 ||
            patch->strength <= 0.0 || patch->strength > 1.0) goto fail;
    }
    *out_recipe = recipe;
    json_object_put(root);
    report_set(report, true, "", "ok");
    return true;
fail:
    if (root) json_object_put(root);
    return false;
}

static double patch_noise(
    const double p[3],
    const ProceduralImportedSurfaceRegionPatchV1 *patch) {
    double seed = (double)patch->seed;
    double a = sin((p[0] * 1.31 + p[1] * 0.73 + p[2] * 1.91) *
                   patch->noise_scale + seed * 0.017);
    double b = sin((p[0] * -0.47 + p[1] * 1.67 + p[2] * 0.89) *
                   patch->noise_scale * 1.73 + seed * 0.031);
    return 0.5 * (0.5 * (a + b) + 1.0);
}

bool ProceduralImportedSurfaceRegionV1_Compile(
    const ProceduralImportedSurfaceRegionRecipeV1 *recipe,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *source_runtime_path,
    ProceduralImportedSurfaceRegionV1 *out_region,
    ProceduralImportedSurfaceRegionReport *report) {
    ProceduralImportedSurfaceRegionV1 region;
    double extent[3];
    double sum = 0.0;
    if (!recipe || !mesh || !source_runtime_path || !out_region ||
        strcmp(recipe->source_asset_id, mesh->contract.asset_id) != 0) {
        report_set(report, false, "identity",
                   "recipe source_asset_id does not match runtime mesh");
        return false;
    }
    ProceduralImportedSurfaceRegionV1_Init(&region);
    region.schema_version = PROCEDURAL_IMPORTED_SURFACE_REGION_SCHEMA_VERSION;
    snprintf(region.region_id, sizeof(region.region_id), "%s",
             recipe->region_id);
    snprintf(region.source_asset_id, sizeof(region.source_asset_id), "%s",
             recipe->source_asset_id);
    region.vertex_count = mesh->vertex_count;
    region.triangle_count = mesh->triangle_count;
    region.topology_unchanged = true;
    region.source_triangle_provenance_retained = true;
    region.minimum = 1.0;
    if (!ProceduralSolidMesh_Digest(
            mesh, region.source_mesh_digest_sha256) ||
        !ray_tracing_sha256_file(
            source_runtime_path, region.source_file_digest_sha256) ||
        !recipe_digest(recipe, region.recipe_digest_sha256)) goto fail;
    region.vertex_weights = calloc(
        mesh->vertex_count, sizeof(*region.vertex_weights));
    if (!region.vertex_weights && mesh->vertex_count > 0u) goto fail;
    {
        const double bounds_min[3] = {
            mesh->contract.local_bounds.min.x,
            mesh->contract.local_bounds.min.y,
            mesh->contract.local_bounds.min.z};
        const double bounds_max[3] = {
            mesh->contract.local_bounds.max.x,
            mesh->contract.local_bounds.max.y,
            mesh->contract.local_bounds.max.z};
        for (size_t axis = 0u; axis < 3u; ++axis) {
            extent[axis] = bounds_max[axis] - bounds_min[axis];
            if (!(extent[axis] > 0.0)) extent[axis] = 1.0;
        }
        for (size_t i = 0u; i < mesh->vertex_count; ++i) {
            const double position[3] = {
                mesh->vertices[i].position.x,
                mesh->vertices[i].position.y,
                mesh->vertices[i].position.z};
            double p[3];
            double weight = 0.0;
            for (size_t axis = 0u; axis < 3u; ++axis) {
                p[axis] = (position[axis] - bounds_min[axis]) / extent[axis];
            }
            for (size_t j = 0u; j < recipe->patch_count; ++j) {
                const ProceduralImportedSurfaceRegionPatchV1 *patch =
                    &recipe->patches[j];
                double d2 = 0.0;
                for (size_t axis = 0u; axis < 3u; ++axis) {
                    double q = (p[axis] - patch->center[axis]) /
                               patch->radius[axis];
                    d2 += q * q;
                }
                {
                    double modulation =
                        (patch_noise(p, patch) - 0.5) * patch->noise_strength;
                    double signed_distance = 1.0 - sqrt(d2) + modulation;
                    double candidate =
                        smooth01(signed_distance / patch->feather) *
                        patch->strength;
                    if (candidate > weight) weight = candidate;
                }
            }
            region.vertex_weights[i] = clamp01(weight);
            if (region.vertex_weights[i] < region.minimum)
                region.minimum = region.vertex_weights[i];
            if (region.vertex_weights[i] > region.maximum)
                region.maximum = region.vertex_weights[i];
            if (region.vertex_weights[i] > 1e-6 &&
                region.vertex_weights[i] < 1.0 - 1e-6)
                region.transition_vertex_count += 1u;
            sum += region.vertex_weights[i];
        }
    }
    region.mean = mesh->vertex_count > 0u
                      ? sum / (double)mesh->vertex_count : 0.0;
    if (!values_digest(region.vertex_weights, region.vertex_count,
                       region.value_digest_sha256)) goto fail;
    ProceduralImportedSurfaceRegionV1_Free(out_region);
    *out_region = region;
    report_set(report, true, "", "ok");
    return true;
fail:
    ProceduralImportedSurfaceRegionV1_Free(&region);
    report_set(report, false, "compile",
               "failed to compile deterministic imported-surface region");
    return false;
}

bool ProceduralImportedSurfaceRegionV1_SaveJsonFileAtomic(
    const char *path,
    const ProceduralImportedSurfaceRegionV1 *region,
    ProceduralImportedSurfaceRegionReport *report) {
    json_object *root = NULL;
    json_object *weights = NULL;
    const char *text;
    CoreResult result;
    if (!path || !region || !region->vertex_weights) return false;
    root = json_object_new_object();
    weights = json_object_new_array();
    if (!root || !weights) goto fail;
#define ADD_STRING(key, value) \
    json_object_object_add(root, key, json_object_new_string(value))
    ADD_STRING("schema", PROCEDURAL_IMPORTED_SURFACE_REGION_SCHEMA);
    json_object_object_add(root, "schema_version",
        json_object_new_int((int)region->schema_version));
    ADD_STRING("region_id", region->region_id);
    ADD_STRING("source_asset_id", region->source_asset_id);
    ADD_STRING("source_mesh_digest_sha256",
               region->source_mesh_digest_sha256);
    ADD_STRING("source_file_digest_sha256",
               region->source_file_digest_sha256);
    ADD_STRING("recipe_digest_sha256", region->recipe_digest_sha256);
    ADD_STRING("value_digest_sha256", region->value_digest_sha256);
#undef ADD_STRING
    json_object_object_add(root, "vertex_count",
        json_object_new_int64((int64_t)region->vertex_count));
    json_object_object_add(root, "triangle_count",
        json_object_new_int64((int64_t)region->triangle_count));
    json_object_object_add(root, "minimum",
        json_object_new_double(region->minimum));
    json_object_object_add(root, "maximum",
        json_object_new_double(region->maximum));
    json_object_object_add(root, "mean", json_object_new_double(region->mean));
    json_object_object_add(root, "transition_vertex_count",
        json_object_new_int64((int64_t)region->transition_vertex_count));
    json_object_object_add(root, "topology_unchanged",
        json_object_new_boolean(region->topology_unchanged));
    json_object_object_add(root, "source_triangle_provenance_retained",
        json_object_new_boolean(region->source_triangle_provenance_retained));
    for (size_t i = 0u; i < region->vertex_count; ++i)
        json_object_array_add(weights,
            json_object_new_double(region->vertex_weights[i]));
    json_object_object_add(root, "vertex_weights", weights);
    weights = NULL;
    text = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    result = core_io_write_all_atomic(path, text, strlen(text));
    json_object_put(root);
    if (result.code != CORE_OK) goto fail_no_root;
    report_set(report, true, "", "ok");
    return true;
fail:
    if (weights) json_object_put(weights);
    if (root) json_object_put(root);
fail_no_root:
    report_set(report, false, "write", "failed to save region artifact");
    return false;
}

bool ProceduralImportedSurfaceRegionV1_ValidateForMesh(
    const ProceduralImportedSurfaceRegionV1 *region,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *source_runtime_path,
    ProceduralImportedSurfaceRegionReport *report) {
    char mesh_digest[PROCEDURAL_IMPORTED_SURFACE_REGION_DIGEST_CAPACITY] = {0};
    char file_digest[PROCEDURAL_IMPORTED_SURFACE_REGION_DIGEST_CAPACITY] = {0};
    char value_digest[PROCEDURAL_IMPORTED_SURFACE_REGION_DIGEST_CAPACITY] = {0};
    bool ok = region && mesh && source_runtime_path &&
        region->schema_version == PROCEDURAL_IMPORTED_SURFACE_REGION_SCHEMA_VERSION &&
        strcmp(region->source_asset_id, mesh->contract.asset_id) == 0 &&
        region->vertex_count == mesh->vertex_count &&
        region->triangle_count == mesh->triangle_count &&
        region->topology_unchanged &&
        region->source_triangle_provenance_retained &&
        region->vertex_weights &&
        ProceduralSolidMesh_Digest(mesh, mesh_digest) &&
        ray_tracing_sha256_file(source_runtime_path, file_digest) &&
        values_digest(region->vertex_weights, region->vertex_count,
                      value_digest) &&
        strcmp(mesh_digest, region->source_mesh_digest_sha256) == 0 &&
        strcmp(file_digest, region->source_file_digest_sha256) == 0 &&
        strcmp(value_digest, region->value_digest_sha256) == 0;
    report_set(report, ok, ok ? "" : "identity",
               ok ? "ok" : "region artifact is stale for source mesh");
    return ok;
}

bool ProceduralImportedSurfaceRegionV1_LoadJsonFile(
    const char *path,
    const CoreMeshAssetRuntimeDocument *mesh,
    const char *source_runtime_path,
    ProceduralImportedSurfaceRegionV1 *out_region,
    ProceduralImportedSurfaceRegionReport *report) {
    json_object *root = NULL;
    json_object *value = NULL;
    json_object *weights = NULL;
    ProceduralImportedSurfaceRegionV1 region;
    ProceduralImportedSurfaceRegionV1_Init(&region);
    if (!path || !mesh || !source_runtime_path || !out_region ||
        !(root = json_object_from_file(path))) goto fail;
#define READ_STRING(key, target) \
    do { \
        if (!json_object_object_get_ex(root, key, &value)) goto fail; \
        snprintf(target, sizeof(target), "%s", json_object_get_string(value)); \
    } while (0)
    if (!json_object_object_get_ex(root, "schema", &value) ||
        strcmp(json_object_get_string(value),
               PROCEDURAL_IMPORTED_SURFACE_REGION_SCHEMA) != 0 ||
        !json_object_object_get_ex(root, "schema_version", &value))
        goto fail;
    region.schema_version = (uint32_t)json_object_get_int(value);
    READ_STRING("region_id", region.region_id);
    READ_STRING("source_asset_id", region.source_asset_id);
    READ_STRING("source_mesh_digest_sha256",
                region.source_mesh_digest_sha256);
    READ_STRING("source_file_digest_sha256",
                region.source_file_digest_sha256);
    READ_STRING("recipe_digest_sha256", region.recipe_digest_sha256);
    READ_STRING("value_digest_sha256", region.value_digest_sha256);
#undef READ_STRING
#define READ_SIZE(key, target) \
    do { \
        if (!json_object_object_get_ex(root, key, &value)) goto fail; \
        target = (size_t)json_object_get_int64(value); \
    } while (0)
    READ_SIZE("vertex_count", region.vertex_count);
    READ_SIZE("triangle_count", region.triangle_count);
    READ_SIZE("transition_vertex_count", region.transition_vertex_count);
#undef READ_SIZE
    if (!json_number(root, "minimum", &region.minimum) ||
        !json_number(root, "maximum", &region.maximum) ||
        !json_number(root, "mean", &region.mean) ||
        !json_object_object_get_ex(root, "topology_unchanged", &value))
        goto fail;
    region.topology_unchanged = json_object_get_boolean(value);
    if (!json_object_object_get_ex(
            root, "source_triangle_provenance_retained", &value))
        goto fail;
    region.source_triangle_provenance_retained =
        json_object_get_boolean(value);
    if (!json_object_object_get_ex(root, "vertex_weights", &weights) ||
        !json_object_is_type(weights, json_type_array) ||
        json_object_array_length(weights) != region.vertex_count)
        goto fail;
    region.vertex_weights = calloc(
        region.vertex_count, sizeof(*region.vertex_weights));
    if (!region.vertex_weights && region.vertex_count > 0u) goto fail;
    for (size_t i = 0u; i < region.vertex_count; ++i) {
        json_object *entry = json_object_array_get_idx(weights, i);
        region.vertex_weights[i] = json_object_get_double(entry);
        if (!isfinite(region.vertex_weights[i]) ||
            region.vertex_weights[i] < 0.0 ||
            region.vertex_weights[i] > 1.0) goto fail;
    }
    if (!ProceduralImportedSurfaceRegionV1_ValidateForMesh(
            &region, mesh, source_runtime_path, report)) goto fail;
    ProceduralImportedSurfaceRegionV1_Free(out_region);
    *out_region = region;
    json_object_put(root);
    return true;
fail:
    ProceduralImportedSurfaceRegionV1_Free(&region);
    if (root) json_object_put(root);
    if (!report || report->ok || !report->message[0])
        report_set(report, false, "artifact", "invalid region artifact");
    return false;
}
