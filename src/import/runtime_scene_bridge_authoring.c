#include "import/runtime_scene_bridge_internal.h"

#include "camera/camera_path_3d.h"
#include "config/config_manager.h"
#include "config/config_scene_path_io.h"
#include "import/runtime_scene_bridge_json_utils.h"
#include "material/material_manager.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void scale_path_world_units(Path *path, double world_scale) {
    int i = 0;
    if (!path) return;
    for (i = 0; i < path->numPoints && i < MAX_BEZIER_POINTS; ++i) {
        path->points[i].x *= world_scale;
        path->points[i].y *= world_scale;
        if (i < MAX_BEZIER_POINTS - 1) {
            path->handles[i][0].vx *= world_scale;
            path->handles[i][0].vy *= world_scale;
            path->handles[i][1].vx *= world_scale;
            path->handles[i][1].vy *= world_scale;
        }
    }
}

static bool apply_authoring_path_scaled(json_object *authoring_obj,
                                        const char *key,
                                        Path *target_path,
                                        double world_scale,
                                        bool allow_empty) {
    json_object *path_obj = NULL;
    Path loaded = {0};
    if (!authoring_obj || !key || !target_path) return false;
    if (!json_object_object_get_ex(authoring_obj, key, &path_obj)) {
        return false;
    }
    if (!config_scene_load_path_from_json_object(path_obj, &loaded, allow_empty)) {
        return false;
    }
    scale_path_world_units(&loaded, world_scale);
    *target_path = loaded;
    return true;
}

static void apply_ray_authoring_light_settings(json_object *authoring, double world_scale) {
    json_object *light_settings = NULL;
    json_object *intensity_obj = NULL;
    json_object *radius_obj = NULL;
    if (!authoring) return;
    if (!json_object_object_get_ex(authoring, "light_settings", &light_settings) ||
        !json_object_is_type(light_settings, json_type_object)) {
        return;
    }
    if (json_object_object_get_ex(light_settings, "intensity", &intensity_obj) &&
        (json_object_is_type(intensity_obj, json_type_int) ||
         json_object_is_type(intensity_obj, json_type_double))) {
        animSettings.lightIntensity = json_object_get_double(intensity_obj);
    }
    (void)world_scale;
    if (json_object_object_get_ex(light_settings, "radius", &radius_obj) &&
        (json_object_is_type(radius_obj, json_type_int) ||
         json_object_is_type(radius_obj, json_type_double))) {
        animSettings.lightRadius = json_object_get_double(radius_obj);
        if (animSettings.lightRadius < 0.0) {
            animSettings.lightRadius = 0.0;
        }
    }
}

static int runtime_scene_bridge_color_from_material_preset(int material_id) {
    const Material *preset = MaterialManagerGet(material_id);
    int r = 255;
    int g = 255;
    int b = 255;
    if (!preset) return 0xFFFFFF;
    r = runtime_scene_bridge_clamp_color_channel(preset->base_color.x);
    g = runtime_scene_bridge_clamp_color_channel(preset->base_color.y);
    b = runtime_scene_bridge_clamp_color_channel(preset->base_color.z);
    return (r << 16) | (g << 8) | b;
}

static void runtime_scene_bridge_apply_object_material_preset(SceneObject *out_object,
                                                              int material_id) {
    if (!out_object) return;
    if (material_id < 0 || material_id >= MaterialManagerCount()) {
        material_id = MaterialManagerDefaultId();
    }
    out_object->material_id = material_id;
    out_object->color = runtime_scene_bridge_color_from_material_preset(material_id);
}

static void apply_ray_authoring_object_materials(json_object *authoring) {
    json_object *object_materials = NULL;
    size_t i = 0;
    if (!authoring) return;
    if (!json_object_object_get_ex(authoring, "object_materials", &object_materials) ||
        !json_object_is_type(object_materials, json_type_array)) {
        return;
    }
    for (i = 0; i < json_object_array_length(object_materials); ++i) {
        json_object *entry = json_object_array_get_idx(object_materials, i);
        json_object *object_id_obj = NULL;
        json_object *material_id_obj = NULL;
        json_object *object_color_obj = NULL;
        json_object *alpha_obj = NULL;
        json_object *transparency_obj = NULL;
        json_object *emissive_strength_obj = NULL;
        const char *object_id = NULL;
        int material_id = MaterialManagerDefaultId();
        int object_color = 0xFFFFFF;
        bool has_object_color = false;
        double alpha = 1.0;
        double emissive_strength = 1.0;
        int scene_index = 0;
        if (!entry || !json_object_is_type(entry, json_type_object)) continue;
        if (!json_object_object_get_ex(entry, "object_id", &object_id_obj) ||
            !json_object_is_type(object_id_obj, json_type_string)) {
            continue;
        }
        if (!json_object_object_get_ex(entry, "material_id", &material_id_obj) ||
            (!json_object_is_type(material_id_obj, json_type_int) &&
             !json_object_is_type(material_id_obj, json_type_double))) {
            continue;
        }
        object_id = json_object_get_string(object_id_obj);
        material_id = json_object_get_int(material_id_obj);
        if (json_object_object_get_ex(entry, "object_color", &object_color_obj) &&
            (json_object_is_type(object_color_obj, json_type_int) ||
             json_object_is_type(object_color_obj, json_type_double))) {
            object_color = json_object_get_int(object_color_obj) & 0xFFFFFF;
            has_object_color = true;
        }
        if (json_object_object_get_ex(entry, "alpha", &alpha_obj) &&
            (json_object_is_type(alpha_obj, json_type_int) ||
             json_object_is_type(alpha_obj, json_type_double))) {
            alpha = json_object_get_double(alpha_obj);
        } else if (json_object_object_get_ex(entry, "transparency", &transparency_obj) &&
                   (json_object_is_type(transparency_obj, json_type_int) ||
                    json_object_is_type(transparency_obj, json_type_double))) {
            alpha = json_object_get_double(transparency_obj);
        }
        if (json_object_object_get_ex(entry, "emissive_strength", &emissive_strength_obj) &&
            (json_object_is_type(emissive_strength_obj, json_type_int) ||
             json_object_is_type(emissive_strength_obj, json_type_double))) {
            emissive_strength = json_object_get_double(emissive_strength_obj);
        }
        if (!object_id || !object_id[0]) continue;
        for (scene_index = 0;
             scene_index < sceneSettings.objectCount && scene_index < g_last_runtime_object_id_count;
             ++scene_index) {
            if (strcmp(g_last_runtime_object_ids[scene_index], object_id) == 0) {
                runtime_scene_bridge_apply_object_material_preset(
                    &sceneSettings.sceneObjects[scene_index], material_id);
                if (has_object_color) {
                    sceneSettings.sceneObjects[scene_index].color = object_color;
                }
                sceneSettings.sceneObjects[scene_index].alpha = fmax(0.0, fmin(1.0, alpha));
                sceneSettings.sceneObjects[scene_index].emissiveStrength =
                    fmax(0.0, fmin(1.0, emissive_strength));
                break;
            }
        }
    }
}

void runtime_scene_bridge_apply_scene3d_extension_digest(json_object *root,
                                                         double world_scale) {
    json_object *extensions = NULL;
    json_object *line_drawing = NULL;
    json_object *scene3d = NULL;
    json_object *bounds = NULL;
    json_object *construction_plane = NULL;

    if (!root || !json_object_is_type(root, json_type_object)) return;
    if (!json_object_object_get_ex(root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object)) {
        return;
    }
    if (!json_object_object_get_ex(extensions, "line_drawing", &line_drawing) ||
        !json_object_is_type(line_drawing, json_type_object)) {
        return;
    }
    if (!json_object_object_get_ex(line_drawing, "scene3d", &scene3d) ||
        !json_object_is_type(scene3d, json_type_object)) {
        return;
    }

    if (json_object_object_get_ex(scene3d, "bounds", &bounds) &&
        json_object_is_type(bounds, json_type_object)) {
        double min_x = 0.0, min_y = 0.0, min_z = 0.0;
        double max_x = 0.0, max_y = 0.0, max_z = 0.0;
        bool has_enabled = false;
        bool has_clamp = false;
        bool enabled = false;
        bool clamp_on_edit = false;
        g_last_3d_digest.has_scene_bounds = true;
        has_enabled = runtime_scene_bridge_parse_bool_field(bounds, "enabled", &enabled);
        has_clamp = runtime_scene_bridge_parse_bool_field(bounds, "clamp_on_edit", &clamp_on_edit);
        g_last_3d_digest.bounds_enabled = has_enabled && enabled;
        g_last_3d_digest.bounds_clamp_on_edit = has_clamp && clamp_on_edit;
        if (runtime_scene_bridge_parse_vec3(bounds, "min", &min_x, &min_y, &min_z)) {
            g_last_3d_digest.bounds_min_x = min_x * world_scale;
            g_last_3d_digest.bounds_min_y = min_y * world_scale;
            g_last_3d_digest.bounds_min_z = min_z * world_scale;
        }
        if (runtime_scene_bridge_parse_vec3(bounds, "max", &max_x, &max_y, &max_z)) {
            g_last_3d_digest.bounds_max_x = max_x * world_scale;
            g_last_3d_digest.bounds_max_y = max_y * world_scale;
            g_last_3d_digest.bounds_max_z = max_z * world_scale;
        }
    }

    if (json_object_object_get_ex(scene3d, "construction_plane", &construction_plane) &&
        json_object_is_type(construction_plane, json_type_object)) {
        const char *mode = runtime_scene_bridge_json_string_field_or_null(construction_plane, "mode");
        const char *axis = runtime_scene_bridge_json_string_field_or_null(construction_plane, "axis");
        double offset = 0.0;
        g_last_3d_digest.has_construction_plane = true;
        if (mode && mode[0]) {
            snprintf(g_last_3d_digest.construction_plane_mode,
                     sizeof(g_last_3d_digest.construction_plane_mode),
                     "%s",
                     mode);
        }
        if (axis && axis[0]) {
            snprintf(g_last_3d_digest.construction_plane_axis,
                     sizeof(g_last_3d_digest.construction_plane_axis),
                     "%s",
                     axis);
        }
        if (runtime_scene_bridge_parse_double_field(construction_plane, "offset", &offset)) {
            g_last_3d_digest.construction_plane_offset = offset * world_scale;
        }
    }
}

void runtime_scene_bridge_apply_space_mode(json_object *root) {
    json_object *space_mode = NULL;
    const char *mode = NULL;
    if (!root) return;
    if (!json_object_object_get_ex(root, "space_mode_default", &space_mode) ||
        !json_object_is_type(space_mode, json_type_string)) {
        animSettings.spaceMode = SPACE_MODE_2D;
        return;
    }
    mode = json_object_get_string(space_mode);
    animSettings.spaceMode = (mode && strcmp(mode, "3d") == 0) ? SPACE_MODE_3D : SPACE_MODE_2D;
}

void runtime_scene_bridge_apply_light_seed_scaled(json_object *lights_array,
                                                  double world_scale) {
    json_object *light0 = NULL;
    double lx = 0.0, ly = 0.0, lz = 0.0;
    if (!lights_array || !json_object_is_type(lights_array, json_type_array) ||
        json_object_array_length(lights_array) == 0u) {
        return;
    }
    light0 = json_object_array_get_idx(lights_array, 0);
    if (!light0 || !json_object_is_type(light0, json_type_object)) return;
    if (runtime_scene_bridge_parse_vec3(light0, "position", &lx, &ly, &lz)) {
        sceneSettings.bezierPath.numPoints = 1;
        sceneSettings.bezierPath.points[0].x = lx * world_scale;
        sceneSettings.bezierPath.points[0].y = ly * world_scale;
        sceneSettings.bezierPath3D.point_z[0] = lz * world_scale;
        if (lz * world_scale > 0.0) {
            animSettings.lightHeight = lz * world_scale;
        }
    }
}

void runtime_scene_bridge_apply_camera_seed_scaled(json_object *cameras_array,
                                                   double world_scale) {
    json_object *camera0 = NULL;
    double cx = 0.0, cy = 0.0, cz = 0.0;
    if (!cameras_array || !json_object_is_type(cameras_array, json_type_array) ||
        json_object_array_length(cameras_array) == 0u) {
        return;
    }
    camera0 = json_object_array_get_idx(cameras_array, 0);
    if (!camera0 || !json_object_is_type(camera0, json_type_object)) return;
    if (runtime_scene_bridge_parse_vec3(camera0, "position", &cx, &cy, &cz)) {
        sceneSettings.camera.x = cx * world_scale;
        sceneSettings.camera.y = cy * world_scale;
        sceneSettings.cameraZ = cz * world_scale;
        g_last_3d_scaffold.has_camera_seed = true;
        g_last_3d_scaffold.camera_z = cz * world_scale;
    }
}

void runtime_scene_bridge_apply_ray_authoring_paths(json_object *root,
                                                    double world_scale) {
    json_object *extensions = NULL;
    json_object *ray_tracing = NULL;
    json_object *authoring = NULL;
    json_object *light_path_depth = NULL;
    json_object *camera_path_depth = NULL;
    if (!root) return;
    if (!json_object_object_get_ex(root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object) ||
        !json_object_object_get_ex(extensions, "ray_tracing", &ray_tracing) ||
        !json_object_is_type(ray_tracing, json_type_object) ||
        !json_object_object_get_ex(ray_tracing, "authoring", &authoring) ||
        !json_object_is_type(authoring, json_type_object)) {
        return;
    }

    if (apply_authoring_path_scaled(authoring,
                                    "light_path",
                                    &sceneSettings.bezierPath,
                                    world_scale,
                                    true)) {
        if (json_object_object_get_ex(authoring, "light_path_depth", &light_path_depth) &&
            CameraPath3D_LoadFromJsonObject(light_path_depth,
                                            &sceneSettings.bezierPath3D,
                                            &sceneSettings.bezierPath,
                                            true)) {
            CameraPath3D_ScaleWorldUnits(&sceneSettings.bezierPath3D,
                                         &sceneSettings.bezierPath,
                                         world_scale);
        } else {
            CameraPath3D_Reset(&sceneSettings.bezierPath3D);
            CameraPath3D_SyncDefaults(&sceneSettings.bezierPath3D,
                                      &sceneSettings.bezierPath,
                                      animSettings.lightHeight);
        }
        if (sceneSettings.bezierPath.numPoints > 0) {
            animSettings.lightHeight = sceneSettings.bezierPath3D.point_z[0];
        }
    }
    if (apply_authoring_path_scaled(authoring,
                                    "camera_path",
                                    &sceneSettings.cameraPath,
                                    world_scale,
                                    true)) {
        if (json_object_object_get_ex(authoring, "camera_path_depth", &camera_path_depth) &&
            CameraPath3D_LoadFromJsonObject(camera_path_depth,
                                            &sceneSettings.cameraPath3D,
                                            &sceneSettings.cameraPath,
                                            true)) {
            CameraPath3D_ScaleWorldUnits(&sceneSettings.cameraPath3D,
                                         &sceneSettings.cameraPath,
                                         world_scale);
        } else {
            CameraPath3D_Reset(&sceneSettings.cameraPath3D);
            CameraPath3D_SyncDefaults(&sceneSettings.cameraPath3D,
                                      &sceneSettings.cameraPath,
                                      sceneSettings.cameraZ);
        }
        if (sceneSettings.cameraPath.numPoints > 0) {
            sceneSettings.camera.x = sceneSettings.cameraPath.points[0].x;
            sceneSettings.camera.y = sceneSettings.cameraPath.points[0].y;
            sceneSettings.cameraZ = sceneSettings.cameraPath3D.point_z[0];
            if (sceneSettings.cameraPath.rotationSet[0]) {
                sceneSettings.camera.rotation = sceneSettings.cameraPath.rotations[0];
            }
        }
    }
    apply_ray_authoring_light_settings(authoring, world_scale);
    apply_ray_authoring_object_materials(authoring);
}
