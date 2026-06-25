#include "import/runtime_mesh_asset_loader.h"

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

typedef struct RuntimeMeshAssetCacheEntry {
    bool valid;
    char asset_id[64];
    char path[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    long long mtime_sec;
    long long mtime_nsec;
    long long file_size;
    CoreMeshAssetRuntimeDocument document;
} RuntimeMeshAssetCacheEntry;

static RuntimeMeshAssetCacheEntry
    g_runtime_mesh_asset_cache[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS];
static unsigned long long g_runtime_mesh_asset_cache_hits = 0u;
static unsigned long long g_runtime_mesh_asset_cache_misses = 0u;
static unsigned long long g_runtime_mesh_asset_cache_invalidations = 0u;
static RayTracingRuntimeMeshAssetTimingStats g_runtime_mesh_asset_timing;

static double runtime_mesh_asset_elapsed_ms_since(const struct timespec* start_time) {
    struct timespec now = {0};
    double elapsed = 0.0;
    if (!start_time) return 0.0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    elapsed = (double)(now.tv_sec - start_time->tv_sec) * 1000.0;
    elapsed += (double)(now.tv_nsec - start_time->tv_nsec) / 1000000.0;
    return elapsed < 0.0 ? 0.0 : elapsed;
}

static bool ray_tracing_runtime_mesh_asset_resolve_path_with_hint(const char* runtime_scene_path,
                                                                  const char* asset_id,
                                                                  const char* explicit_runtime_path,
                                                                  char* out_path,
                                                                  size_t out_path_size,
                                                                  char* out_diagnostics,
                                                                  size_t out_diagnostics_size);

static void runtime_mesh_asset_diag(char* out_diagnostics,
                                    size_t out_diagnostics_size,
                                    const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0u || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static bool runtime_mesh_asset_copy_id(char* out_id,
                                       size_t out_id_size,
                                       const char* id,
                                       char* out_diagnostics,
                                       size_t out_diagnostics_size) {
    size_t len = 0u;
    if (!out_id || out_id_size == 0u || !id || !id[0]) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset id missing");
        return false;
    }
    len = strlen(id);
    if (len >= out_id_size) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset id too long");
        return false;
    }
    memcpy(out_id, id, len + 1u);
    return true;
}

static bool runtime_mesh_asset_read_text(const char* path,
                                         char** out_text,
                                         char* out_diagnostics,
                                         size_t out_diagnostics_size) {
    CoreBuffer file_data = {0};
    CoreResult read_result = core_result_ok();
    char* text = NULL;

    if (out_text) *out_text = NULL;
    if (!path || !path[0] || !out_text) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "runtime scene path missing");
        return false;
    }

    read_result = core_io_read_all(path, &file_data);
    if (read_result.code != CORE_OK || !file_data.data || file_data.size == 0u) {
        core_io_buffer_free(&file_data);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "failed to read runtime scene");
        return false;
    }

    text = (char*)malloc(file_data.size + 1u);
    if (!text) {
        core_io_buffer_free(&file_data);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        return false;
    }
    memcpy(text, file_data.data, file_data.size);
    text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);
    *out_text = text;
    return true;
}

static const char* runtime_mesh_asset_string_field(json_object* obj, const char* key) {
    json_object* value = NULL;
    if (!obj || !json_object_is_type(obj, json_type_object) || !key) return NULL;
    if (!json_object_object_get_ex(obj, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

static const char* runtime_mesh_asset_object_runtime_path(json_object* object) {
    json_object* extensions = NULL;
    json_object* line_drawing = NULL;
    const char* path = NULL;
    if (!object || !json_object_is_type(object, json_type_object)) return NULL;
    if (!json_object_object_get_ex(object, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object)) {
        return NULL;
    }
    if (!json_object_object_get_ex(extensions, "line_drawing", &line_drawing) ||
        !json_object_is_type(line_drawing, json_type_object)) {
        return NULL;
    }
    path = runtime_mesh_asset_string_field(line_drawing, "runtime_mesh_path");
    return (path && path[0]) ? path : NULL;
}

static bool runtime_mesh_asset_object_is_line_drawing_mesh_instance(json_object* object) {
    json_object* extensions = NULL;
    json_object* line_drawing = NULL;
    const char* geometry_source = NULL;
    if (!object || !json_object_is_type(object, json_type_object)) return false;
    if (!json_object_object_get_ex(object, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(extensions, "line_drawing", &line_drawing) ||
        !json_object_is_type(line_drawing, json_type_object)) {
        return false;
    }
    geometry_source = runtime_mesh_asset_string_field(line_drawing, "geometry_source");
    if (geometry_source && strcmp(geometry_source, "mesh_asset_instance") == 0) return true;
    return runtime_mesh_asset_object_runtime_path(object) != NULL;
}

static bool runtime_mesh_asset_try_asset_root(const char* root,
                                              const char* asset_id,
                                              char* out_path,
                                              size_t out_path_size,
                                              char* out_diagnostics,
                                              size_t out_diagnostics_size) {
    char candidate[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX] = {0};
    if (!root || !root[0] || !asset_id || !asset_id[0] || !out_path || out_path_size == 0u) {
        return false;
    }
    if (snprintf(candidate,
                 sizeof(candidate),
                 "%s/%s.runtime.json",
                 root,
                 asset_id) < (int)sizeof(candidate) &&
        core_io_path_exists(candidate)) {
        snprintf(out_path, out_path_size, "%s", candidate);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
        return true;
    }
    if (snprintf(candidate,
                 sizeof(candidate),
                 "%s/assets/mesh_assets/%s.runtime.json",
                 root,
                 asset_id) < (int)sizeof(candidate) &&
        core_io_path_exists(candidate)) {
        snprintf(out_path, out_path_size, "%s", candidate);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
        return true;
    }
    if (snprintf(candidate,
                 sizeof(candidate),
                 "%s/mesh_assets/%s.runtime.json",
                 root,
                 asset_id) < (int)sizeof(candidate) &&
        core_io_path_exists(candidate)) {
        snprintf(out_path, out_path_size, "%s", candidate);
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "ok");
        return true;
    }
    return false;
}

static bool runtime_mesh_asset_is_authoring_helper_object_type(const char* object_type) {
    if (!object_type || !object_type[0]) return false;
    return strcmp(object_type, "curve_path") == 0 ||
           strcmp(object_type, "point_set") == 0 ||
           strcmp(object_type, "edge_set") == 0;
}

static void runtime_mesh_asset_probe_preview(const char* runtime_mesh_path,
                                             RayTracingRuntimeMeshPreviewInfo* out_preview) {
    CoreResult path_result = core_result_ok();
    CoreResult probe_result = core_result_ok();
    CoreMeshPreviewFileProbe probe;

    if (!out_preview) return;
    memset(out_preview, 0, sizeof(*out_preview));
    core_mesh_preview_runtime_metadata_init(&out_preview->metadata);
    if (!runtime_mesh_path || !runtime_mesh_path[0]) return;

    path_result = core_mesh_preview_path_from_runtime(runtime_mesh_path,
                                                      out_preview->preview_path,
                                                      sizeof(out_preview->preview_path));
    if (path_result.code != CORE_OK) {
        out_preview->preview_path[0] = '\0';
        return;
    }
    out_preview->preview_path_resolved = true;

    core_mesh_preview_file_probe_init(&probe);
    probe_result = core_mesh_preview_probe_file(out_preview->preview_path, &probe);
    (void)probe_result;
    out_preview->preview_file_exists = probe.exists;
    out_preview->preview_file_readable = probe.readable;
    out_preview->preview_schema_supported = probe.schema_supported;
    out_preview->preview_metadata_valid = probe.metadata_valid;
    if (probe.metadata_valid) {
        out_preview->metadata = probe.metadata;
    }
}

static double runtime_mesh_asset_number_field_or(json_object* obj,
                                                 const char* key,
                                                 double fallback) {
    json_object* value = NULL;
    if (!obj || !json_object_is_type(obj, json_type_object) || !key) return fallback;
    if (!json_object_object_get_ex(obj, key, &value)) return fallback;
    return json_object_get_double(value);
}

static void runtime_mesh_asset_read_vec3_or(json_object* obj,
                                            double fallback_x,
                                            double fallback_y,
                                            double fallback_z,
                                            double* out_x,
                                            double* out_y,
                                            double* out_z) {
    if (out_x) *out_x = runtime_mesh_asset_number_field_or(obj, "x", fallback_x);
    if (out_y) *out_y = runtime_mesh_asset_number_field_or(obj, "y", fallback_y);
    if (out_z) *out_z = runtime_mesh_asset_number_field_or(obj, "z", fallback_z);
}

static void runtime_mesh_asset_read_transform(json_object* object,
                                              double world_scale,
                                              RayTracingRuntimeMeshAssetInstance* instance) {
    static const double kDegreesToRadians = 0.017453292519943295769;
    json_object* transform = NULL;
    json_object* position = NULL;
    json_object* rotation = NULL;
    json_object* scale = NULL;
    double px = 0.0;
    double py = 0.0;
    double pz = 0.0;
    double sx = 1.0;
    double sy = 1.0;
    double sz = 1.0;
    if (!object || !instance) return;

    instance->position_x = 0.0;
    instance->position_y = 0.0;
    instance->position_z = 0.0;
    instance->rotation_x = 0.0;
    instance->rotation_y = 0.0;
    instance->rotation_z = 0.0;
    instance->scale_x = world_scale;
    instance->scale_y = world_scale;
    instance->scale_z = world_scale;

    if (!json_object_object_get_ex(object, "transform", &transform) ||
        !json_object_is_type(transform, json_type_object)) {
        return;
    }

    if (json_object_object_get_ex(transform, "position", &position) &&
        json_object_is_type(position, json_type_object)) {
        runtime_mesh_asset_read_vec3_or(position, 0.0, 0.0, 0.0, &px, &py, &pz);
    }
    if (json_object_object_get_ex(transform, "rotation", &rotation) &&
        json_object_is_type(rotation, json_type_object)) {
        runtime_mesh_asset_read_vec3_or(rotation,
                                        0.0,
                                        0.0,
                                        0.0,
                                        &instance->rotation_x,
                                        &instance->rotation_y,
                                        &instance->rotation_z);
        if (runtime_mesh_asset_object_is_line_drawing_mesh_instance(object)) {
            instance->rotation_x *= kDegreesToRadians;
            instance->rotation_y *= kDegreesToRadians;
            instance->rotation_z *= kDegreesToRadians;
        }
    }
    if (json_object_object_get_ex(transform, "scale", &scale) &&
        json_object_is_type(scale, json_type_object)) {
        runtime_mesh_asset_read_vec3_or(scale, 1.0, 1.0, 1.0, &sx, &sy, &sz);
    }

    instance->position_x = px * world_scale;
    instance->position_y = py * world_scale;
    instance->position_z = pz * world_scale;
    instance->scale_x = sx * world_scale;
    instance->scale_y = sy * world_scale;
    instance->scale_z = sz * world_scale;
}

static int runtime_mesh_asset_find_asset_index(const RayTracingRuntimeMeshAssetSet* set,
                                               const char* asset_id) {
    int i = 0;
    if (!set || !asset_id || !asset_id[0]) return -1;
    for (i = 0; i < set->asset_count; ++i) {
        if (strcmp(set->assets[i].asset_id, asset_id) == 0) return i;
    }
    return -1;
}

static void runtime_mesh_asset_file_stamp(const struct stat* st,
                                          long long* out_mtime_sec,
                                          long long* out_mtime_nsec,
                                          long long* out_file_size) {
    if (out_mtime_sec) *out_mtime_sec = 0;
    if (out_mtime_nsec) *out_mtime_nsec = 0;
    if (out_file_size) *out_file_size = 0;
    if (!st) return;
    if (out_mtime_sec) *out_mtime_sec = (long long)st->st_mtime;
#if defined(__APPLE__)
    if (out_mtime_nsec) *out_mtime_nsec = (long long)st->st_mtimespec.tv_nsec;
#elif defined(st_mtim)
    if (out_mtime_nsec) *out_mtime_nsec = (long long)st->st_mtim.tv_nsec;
#else
    if (out_mtime_nsec) *out_mtime_nsec = 0;
#endif
    if (out_file_size) *out_file_size = (long long)st->st_size;
}

static bool runtime_mesh_asset_document_copy(const CoreMeshAssetRuntimeDocument* src,
                                             CoreMeshAssetRuntimeDocument* dst,
                                             char* out_diagnostics,
                                             size_t out_diagnostics_size) {
    CoreResult result = core_result_ok();
    if (!src || !dst) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset copy input missing");
        return false;
    }

    core_mesh_asset_runtime_document_init(dst);
    dst->contract = src->contract;

    result = core_mesh_asset_runtime_document_set_vertex_count(dst, src->vertex_count);
    if (result.code != CORE_OK) goto fail;
    result = core_mesh_asset_runtime_document_set_triangle_count(dst, src->triangle_count);
    if (result.code != CORE_OK) goto fail;
    result = core_mesh_asset_runtime_document_set_surface_group_count(dst,
                                                                      src->surface_group_count);
    if (result.code != CORE_OK) goto fail;

    if (src->vertex_count > 0u && src->vertices) {
        memcpy(dst->vertices, src->vertices, sizeof(*src->vertices) * src->vertex_count);
    }
    if (src->triangle_count > 0u && src->triangles) {
        memcpy(dst->triangles, src->triangles, sizeof(*src->triangles) * src->triangle_count);
    }
    if (src->surface_group_count > 0u && src->surface_groups) {
        memcpy(dst->surface_groups,
               src->surface_groups,
               sizeof(*src->surface_groups) * src->surface_group_count);
    }
    return true;

fail:
    runtime_mesh_asset_diag(out_diagnostics,
                            out_diagnostics_size,
                            result.message ? result.message : "mesh asset copy failed");
    core_mesh_asset_runtime_document_free(dst);
    return false;
}

static bool runtime_mesh_asset_cache_entry_matches(const RuntimeMeshAssetCacheEntry* entry,
                                                   const char* asset_id,
                                                   const char* path,
                                                   long long mtime_sec,
                                                   long long mtime_nsec,
                                                   long long file_size) {
    return entry && entry->valid &&
           strcmp(entry->asset_id, asset_id) == 0 &&
           strcmp(entry->path, path) == 0 &&
           entry->mtime_sec == mtime_sec &&
           entry->mtime_nsec == mtime_nsec &&
           entry->file_size == file_size;
}

static int runtime_mesh_asset_cache_find(const char* asset_id,
                                         const char* path,
                                         long long mtime_sec,
                                         long long mtime_nsec,
                                         long long file_size) {
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        if (runtime_mesh_asset_cache_entry_matches(&g_runtime_mesh_asset_cache[i],
                                                   asset_id,
                                                   path,
                                                   mtime_sec,
                                                   mtime_nsec,
                                                   file_size)) {
            return i;
        }
    }
    return -1;
}

static int runtime_mesh_asset_cache_slot_for(const char* asset_id, const char* path) {
    int free_slot = -1;
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        RuntimeMeshAssetCacheEntry* entry = &g_runtime_mesh_asset_cache[i];
        if (!entry->valid) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (strcmp(entry->asset_id, asset_id) == 0 && strcmp(entry->path, path) == 0) {
            core_mesh_asset_runtime_document_free(&entry->document);
            memset(entry, 0, sizeof(*entry));
            g_runtime_mesh_asset_cache_invalidations += 1u;
            return i;
        }
    }
    return free_slot;
}

static bool runtime_mesh_asset_load_document_cached(const char* resolved_path,
                                                    const char* asset_id,
                                                    CoreMeshAssetRuntimeDocument* out_document,
                                                    char* out_diagnostics,
                                                    size_t out_diagnostics_size) {
    struct stat st;
    long long mtime_sec = 0;
    long long mtime_nsec = 0;
    long long file_size = 0;
    int cached_index = -1;
    int cache_slot = -1;
    RuntimeMeshAssetCacheEntry* entry = NULL;
    CoreResult load_result = core_result_ok();
    struct timespec load_start = {0};
    struct timespec copy_start = {0};
    bool copy_ok = false;

    (void)clock_gettime(CLOCK_MONOTONIC, &load_start);
    g_runtime_mesh_asset_timing.asset_load_calls += 1;
    if (!resolved_path || !asset_id || !out_document) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset cache input missing");
        return false;
    }
    if (stat(resolved_path, &st) != 0) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset runtime file stat failed");
        return false;
    }
    runtime_mesh_asset_file_stamp(&st, &mtime_sec, &mtime_nsec, &file_size);

    cached_index = runtime_mesh_asset_cache_find(asset_id,
                                                resolved_path,
                                                mtime_sec,
                                                mtime_nsec,
                                                file_size);
    if (cached_index >= 0) {
        g_runtime_mesh_asset_cache_hits += 1u;
        g_runtime_mesh_asset_timing.asset_cache_hits += 1;
        (void)clock_gettime(CLOCK_MONOTONIC, &copy_start);
        copy_ok = runtime_mesh_asset_document_copy(&g_runtime_mesh_asset_cache[cached_index].document,
                                                   out_document,
                                                   out_diagnostics,
                                                   out_diagnostics_size);
        g_runtime_mesh_asset_timing.asset_document_copy_ms +=
            runtime_mesh_asset_elapsed_ms_since(&copy_start);
        g_runtime_mesh_asset_timing.asset_load_total_ms +=
            runtime_mesh_asset_elapsed_ms_since(&load_start);
        return copy_ok;
    }

    g_runtime_mesh_asset_cache_misses += 1u;
    g_runtime_mesh_asset_timing.asset_cache_misses += 1;
    cache_slot = runtime_mesh_asset_cache_slot_for(asset_id, resolved_path);
    if (cache_slot < 0) {
        runtime_mesh_asset_diag(out_diagnostics, out_diagnostics_size, "mesh asset cache full");
        return false;
    }

    entry = &g_runtime_mesh_asset_cache[cache_slot];
    memset(entry, 0, sizeof(*entry));
    core_mesh_asset_runtime_document_init(&entry->document);
    {
        struct timespec document_load_start = {0};
        (void)clock_gettime(CLOCK_MONOTONIC, &document_load_start);
    load_result = core_mesh_asset_runtime_document_load_file(resolved_path, &entry->document);
        g_runtime_mesh_asset_timing.asset_runtime_document_load_ms +=
            runtime_mesh_asset_elapsed_ms_since(&document_load_start);
    }
    if (load_result.code != CORE_OK) {
        runtime_mesh_asset_diag(out_diagnostics,
                                out_diagnostics_size,
                                load_result.message ? load_result.message : "validation failed");
        core_mesh_asset_runtime_document_free(&entry->document);
        memset(entry, 0, sizeof(*entry));
        return false;
    }

    snprintf(entry->asset_id, sizeof(entry->asset_id), "%s", asset_id);
    snprintf(entry->path, sizeof(entry->path), "%s", resolved_path);
    entry->mtime_sec = mtime_sec;
    entry->mtime_nsec = mtime_nsec;
    entry->file_size = file_size;
    entry->valid = true;

    (void)clock_gettime(CLOCK_MONOTONIC, &copy_start);
    copy_ok = runtime_mesh_asset_document_copy(&entry->document,
                                               out_document,
                                               out_diagnostics,
                                               out_diagnostics_size);
    g_runtime_mesh_asset_timing.asset_document_copy_ms +=
        runtime_mesh_asset_elapsed_ms_since(&copy_start);
    g_runtime_mesh_asset_timing.asset_load_total_ms +=
        runtime_mesh_asset_elapsed_ms_since(&load_start);
    return copy_ok;
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
    if (!ray_tracing_runtime_mesh_asset_resolve_path_with_hint(runtime_scene_path,
                                                               asset_id,
                                                               explicit_runtime_path,
                                                               resolved_path,
                                                               sizeof(resolved_path),
                                                               out_diagnostics,
                                                               out_diagnostics_size)) {
        return false;
    }
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
}

void ray_tracing_runtime_mesh_assets_take_last(RayTracingRuntimeMeshAssetSet* loaded) {
    ray_tracing_runtime_mesh_assets_reset_last();
    if (!loaded) return;
    g_last_runtime_mesh_assets = *loaded;
    memset(loaded, 0, sizeof(*loaded));
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
    ray_tracing_runtime_mesh_assets_take_last(&loaded);
    return true;
}

const RayTracingRuntimeMeshAssetSet* ray_tracing_runtime_mesh_assets_last(void) {
    return &g_last_runtime_mesh_assets;
}

void ray_tracing_runtime_mesh_assets_reset_cache(void) {
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        if (g_runtime_mesh_asset_cache[i].valid) {
            core_mesh_asset_runtime_document_free(&g_runtime_mesh_asset_cache[i].document);
        }
        memset(&g_runtime_mesh_asset_cache[i], 0, sizeof(g_runtime_mesh_asset_cache[i]));
    }
    g_runtime_mesh_asset_cache_hits = 0u;
    g_runtime_mesh_asset_cache_misses = 0u;
    g_runtime_mesh_asset_cache_invalidations = 0u;
}

void ray_tracing_runtime_mesh_assets_cache_stats(unsigned long long* out_hits,
                                                unsigned long long* out_misses,
                                                unsigned long long* out_invalidations,
                                                int* out_cached_assets) {
    int cached_assets = 0;
    for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_ASSETS; ++i) {
        if (g_runtime_mesh_asset_cache[i].valid) cached_assets += 1;
    }
    if (out_hits) *out_hits = g_runtime_mesh_asset_cache_hits;
    if (out_misses) *out_misses = g_runtime_mesh_asset_cache_misses;
    if (out_invalidations) *out_invalidations = g_runtime_mesh_asset_cache_invalidations;
    if (out_cached_assets) *out_cached_assets = cached_assets;
}

void ray_tracing_runtime_mesh_assets_timing_reset(void) {
    memset(&g_runtime_mesh_asset_timing, 0, sizeof(g_runtime_mesh_asset_timing));
}

void ray_tracing_runtime_mesh_assets_timing_snapshot(
    RayTracingRuntimeMeshAssetTimingStats* out_stats) {
    if (!out_stats) return;
    *out_stats = g_runtime_mesh_asset_timing;
}
