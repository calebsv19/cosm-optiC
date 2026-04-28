#include "editor/scene_editor_runtime_scene_persistence.h"

#include "camera/camera_path_3d.h"
#include "config/config_manager.h"
#include "config/config_scene_path_io.h"
#include "core_io.h"
#include "import/runtime_scene_bridge.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scene_editor_runtime_scene_diag(char* out_diagnostics,
                                            size_t out_diagnostics_size,
                                            const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0 || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static bool scene_editor_runtime_scene_read_file(const char* path,
                                                 char** out_text,
                                                 size_t* out_size,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size) {
    CoreBuffer file_data = {0};
    char* text = NULL;
    CoreResult io_result;
    if (out_text) *out_text = NULL;
    if (out_size) *out_size = 0;
    if (!path || !path[0] || !out_text) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "runtime scene path missing");
        return false;
    }
    io_result = core_io_read_all(path, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "failed to read runtime scene");
        core_io_buffer_free(&file_data);
        return false;
    }
    text = (char*)malloc(file_data.size + 1u);
    if (!text) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(text, file_data.data, file_data.size);
    text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);
    *out_text = text;
    if (out_size) *out_size = file_data.size;
    scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static void scene_editor_runtime_scene_scale_path(Path* dst,
                                                  const Path* src,
                                                  double factor) {
    int i = 0;
    if (!dst || !src) return;
    *dst = *src;
    for (i = 0; i < dst->numPoints && i < MAX_BEZIER_POINTS; ++i) {
        dst->points[i].x *= factor;
        dst->points[i].y *= factor;
        if (i < MAX_BEZIER_POINTS - 1) {
            dst->handles[i][0].vx *= factor;
            dst->handles[i][0].vy *= factor;
            dst->handles[i][1].vx *= factor;
            dst->handles[i][1].vy *= factor;
        }
    }
}

static void scene_editor_runtime_scene_scale_camera_path3d(CameraPath3D* dst,
                                                           const CameraPath3D* src,
                                                           const Path* path,
                                                           double factor) {
    if (!dst || !src || !path) return;
    *dst = *src;
    CameraPath3D_ScaleWorldUnits(dst, path, factor);
}

static bool scene_editor_runtime_scene_resolve_metrics(const char* runtime_scene_json,
                                                       double* out_world_scale,
                                                       long long* out_next_clock,
                                                       char* out_diagnostics,
                                                       size_t out_diagnostics_size) {
    json_object* root = NULL;
    json_object* world_scale_obj = NULL;
    json_object* extensions = NULL;
    json_object* overlay_merge = NULL;
    json_object* producer_clocks = NULL;
    json_object* ray_clock = NULL;
    double world_scale = 1.0;
    long long next_clock = 1;

    if (!runtime_scene_json || !out_world_scale || !out_next_clock) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "invalid input");
        return false;
    }

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "invalid runtime scene json");
        return false;
    }

    if (json_object_object_get_ex(root, "world_scale", &world_scale_obj) &&
        (json_object_is_type(world_scale_obj, json_type_double) ||
         json_object_is_type(world_scale_obj, json_type_int))) {
        world_scale = json_object_get_double(world_scale_obj);
    }
    if (!(world_scale > 0.0)) {
        world_scale = 1.0;
    }

    if (json_object_object_get_ex(root, "extensions", &extensions) &&
        json_object_is_type(extensions, json_type_object) &&
        json_object_object_get_ex(extensions, "overlay_merge", &overlay_merge) &&
        json_object_is_type(overlay_merge, json_type_object) &&
        json_object_object_get_ex(overlay_merge, "producer_clocks", &producer_clocks) &&
        json_object_is_type(producer_clocks, json_type_object) &&
        json_object_object_get_ex(producer_clocks, "ray_tracing", &ray_clock) &&
        json_object_is_type(ray_clock, json_type_int)) {
        next_clock = json_object_get_int64(ray_clock) + 1;
    }

    json_object_put(root);
    *out_world_scale = world_scale;
    *out_next_clock = next_clock;
    scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static json_object* scene_editor_runtime_scene_build_object_materials_json(void) {
    json_object* object_materials = json_object_new_array();
    int i = 0;
    if (!object_materials) return NULL;
    for (i = 0; i < sceneSettings.objectCount; ++i) {
        char object_id[64];
        json_object* entry = NULL;
        if (!runtime_scene_bridge_get_last_object_id_for_scene_index(i, object_id, sizeof(object_id))) {
            continue;
        }
        entry = json_object_new_object();
        if (!entry) {
            json_object_put(object_materials);
            return NULL;
        }
        json_object_object_add(entry, "object_id", json_object_new_string(object_id));
        json_object_object_add(entry, "material_id", json_object_new_int(sceneSettings.sceneObjects[i].material_id));
        json_object_object_add(entry,
                               "object_color",
                               json_object_new_int(sceneSettings.sceneObjects[i].color & 0xFFFFFF));
        json_object_object_add(entry,
                               "transparency",
                               json_object_new_double(sceneSettings.sceneObjects[i].transparency));
        json_object_object_add(entry,
                               "emissive_strength",
                               json_object_new_double(sceneSettings.sceneObjects[i].emissiveStrength));
        json_object_array_add(object_materials, entry);
    }
    return object_materials;
}

static char* scene_editor_runtime_scene_build_overlay_json(double world_scale,
                                                           long long logical_clock,
                                                           char* out_diagnostics,
                                                           size_t out_diagnostics_size) {
    json_object* overlay_root = NULL;
    json_object* overlay_meta = NULL;
    json_object* extensions = NULL;
    json_object* ray_tracing = NULL;
    json_object* authoring = NULL;
    json_object* light_path = NULL;
    json_object* light_path_depth = NULL;
    json_object* camera_path = NULL;
    json_object* camera_path_depth = NULL;
    json_object* object_materials = NULL;
    const char* serialized = NULL;
    char* out = NULL;
    Path saved_light_path = {0};
    CameraPath3D saved_light_path3d = {0};
    Path saved_camera_path = {0};
    CameraPath3D saved_camera_path3d = {0};
    double authored_to_runtime = 1.0;
    size_t out_len = 0;

    if (!(world_scale > 0.0)) {
        world_scale = 1.0;
    }
    authored_to_runtime = 1.0 / world_scale;
    scene_editor_runtime_scene_scale_path(&saved_light_path, &sceneSettings.bezierPath, authored_to_runtime);
    scene_editor_runtime_scene_scale_camera_path3d(&saved_light_path3d,
                                                   &sceneSettings.bezierPath3D,
                                                   &saved_light_path,
                                                   authored_to_runtime);
    scene_editor_runtime_scene_scale_path(&saved_camera_path, &sceneSettings.cameraPath, authored_to_runtime);
    scene_editor_runtime_scene_scale_camera_path3d(&saved_camera_path3d,
                                                   &sceneSettings.cameraPath3D,
                                                   &saved_camera_path,
                                                   authored_to_runtime);

    overlay_root = json_object_new_object();
    overlay_meta = json_object_new_object();
    extensions = json_object_new_object();
    ray_tracing = json_object_new_object();
    authoring = json_object_new_object();
    light_path = config_scene_path_to_json_object(&saved_light_path);
    light_path_depth = CameraPath3D_ToJsonObject(&saved_light_path3d, &saved_light_path);
    camera_path = config_scene_path_to_json_object(&saved_camera_path);
    camera_path_depth = CameraPath3D_ToJsonObject(&saved_camera_path3d, &saved_camera_path);
    object_materials = scene_editor_runtime_scene_build_object_materials_json();

    if (!overlay_root || !overlay_meta || !extensions || !ray_tracing || !authoring ||
        !light_path || !light_path_depth || !camera_path || !camera_path_depth || !object_materials) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "failed to build overlay json");
        if (object_materials) json_object_put(object_materials);
        if (light_path_depth) json_object_put(light_path_depth);
        if (camera_path_depth) json_object_put(camera_path_depth);
        if (camera_path) json_object_put(camera_path);
        if (light_path) json_object_put(light_path);
        if (authoring) json_object_put(authoring);
        if (ray_tracing) json_object_put(ray_tracing);
        if (extensions) json_object_put(extensions);
        if (overlay_meta) json_object_put(overlay_meta);
        if (overlay_root) json_object_put(overlay_root);
        return NULL;
    }

    json_object_object_add(overlay_meta, "producer", json_object_new_string("ray_tracing"));
    json_object_object_add(overlay_meta, "logical_clock", json_object_new_int64(logical_clock));
    json_object_object_add(overlay_root, "overlay_meta", overlay_meta);

    json_object_object_add(authoring, "light_path", light_path);
    json_object_object_add(authoring, "light_path_depth", light_path_depth);
    json_object_object_add(authoring, "camera_path", camera_path);
    json_object_object_add(authoring, "camera_path_depth", camera_path_depth);
    json_object_object_add(authoring, "object_materials", object_materials);
    json_object_object_add(ray_tracing, "authoring", authoring);
    json_object_object_add(extensions, "ray_tracing", ray_tracing);
    json_object_object_add(overlay_root, "extensions", extensions);

    serialized = json_object_to_json_string_ext(overlay_root,
                                                JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    if (!serialized) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "failed to serialize overlay json");
        json_object_put(overlay_root);
        return NULL;
    }

    out_len = strlen(serialized);
    out = (char*)malloc(out_len + 1u);
    if (!out) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        json_object_put(overlay_root);
        return NULL;
    }
    memcpy(out, serialized, out_len + 1u);
    json_object_put(overlay_root);
    scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "ok");
    return out;
}

bool SceneEditorRuntimeScenePersistAuthoring(char* out_diagnostics, size_t out_diagnostics_size) {
    RuntimeSceneBridgePreflight summary = {0};
    char* runtime_scene_json = NULL;
    char* overlay_json = NULL;
    char* merged_json = NULL;
    double world_scale = 1.0;
    long long logical_clock = 1;
    CoreResult write_result;
    bool ok = false;

    scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (animSettings.sceneSource != SCENE_SOURCE_RUNTIME_SCENE ||
        animSettings.runtimeScenePath[0] == '\0') {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "runtime scene source is not active");
        return false;
    }

    if (!scene_editor_runtime_scene_read_file(animSettings.runtimeScenePath,
                                              &runtime_scene_json,
                                              NULL,
                                              out_diagnostics,
                                              out_diagnostics_size)) {
        return false;
    }

    if (!scene_editor_runtime_scene_resolve_metrics(runtime_scene_json,
                                                    &world_scale,
                                                    &logical_clock,
                                                    out_diagnostics,
                                                    out_diagnostics_size)) {
        free(runtime_scene_json);
        return false;
    }

    overlay_json = scene_editor_runtime_scene_build_overlay_json(world_scale,
                                                                 logical_clock,
                                                                 out_diagnostics,
                                                                 out_diagnostics_size);
    if (!overlay_json) {
        free(runtime_scene_json);
        return false;
    }

    ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_scene_json,
                                                         overlay_json,
                                                         &merged_json,
                                                         out_diagnostics,
                                                         out_diagnostics_size);
    free(runtime_scene_json);
    free(overlay_json);
    if (!ok || !merged_json) {
        free(merged_json);
        return false;
    }

    write_result = core_io_write_all(animSettings.runtimeScenePath, merged_json, strlen(merged_json));
    free(merged_json);
    if (write_result.code != CORE_OK) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "failed to write merged runtime scene");
        return false;
    }

    if (!runtime_scene_bridge_apply_file(animSettings.runtimeScenePath, &summary)) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, summary.diagnostics);
        return false;
    }

    scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
