#include "import/runtime_scene_bridge.h"

#include "config/config_manager.h"
#include "core_io.h"
#include "scene/object_manager.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    const char *schema_family_str = NULL;
    const char *schema_variant_str = NULL;
    const char *scene_id_str = NULL;

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

static bool overlay_has_forbidden_top_level_key(json_object *overlay_root,
                                                const char **out_key) {
    static const char *k_allowed[] = {
        "extensions",
        "space_mode_default"
    };
    json_object_object_foreach(overlay_root, key, value) {
        bool allowed = false;
        size_t i;
        (void)value;
        for (i = 0; i < sizeof(k_allowed) / sizeof(k_allowed[0]); ++i) {
            if (strcmp(key, k_allowed[i]) == 0) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            if (out_key) *out_key = key;
            return true;
        }
    }
    return false;
}

static bool overlay_extensions_only_ray_tracing(json_object *extensions_obj,
                                                const char **out_key) {
    json_object_object_foreach(extensions_obj, key, value) {
        (void)value;
        if (strcmp(key, "ray_tracing") != 0) {
            if (out_key) *out_key = key;
            return false;
        }
    }
    return true;
}

static bool set_or_replace_key_with_deep_copy(json_object *target,
                                              const char *key,
                                              json_object *src_value) {
    json_object *copy = NULL;
    const char *serialized = NULL;
    if (!target || !key || !src_value) return false;
    serialized = json_object_to_json_string_ext(src_value, JSON_C_TO_STRING_PLAIN);
    if (!serialized) return false;
    copy = json_tokener_parse(serialized);
    if (!copy) return false;
    json_object_object_del(target, key);
    json_object_object_add(target, key, copy);
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

static void apply_light_seed(json_object *lights_array) {
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
        sceneSettings.bezierPath.points[0].x = lx;
        sceneSettings.bezierPath.points[0].y = ly;
    }
}

static void apply_camera_seed(json_object *cameras_array) {
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
        sceneSettings.camera.x = cx;
        sceneSettings.camera.y = cy;
        sceneSettings.cameraPath.numPoints = 1;
        sceneSettings.cameraPath.points[0].x = cx;
        sceneSettings.cameraPath.points[0].y = cy;
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
                          RuntimeSceneBridgePreflight *out_summary) {
    static double k_default_poly[4][2] = {
        {-10.0, -10.0},
        {10.0, -10.0},
        {10.0, 10.0},
        {-10.0, 10.0}
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
        json_object *transform = NULL;
        json_object *position = NULL;
        json_object *scale = NULL;
        const char *type_str = NULL;
        double x = 0.0, y = 0.0, z = 0.0;
        double sx = 1.0, sy = 1.0, sz = 1.0;
        bool is_circle = true;
        SceneObject *dst = NULL;
        if (!obj || !json_object_is_type(obj, json_type_object)) continue;

        if (json_object_object_get_ex(obj, "object_type", &object_type) &&
            json_object_is_type(object_type, json_type_string)) {
            type_str = json_object_get_string(object_type);
        }
        if (type_str && strstr(type_str, "circle") == NULL) {
            is_circle = false;
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
        } else {
            InitObject(dst, OBJECT_POLYGON, x, y, 0.0, 0.0, k_default_poly, 4);
        }
        dst->z = z;
        dst->scale = (sx + sy + sz) / 3.0;
        if (dst->scale <= 0.01) dst->scale = 0.01;
        apply_object_material(obj, materials_array, dst);
        apply_object_flags(obj, dst);
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
    apply_space_mode(root);

    json_object_object_get_ex(root, "materials", &materials);
    json_object_object_get_ex(root, "objects", &objects);
    json_object_object_get_ex(root, "lights", &lights);
    json_object_object_get_ex(root, "cameras", &cameras);

    apply_objects(objects, materials, out_summary);
    apply_light_seed(lights);
    apply_camera_seed(cameras);

    preflight_diag(out_summary, "ok");
    json_object_put(root);
    return true;
}

bool runtime_scene_bridge_apply_file(const char *runtime_scene_path,
                                     RuntimeSceneBridgePreflight *out_summary) {
    CoreBuffer file_data = {0};
    CoreResult io_result;
    char *json_text = NULL;
    bool ok;

    if (!runtime_scene_path || !out_summary) return false;
    preflight_reset(out_summary);

    io_result = core_io_read_all(runtime_scene_path, &file_data);
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
    json_object *overlay_extensions = NULL;
    json_object *overlay_ray_extension = NULL;
    json_object *overlay_space_mode = NULL;
    json_object *runtime_extensions = NULL;
    const char *forbidden_key = NULL;
    const char *bad_extension_key = NULL;
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

    if (overlay_has_forbidden_top_level_key(overlay_root, &forbidden_key)) {
        char tmp[192];
        snprintf(tmp,
                 sizeof(tmp),
                 "overlay key not allowed: %s",
                 forbidden_key ? forbidden_key : "(unknown)");
        bridge_diag(out_diagnostics, out_diagnostics_size, tmp);
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }

    if (json_object_object_get_ex(overlay_root, "extensions", &overlay_extensions)) {
        if (!json_object_is_type(overlay_extensions, json_type_object)) {
            bridge_diag(out_diagnostics, out_diagnostics_size, "overlay extensions must be object");
            json_object_put(runtime_root);
            json_object_put(overlay_root);
            return false;
        }
        if (!overlay_extensions_only_ray_tracing(overlay_extensions, &bad_extension_key)) {
            char tmp[192];
            snprintf(tmp,
                     sizeof(tmp),
                     "overlay extension namespace not allowed: %s",
                     bad_extension_key ? bad_extension_key : "(unknown)");
            bridge_diag(out_diagnostics, out_diagnostics_size, tmp);
            json_object_put(runtime_root);
            json_object_put(overlay_root);
            return false;
        }
    }

    if (json_object_object_get_ex(overlay_root, "space_mode_default", &overlay_space_mode)) {
        if (!json_object_is_type(overlay_space_mode, json_type_string)) {
            bridge_diag(out_diagnostics, out_diagnostics_size, "space_mode_default must be string");
            json_object_put(runtime_root);
            json_object_put(overlay_root);
            return false;
        }
        if (!set_or_replace_key_with_deep_copy(runtime_root, "space_mode_default", overlay_space_mode)) {
            bridge_diag(out_diagnostics, out_diagnostics_size, "failed to apply space_mode_default");
            json_object_put(runtime_root);
            json_object_put(overlay_root);
            return false;
        }
    }

    if (json_object_object_get_ex(overlay_root, "extensions", &overlay_extensions)) {
        if (!json_object_object_get_ex(runtime_root, "extensions", &runtime_extensions) ||
            !json_object_is_type(runtime_extensions, json_type_object)) {
            runtime_extensions = json_object_new_object();
            if (!runtime_extensions) {
                bridge_diag(out_diagnostics, out_diagnostics_size, "failed to create runtime extensions");
                json_object_put(runtime_root);
                json_object_put(overlay_root);
                return false;
            }
            json_object_object_add(runtime_root, "extensions", runtime_extensions);
        }

        if (json_object_object_get_ex(overlay_extensions, "ray_tracing", &overlay_ray_extension)) {
            if (!set_or_replace_key_with_deep_copy(runtime_extensions, "ray_tracing", overlay_ray_extension)) {
                bridge_diag(out_diagnostics, out_diagnostics_size, "failed to apply ray_tracing extension");
                json_object_put(runtime_root);
                json_object_put(overlay_root);
                return false;
            }
        }
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
