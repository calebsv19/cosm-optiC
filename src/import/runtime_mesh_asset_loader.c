#include "import/runtime_mesh_asset_loader_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include <json-c/json.h>

#include "config/config_manager.h"
#include "core_io.h"
#include "core_scene.h"

static RayTracingRuntimeMeshAssetSet g_last_runtime_mesh_assets;
static bool g_last_runtime_mesh_asset_scene_stamp_valid = false;
static char g_last_runtime_mesh_asset_scene_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
static long long g_last_runtime_mesh_asset_scene_mtime_sec = 0;
static long long g_last_runtime_mesh_asset_scene_mtime_nsec = 0;
static long long g_last_runtime_mesh_asset_scene_file_size = 0;

static bool ray_tracing_runtime_mesh_asset_resolve_path_with_hint(const char* runtime_scene_path,
                                                                  const char* asset_id,
                                                                  const char* explicit_runtime_path,
                                                                  char* out_path,
                                                                  size_t out_path_size,
                                                                  char* out_diagnostics,
                                                                  size_t out_diagnostics_size);

static void runtime_mesh_asset_clear_last_scene_stamp(void) {
    g_last_runtime_mesh_asset_scene_stamp_valid = false;
    g_last_runtime_mesh_asset_scene_path[0] = '\0';
    g_last_runtime_mesh_asset_scene_mtime_sec = 0;
    g_last_runtime_mesh_asset_scene_mtime_nsec = 0;
    g_last_runtime_mesh_asset_scene_file_size = 0;
}

static void runtime_mesh_asset_capture_last_scene_stamp(const char* runtime_scene_path) {
    long long mtime_sec = 0;
    long long mtime_nsec = 0;
    long long file_size = 0;
    runtime_mesh_asset_clear_last_scene_stamp();
    if (!runtime_scene_path || !runtime_scene_path[0]) return;
    if (!runtime_mesh_asset_stat_path(runtime_scene_path, &mtime_sec, &mtime_nsec, &file_size)) {
        return;
    }
    snprintf(g_last_runtime_mesh_asset_scene_path,
             sizeof(g_last_runtime_mesh_asset_scene_path),
             "%s",
             runtime_scene_path);
    g_last_runtime_mesh_asset_scene_mtime_sec = mtime_sec;
    g_last_runtime_mesh_asset_scene_mtime_nsec = mtime_nsec;
    g_last_runtime_mesh_asset_scene_file_size = file_size;
    g_last_runtime_mesh_asset_scene_stamp_valid = true;
}


static bool runtime_mesh_asset_append_instance(RayTracingRuntimeMeshAssetSet* set,
                                               const char* object_id,
                                               const char* asset_id,
                                               int asset_index,
                                               int scene_object_index,
                                               json_object* object,
                                               double world_scale,
                                               char* out_diagnostics,
                                               size_t out_diagnostics_size) {
    RayTracingRuntimeMeshAssetInstance* instance = NULL;
    if (!set || !object_id || !object_id[0] || !asset_id || !asset_id[0] || asset_index < 0) {
        runtime_mesh_asset_diag(out_diagnostics,
                                out_diagnostics_size,
                                "mesh asset instance is invalid");
        return false;
    }
    if (set->instance_count >= RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES) {
        runtime_mesh_asset_diag(out_diagnostics,
                                out_diagnostics_size,
                                "too many mesh asset instances");
        return false;
    }
    instance = &set->instances[set->instance_count++];
    memset(instance, 0, sizeof(*instance));
    if (!runtime_mesh_asset_copy_id(instance->object_id,
                                    sizeof(instance->object_id),
                                    object_id,
                                    out_diagnostics,
                                    out_diagnostics_size) ||
        !runtime_mesh_asset_copy_id(instance->asset_id,
                                    sizeof(instance->asset_id),
                                    asset_id,
                                    out_diagnostics,
                                    out_diagnostics_size)) {
        set->instance_count -= 1;
        memset(instance, 0, sizeof(*instance));
        return false;
    }
    instance->asset_index = asset_index;
    instance->scene_object_index = scene_object_index;
    runtime_mesh_asset_read_transform(object, world_scale, instance);
    return true;
}

static bool runtime_mesh_asset_load_unique_asset(const char* runtime_scene_path,
                                                 const char* asset_id,
                                                 const char* explicit_runtime_path,
                                                 RayTracingRuntimeMeshAssetSet* set,
                                                 int* out_asset_index,
                                                 bool* out_skipped,
                                                 char* out_resolved_path,
                                                 size_t out_resolved_path_size,
                                                 size_t* out_file_size_bytes,
                                                 size_t max_asset_file_bytes,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size) {
    RayTracingRuntimeMeshAsset* asset = NULL;
    char resolved_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    int existing = runtime_mesh_asset_find_asset_index(set, asset_id);
    struct timespec resolve_start = {0};

    if (out_asset_index) *out_asset_index = -1;
    if (out_skipped) *out_skipped = false;
    if (out_resolved_path && out_resolved_path_size > 0u) out_resolved_path[0] = '\0';
    if (out_file_size_bytes) *out_file_size_bytes = 0u;
    if (existing >= 0) {
        if (out_asset_index) *out_asset_index = existing;
        return true;
    }
    if (!set || !asset_id || !asset_id[0]) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset id missing");
        return false;
    }
    if (set->asset_count >= RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "too many mesh assets");
        return false;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &resolve_start);
    if (!ray_tracing_runtime_mesh_asset_resolve_path_with_hint(runtime_scene_path,
                                                               asset_id,
                                                               explicit_runtime_path,
                                                               resolved_path,
                                                               sizeof(resolved_path),
                                                               out_diagnostics,
                                                               out_diagnostics_size)) {
        g_runtime_mesh_asset_timing.sidecar_path_resolution_ms +=
            runtime_mesh_asset_elapsed_ms_since(&resolve_start);
        return false;
    }
    g_runtime_mesh_asset_timing.sidecar_path_resolution_ms +=
        runtime_mesh_asset_elapsed_ms_since(&resolve_start);
    if (max_asset_file_bytes > 0u) {
        struct stat st;
        if (stat(resolved_path, &st) != 0) {
            runtime_mesh_asset_diag(out_diagnostics,
                                    out_diagnostics_size,
                                    "mesh asset runtime file stat failed");
            return false;
        }
        if (out_file_size_bytes) *out_file_size_bytes = (size_t)st.st_size;
        if (st.st_size > (off_t)max_asset_file_bytes) {
            if (out_skipped) *out_skipped = true;
            if (out_resolved_path && out_resolved_path_size > 0u) {
                snprintf(out_resolved_path, out_resolved_path_size, "%s", resolved_path);
            }
            runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
            return true;
        }
    }
    if (out_resolved_path && out_resolved_path_size > 0u) {
        snprintf(out_resolved_path, out_resolved_path_size, "%s", resolved_path);
    }

    asset = &set->assets[set->asset_count];
    memset(asset, 0, sizeof(*asset));
    core_mesh_asset_runtime_document_init(&asset->document);
    if (!runtime_mesh_asset_copy_id(asset->asset_id,
                                    sizeof(asset->asset_id),
                                    asset_id,
                                    out_diagnostics,
                                    out_diagnostics_size)) {
        core_mesh_asset_runtime_document_free(&asset->document);
        return false;
    }
    snprintf(asset->path, sizeof(asset->path), "%s", resolved_path);
    asset->file_stamp_valid =
        runtime_mesh_asset_stat_path(resolved_path,
                                     &asset->file_mtime_sec,
                                     &asset->file_mtime_nsec,
                                     &asset->file_size_bytes);
    runtime_mesh_asset_probe_preview(resolved_path, &asset->preview);
    if (!runtime_mesh_asset_load_document_cached(resolved_path,
                                                asset_id,
                                                &asset->document,
                                                out_diagnostics,
                                                out_diagnostics_size)) {
        char message[256] = {0};
        snprintf(message,
                 sizeof(message),
                 "mesh asset '%s' invalid: %s",
                 asset_id,
                 (out_diagnostics && out_diagnostics[0]) ? out_diagnostics : "validation failed");
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, message);
        core_mesh_asset_runtime_document_free(&asset->document);
        memset(asset, 0, sizeof(*asset));
        return false;
    }
    if (strcmp(asset->document.contract.asset_id, asset_id) != 0) {
        char message[256] = {0};
        snprintf(message, sizeof(message), "mesh asset '%s' file asset_id mismatch", asset_id);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, message);
        core_mesh_asset_runtime_document_free(&asset->document);
        memset(asset, 0, sizeof(*asset));
        return false;
    }
    if (out_asset_index) *out_asset_index = set->asset_count;
    set->asset_count += 1;
    return true;
}

void ray_tracing_runtime_mesh_asset_set_init(RayTracingRuntimeMeshAssetSet* set) {
    if (!set) return;
    memset(set, 0, sizeof(*set));
}

void ray_tracing_runtime_mesh_asset_set_free(RayTracingRuntimeMeshAssetSet* set) {
    int i = 0;
    if (!set) return;
    for (i = 0; i < set->asset_count; ++i) {
        core_mesh_asset_runtime_document_free(&set->assets[i].document);
    }
    memset(set, 0, sizeof(*set));
}

bool ray_tracing_runtime_mesh_asset_resolve_path(const char* runtime_scene_path,
                                                 const char* asset_id,
                                                 char* out_path,
                                                 size_t out_path_size,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size) {
    return ray_tracing_runtime_mesh_asset_resolve_path_with_hint(runtime_scene_path,
                                                                 asset_id,
                                                                 NULL,
                                                                 out_path,
                                                                 out_path_size,
                                                                 out_diagnostics,
                                                                 out_diagnostics_size);
}

static bool ray_tracing_runtime_mesh_asset_resolve_path_with_hint(const char* runtime_scene_path,
                                                                  const char* asset_id,
                                                                  const char* explicit_runtime_path,
                                                                  char* out_path,
                                                                  size_t out_path_size,
                                                                  char* out_diagnostics,
                                                                  size_t out_diagnostics_size) {
    char base_dir[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    char relative_path[256] = {0};
    CoreResult resolve_result = core_result_ok();

    if (out_path && out_path_size > 0u) out_path[0] = '\0';
    if (!runtime_scene_path || !runtime_scene_path[0] || !asset_id || !asset_id[0] ||
        !out_path || out_path_size == 0u) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset resolve input missing");
        return false;
    }
    if (core_scene_dirname(runtime_scene_path, base_dir, sizeof(base_dir)).code != CORE_OK) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "runtime scene directory invalid");
        return false;
    }

    if (explicit_runtime_path && explicit_runtime_path[0]) {
        char resolved_hint[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
        const char* use_hint = explicit_runtime_path;
        if (explicit_runtime_path[0] != '/') {
            resolve_result = core_scene_resolve_path(base_dir,
                                                     explicit_runtime_path,
                                                     resolved_hint,
                                                     sizeof(resolved_hint));
            if (resolve_result.code == CORE_OK && resolved_hint[0]) {
                use_hint = resolved_hint;
            }
        }
        if (core_io_path_exists(use_hint)) {
            snprintf(out_path, out_path_size, "%s", use_hint);
            runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
            return true;
        }
    }

    snprintf(relative_path, sizeof(relative_path), "assets/mesh_assets/%s.runtime.json", asset_id);
    resolve_result = core_scene_resolve_path(base_dir, relative_path, out_path, out_path_size);
    if (resolve_result.code == CORE_OK && out_path[0] && core_io_path_exists(out_path)) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
        return true;
    }

    snprintf(relative_path, sizeof(relative_path), "mesh_assets/%s.runtime.json", asset_id);
    resolve_result = core_scene_resolve_path(base_dir, relative_path, out_path, out_path_size);
    if (resolve_result.code == CORE_OK && out_path[0] && core_io_path_exists(out_path)) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
        return true;
    }

    {
        const char* config_mesh_asset_root = animSettings.meshAssetRoot;
        const char* mesh_asset_root = getenv("RAY_TRACING_MESH_ASSET_ROOT");
        const char* input_root = getenv("RAY_TRACING_INPUT_ROOT");

        if (runtime_mesh_asset_try_asset_root(config_mesh_asset_root,
                                              asset_id,
                                              out_path,
                                              out_path_size,
                                              out_diagnostics,
                                              out_diagnostics_size)) {
            return true;
        }
        if (runtime_mesh_asset_try_asset_root(mesh_asset_root,
                                              asset_id,
                                              out_path,
                                              out_path_size,
                                              out_diagnostics,
                                              out_diagnostics_size)) {
            return true;
        }
        if (runtime_mesh_asset_try_asset_root(input_root,
                                              asset_id,
                                              out_path,
                                              out_path_size,
                                              out_diagnostics,
                                              out_diagnostics_size)) {
            return true;
        }
    }

    {
        char message[256] = {0};
        snprintf(message, sizeof(message), "mesh asset '%s' runtime file not found", asset_id);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, message);
    }
    if (out_path && out_path_size > 0u) out_path[0] = '\0';
    return false;
}

static bool ray_tracing_runtime_mesh_assets_load_scene_file_with_options(
    const char* runtime_scene_path,
    size_t max_asset_file_bytes,
    RayTracingRuntimeMeshAssetSet* out_set,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    char* scene_text = NULL;
    json_object* root = NULL;
    json_object* objects = NULL;
    json_object* world_scale_obj = NULL;
    double world_scale = 1.0;
    int object_count = 0;
    int i = 0;
    int runtime_object_index = 0;
    struct timespec total_start = {0};
    struct timespec stage_start = {0};

    ray_tracing_runtime_mesh_assets_timing_reset();
    g_runtime_mesh_asset_timing.asset_persistent_cache_mode =
        runtime_mesh_asset_pack_cache_mode();
    (void)clock_gettime(CLOCK_MONOTONIC, &total_start);
    if (!out_set) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset output missing");
        return false;
    }
    ray_tracing_runtime_mesh_asset_set_init(out_set);
    runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    if (!runtime_mesh_asset_read_text(runtime_scene_path,
                                      &scene_text,
                                      out_diagnostics,
                                      out_diagnostics_size)) {
        return false;
    }
    g_runtime_mesh_asset_timing.scene_read_ms +=
        runtime_mesh_asset_elapsed_ms_since(&stage_start);

    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    root = json_tokener_parse(scene_text);
    free(scene_text);
    scene_text = NULL;
    g_runtime_mesh_asset_timing.scene_parse_ms +=
        runtime_mesh_asset_elapsed_ms_since(&stage_start);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "invalid runtime scene json");
        return false;
    }

    if (!json_object_object_get_ex(root, "objects", &objects) ||
        !json_object_is_type(objects, json_type_array)) {
        json_object_put(root);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "runtime scene objects missing");
        return false;
    }

    object_count = (int)json_object_array_length(objects);
    if (json_object_object_get_ex(root, "world_scale", &world_scale_obj)) {
        world_scale = json_object_get_double(world_scale_obj);
    }
    for (i = 0; i < object_count; ++i) {
        json_object* object = json_object_array_get_idx(objects, (size_t)i);
        json_object* geometry_ref = NULL;
        const char* object_type = NULL;
        const char* object_id = NULL;
        const char* geometry_kind = NULL;
        const char* asset_id = NULL;
        const char* explicit_runtime_path = NULL;
        int asset_index = -1;
        bool asset_skipped = false;
        char resolved_asset_path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
        size_t asset_file_size_bytes = 0u;

        if (!object || !json_object_is_type(object, json_type_object)) continue;
        object_type = runtime_mesh_asset_string_field(object, "object_type");
        if (runtime_mesh_asset_is_authoring_helper_object_type(object_type)) continue;
        if (!object_type || strcmp(object_type, "mesh_asset_instance") != 0) {
            runtime_object_index += 1;
            continue;
        }

        object_id = runtime_mesh_asset_string_field(object, "object_id");
        if (!object_id || !object_id[0]) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset object_id missing");
            return false;
        }
        if (!json_object_object_get_ex(object, "geometry_ref", &geometry_ref) ||
            !json_object_is_type(geometry_ref, json_type_object)) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            runtime_mesh_asset_diag(out_diagnostics,
                                    out_diagnostics_size,
                                    "mesh_asset_instance geometry_ref missing");
            return false;
        }
        geometry_kind = runtime_mesh_asset_string_field(geometry_ref, "kind");
        asset_id = runtime_mesh_asset_string_field(geometry_ref, "id");
        if (!geometry_kind || strcmp(geometry_kind, "mesh_asset") != 0) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            runtime_mesh_asset_diag(out_diagnostics,
                                    out_diagnostics_size,
                                    "mesh_asset_instance geometry_ref.kind must be mesh_asset");
            return false;
        }
        if (!asset_id || !asset_id[0]) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            runtime_mesh_asset_diag(out_diagnostics,
                                    out_diagnostics_size,
                                    "mesh_asset_instance geometry_ref.id missing");
            return false;
        }
        explicit_runtime_path = runtime_mesh_asset_object_runtime_path(object);

        if (!runtime_mesh_asset_load_unique_asset(runtime_scene_path,
                                                 asset_id,
                                                 explicit_runtime_path,
                                                 out_set,
                                                 &asset_index,
                                                 &asset_skipped,
                                                 resolved_asset_path,
                                                 sizeof(resolved_asset_path),
                                                 &asset_file_size_bytes,
                                                 max_asset_file_bytes,
                                                 out_diagnostics,
                                                 out_diagnostics_size)) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            return false;
        }
        if (!asset_skipped &&
            !runtime_mesh_asset_append_instance(out_set,
                                                object_id,
                                                asset_id,
                                                asset_index,
                                                runtime_object_index,
                                                object,
                                                world_scale,
                                                out_diagnostics,
                                                out_diagnostics_size)) {
            json_object_put(root);
            ray_tracing_runtime_mesh_asset_set_free(out_set);
            return false;
        }
        if (asset_skipped) {
            if (out_set->skipped_instance_count <
                RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES) {
                RayTracingRuntimeMeshAssetSkippedInstance* skipped =
                    &out_set->skipped_instances[out_set->skipped_instance_count];
                memset(skipped, 0, sizeof(*skipped));
                runtime_mesh_asset_copy_id(skipped->object_id,
                                           sizeof(skipped->object_id),
                                           object_id,
                                           out_diagnostics,
                                           out_diagnostics_size);
                runtime_mesh_asset_copy_id(skipped->asset_id,
                                           sizeof(skipped->asset_id),
                                           asset_id,
                                           out_diagnostics,
                                           out_diagnostics_size);
                snprintf(skipped->path,
                         sizeof(skipped->path),
                         "%s",
                         resolved_asset_path);
                skipped->scene_object_index = runtime_object_index;
                skipped->file_size_bytes = asset_file_size_bytes;
                skipped->max_file_size_bytes = max_asset_file_bytes;
                runtime_mesh_asset_probe_preview(resolved_asset_path, &skipped->preview);
                out_set->skipped_instance_count += 1;
            }
        }
        runtime_object_index += 1;
    }

    json_object_put(root);
    g_runtime_mesh_asset_timing.loaded_assets = out_set->asset_count;
    g_runtime_mesh_asset_timing.loaded_instances = out_set->instance_count;
    for (i = 0; i < out_set->asset_count; ++i) {
        const RayTracingRuntimeMeshAsset* asset = &out_set->assets[i];
        struct stat st;
        if (stat(asset->path, &st) == 0 && st.st_size > 0) {
            g_runtime_mesh_asset_timing.loaded_asset_bytes +=
                (unsigned long long)st.st_size;
        }
        g_runtime_mesh_asset_timing.loaded_vertices +=
            (unsigned long long)asset->document.vertex_count;
        g_runtime_mesh_asset_timing.loaded_triangles +=
            (unsigned long long)asset->document.triangle_count;
    }
    g_runtime_mesh_asset_timing.total_ms += runtime_mesh_asset_elapsed_ms_since(&total_start);
    runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool ray_tracing_runtime_mesh_assets_load_scene_file(const char* runtime_scene_path,
                                                     RayTracingRuntimeMeshAssetSet* out_set,
                                                     char* out_diagnostics,
                                                     size_t out_diagnostics_size) {
    return ray_tracing_runtime_mesh_assets_load_scene_file_with_options(runtime_scene_path,
                                                                        0u,
                                                                        out_set,
                                                                        out_diagnostics,
                                                                        out_diagnostics_size);
}

bool ray_tracing_runtime_mesh_assets_load_scene_file_preview_limited(
    const char* runtime_scene_path,
    size_t max_asset_file_bytes,
    RayTracingRuntimeMeshAssetSet* out_set,
    char* out_diagnostics,
    size_t out_diagnostics_size) {
    return ray_tracing_runtime_mesh_assets_load_scene_file_with_options(runtime_scene_path,
                                                                        max_asset_file_bytes,
                                                                        out_set,
                                                                        out_diagnostics,
                                                                        out_diagnostics_size);
}

void ray_tracing_runtime_mesh_assets_reset_last(void) {
    ray_tracing_runtime_mesh_asset_set_free(&g_last_runtime_mesh_assets);
    runtime_mesh_asset_clear_last_scene_stamp();
}

void ray_tracing_runtime_mesh_assets_take_last(RayTracingRuntimeMeshAssetSet* loaded) {
    ray_tracing_runtime_mesh_assets_take_last_for_scene(NULL, loaded);
}

void ray_tracing_runtime_mesh_assets_take_last_for_scene(const char* runtime_scene_path,
                                                        RayTracingRuntimeMeshAssetSet* loaded) {
    ray_tracing_runtime_mesh_assets_reset_last();
    if (!loaded) return;
    g_last_runtime_mesh_assets = *loaded;
    memset(loaded, 0, sizeof(*loaded));
    runtime_mesh_asset_capture_last_scene_stamp(runtime_scene_path);
}

bool ray_tracing_runtime_mesh_assets_last_matches_scene_file(const char* runtime_scene_path) {
    if (!runtime_scene_path || !runtime_scene_path[0]) return false;
    if (!g_last_runtime_mesh_asset_scene_stamp_valid) return false;
    if (strcmp(g_last_runtime_mesh_asset_scene_path, runtime_scene_path) != 0) return false;
    if (!runtime_mesh_asset_stamp_matches_path(runtime_scene_path,
                                              g_last_runtime_mesh_asset_scene_mtime_sec,
                                              g_last_runtime_mesh_asset_scene_mtime_nsec,
                                              g_last_runtime_mesh_asset_scene_file_size)) {
        return false;
    }
    for (int i = 0; i < g_last_runtime_mesh_assets.asset_count; ++i) {
        const RayTracingRuntimeMeshAsset* asset = &g_last_runtime_mesh_assets.assets[i];
        if (!asset->file_stamp_valid) return false;
        if (!runtime_mesh_asset_stamp_matches_path(asset->path,
                                                  asset->file_mtime_sec,
                                                  asset->file_mtime_nsec,
                                                  asset->file_size_bytes)) {
            return false;
        }
    }
    return true;
}

bool ray_tracing_runtime_mesh_assets_load_scene_file_to_last(const char* runtime_scene_path,
                                                            char* out_diagnostics,
                                                            size_t out_diagnostics_size) {
    RayTracingRuntimeMeshAssetSet loaded;
    ray_tracing_runtime_mesh_asset_set_init(&loaded);
    if (!ray_tracing_runtime_mesh_assets_load_scene_file(runtime_scene_path,
                                                        &loaded,
                                                        out_diagnostics,
                                                        out_diagnostics_size)) {
        return false;
    }
    ray_tracing_runtime_mesh_assets_take_last_for_scene(runtime_scene_path, &loaded);
    return true;
}

const RayTracingRuntimeMeshAssetSet* ray_tracing_runtime_mesh_assets_last(void) {
    return &g_last_runtime_mesh_assets;
}
