#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_bridge_internal.h"
#include "import/runtime_scene_bridge_json_utils.h"
#include "import/runtime_mesh_asset_loader.h"
#include "import/runtime_scene_volume_defaults.h"

#include "camera/camera_path_3d.h"
#include "core_scene_overlay_merge_shared.h"
#include "config/config_manager.h"
#include "config/config_scene_path_io.h"
#include "core_io.h"
#include "editor/scene_editor_material_graph.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "material/material_manager.h"
#include "render/runtime_native_3d_prepare_cache.h"
#include "render/runtime_material_authored_texture_3d.h"
#include "scene/object_manager.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUNTIME_SCENE_BRIDGE_EDITOR_MESH_PREVIEW_MAX_ASSET_BYTES (1024u * 1024u)

RuntimeSceneBridge3DScaffoldState g_last_3d_scaffold = {0};
RuntimeSceneBridge3DDigestState g_last_3d_digest = {0};
RuntimeSceneBridge3DPrimitiveSeedState g_last_3d_primitive_seeds = {0};
RuntimeSceneBridge3DLightSeedState g_last_3d_light_seeds = {0};
char g_last_runtime_object_ids[MAX_OBJECTS][64] = {{0}};
int g_last_runtime_object_id_count = 0;

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
    SceneEditorMaterialFacePlacementResetAll();
    SceneEditorMaterialStackResetAll();
    SceneEditorMaterialGraphResetAll();
    RuntimeMaterialAuthoredTextureResetAll();
    memset(g_last_runtime_object_ids, 0, sizeof(g_last_runtime_object_ids));
    g_last_runtime_object_id_count = 0;
}

static void scaffold_state_reset(void) {
    memset(&g_last_3d_scaffold, 0, sizeof(g_last_3d_scaffold));
    memset(&g_last_3d_digest, 0, sizeof(g_last_3d_digest));
    memset(&g_last_3d_primitive_seeds, 0, sizeof(g_last_3d_primitive_seeds));
    memset(&g_last_3d_light_seeds, 0, sizeof(g_last_3d_light_seeds));
}

static double runtime_scene_bridge_scale_scene_length(
    double scene_length [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double world_scale) {
    double authored_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = scene_length;
    return authored_length * world_scale;
}

static double runtime_scene_bridge_scale_scene_length_axis(
    double scene_length [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double authored_axis_scale,
    double world_scale) {
    double authored_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = scene_length;
    return authored_length * fabs(authored_axis_scale) * world_scale;
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

static bool runtime_scene_bridge_object_has_ray_authoring_material_binding(json_object *root,
                                                                           const char *object_id) {
    json_object *extensions = NULL;
    json_object *ray_tracing = NULL;
    json_object *authoring = NULL;
    json_object *object_materials = NULL;
    size_t count = 0u;
    size_t i = 0u;
    if (!root || !object_id || !object_id[0]) return false;
    if (!json_object_object_get_ex(root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object) ||
        !json_object_object_get_ex(extensions, "ray_tracing", &ray_tracing) ||
        !json_object_is_type(ray_tracing, json_type_object) ||
        !json_object_object_get_ex(ray_tracing, "authoring", &authoring) ||
        !json_object_is_type(authoring, json_type_object) ||
        !json_object_object_get_ex(authoring, "object_materials", &object_materials) ||
        !json_object_is_type(object_materials, json_type_array)) {
        return false;
    }
    count = json_object_array_length(object_materials);
    for (i = 0u; i < count; ++i) {
        json_object *entry = json_object_array_get_idx(object_materials, (int)i);
        json_object *entry_object_id = NULL;
        const char *entry_object_id_str = NULL;
        if (!entry || !json_object_is_type(entry, json_type_object)) continue;
        if (!json_object_object_get_ex(entry, "object_id", &entry_object_id) ||
            !json_object_is_type(entry_object_id, json_type_string)) {
            continue;
        }
        entry_object_id_str = json_object_get_string(entry_object_id);
        if (entry_object_id_str && strcmp(entry_object_id_str, object_id) == 0) {
            return true;
        }
    }
    return false;
}

static int runtime_scene_bridge_physics_emitter_packed_color(const char *type_str) {
    if (type_str && strcmp(type_str, "Jet") == 0) {
        return SceneObjectPackRGBBytes(74, 232, 124);
    }
    if (type_str && strcmp(type_str, "Sink") == 0) {
        return SceneObjectPackRGBBytes(232, 96, 136);
    }
    return SceneObjectPackRGBBytes(246, 233, 90);
}

static bool runtime_scene_bridge_object_has_active_physics_emitter_overlay(json_object *root,
                                                                           const char *object_id,
                                                                           int *out_packed_color) {
    json_object *extensions = NULL;
    json_object *physics_sim = NULL;
    json_object *object_overlays = NULL;
    size_t count = 0u;
    size_t i = 0u;
    if (out_packed_color) *out_packed_color = 0;
    if (!root || !object_id || !object_id[0]) return false;
    if (!json_object_object_get_ex(root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object) ||
        !json_object_object_get_ex(extensions, "physics_sim", &physics_sim) ||
        !json_object_is_type(physics_sim, json_type_object) ||
        !json_object_object_get_ex(physics_sim, "object_overlays", &object_overlays) ||
        !json_object_is_type(object_overlays, json_type_array)) {
        return false;
    }
    count = json_object_array_length(object_overlays);
    for (i = 0u; i < count; ++i) {
        json_object *entry = json_object_array_get_idx(object_overlays, (int)i);
        json_object *entry_object_id = NULL;
        json_object *emitter = NULL;
        json_object *active = NULL;
        json_object *type = NULL;
        const char *entry_object_id_str = NULL;
        const char *type_str = NULL;
        if (!entry || !json_object_is_type(entry, json_type_object)) continue;
        if (!json_object_object_get_ex(entry, "object_id", &entry_object_id) ||
            !json_object_is_type(entry_object_id, json_type_string)) {
            continue;
        }
        entry_object_id_str = json_object_get_string(entry_object_id);
        if (!entry_object_id_str || strcmp(entry_object_id_str, object_id) != 0) continue;
        if (!json_object_object_get_ex(entry, "emitter", &emitter) ||
            !json_object_is_type(emitter, json_type_object) ||
            !json_object_object_get_ex(emitter, "active", &active) ||
            !json_object_is_type(active, json_type_boolean)) {
            return false;
        }
        if (json_object_object_get_ex(emitter, "type", &type) &&
            json_object_is_type(type, json_type_string)) {
            type_str = json_object_get_string(type);
        }
        if (out_packed_color) {
            *out_packed_color = runtime_scene_bridge_physics_emitter_packed_color(type_str);
        }
        return json_object_get_boolean(active) != 0;
    }
    return false;
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
                                    double world_scale,
                                    int scene_object_index,
                                    bool guide_only) {
    RuntimeSceneBridgePrimitiveDigest *entry = NULL;
    const char *object_id = NULL;
    json_object *frame = NULL;
    json_object *origin_source = NULL;
    json_object *position_source = NULL;
    double width [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double height [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double depth [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    bool has_width = false;
    bool has_height = false;
    bool has_depth = false;

    if (g_last_3d_digest.primitive_count >= RUNTIME_SCENE_BRIDGE_MAX_DIGEST_PRIMITIVES) return;
    entry = &g_last_3d_digest.primitives[g_last_3d_digest.primitive_count];
    memset(entry, 0, sizeof(*entry));
    entry->kind = kind;
    entry->scene_object_index = scene_object_index;
    entry->guide_only = guide_only;

    object_id = runtime_scene_bridge_json_string_field_or_null(object_obj, "object_id");
    if (object_id && object_id[0]) {
        snprintf(entry->object_id, sizeof(entry->object_id), "%s", object_id);
    }

    if (primitive_obj && json_object_is_type(primitive_obj, json_type_object)) {
        if (json_object_object_get_ex(primitive_obj, "frame", &frame) &&
            json_object_is_type(frame, json_type_object) &&
            json_object_object_get_ex(frame, "origin", &origin_source) &&
            json_object_is_type(origin_source, json_type_object)) {
            double ox [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
            double oy [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
            double oz [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
            if (runtime_scene_bridge_parse_vec3(frame, "origin", &ox, &oy, &oz)) {
                entry->origin_x = runtime_scene_bridge_scale_scene_length(ox, world_scale);
                entry->origin_y = runtime_scene_bridge_scale_scene_length(oy, world_scale);
                entry->origin_z = runtime_scene_bridge_scale_scene_length(oz, world_scale);
            }
        }
        has_width = runtime_scene_bridge_parse_double_field(primitive_obj, "width", &width);
        has_height = runtime_scene_bridge_parse_double_field(primitive_obj, "height", &height);
        has_depth = runtime_scene_bridge_parse_double_field(primitive_obj, "depth", &depth);
    }

    if (transform_obj && json_object_is_type(transform_obj, json_type_object) &&
        json_object_object_get_ex(transform_obj, "position", &position_source) &&
        json_object_is_type(position_source, json_type_object)) {
        json_object *jx = NULL;
        json_object *jy = NULL;
        json_object *jz = NULL;
        if (json_object_object_get_ex(position_source, "x", &jx)) {
            double scene_x [[fisics::dim(length)]] [[fisics::unit(meter)]] =
                json_object_get_double(jx);
            entry->origin_x = runtime_scene_bridge_scale_scene_length(scene_x, world_scale);
        }
        if (json_object_object_get_ex(position_source, "y", &jy)) {
            double scene_y [[fisics::dim(length)]] [[fisics::unit(meter)]] =
                json_object_get_double(jy);
            entry->origin_y = runtime_scene_bridge_scale_scene_length(scene_y, world_scale);
        }
        if (json_object_object_get_ex(position_source, "z", &jz)) {
            double scene_z [[fisics::dim(length)]] [[fisics::unit(meter)]] =
                json_object_get_double(jz);
            entry->origin_z = runtime_scene_bridge_scale_scene_length(scene_z, world_scale);
        }
    }

    entry->has_dimensions = has_width || has_height || has_depth;
    if (has_width) entry->width = runtime_scene_bridge_scale_scene_length(width, world_scale);
    if (has_height) entry->height = runtime_scene_bridge_scale_scene_length(height, world_scale);
    if (has_depth) entry->depth = runtime_scene_bridge_scale_scene_length(depth, world_scale);

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

static void primitive_seed_rotate_basis_vector(double *x,
                                               double *y,
                                               double *z,
                                               double rx,
                                               double ry,
                                               double rz) {
    double cx = cos(rx);
    double sx = sin(rx);
    double cy = cos(ry);
    double sy = sin(ry);
    double cz = cos(rz);
    double sz = sin(rz);
    double vx = x ? *x : 0.0;
    double vy = y ? *y : 0.0;
    double vz = z ? *z : 0.0;
    double ny = vy * cx - vz * sx;
    double nz = vy * sx + vz * cx;
    double nx = vx * cy + nz * sy;
    double nz2 = -vx * sy + nz * cy;
    double nx2 = nx * cz - ny * sz;
    double ny2 = nx * sz + ny * cz;
    if (x) *x = nx2;
    if (y) *y = ny2;
    if (z) *z = nz2;
}

static bool primitive_seed_parse_transform_rotation_radians(json_object *transform_obj,
                                                            double *out_x,
                                                            double *out_y,
                                                            double *out_z) {
    static const double kDegreesToRadians = 0.017453292519943295769;
    json_object *rotation = NULL;
    json_object *jx = NULL;
    json_object *jy = NULL;
    json_object *jz = NULL;
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    if (out_x) *out_x = 0.0;
    if (out_y) *out_y = 0.0;
    if (out_z) *out_z = 0.0;
    if (!transform_obj || !json_object_is_type(transform_obj, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(transform_obj, "rotation", &rotation) ||
        !json_object_is_type(rotation, json_type_object)) {
        return false;
    }
    if (json_object_object_get_ex(rotation, "x", &jx)) rx = json_object_get_double(jx);
    if (json_object_object_get_ex(rotation, "y", &jy)) ry = json_object_get_double(jy);
    if (json_object_object_get_ex(rotation, "z", &jz)) rz = json_object_get_double(jz);
    if (out_x) *out_x = rx * kDegreesToRadians;
    if (out_y) *out_y = ry * kDegreesToRadians;
    if (out_z) *out_z = rz * kDegreesToRadians;
    return true;
}

static void primitive_seed_apply_transform_rotation(RuntimeSceneBridgePrimitiveSeed *entry,
                                                    json_object *transform_obj) {
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    if (!entry) return;
    if (!primitive_seed_parse_transform_rotation_radians(transform_obj, &rx, &ry, &rz)) {
        return;
    }
    primitive_seed_rotate_basis_vector(&entry->axis_u_x, &entry->axis_u_y, &entry->axis_u_z, rx, ry, rz);
    primitive_seed_rotate_basis_vector(&entry->axis_v_x, &entry->axis_v_y, &entry->axis_v_z, rx, ry, rz);
    primitive_seed_rotate_basis_vector(&entry->normal_x, &entry->normal_y, &entry->normal_z, rx, ry, rz);
}

static void primitive_seed_append(json_object *object_obj,
                                  json_object *transform_obj,
                                  json_object *primitive_obj,
                                  RuntimeSceneBridgePrimitiveKind kind,
                                  double world_scale,
                                  int scene_object_index,
                                  bool guide_only) {
    RuntimeSceneBridgePrimitiveSeed *entry = NULL;
    const char *object_id = NULL;
    const char *object_type = runtime_scene_bridge_json_string_field_or_null(object_obj,
                                                                             "object_type");
    json_object *frame = NULL;
    json_object *position_source = NULL;
    double width [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double height [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double depth [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    double scale_x = 1.0;
    double scale_y = 1.0;
    double scale_z = 1.0;
    bool has_width = false;
    bool has_height = false;
    bool has_depth = false;

    if (guide_only) {
        return;
    }
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
    entry->guide_only = false;
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
                double scene_x [[fisics::dim(length)]] [[fisics::unit(meter)]] =
                    json_object_get_double(jx);
                entry->origin_x = runtime_scene_bridge_scale_scene_length(scene_x, world_scale);
            }
            if (json_object_object_get_ex(position_source, "y", &jy)) {
                double scene_y [[fisics::dim(length)]] [[fisics::unit(meter)]] =
                    json_object_get_double(jy);
                entry->origin_y = runtime_scene_bridge_scale_scene_length(scene_y, world_scale);
            }
            if (json_object_object_get_ex(position_source, "z", &jz)) {
                double scene_z [[fisics::dim(length)]] [[fisics::unit(meter)]] =
                    json_object_get_double(jz);
                entry->origin_z = runtime_scene_bridge_scale_scene_length(scene_z, world_scale);
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
        double ox [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double oy [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double oz [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double ax = 0.0;
        double ay = 0.0;
        double az = 0.0;
        if (json_object_object_get_ex(primitive_obj, "frame", &frame) &&
            json_object_is_type(frame, json_type_object)) {
            if (runtime_scene_bridge_parse_vec3(frame, "origin", &ox, &oy, &oz)) {
                if (!position_source || !json_object_is_type(position_source, json_type_object)) {
                    entry->origin_x = runtime_scene_bridge_scale_scene_length(ox, world_scale);
                    entry->origin_y = runtime_scene_bridge_scale_scene_length(oy, world_scale);
                    entry->origin_z = runtime_scene_bridge_scale_scene_length(oz, world_scale);
                }
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

    if (kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM ||
        kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
        primitive_seed_apply_transform_rotation(entry, transform_obj);
    }

    entry->has_dimensions = has_width || has_height || has_depth;
    entry->width = has_width
                       ? runtime_scene_bridge_scale_scene_length_axis(width, scale_x, world_scale)
                       : 0.0;
    entry->height = has_height
                        ? runtime_scene_bridge_scale_scene_length_axis(height, scale_y, world_scale)
                        : 0.0;
    entry->depth = has_depth
                       ? runtime_scene_bridge_scale_scene_length_axis(depth, scale_z, world_scale)
                       : 0.0;

    g_last_3d_primitive_seeds.primitive_count += 1;
    if (kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
        g_last_3d_primitive_seeds.plane_primitive_count += 1;
    } else if (kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM) {
        g_last_3d_primitive_seeds.rect_prism_primitive_count += 1;
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

static void apply_objects(json_object *root,
                          json_object *objects_array,
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
        double x [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double y [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double z [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
        double sx = 1.0, sy = 1.0, sz = 1.0;
        bool is_circle = true;
        bool is_plane = false;
        bool is_triangle_mesh = false;
        bool is_box = false;
        bool is_authoring_helper = false;
        bool has_active_physics_emitter_overlay = false;
        bool has_ray_authoring_material_binding = false;
        bool is_guide_only = false;
        int guide_overlay_color = 0;
        RuntimeSceneBridgePrimitiveKind digest_kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_UNKNOWN;
        SceneObject *dst = NULL;
        const char *object_id = NULL;
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

        object_id = runtime_scene_bridge_json_string_field_or_null(obj, "object_id");
        is_authoring_helper = runtime_scene_bridge_is_authoring_helper_object_type(type_str);
        has_active_physics_emitter_overlay =
            runtime_scene_bridge_object_has_active_physics_emitter_overlay(root,
                                                                           object_id,
                                                                           &guide_overlay_color);
        has_ray_authoring_material_binding =
            runtime_scene_bridge_object_has_ray_authoring_material_binding(root, object_id);
        is_guide_only =
            ((animSettings.spaceMode == SPACE_MODE_3D) && is_authoring_helper) ||
            has_active_physics_emitter_overlay;
        /*
         * Authoring helper objects describe editor paths/anchors only. Do not
         * consume live scene-object slots with them in native 3D mode, or
         * later renderable objects can be pushed past the fixed runtime object
         * array. In 2D mode, `curve_path` remains a live runtime object type.
         */
        if ((animSettings.spaceMode == SPACE_MODE_3D) && is_authoring_helper) {
            continue;
        }
        if (has_active_physics_emitter_overlay && !has_ray_authoring_material_binding) {
            digest_append_primitive(obj,
                                    transform,
                                    primitive,
                                    digest_kind,
                                    world_scale,
                                    sceneSettings.objectCount,
                                    true);
            primitive_seed_append(obj,
                                  transform,
                                  primitive,
                                  digest_kind,
                                  world_scale,
                                  sceneSettings.objectCount,
                                  true);
            continue;
        }
        if (sceneSettings.objectCount >= MAX_OBJECTS) {
            continue;
        }
        dst = &sceneSettings.sceneObjects[sceneSettings.objectCount];
        memset(g_last_runtime_object_ids[sceneSettings.objectCount],
               0,
               sizeof(g_last_runtime_object_ids[sceneSettings.objectCount]));
        if (object_id && object_id[0]) {
            snprintf(g_last_runtime_object_ids[sceneSettings.objectCount],
                     sizeof(g_last_runtime_object_ids[sceneSettings.objectCount]),
                     "%s",
                     object_id);
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
        dst->x = runtime_scene_bridge_scale_scene_length(x, world_scale);
        dst->y = runtime_scene_bridge_scale_scene_length(y, world_scale);
        dst->z = runtime_scene_bridge_scale_scene_length(z, world_scale);
        dst->scale = ((sx + sy + sz) / 3.0) * world_scale;
        if (dst->scale <= 0.01) dst->scale = 0.01;
        dst->guideOnly = is_guide_only;
        apply_object_material(obj, materials_array, dst);
        apply_object_flags(obj, dst);
        if (is_guide_only && guide_overlay_color != 0) {
            dst->color = guide_overlay_color;
        }
        digest_append_primitive(obj,
                                transform,
                                primitive,
                                digest_kind,
                                world_scale,
                                sceneSettings.objectCount,
                                is_guide_only);
        primitive_seed_append(obj,
                              transform,
                              primitive,
                              digest_kind,
                              world_scale,
                              sceneSettings.objectCount,
                              is_guide_only);
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
    if (ok) {
        RayTracingRuntimeMeshAssetSet mesh_assets;
        ray_tracing_runtime_mesh_asset_set_init(&mesh_assets);
        ok = ray_tracing_runtime_mesh_assets_load_scene_file(runtime_scene_path,
                                                            &mesh_assets,
                                                            out_preflight->diagnostics,
                                                            sizeof(out_preflight->diagnostics));
        ray_tracing_runtime_mesh_asset_set_free(&mesh_assets);
    }
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
    ray_tracing_runtime_mesh_assets_reset_last();
    RuntimeNative3DPreparedSceneMarkDirty("runtime_scene_bridge_apply_json");

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
    runtime_scene_bridge_apply_space_mode(root);
    if (json_object_object_get_ex(root, "world_scale", &world_scale_obj)) {
        world_scale = json_object_get_double(world_scale_obj);
    }

    json_object_object_get_ex(root, "materials", &materials);
    json_object_object_get_ex(root, "objects", &objects);
    json_object_object_get_ex(root, "lights", &lights);
    json_object_object_get_ex(root, "cameras", &cameras);

    apply_objects(root, objects, materials, world_scale, out_summary);
    runtime_scene_bridge_apply_light_seed_scaled(lights, world_scale);
    runtime_scene_bridge_apply_camera_seed_scaled(cameras, world_scale);
    runtime_scene_bridge_apply_ray_authoring_paths(root, world_scale);
    runtime_scene_bridge_apply_scene3d_extension_digest(root, world_scale);
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

static bool runtime_scene_bridge_apply_file_with_options(const char *runtime_scene_path,
                                                         RuntimeSceneBridgePreflight *out_summary,
                                                         bool load_mesh_assets) {
    CoreBuffer file_data = {0};
    CoreResult io_result;
    char runtime_scene_path_copy[sizeof(animSettings.runtimeScenePath)];
    char previous_runtime_scene_path[sizeof(animSettings.runtimeScenePath)];
    char *json_text = NULL;
    RayTracingRuntimeMeshAssetSet mesh_assets;
    bool ok;

    if (!runtime_scene_path || !out_summary) return false;
    runtime_scene_bridge_preflight_reset(out_summary);
    ray_tracing_runtime_mesh_asset_set_init(&mesh_assets);
    snprintf(previous_runtime_scene_path,
             sizeof(previous_runtime_scene_path),
             "%s",
             animSettings.runtimeScenePath);
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

    if (load_mesh_assets) {
        if (!ray_tracing_runtime_mesh_assets_load_scene_file(
                runtime_scene_path_copy,
                &mesh_assets,
                out_summary->diagnostics,
                sizeof(out_summary->diagnostics))) {
            free(json_text);
            return false;
        }
    } else {
        if (!ray_tracing_runtime_mesh_assets_load_scene_file_preview_limited(
                runtime_scene_path_copy,
                RUNTIME_SCENE_BRIDGE_EDITOR_MESH_PREVIEW_MAX_ASSET_BYTES,
                &mesh_assets,
                out_summary->diagnostics,
                sizeof(out_summary->diagnostics))) {
            ray_tracing_runtime_mesh_asset_set_free(&mesh_assets);
            ray_tracing_runtime_mesh_asset_set_init(&mesh_assets);
        }
    }

    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s",
             runtime_scene_path_copy);
    ok = runtime_scene_bridge_apply_json(json_text, out_summary);
    if (ok) {
        ray_tracing_runtime_mesh_assets_take_last(&mesh_assets);
    }
    if (ok) {
        runtime_scene_volume_defaults_apply_transition(&animSettings,
                                                       previous_runtime_scene_path,
                                                       runtime_scene_path_copy);
        snprintf(animSettings.runtimeScenePath,
                 sizeof(animSettings.runtimeScenePath),
                 "%s",
                 runtime_scene_path_copy);
    } else {
        snprintf(animSettings.runtimeScenePath,
                 sizeof(animSettings.runtimeScenePath),
                 "%s",
                 previous_runtime_scene_path);
        ray_tracing_runtime_mesh_asset_set_free(&mesh_assets);
    }
    free(json_text);
    return ok;
}

bool runtime_scene_bridge_apply_file(const char *runtime_scene_path,
                                     RuntimeSceneBridgePreflight *out_summary) {
    return runtime_scene_bridge_apply_file_with_options(runtime_scene_path, out_summary, true);
}

bool runtime_scene_bridge_apply_file_defer_mesh_assets(const char *runtime_scene_path,
                                                       RuntimeSceneBridgePreflight *out_summary) {
    return runtime_scene_bridge_apply_file_with_options(runtime_scene_path, out_summary, false);
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

void runtime_scene_bridge_get_last_3d_light_seed_state(
    RuntimeSceneBridge3DLightSeedState *out_state) {
    if (!out_state) return;
    *out_state = g_last_3d_light_seeds;
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
