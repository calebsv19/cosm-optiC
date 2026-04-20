#include "import/runtime_scene_bridge.h"

#include "core_scene_overlay_merge_shared.h"
#include "config/config_manager.h"
#include "core_io.h"
#include "scene/object_manager.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RuntimeSceneBridge3DScaffoldState g_last_3d_scaffold = {0};
static RuntimeSceneBridge3DDigestState g_last_3d_digest = {0};

static void preflight_reset(RuntimeSceneBridgePreflight *out_preflight) {
    if (!out_preflight) return;
    memset(out_preflight, 0, sizeof(*out_preflight));
    out_preflight->valid_contract = false;
}

static void preflight_diag(RuntimeSceneBridgePreflight *out_preflight, const char *message) {
    if (!out_preflight || !message) return;
    snprintf(out_preflight->diagnostics,
             sizeof(out_preflight->diagnostics),
             "%s",
             message);
}

static void bridge_diag(char *out_diagnostics, size_t out_diagnostics_size, const char *message) {
    if (!out_diagnostics || out_diagnostics_size == 0 || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static int json_array_len_or_zero(json_object *obj, const char *key) {
    json_object *array_obj = NULL;
    if (!obj || !key) return 0;
    if (!json_object_object_get_ex(obj, key, &array_obj)) return 0;
    if (!array_obj || !json_object_is_type(array_obj, json_type_array)) return 0;
    return (int)json_object_array_length(array_obj);
}

static bool parse_vec3(json_object *obj,
                       const char *key,
                       double *out_x,
                       double *out_y,
                       double *out_z) {
    json_object *node = NULL;
    json_object *x = NULL;
    json_object *y = NULL;
    json_object *z = NULL;
    if (!obj || !key || !out_x || !out_y || !out_z) return false;
    if (!json_object_object_get_ex(obj, key, &node) || !json_object_is_type(node, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(node, "x", &x) ||
        !json_object_object_get_ex(node, "y", &y) ||
        !json_object_object_get_ex(node, "z", &z)) {
        return false;
    }
    *out_x = json_object_get_double(x);
    *out_y = json_object_get_double(y);
    *out_z = json_object_get_double(z);
    return true;
}

static bool parse_bool_field(json_object *obj, const char *key, bool *out_value) {
    json_object *value = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_boolean)) {
        return false;
    }
    *out_value = json_object_get_boolean(value) != 0;
    return true;
}

static bool parse_double_field(json_object *obj, const char *key, double *out_value) {
    json_object *value = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value) ||
        (!json_object_is_type(value, json_type_double) && !json_object_is_type(value, json_type_int))) {
        return false;
    }
    *out_value = json_object_get_double(value);
    return true;
}

static const char *json_string_field_or_null(json_object *obj, const char *key) {
    json_object *value = NULL;
    if (!obj || !key) return NULL;
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

static int clamp_color_channel(double value01) {
    if (value01 < 0.0) value01 = 0.0;
    if (value01 > 1.0) value01 = 1.0;
    return (int)lround(value01 * 255.0);
}

static int color_from_material_albedo(json_object *materials_array,
                                      const char *material_id) {
    size_t i = 0;
    if (!materials_array || !json_object_is_type(materials_array, json_type_array) || !material_id) {
        return 0xFFFFFF;
    }

    for (i = 0; i < json_object_array_length(materials_array); ++i) {
        json_object *mat = json_object_array_get_idx(materials_array, i);
        json_object *id = NULL;
        json_object *albedo = NULL;
        if (!mat || !json_object_is_type(mat, json_type_object)) continue;
        if (!json_object_object_get_ex(mat, "material_id", &id) || !json_object_is_type(id, json_type_string)) {
            continue;
        }
        if (strcmp(json_object_get_string(id), material_id) != 0) continue;
        if (!json_object_object_get_ex(mat, "albedo", &albedo) || !json_object_is_type(albedo, json_type_array)) {
            return 0xFFFFFF;
        }
        if (json_object_array_length(albedo) < 3u) return 0xFFFFFF;
        {
            int r = clamp_color_channel(json_object_get_double(json_object_array_get_idx(albedo, 0)));
            int g = clamp_color_channel(json_object_get_double(json_object_array_get_idx(albedo, 1)));
            int b = clamp_color_channel(json_object_get_double(json_object_array_get_idx(albedo, 2)));
            return (r << 16) | (g << 8) | b;
        }
    }
    return 0xFFFFFF;
}

static bool validate_runtime_scene_root(json_object *root,
                                        RuntimeSceneBridgePreflight *out_preflight) {
    json_object *schema_family = NULL;
    json_object *schema_variant = NULL;
    json_object *scene_id = NULL;
    json_object *unit_system = NULL;
    json_object *world_scale = NULL;
    const char *schema_family_str = NULL;
    const char *schema_variant_str = NULL;
    const char *scene_id_str = NULL;
    const char *unit_system_str = NULL;
    double world_scale_value = 1.0;

    if (!root || !out_preflight) return false;

    if (!json_object_object_get_ex(root, "schema_family", &schema_family) ||
        !json_object_is_type(schema_family, json_type_string)) {
        preflight_diag(out_preflight, "missing schema_family");
        return false;
    }
    schema_family_str = json_object_get_string(schema_family);
    if (!schema_family_str || strcmp(schema_family_str, "codework_scene") != 0) {
        preflight_diag(out_preflight, "schema_family must be codework_scene");
        return false;
    }

    if (!json_object_object_get_ex(root, "schema_variant", &schema_variant) ||
        !json_object_is_type(schema_variant, json_type_string)) {
        preflight_diag(out_preflight, "missing schema_variant");
        return false;
    }
    schema_variant_str = json_object_get_string(schema_variant);
    if (!schema_variant_str || strcmp(schema_variant_str, "scene_runtime_v1") != 0) {
        preflight_diag(out_preflight, "schema_variant must be scene_runtime_v1");
        return false;
    }

    if (!json_object_object_get_ex(root, "scene_id", &scene_id) ||
        !json_object_is_type(scene_id, json_type_string)) {
        preflight_diag(out_preflight, "missing scene_id");
        return false;
    }
    scene_id_str = json_object_get_string(scene_id);
    if (!scene_id_str || !scene_id_str[0]) {
        preflight_diag(out_preflight, "scene_id is empty");
        return false;
    }

    if (!json_object_object_get_ex(root, "unit_system", &unit_system) ||
        !json_object_is_type(unit_system, json_type_string)) {
        preflight_diag(out_preflight, "missing unit_system");
        return false;
    }
    unit_system_str = json_object_get_string(unit_system);
    if (!unit_system_str || strcmp(unit_system_str, "meters") != 0) {
        preflight_diag(out_preflight, "unit_system must be meters");
        return false;
    }

    if (!json_object_object_get_ex(root, "world_scale", &world_scale) ||
        (!json_object_is_type(world_scale, json_type_double) &&
         !json_object_is_type(world_scale, json_type_int))) {
        preflight_diag(out_preflight, "missing world_scale");
        return false;
    }
    world_scale_value = json_object_get_double(world_scale);
    if (!(world_scale_value > 0.0) || !isfinite(world_scale_value)) {
        preflight_diag(out_preflight, "world_scale must be finite and > 0");
        return false;
    }

    snprintf(out_preflight->scene_id, sizeof(out_preflight->scene_id), "%s", scene_id_str);
    out_preflight->object_count = json_array_len_or_zero(root, "objects");
    out_preflight->material_count = json_array_len_or_zero(root, "materials");
    out_preflight->light_count = json_array_len_or_zero(root, "lights");
    out_preflight->camera_count = json_array_len_or_zero(root, "cameras");
    out_preflight->valid_contract = true;
    preflight_diag(out_preflight, "ok");
    return true;
}

static bool validate_runtime_scene_root_diag(json_object *root,
                                             char *out_diagnostics,
                                             size_t out_diagnostics_size) {
    RuntimeSceneBridgePreflight preflight;
    preflight_reset(&preflight);
    if (!validate_runtime_scene_root(root, &preflight)) {
        bridge_diag(out_diagnostics, out_diagnostics_size, preflight.diagnostics);
        return false;
    }
    bridge_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static void scene_defaults_reset(void) {
    sceneSettings.objectCount = 0;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.camera.zoom = 1.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.bezierPath.numPoints = 1;
    sceneSettings.bezierPath.points[0].x = 0.0;
    sceneSettings.bezierPath.points[0].y = 0.0;
    sceneSettings.bezierPath.mode = BEZIER_CUBIC;
    sceneSettings.cameraPath.numPoints = 1;
    sceneSettings.cameraPath.points[0].x = 0.0;
    sceneSettings.cameraPath.points[0].y = 0.0;
    sceneSettings.cameraPath.mode = BEZIER_CUBIC;
}

static void scaffold_state_reset(void) {
    memset(&g_last_3d_scaffold, 0, sizeof(g_last_3d_scaffold));
    memset(&g_last_3d_digest, 0, sizeof(g_last_3d_digest));
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

    object_id = json_string_field_or_null(object_obj, "object_id");
    if (object_id && object_id[0]) {
        snprintf(entry->object_id, sizeof(entry->object_id), "%s", object_id);
    }

    if (primitive_obj && json_object_is_type(primitive_obj, json_type_object)) {
        if (json_object_object_get_ex(primitive_obj, "frame", &frame) &&
            json_object_is_type(frame, json_type_object) &&
            json_object_object_get_ex(frame, "origin", &origin_source) &&
            json_object_is_type(origin_source, json_type_object)) {
            double ox = 0.0, oy = 0.0, oz = 0.0;
            if (parse_vec3(frame, "origin", &ox, &oy, &oz)) {
                entry->origin_x = ox * world_scale;
                entry->origin_y = oy * world_scale;
                entry->origin_z = oz * world_scale;
            }
        }
        has_width = parse_double_field(primitive_obj, "width", &width);
        has_height = parse_double_field(primitive_obj, "height", &height);
        has_depth = parse_double_field(primitive_obj, "depth", &depth);
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
        has_enabled = parse_bool_field(bounds, "enabled", &enabled);
        has_clamp = parse_bool_field(bounds, "clamp_on_edit", &clamp_on_edit);
        g_last_3d_digest.bounds_enabled = has_enabled && enabled;
        g_last_3d_digest.bounds_clamp_on_edit = has_clamp && clamp_on_edit;
        if (parse_vec3(bounds, "min", &min_x, &min_y, &min_z)) {
            g_last_3d_digest.bounds_min_x = min_x * world_scale;
            g_last_3d_digest.bounds_min_y = min_y * world_scale;
            g_last_3d_digest.bounds_min_z = min_z * world_scale;
        }
        if (parse_vec3(bounds, "max", &max_x, &max_y, &max_z)) {
            g_last_3d_digest.bounds_max_x = max_x * world_scale;
            g_last_3d_digest.bounds_max_y = max_y * world_scale;
            g_last_3d_digest.bounds_max_z = max_z * world_scale;
        }
    }

    if (json_object_object_get_ex(scene3d, "construction_plane", &construction_plane) &&
        json_object_is_type(construction_plane, json_type_object)) {
        const char *mode = json_string_field_or_null(construction_plane, "mode");
        const char *axis = json_string_field_or_null(construction_plane, "axis");
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
        if (parse_double_field(construction_plane, "offset", &offset)) {
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
    if (parse_vec3(light0, "position", &lx, &ly, &lz)) {
        (void)lz;
        sceneSettings.bezierPath.numPoints = 1;
        sceneSettings.bezierPath.points[0].x = lx * world_scale;
        sceneSettings.bezierPath.points[0].y = ly * world_scale;
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
    if (parse_vec3(camera0, "position", &cx, &cy, &cz)) {
        (void)cz;
        sceneSettings.camera.x = cx * world_scale;
        sceneSettings.camera.y = cy * world_scale;
        sceneSettings.cameraPath.numPoints = 1;
        sceneSettings.cameraPath.points[0].x = cx * world_scale;
        sceneSettings.cameraPath.points[0].y = cy * world_scale;
        g_last_3d_scaffold.has_camera_seed = true;
        g_last_3d_scaffold.camera_z = cz * world_scale;
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
    out_object->color = color_from_material_albedo(materials_array, mat_id);
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
        sceneSettings.objectCount++;
    }

    if (out_summary) out_summary->object_count = sceneSettings.objectCount;
}

bool runtime_scene_bridge_preflight_json(const char *runtime_scene_json,
                                         RuntimeSceneBridgePreflight *out_preflight) {
    json_object *root = NULL;

    if (!runtime_scene_json || !out_preflight) return false;
    preflight_reset(out_preflight);

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        preflight_diag(out_preflight, "invalid JSON object");
        if (root) json_object_put(root);
        return false;
    }

    if (!validate_runtime_scene_root(root, out_preflight)) {
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
    preflight_reset(out_preflight);

    io_result = core_io_read_all(runtime_scene_path, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        preflight_diag(out_preflight, "failed to read runtime scene file");
        core_io_buffer_free(&file_data);
        return false;
    }

    json_text = (char *)malloc(file_data.size + 1u);
    if (!json_text) {
        preflight_diag(out_preflight, "out of memory");
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
    preflight_reset(out_summary);

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        preflight_diag(out_summary, "invalid JSON object");
        if (root) json_object_put(root);
        return false;
    }

    if (!validate_runtime_scene_root(root, out_summary)) {
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
    apply_scene3d_extension_digest(root, world_scale);
    g_last_3d_scaffold.valid = true;
    g_last_3d_digest.valid = true;
    animSettings.runtimeScenePath[0] = '\0';
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.useFluidScene = false;

    preflight_diag(out_summary, "ok");
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
    preflight_reset(out_summary);
    snprintf(runtime_scene_path_copy,
             sizeof(runtime_scene_path_copy),
             "%s",
             runtime_scene_path);

    io_result = core_io_read_all(runtime_scene_path_copy, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        preflight_diag(out_summary, "failed to read runtime scene file");
        core_io_buffer_free(&file_data);
        return false;
    }

    json_text = (char *)malloc(file_data.size + 1u);
    if (!json_text) {
        preflight_diag(out_summary, "out of memory");
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
    bridge_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!runtime_scene_json || !overlay_json || !out_runtime_scene_json) return false;

    runtime_root = json_tokener_parse(runtime_scene_json);
    overlay_root = json_tokener_parse(overlay_json);
    if (!runtime_root || !json_object_is_type(runtime_root, json_type_object) ||
        !overlay_root || !json_object_is_type(overlay_root, json_type_object)) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "invalid JSON object");
        if (runtime_root) json_object_put(runtime_root);
        if (overlay_root) json_object_put(overlay_root);
        return false;
    }

    if (!validate_runtime_scene_root_diag(runtime_root, out_diagnostics, out_diagnostics_size)) {
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
        bridge_diag(out_diagnostics, out_diagnostics_size, "failed to serialize merged runtime scene");
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }

    out_len = strlen(serialized);
    out = (char *)malloc(out_len + 1u);
    if (!out) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }
    memcpy(out, serialized, out_len + 1u);
    *out_runtime_scene_json = out;
    bridge_diag(out_diagnostics, out_diagnostics_size, "ok");

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
