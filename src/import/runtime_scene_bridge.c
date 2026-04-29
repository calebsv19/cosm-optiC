#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_bridge_json_utils.h"

#include "camera/camera_path_3d.h"
#include "core_scene_overlay_merge_shared.h"
#include "config/config_manager.h"
#include "config/config_scene_path_io.h"
#include "core_io.h"
#include "material/material_manager.h"
#include "scene/object_manager.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RuntimeSceneBridge3DScaffoldState g_last_3d_scaffold = {0};
static RuntimeSceneBridge3DDigestState g_last_3d_digest = {0};
static RuntimeSceneBridge3DPrimitiveSeedState g_last_3d_primitive_seeds = {0};
static char g_last_runtime_object_ids[MAX_OBJECTS][64] = {{0}};
static int g_last_runtime_object_id_count = 0;

static void apply_ray_authoring_object_materials(json_object *authoring);
static void apply_ray_authoring_light_settings(json_object *authoring, double world_scale);

static void scene_defaults_reset(void) {
    sceneSettings.objectCount = 0;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.zoom = 1.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.bezierPath.numPoints = 0;
    sceneSettings.bezierPath.points[0].x = 0.0;
    sceneSettings.bezierPath.points[0].y = 0.0;
    sceneSettings.bezierPath.mode = BEZIER_CUBIC;
    CameraPath3D_Reset(&sceneSettings.bezierPath3D);
    sceneSettings.cameraPath.numPoints = 0;
    sceneSettings.cameraPath.points[0].x = 0.0;
    sceneSettings.cameraPath.points[0].y = 0.0;
    sceneSettings.cameraPath.mode = BEZIER_CUBIC;
    CameraPath3D_Reset(&sceneSettings.cameraPath3D);
    memset(g_last_runtime_object_ids, 0, sizeof(g_last_runtime_object_ids));
    g_last_runtime_object_id_count = 0;
}

static void scaffold_state_reset(void) {
    memset(&g_last_3d_scaffold, 0, sizeof(g_last_3d_scaffold));
    memset(&g_last_3d_digest, 0, sizeof(g_last_3d_digest));
    memset(&g_last_3d_primitive_seeds, 0, sizeof(g_last_3d_primitive_seeds));
}

static bool primitive_seed_kind_supported_by_r0(RuntimeSceneBridgePrimitiveKind kind) {
    return kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE ||
           kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM;
}

static bool runtime_scene_bridge_is_authoring_helper_object_type(const char *object_type) {
    if (!object_type || !object_type[0]) return false;
    return strcmp(object_type, "curve_path") == 0 ||
           strcmp(object_type, "point_set") == 0 ||
           strcmp(object_type, "edge_set") == 0;
}

static RuntimeSceneBridgePrimitiveKind digest_kind_from_labels(const char *object_type,
                                                               const char *primitive_kind) {
    const char *label = primitive_kind && primitive_kind[0] ? primitive_kind : object_type;
    if (!label || !label[0]) return RUNTIME_SCENE_BRIDGE_PRIMITIVE_UNKNOWN;
    if (strstr(label, "rect_prism")) return RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM;
    if (strstr(label, "plane")) return RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE;
    if (strstr(label, "triangle_mesh")) return RUNTIME_SCENE_BRIDGE_PRIMITIVE_TRIANGLE_MESH;
    if (strstr(label, "box")) return RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX;
    return RUNTIME_SCENE_BRIDGE_PRIMITIVE_UNKNOWN;
}

static void digest_append_primitive(json_object *object_obj,
                                    json_object *transform_obj,
                                    json_object *primitive_obj,
                                    RuntimeSceneBridgePrimitiveKind kind,
                                    double world_scale) {
    RuntimeSceneBridgePrimitiveDigest *entry = NULL;
    const char *object_id = NULL;
    json_object *frame = NULL;
    json_object *origin_source = NULL;
    json_object *position_source = NULL;
    double width = 0.0;
    double height = 0.0;
    double depth = 0.0;
    bool has_width = false;
    bool has_height = false;
    bool has_depth = false;

    if (g_last_3d_digest.primitive_count >= RUNTIME_SCENE_BRIDGE_MAX_DIGEST_PRIMITIVES) return;
    entry = &g_last_3d_digest.primitives[g_last_3d_digest.primitive_count];
    memset(entry, 0, sizeof(*entry));
    entry->kind = kind;

    object_id = runtime_scene_bridge_json_string_field_or_null(object_obj, "object_id");
    if (object_id && object_id[0]) {
        snprintf(entry->object_id, sizeof(entry->object_id), "%s", object_id);
    }

    if (primitive_obj && json_object_is_type(primitive_obj, json_type_object)) {
        if (json_object_object_get_ex(primitive_obj, "frame", &frame) &&
            json_object_is_type(frame, json_type_object) &&
            json_object_object_get_ex(frame, "origin", &origin_source) &&
            json_object_is_type(origin_source, json_type_object)) {
            double ox = 0.0, oy = 0.0, oz = 0.0;
            if (runtime_scene_bridge_parse_vec3(frame, "origin", &ox, &oy, &oz)) {
                entry->origin_x = ox * world_scale;
                entry->origin_y = oy * world_scale;
                entry->origin_z = oz * world_scale;
            }
        }
        has_width = runtime_scene_bridge_parse_double_field(primitive_obj, "width", &width);
        has_height = runtime_scene_bridge_parse_double_field(primitive_obj, "height", &height);
        has_depth = runtime_scene_bridge_parse_double_field(primitive_obj, "depth", &depth);
    }

    if ((!origin_source || !json_object_is_type(origin_source, json_type_object)) &&
        transform_obj && json_object_is_type(transform_obj, json_type_object) &&
        json_object_object_get_ex(transform_obj, "position", &position_source) &&
        json_object_is_type(position_source, json_type_object)) {
        json_object *jx = NULL;
        json_object *jy = NULL;
        json_object *jz = NULL;
        if (json_object_object_get_ex(position_source, "x", &jx)) {
            entry->origin_x = json_object_get_double(jx) * world_scale;
        }
        if (json_object_object_get_ex(position_source, "y", &jy)) {
            entry->origin_y = json_object_get_double(jy) * world_scale;
        }
        if (json_object_object_get_ex(position_source, "z", &jz)) {
            entry->origin_z = json_object_get_double(jz) * world_scale;
        }
    }

    entry->has_dimensions = has_width || has_height || has_depth;
    if (has_width) entry->width = width * world_scale;
    if (has_height) entry->height = height * world_scale;
    if (has_depth) entry->depth = depth * world_scale;

    g_last_3d_digest.primitive_count += 1;
    if (kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
        g_last_3d_digest.plane_primitive_count += 1;
    } else if (kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM) {
        g_last_3d_digest.rect_prism_primitive_count += 1;
    }
}

static void primitive_seed_reset_basis(RuntimeSceneBridgePrimitiveSeed *entry) {
    if (!entry) return;
    entry->axis_u_x = 1.0;
    entry->axis_u_y = 0.0;
    entry->axis_u_z = 0.0;
    entry->axis_v_x = 0.0;
    entry->axis_v_y = 1.0;
    entry->axis_v_z = 0.0;
    entry->normal_x = 0.0;
    entry->normal_y = 0.0;
    entry->normal_z = 1.0;
}

static void primitive_seed_append(json_object *object_obj,
                                  json_object *transform_obj,
                                  json_object *primitive_obj,
                                  RuntimeSceneBridgePrimitiveKind kind,
                                  double world_scale,
                                  int scene_object_index) {
    RuntimeSceneBridgePrimitiveSeed *entry = NULL;
    const char *object_id = NULL;
    const char *object_type = runtime_scene_bridge_json_string_field_or_null(object_obj,
                                                                             "object_type");
    json_object *frame = NULL;
    json_object *position_source = NULL;
    double width = 0.0;
    double height = 0.0;
    double depth = 0.0;
    double scale_x = 1.0;
    double scale_y = 1.0;
    double scale_z = 1.0;
    bool has_width = false;
    bool has_height = false;
    bool has_depth = false;

    if (!primitive_seed_kind_supported_by_r0(kind)) {
        if (kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_UNKNOWN &&
            runtime_scene_bridge_is_authoring_helper_object_type(object_type)) {
            return;
        }
        g_last_3d_primitive_seeds.excluded_primitive_count += 1;
        return;
    }
    if (g_last_3d_primitive_seeds.primitive_count >= RUNTIME_SCENE_BRIDGE_MAX_PRIMITIVE_SEEDS) {
        g_last_3d_primitive_seeds.excluded_primitive_count += 1;
        return;
    }

    entry = &g_last_3d_primitive_seeds.primitives[g_last_3d_primitive_seeds.primitive_count];
    memset(entry, 0, sizeof(*entry));
    entry->kind = kind;
    entry->scene_object_index = scene_object_index;
    primitive_seed_reset_basis(entry);

    object_id = runtime_scene_bridge_json_string_field_or_null(object_obj, "object_id");
    if (object_id && object_id[0]) {
        snprintf(entry->object_id, sizeof(entry->object_id), "%s", object_id);
    }

    if (transform_obj && json_object_is_type(transform_obj, json_type_object)) {
        json_object *scale = NULL;
        if (json_object_object_get_ex(transform_obj, "position", &position_source) &&
            json_object_is_type(position_source, json_type_object)) {
            json_object *jx = NULL;
            json_object *jy = NULL;
            json_object *jz = NULL;
            if (json_object_object_get_ex(position_source, "x", &jx)) {
                entry->origin_x = json_object_get_double(jx) * world_scale;
            }
            if (json_object_object_get_ex(position_source, "y", &jy)) {
                entry->origin_y = json_object_get_double(jy) * world_scale;
            }
            if (json_object_object_get_ex(position_source, "z", &jz)) {
                entry->origin_z = json_object_get_double(jz) * world_scale;
            }
        }
        if (json_object_object_get_ex(transform_obj, "scale", &scale) &&
            json_object_is_type(scale, json_type_object)) {
            json_object *jsx = NULL;
            json_object *jsy = NULL;
            json_object *jsz = NULL;
            if (json_object_object_get_ex(scale, "x", &jsx)) scale_x = json_object_get_double(jsx);
            if (json_object_object_get_ex(scale, "y", &jsy)) scale_y = json_object_get_double(jsy);
            if (json_object_object_get_ex(scale, "z", &jsz)) scale_z = json_object_get_double(jsz);
        }
    }

    if (primitive_obj && json_object_is_type(primitive_obj, json_type_object)) {
        double ox = 0.0;
        double oy = 0.0;
        double oz = 0.0;
        double ax = 0.0;
        double ay = 0.0;
        double az = 0.0;
        if (json_object_object_get_ex(primitive_obj, "frame", &frame) &&
            json_object_is_type(frame, json_type_object)) {
            if (runtime_scene_bridge_parse_vec3(frame, "origin", &ox, &oy, &oz)) {
                entry->origin_x = ox * world_scale;
                entry->origin_y = oy * world_scale;
                entry->origin_z = oz * world_scale;
            }
            if (runtime_scene_bridge_parse_vec3(frame, "axis_u", &ax, &ay, &az)) {
                entry->axis_u_x = ax;
                entry->axis_u_y = ay;
                entry->axis_u_z = az;
            }
            if (runtime_scene_bridge_parse_vec3(frame, "axis_v", &ax, &ay, &az)) {
                entry->axis_v_x = ax;
                entry->axis_v_y = ay;
                entry->axis_v_z = az;
            }
            if (runtime_scene_bridge_parse_vec3(frame, "normal", &ax, &ay, &az)) {
                entry->normal_x = ax;
                entry->normal_y = ay;
                entry->normal_z = az;
            }
        }
        has_width = runtime_scene_bridge_parse_double_field(primitive_obj, "width", &width);
        has_height = runtime_scene_bridge_parse_double_field(primitive_obj, "height", &height);
        has_depth = runtime_scene_bridge_parse_double_field(primitive_obj, "depth", &depth);
    }

    entry->has_dimensions = has_width || has_height || has_depth;
    entry->width = has_width ? width * fabs(scale_x) * world_scale : 0.0;
    entry->height = has_height ? height * fabs(scale_y) * world_scale : 0.0;
    entry->depth = has_depth ? depth * fabs(scale_z) * world_scale : 0.0;

    g_last_3d_primitive_seeds.primitive_count += 1;
    if (kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
        g_last_3d_primitive_seeds.plane_primitive_count += 1;
    } else if (kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM) {
        g_last_3d_primitive_seeds.rect_prism_primitive_count += 1;
    }
}

static void apply_scene3d_extension_digest(json_object *root, double world_scale) {
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

static void apply_space_mode(json_object *root) {
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

static void apply_light_seed_scaled(json_object *lights_array, double world_scale) {
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

static void apply_camera_seed_scaled(json_object *cameras_array, double world_scale) {
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

static void apply_ray_authoring_paths(json_object *root, double world_scale) {
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

static void apply_object_material(json_object *object_obj,
                                  json_object *materials_array,
                                  SceneObject *out_object) {
    json_object *material_ref = NULL;
    json_object *id = NULL;
    const char *mat_id = NULL;
    if (!object_obj || !out_object) return;
    if (!json_object_object_get_ex(object_obj, "material_ref", &material_ref) ||
        !json_object_is_type(material_ref, json_type_object)) {
        return;
    }
    if (!json_object_object_get_ex(material_ref, "id", &id) || !json_object_is_type(id, json_type_string)) {
        return;
    }
    mat_id = json_object_get_string(id);
    if (!mat_id) return;
    out_object->color = runtime_scene_bridge_color_from_material_albedo(materials_array, mat_id);
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
                runtime_scene_bridge_apply_object_material_preset(&sceneSettings.sceneObjects[scene_index],
                                                                  material_id);
                if (has_object_color) {
                    sceneSettings.sceneObjects[scene_index].color = object_color;
                }
                sceneSettings.sceneObjects[scene_index].alpha =
                    fmax(0.0, fmin(1.0, alpha));
                sceneSettings.sceneObjects[scene_index].emissiveStrength =
                    fmax(0.0, fmin(1.0, emissive_strength));
                break;
            }
        }
    }
}

static void apply_object_flags(json_object *object_obj, SceneObject *out_object) {
    json_object *flags = NULL;
    json_object *visible = NULL;
    if (!object_obj || !out_object) return;
    if (!json_object_object_get_ex(object_obj, "flags", &flags) || !json_object_is_type(flags, json_type_object)) {
        return;
    }
    if (json_object_object_get_ex(flags, "visible", &visible) && json_object_is_type(visible, json_type_boolean)) {
        if (!json_object_get_boolean(visible)) {
            out_object->opacity = 0.2;
        }
    }
}

static void apply_objects(json_object *objects_array,
                          json_object *materials_array,
                          double world_scale,
                          RuntimeSceneBridgePreflight *out_summary) {
    static double k_default_poly[4][2] = {
        {-10.0, -10.0},
        {10.0, -10.0},
        {10.0, 10.0},
        {-10.0, 10.0}
    };
    static double k_plane_poly[4][2] = {
        {-120.0, -6.0},
        {120.0, -6.0},
        {120.0, 6.0},
        {-120.0, 6.0}
    };
    static double k_triangle_poly[3][2] = {
        {-12.0, -10.0},
        {12.0, -10.0},
        {0.0, 12.0}
    };
    size_t src_count = 0;
    size_t i = 0;

    if (!objects_array || !json_object_is_type(objects_array, json_type_array)) {
        sceneSettings.objectCount = 0;
        return;
    }

    src_count = json_object_array_length(objects_array);
    if (src_count > (size_t)MAX_OBJECTS) src_count = (size_t)MAX_OBJECTS;

    sceneSettings.objectCount = 0;
    for (i = 0; i < src_count; ++i) {
        json_object *obj = json_object_array_get_idx(objects_array, i);
        json_object *object_type = NULL;
        json_object *primitive = NULL;
        json_object *primitive_kind_obj = NULL;
        json_object *transform = NULL;
        json_object *position = NULL;
        json_object *scale = NULL;
        const char *type_str = NULL;
        const char *primitive_kind_str = NULL;
        double x = 0.0, y = 0.0, z = 0.0;
        double sx = 1.0, sy = 1.0, sz = 1.0;
        bool is_circle = true;
        bool is_plane = false;
        bool is_triangle_mesh = false;
        bool is_box = false;
        RuntimeSceneBridgePrimitiveKind digest_kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_UNKNOWN;
        SceneObject *dst = NULL;
        if (!obj || !json_object_is_type(obj, json_type_object)) continue;

        if (json_object_object_get_ex(obj, "object_type", &object_type) &&
            json_object_is_type(object_type, json_type_string)) {
            type_str = json_object_get_string(object_type);
        }
        if (json_object_object_get_ex(obj, "primitive", &primitive) &&
            json_object_is_type(primitive, json_type_object) &&
            json_object_object_get_ex(primitive, "kind", &primitive_kind_obj) &&
            json_object_is_type(primitive_kind_obj, json_type_string)) {
            primitive_kind_str = json_object_get_string(primitive_kind_obj);
        }
        digest_kind = digest_kind_from_labels(type_str, primitive_kind_str);
        if (type_str) {
            if (strstr(type_str, "circle")) {
                is_circle = true;
            } else {
                is_circle = false;
            }
            if (strstr(type_str, "plane")) is_plane = true;
            if (strstr(type_str, "triangle_mesh")) is_triangle_mesh = true;
            if (strstr(type_str, "box")) is_box = true;
        }
        if (primitive_kind_str) {
            if (strstr(primitive_kind_str, "plane")) {
                is_plane = true;
                is_circle = false;
            }
            if (strstr(primitive_kind_str, "triangle_mesh")) {
                is_triangle_mesh = true;
                is_circle = false;
            }
            if (strstr(primitive_kind_str, "rect_prism") || strstr(primitive_kind_str, "box")) {
                is_box = true;
                is_circle = false;
            }
        }

        if (json_object_object_get_ex(obj, "transform", &transform) &&
            json_object_is_type(transform, json_type_object)) {
            if (json_object_object_get_ex(transform, "position", &position) &&
                json_object_is_type(position, json_type_object)) {
                json_object *jx = NULL, *jy = NULL, *jz = NULL;
                if (json_object_object_get_ex(position, "x", &jx)) x = json_object_get_double(jx);
                if (json_object_object_get_ex(position, "y", &jy)) y = json_object_get_double(jy);
                if (json_object_object_get_ex(position, "z", &jz)) z = json_object_get_double(jz);
            }
            if (json_object_object_get_ex(transform, "scale", &scale) &&
                json_object_is_type(scale, json_type_object)) {
                json_object *jsx = NULL, *jsy = NULL, *jsz = NULL;
                if (json_object_object_get_ex(scale, "x", &jsx)) sx = json_object_get_double(jsx);
                if (json_object_object_get_ex(scale, "y", &jsy)) sy = json_object_get_double(jsy);
                if (json_object_object_get_ex(scale, "z", &jsz)) sz = json_object_get_double(jsz);
            }
        }

        dst = &sceneSettings.sceneObjects[sceneSettings.objectCount];
        memset(g_last_runtime_object_ids[sceneSettings.objectCount],
               0,
               sizeof(g_last_runtime_object_ids[sceneSettings.objectCount]));
        {
            const char *object_id = runtime_scene_bridge_json_string_field_or_null(obj, "object_id");
            if (object_id && object_id[0]) {
                snprintf(g_last_runtime_object_ids[sceneSettings.objectCount],
                         sizeof(g_last_runtime_object_ids[sceneSettings.objectCount]),
                         "%s",
                         object_id);
            }
        }
        if (is_circle) {
            InitObject(dst, OBJECT_CIRCLE, x, y, 10.0, 0.0, NULL, 0);
        } else if (is_plane) {
            InitObject(dst, OBJECT_POLYGON, x, y, 0.0, 0.0, k_plane_poly, 4);
            g_last_3d_scaffold.plane_count++;
        } else if (is_triangle_mesh) {
            InitObject(dst, OBJECT_POLYGON, x, y, 0.0, 0.0, k_triangle_poly, 3);
            g_last_3d_scaffold.triangle_mesh_count++;
        } else {
            InitObject(dst, OBJECT_POLYGON, x, y, 0.0, 0.0, k_default_poly, 4);
            if (is_box) g_last_3d_scaffold.box_count++;
        }
        dst->x *= world_scale;
        dst->y *= world_scale;
        dst->z = z * world_scale;
        dst->scale = ((sx + sy + sz) / 3.0) * world_scale;
        if (dst->scale <= 0.01) dst->scale = 0.01;
        apply_object_material(obj, materials_array, dst);
        apply_object_flags(obj, dst);
        digest_append_primitive(obj, transform, primitive, digest_kind, world_scale);
        primitive_seed_append(obj,
                              transform,
                              primitive,
                              digest_kind,
                              world_scale,
                              sceneSettings.objectCount);
        sceneSettings.objectCount++;
        g_last_runtime_object_id_count = sceneSettings.objectCount;
    }

    if (out_summary) out_summary->object_count = sceneSettings.objectCount;
}

bool runtime_scene_bridge_preflight_json(const char *runtime_scene_json,
                                         RuntimeSceneBridgePreflight *out_preflight) {
    json_object *root = NULL;

    if (!runtime_scene_json || !out_preflight) return false;
    runtime_scene_bridge_preflight_reset(out_preflight);

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        runtime_scene_bridge_preflight_diag(out_preflight, "invalid JSON object");
        if (root) json_object_put(root);
        return false;
    }

    if (!runtime_scene_bridge_validate_root(root, out_preflight)) {
        json_object_put(root);
        return false;
    }

    json_object_put(root);
    return true;
}

bool runtime_scene_bridge_preflight_file(const char *runtime_scene_path,
                                         RuntimeSceneBridgePreflight *out_preflight) {
    CoreBuffer file_data = {0};
    CoreResult io_result;
    char *json_text = NULL;
    bool ok;

    if (!runtime_scene_path || !out_preflight) return false;
    runtime_scene_bridge_preflight_reset(out_preflight);

    io_result = core_io_read_all(runtime_scene_path, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        runtime_scene_bridge_preflight_diag(out_preflight, "failed to read runtime scene file");
        core_io_buffer_free(&file_data);
        return false;
    }

    json_text = (char *)malloc(file_data.size + 1u);
    if (!json_text) {
        runtime_scene_bridge_preflight_diag(out_preflight, "out of memory");
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(json_text, file_data.data, file_data.size);
    json_text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    ok = runtime_scene_bridge_preflight_json(json_text, out_preflight);
    free(json_text);
    return ok;
}

bool runtime_scene_bridge_apply_json(const char *runtime_scene_json,
                                     RuntimeSceneBridgePreflight *out_summary) {
    json_object *root = NULL;
    json_object *objects = NULL;
    json_object *materials = NULL;
    json_object *lights = NULL;
    json_object *cameras = NULL;
    json_object *world_scale_obj = NULL;
    double world_scale = 1.0;

    if (!runtime_scene_json || !out_summary) return false;
    runtime_scene_bridge_preflight_reset(out_summary);

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        runtime_scene_bridge_preflight_diag(out_summary, "invalid JSON object");
        if (root) json_object_put(root);
        return false;
    }

    if (!runtime_scene_bridge_validate_root(root, out_summary)) {
        json_object_put(root);
        return false;
    }

    scene_defaults_reset();
    scaffold_state_reset();
    apply_space_mode(root);
    if (json_object_object_get_ex(root, "world_scale", &world_scale_obj)) {
        world_scale = json_object_get_double(world_scale_obj);
    }

    json_object_object_get_ex(root, "materials", &materials);
    json_object_object_get_ex(root, "objects", &objects);
    json_object_object_get_ex(root, "lights", &lights);
    json_object_object_get_ex(root, "cameras", &cameras);

    apply_objects(objects, materials, world_scale, out_summary);
    apply_light_seed_scaled(lights, world_scale);
    apply_camera_seed_scaled(cameras, world_scale);
    apply_ray_authoring_paths(root, world_scale);
    apply_scene3d_extension_digest(root, world_scale);
    g_last_3d_scaffold.valid = true;
    g_last_3d_digest.valid = true;
    g_last_3d_primitive_seeds.valid = true;
    animSettings.runtimeScenePath[0] = '\0';
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.useFluidScene = false;

    runtime_scene_bridge_preflight_diag(out_summary, "ok");
    json_object_put(root);
    return true;
}

bool runtime_scene_bridge_apply_file(const char *runtime_scene_path,
                                     RuntimeSceneBridgePreflight *out_summary) {
    CoreBuffer file_data = {0};
    CoreResult io_result;
    char runtime_scene_path_copy[sizeof(animSettings.runtimeScenePath)];
    char *json_text = NULL;
    bool ok;

    if (!runtime_scene_path || !out_summary) return false;
    runtime_scene_bridge_preflight_reset(out_summary);
    snprintf(runtime_scene_path_copy,
             sizeof(runtime_scene_path_copy),
             "%s",
             runtime_scene_path);

    io_result = core_io_read_all(runtime_scene_path_copy, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        runtime_scene_bridge_preflight_diag(out_summary, "failed to read runtime scene file");
        core_io_buffer_free(&file_data);
        return false;
    }

    json_text = (char *)malloc(file_data.size + 1u);
    if (!json_text) {
        runtime_scene_bridge_preflight_diag(out_summary, "out of memory");
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(json_text, file_data.data, file_data.size);
    json_text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    ok = runtime_scene_bridge_apply_json(json_text, out_summary);
    if (ok) {
        snprintf(animSettings.runtimeScenePath,
                 sizeof(animSettings.runtimeScenePath),
                 "%s",
                 runtime_scene_path_copy);
    }
    free(json_text);
    return ok;
}

bool runtime_scene_bridge_writeback_ray_overlay_json(const char *runtime_scene_json,
                                                     const char *overlay_json,
                                                     char **out_runtime_scene_json,
                                                     char *out_diagnostics,
                                                     size_t out_diagnostics_size) {
    json_object *runtime_root = NULL;
    json_object *overlay_root = NULL;
    const char *serialized = NULL;
    char *out = NULL;
    size_t out_len = 0;

    if (out_runtime_scene_json) *out_runtime_scene_json = NULL;
    runtime_scene_bridge_bridge_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!runtime_scene_json || !overlay_json || !out_runtime_scene_json) return false;

    runtime_root = json_tokener_parse(runtime_scene_json);
    overlay_root = json_tokener_parse(overlay_json);
    if (!runtime_root || !json_object_is_type(runtime_root, json_type_object) ||
        !overlay_root || !json_object_is_type(overlay_root, json_type_object)) {
        runtime_scene_bridge_bridge_diag(out_diagnostics, out_diagnostics_size, "invalid JSON object");
        if (runtime_root) json_object_put(runtime_root);
        if (overlay_root) json_object_put(overlay_root);
        return false;
    }

    if (!runtime_scene_bridge_validate_root_diag(runtime_root, out_diagnostics, out_diagnostics_size)) {
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }
    if (!core_scene_overlay_merge_apply(runtime_root,
                                        overlay_root,
                                        "ray_tracing",
                                        "ray_tracing",
                                        out_diagnostics,
                                        out_diagnostics_size)) {
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }

    serialized = json_object_to_json_string_ext(runtime_root,
                                                JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    if (!serialized) {
        runtime_scene_bridge_bridge_diag(out_diagnostics, out_diagnostics_size, "failed to serialize merged runtime scene");
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }

    out_len = strlen(serialized);
    out = (char *)malloc(out_len + 1u);
    if (!out) {
        runtime_scene_bridge_bridge_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }
    memcpy(out, serialized, out_len + 1u);
    *out_runtime_scene_json = out;
    runtime_scene_bridge_bridge_diag(out_diagnostics, out_diagnostics_size, "ok");

    json_object_put(runtime_root);
    json_object_put(overlay_root);
    return true;
}

void runtime_scene_bridge_get_last_3d_scaffold_state(RuntimeSceneBridge3DScaffoldState *out_state) {
    if (!out_state) return;
    *out_state = g_last_3d_scaffold;
}

void runtime_scene_bridge_get_last_3d_digest_state(RuntimeSceneBridge3DDigestState *out_state) {
    if (!out_state) return;
    *out_state = g_last_3d_digest;
}

void runtime_scene_bridge_get_last_3d_primitive_seed_state(
    RuntimeSceneBridge3DPrimitiveSeedState *out_state) {
    if (!out_state) return;
    *out_state = g_last_3d_primitive_seeds;
}

bool runtime_scene_bridge_get_last_object_id_for_scene_index(int scene_index,
                                                             char *out_object_id,
                                                             size_t out_object_id_size) {
    if (out_object_id && out_object_id_size > 0) {
        out_object_id[0] = '\0';
    }
    if (scene_index < 0 ||
        scene_index >= g_last_runtime_object_id_count ||
        !out_object_id ||
        out_object_id_size == 0) {
        return false;
    }
    if (!g_last_runtime_object_ids[scene_index][0]) {
        return false;
    }
    snprintf(out_object_id, out_object_id_size, "%s", g_last_runtime_object_ids[scene_index]);
    return true;
}
