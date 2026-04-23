#include "config/config_scene_path_io.h"

#include "camera/camera_path_3d.h"

#include <string.h>

static void config_scene_reset_path(Path* path) {
    if (!path) return;
    memset(path, 0, sizeof(Path));
    path->mode = BEZIER_CUBIC;
}

static const char* config_scene_path_mode_to_string(BezierMode mode) {
    return (mode == BEZIER_QUADRATIC) ? "BEZIER_QUADRATIC" : "BEZIER_CUBIC";
}

static BezierMode config_scene_path_mode_from_string(const char* mode_str) {
    if (!mode_str) return BEZIER_CUBIC;
    if (strcmp(mode_str, "BEZIER_QUADRATIC") == 0 || strcmp(mode_str, "Quadratic") == 0) {
        return BEZIER_QUADRATIC;
    }
    return BEZIER_CUBIC;
}

static void config_scene_load_velocity_handle(struct json_object* point_obj,
                                              const char* key,
                                              Velocity* handle) {
    struct json_object *velocity_obj, *vx_obj, *vy_obj;
    if (json_object_object_get_ex(point_obj, key, &velocity_obj) &&
        json_object_object_get_ex(velocity_obj, "vx", &vx_obj) &&
        json_object_object_get_ex(velocity_obj, "vy", &vy_obj)) {
        handle->vx = json_object_get_double(vx_obj);
        handle->vy = json_object_get_double(vy_obj);
    } else {
        handle->vx = 0;
        handle->vy = 0;
    }
}

struct json_object* config_scene_path_to_json_object(const Path* path) {
    struct json_object* path_obj = json_object_new_object();
    if (!path_obj) return NULL;
    if (!path) return path_obj;
    json_object_object_add(path_obj, "mode", json_object_new_string(config_scene_path_mode_to_string(path->mode)));

    struct json_object* points_array = json_object_new_array();
    for (int i = 0; i < path->numPoints; i++) {
        struct json_object* point_obj = json_object_new_object();
        json_object_object_add(point_obj, "x", json_object_new_double(path->points[i].x));
        json_object_object_add(point_obj, "y", json_object_new_double(path->points[i].y));
        json_object_object_add(point_obj, "rotation", json_object_new_double(path->rotations[i]));
        json_object_object_add(point_obj, "handleLink", json_object_new_boolean(path->handleLink[i]));

        if (i < path->numPoints - 1) {
            struct json_object* velocity_1_obj = json_object_new_object();
            json_object_object_add(velocity_1_obj, "vx", json_object_new_double(path->handles[i][0].vx));
            json_object_object_add(velocity_1_obj, "vy", json_object_new_double(path->handles[i][0].vy));
            json_object_object_add(point_obj, "velocity1", velocity_1_obj);
        }
        if (i > 0) {
            struct json_object* velocity_2_obj = json_object_new_object();
            json_object_object_add(velocity_2_obj, "vx", json_object_new_double(path->handles[i - 1][1].vx));
            json_object_object_add(velocity_2_obj, "vy", json_object_new_double(path->handles[i - 1][1].vy));
            json_object_object_add(point_obj, "velocity2", velocity_2_obj);
        }

        json_object_array_add(points_array, point_obj);
    }

    json_object_object_add(path_obj, "points", points_array);
    return path_obj;
}

void config_scene_save_path_to_json(struct json_object* config, const char* key, const Path* path) {
    struct json_object* path_obj = NULL;
    if (!config || !key || !path) return;
    path_obj = config_scene_path_to_json_object(path);
    if (!path_obj) return;
    json_object_object_add(config, key, path_obj);
}

bool config_scene_load_path_from_json_object(struct json_object* path_data, Path* out, bool allow_empty) {
    if (!path_data || !out) return false;
    config_scene_reset_path(out);

    struct json_object *points_array;
    if (!json_object_is_type(path_data, json_type_object) ||
        !json_object_object_get_ex(path_data, "points", &points_array) ||
        !json_object_is_type(points_array, json_type_array)) {
        return false;
    }

    struct json_object* mode_obj;
    if (json_object_object_get_ex(path_data, "mode", &mode_obj)) {
        const char* mode_str = json_object_get_string(mode_obj);
        out->mode = config_scene_path_mode_from_string(mode_str);
    }

    int num_points = json_object_array_length(points_array);
    if (num_points < 1) {
        return allow_empty;
    }

    if (num_points > MAX_BEZIER_POINTS) {
        num_points = MAX_BEZIER_POINTS;
    }
    out->numPoints = num_points;

    for (int i = 0; i < num_points; i++) {
        struct json_object* point_obj = json_object_array_get_idx(points_array, i);
        struct json_object *x_obj, *y_obj;

        if (json_object_object_get_ex(point_obj, "x", &x_obj) &&
            json_object_object_get_ex(point_obj, "y", &y_obj)) {
            out->points[i].x = json_object_get_double(x_obj);
            out->points[i].y = json_object_get_double(y_obj);
        }
        {
            struct json_object* rot_obj;
            if (json_object_object_get_ex(point_obj, "rotation", &rot_obj)) {
                out->rotations[i] = json_object_get_double(rot_obj);
                out->rotationSet[i] = true;
            }
        }
        {
            struct json_object* link_obj;
            if (json_object_object_get_ex(point_obj, "handleLink", &link_obj)) {
                out->handleLink[i] = json_object_get_boolean(link_obj);
            }
        }

        if (i < num_points - 1) {
            config_scene_load_velocity_handle(point_obj, "velocity1", &out->handles[i][0]);
        }
        if (i > 0) {
            config_scene_load_velocity_handle(point_obj, "velocity2", &out->handles[i - 1][1]);
        }
    }

    return true;
}

bool config_scene_load_path_from_json(struct json_object* config, const char* key, Path* out) {
    struct json_object *path_data = NULL;
    if (!config || !key || !out) return false;
    if (!json_object_object_get_ex(config, key, &path_data)) {
        return false;
    }
    return config_scene_load_path_from_json_object(path_data, out, false);
}

bool config_scene_load_camera_path_from_json(struct json_object* config, const char* key, Path* out) {
    struct json_object *path_data = NULL;
    if (!config || !key || !out) return false;
    if (!json_object_object_get_ex(config, key, &path_data)) {
        return false;
    }
    return config_scene_load_path_from_json_object(path_data, out, true);
}

void config_scene_save_path_depth_to_json(struct json_object* config,
                                          const char* key,
                                          const CameraPath3D* path3d,
                                          const Path* path) {
    struct json_object* object = NULL;
    if (!config || !key || !path3d || !path) return;
    object = CameraPath3D_ToJsonObject(path3d, path);
    if (!object) return;
    json_object_object_add(config, key, object);
}

bool config_scene_load_path_depth_from_json(struct json_object* config,
                                            const char* key,
                                            CameraPath3D* out_path3d,
                                            const Path* path) {
    struct json_object* path_data = NULL;
    if (!config || !key || !out_path3d || !path) return false;
    if (!json_object_object_get_ex(config, key, &path_data)) {
        return false;
    }
    return CameraPath3D_LoadFromJsonObject(path_data, out_path3d, path, false);
}

void config_scene_save_camera_path_depth_to_json(struct json_object* config,
                                                 const char* key,
                                                 const CameraPath3D* path3d,
                                                 const Path* path) {
    config_scene_save_path_depth_to_json(config, key, path3d, path);
}

bool config_scene_load_camera_path_depth_from_json(struct json_object* config,
                                                   const char* key,
                                                   CameraPath3D* out_path3d,
                                                   const Path* path) {
    return config_scene_load_path_depth_from_json(config, key, out_path3d, path);
}

void config_scene_ensure_camera_path_default(SceneConfig* scene) {
    if (!scene) return;

    if (scene->cameraPath.numPoints > 0) {
        for (int i = 0; i < scene->cameraPath.numPoints; i++) {
            if (!scene->cameraPath.rotationSet[i]) {
                scene->cameraPath.rotations[i] = (i == 0) ? 0.0 : scene->camera.rotation;
                scene->cameraPath.rotationSet[i] = true;
            }
        }
        CameraPath3D_SyncDefaults(&scene->cameraPath3D, &scene->cameraPath, scene->cameraZ);
        return;
    }

    config_scene_reset_path(&scene->cameraPath);
    CameraPath3D_Reset(&scene->cameraPath3D);
}
