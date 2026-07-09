#include "import/runtime_mesh_asset_loader_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "core_io.h"
#include "core_scene.h"

double runtime_mesh_asset_elapsed_ms_since(const struct timespec* start_time) {
    struct timespec now = {0};
    double elapsed = 0.0;
    if (!start_time) return 0.0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    elapsed = (double)(now.tv_sec - start_time->tv_sec) * 1000.0;
    elapsed += (double)(now.tv_nsec - start_time->tv_nsec) / 1000000.0;
    return elapsed < 0.0 ? 0.0 : elapsed;
}

void runtime_mesh_asset_diag(char* out_diagnostics,
                                    size_t out_diagnostics_size,
                                    const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0u || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

bool runtime_mesh_asset_copy_id(char* out_id,
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

bool runtime_mesh_asset_read_text(const char* path,
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

const char* runtime_mesh_asset_string_field(json_object* obj, const char* key) {
    json_object* value = NULL;
    if (!obj || !json_object_is_type(obj, json_type_object) || !key) return NULL;
    if (!json_object_object_get_ex(obj, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

const char* runtime_mesh_asset_object_runtime_path(json_object* object) {
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

bool runtime_mesh_asset_try_asset_root(const char* root,
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

bool runtime_mesh_asset_is_authoring_helper_object_type(const char* object_type) {
    if (!object_type || !object_type[0]) return false;
    return strcmp(object_type, "curve_path") == 0 ||
           strcmp(object_type, "point_set") == 0 ||
           strcmp(object_type, "edge_set") == 0;
}

void runtime_mesh_asset_probe_preview(const char* runtime_mesh_path,
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

double runtime_mesh_asset_number_field_or(json_object* obj,
                                                 const char* key,
                                                 double fallback) {
    json_object* value = NULL;
    if (!obj || !json_object_is_type(obj, json_type_object) || !key) return fallback;
    if (!json_object_object_get_ex(obj, key, &value)) return fallback;
    return json_object_get_double(value);
}

void runtime_mesh_asset_read_vec3_or(json_object* obj,
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

void runtime_mesh_asset_read_transform(json_object* object,
                                              double world_scale,
                                              RayTracingRuntimeMeshAssetInstance* instance) {
    static const double kDegreesToRadians = 0.017453292519943295769;
    json_object* transform = NULL;
    json_object* position = NULL;
    json_object* rotation = NULL;
    json_object* scale = NULL;
    json_object* pivot_policy = NULL;
    json_object* pivot = NULL;
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
    instance->rotation_pivot_policy =
        RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_AUTHORED_ORIGIN;
    instance->rotation_pivot_x = 0.0;
    instance->rotation_pivot_y = 0.0;
    instance->rotation_pivot_z = 0.0;

    if (!json_object_object_get_ex(object, "transform", &transform) ||
        !json_object_is_type(transform, json_type_object)) {
        return;
    }

    if (json_object_object_get_ex(transform, "pivot_policy", &pivot_policy) &&
        json_object_is_type(pivot_policy, json_type_string)) {
        const char* policy = json_object_get_string(pivot_policy);
        if (policy && strcmp(policy, "bounds_center") == 0) {
            instance->rotation_pivot_policy =
                RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_BOUNDS_CENTER;
        } else if (policy && strcmp(policy, "custom") == 0) {
            instance->rotation_pivot_policy =
                RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM;
        } else {
            instance->rotation_pivot_policy =
                RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_AUTHORED_ORIGIN;
        }
    }
    if (instance->rotation_pivot_policy ==
            RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM &&
        json_object_object_get_ex(transform, "pivot", &pivot) &&
        json_object_is_type(pivot, json_type_object)) {
        runtime_mesh_asset_read_vec3_or(pivot,
                                        0.0,
                                        0.0,
                                        0.0,
                                        &instance->rotation_pivot_x,
                                        &instance->rotation_pivot_y,
                                        &instance->rotation_pivot_z);
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
        instance->rotation_x *= kDegreesToRadians;
        instance->rotation_y *= kDegreesToRadians;
        instance->rotation_z *= kDegreesToRadians;
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

int runtime_mesh_asset_find_asset_index(const RayTracingRuntimeMeshAssetSet* set,
                                               const char* asset_id) {
    int i = 0;
    if (!set || !asset_id || !asset_id[0]) return -1;
    for (i = 0; i < set->asset_count; ++i) {
        if (strcmp(set->assets[i].asset_id, asset_id) == 0) return i;
    }
    return -1;
}

void runtime_mesh_asset_file_stamp(const struct stat* st,
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

bool runtime_mesh_asset_stat_path(const char* path,
                                         long long* out_mtime_sec,
                                         long long* out_mtime_nsec,
                                         long long* out_file_size) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    runtime_mesh_asset_file_stamp(&st, out_mtime_sec, out_mtime_nsec, out_file_size);
    return true;
}

bool runtime_mesh_asset_stamp_matches_path(const char* path,
                                           long long expected_mtime_sec,
                                           long long expected_mtime_nsec,
                                           long long expected_file_size) {
    long long mtime_sec = 0;
    long long mtime_nsec = 0;
    long long file_size = 0;
    if (!runtime_mesh_asset_stat_path(path, &mtime_sec, &mtime_nsec, &file_size)) {
        return false;
    }
    return mtime_sec == expected_mtime_sec &&
           mtime_nsec == expected_mtime_nsec &&
           file_size == expected_file_size;
}
