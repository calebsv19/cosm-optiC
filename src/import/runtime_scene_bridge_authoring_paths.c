#include "import/runtime_scene_bridge_internal.h"

#include "camera/camera_path_3d.h"
#include "import/runtime_scene_bridge_authoring_internal.h"
#include "import/runtime_scene_bridge_json_utils.h"

double runtime_scene_bridge_authoring_zero_length(void) {
    double zero [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    return zero;
}

double runtime_scene_bridge_authoring_scale_scene_length(
    double scene_length [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double world_scale) {
    double authored_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = scene_length;
    return authored_length * world_scale;
}

static void runtime_scene_bridge_scale_path_world_units(Path *path, double world_scale) {
    int i = 0;
    if (!path) return;
    for (i = 0; i < path->numPoints && i < MAX_BEZIER_POINTS; ++i) {
        double point_x [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->points[i].x;
        double point_y [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->points[i].y;
        path->points[i].x = runtime_scene_bridge_authoring_scale_scene_length(point_x, world_scale);
        path->points[i].y = runtime_scene_bridge_authoring_scale_scene_length(point_y, world_scale);
        if (i < MAX_BEZIER_POINTS - 1) {
            double h0x [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->handles[i][0].vx;
            double h0y [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->handles[i][0].vy;
            double h1x [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->handles[i][1].vx;
            double h1y [[fisics::dim(length)]] [[fisics::unit(meter)]] = path->handles[i][1].vy;
            path->handles[i][0].vx =
                runtime_scene_bridge_authoring_scale_scene_length(h0x, world_scale);
            path->handles[i][0].vy =
                runtime_scene_bridge_authoring_scale_scene_length(h0y, world_scale);
            path->handles[i][1].vx =
                runtime_scene_bridge_authoring_scale_scene_length(h1x, world_scale);
            path->handles[i][1].vy =
                runtime_scene_bridge_authoring_scale_scene_length(h1y, world_scale);
        }
    }
}

bool runtime_scene_bridge_apply_authoring_path_scaled(json_object *authoring_obj,
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
    runtime_scene_bridge_scale_path_world_units(&loaded, world_scale);
    *target_path = loaded;
    return true;
}

bool runtime_scene_bridge_parse_position_or_transform_position(json_object *obj,
                                                               double *out_x,
                                                               double *out_y,
                                                               double *out_z) {
    json_object *transform = NULL;
    if (!obj || !out_x || !out_y || !out_z) return false;
    if (runtime_scene_bridge_parse_vec3(obj, "position", out_x, out_y, out_z)) {
        return true;
    }
    if (json_object_object_get_ex(obj, "transform", &transform) &&
        json_object_is_type(transform, json_type_object) &&
        runtime_scene_bridge_parse_vec3(transform, "position", out_x, out_y, out_z)) {
        return true;
    }
    return false;
}

static bool runtime_scene_bridge_parse_transform_rotation(json_object *obj,
                                                          double *out_x,
                                                          double *out_y,
                                                          double *out_z) {
    json_object *transform = NULL;
    if (!obj || !out_x || !out_y || !out_z) return false;
    if (json_object_object_get_ex(obj, "transform", &transform) &&
        json_object_is_type(transform, json_type_object) &&
        runtime_scene_bridge_parse_vec3(transform, "rotation", out_x, out_y, out_z)) {
        return true;
    }
    return false;
}

bool runtime_scene_bridge_parse_camera_seed_yaw(json_object *obj, double *out_yaw) {
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    if (!obj || !out_yaw) return false;
    if (runtime_scene_bridge_parse_double_field(obj, "yaw", out_yaw)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "rotation_z", out_yaw)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "rotationZ", out_yaw)) return true;
    if (runtime_scene_bridge_parse_transform_rotation(obj, &rx, &ry, &rz)) {
        *out_yaw = rz;
        return true;
    }
    return false;
}

bool runtime_scene_bridge_parse_camera_seed_pitch(json_object *obj, double *out_pitch) {
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    if (!obj || !out_pitch) return false;
    if (runtime_scene_bridge_parse_double_field(obj, "look_pitch", out_pitch)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "lookPitch", out_pitch)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "pitch", out_pitch)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "rotation_x", out_pitch)) return true;
    if (runtime_scene_bridge_parse_double_field(obj, "rotationX", out_pitch)) return true;
    if (runtime_scene_bridge_parse_transform_rotation(obj, &rx, &ry, &rz)) {
        *out_pitch = rx;
        return true;
    }
    return false;
}

bool runtime_scene_bridge_parse_focus_target(json_object *obj,
                                             double world_scale,
                                             double *out_x,
                                             double *out_y,
                                             double *out_z) {
    json_object *target = NULL;
    double x [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double y [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double z [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    if (!obj || !out_x || !out_y || !out_z) return false;
    if (!json_object_object_get_ex(obj, "camera_focus_target", &target) ||
        !json_object_is_type(target, json_type_object)) {
        return false;
    }
    if (!runtime_scene_bridge_parse_position_or_transform_position(target, &x, &y, &z) &&
        !runtime_scene_bridge_parse_double_field(target, "x", &x)) {
        return false;
    }
    if (!runtime_scene_bridge_parse_position_or_transform_position(target, &x, &y, &z)) {
        if (!runtime_scene_bridge_parse_double_field(target, "y", &y) ||
            !runtime_scene_bridge_parse_double_field(target, "z", &z)) {
            return false;
        }
    }
    *out_x = runtime_scene_bridge_authoring_scale_scene_length(x, world_scale);
    *out_y = runtime_scene_bridge_authoring_scale_scene_length(y, world_scale);
    *out_z = runtime_scene_bridge_authoring_scale_scene_length(z, world_scale);
    return true;
}

void runtime_scene_bridge_apply_light_seed_scaled(json_object *lights_array,
                                                  double world_scale) {
    json_object *light0 = NULL;
    double lx [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double ly [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double lz [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double zero_length = runtime_scene_bridge_authoring_zero_length();
    if (!lights_array || !json_object_is_type(lights_array, json_type_array) ||
        json_object_array_length(lights_array) == 0u) {
        return;
    }
    light0 = json_object_array_get_idx(lights_array, 0);
    if (!light0 || !json_object_is_type(light0, json_type_object)) return;
    if (runtime_scene_bridge_parse_position_or_transform_position(light0, &lx, &ly, &lz)) {
        sceneSettings.bezierPath.numPoints = 1;
        sceneSettings.bezierPath.points[0].x =
            runtime_scene_bridge_authoring_scale_scene_length(lx, world_scale);
        sceneSettings.bezierPath.points[0].y =
            runtime_scene_bridge_authoring_scale_scene_length(ly, world_scale);
        sceneSettings.bezierPath3D.point_z[0] =
            runtime_scene_bridge_authoring_scale_scene_length(lz, world_scale);
        if (sceneSettings.bezierPath3D.point_z[0] > zero_length) {
            animSettings.lightHeight = sceneSettings.bezierPath3D.point_z[0];
        }
    }
}

void runtime_scene_bridge_apply_camera_seed_scaled(json_object *cameras_array,
                                                   double world_scale) {
    json_object *camera0 = NULL;
    double cx [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double cy [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double cz [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double yaw = 0.0;
    double pitch = 0.0;
    bool has_yaw = false;
    bool has_pitch = false;
    if (!cameras_array || !json_object_is_type(cameras_array, json_type_array) ||
        json_object_array_length(cameras_array) == 0u) {
        return;
    }
    camera0 = json_object_array_get_idx(cameras_array, 0);
    if (!camera0 || !json_object_is_type(camera0, json_type_object)) return;
    if (runtime_scene_bridge_parse_position_or_transform_position(camera0, &cx, &cy, &cz)) {
        sceneSettings.camera.x =
            runtime_scene_bridge_authoring_scale_scene_length(cx, world_scale);
        sceneSettings.camera.y =
            runtime_scene_bridge_authoring_scale_scene_length(cy, world_scale);
        sceneSettings.cameraZ =
            runtime_scene_bridge_authoring_scale_scene_length(cz, world_scale);
        sceneSettings.cameraPath.numPoints = 1;
        sceneSettings.cameraPath.points[0].x = sceneSettings.camera.x;
        sceneSettings.cameraPath.points[0].y = sceneSettings.camera.y;
        sceneSettings.cameraPath.rotationSet[0] = false;
        sceneSettings.cameraPath3D.point_z[0] = sceneSettings.cameraZ;
        sceneSettings.cameraPath3D.point_pitch[0] = 0.0;
        g_last_3d_scaffold.has_camera_seed = true;
        g_last_3d_scaffold.camera_z = sceneSettings.cameraZ;
    }
    has_yaw = runtime_scene_bridge_parse_camera_seed_yaw(camera0, &yaw);
    has_pitch = runtime_scene_bridge_parse_camera_seed_pitch(camera0, &pitch);
    if (has_yaw) {
        sceneSettings.camera.rotation = yaw;
        sceneSettings.cameraPath.rotations[0] = yaw;
        sceneSettings.cameraPath.rotationSet[0] = true;
        g_last_3d_scaffold.has_camera_rotation_seed = true;
    }
    if (has_pitch) {
        sceneSettings.cameraPath3D.point_pitch[0] = pitch;
        g_last_3d_scaffold.has_camera_pitch_seed = true;
    }
}
