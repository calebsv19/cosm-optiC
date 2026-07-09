#include "import/runtime_scene_bridge_json_utils.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void runtime_scene_bridge_preflight_reset(RuntimeSceneBridgePreflight *out_preflight) {
    if (!out_preflight) return;
    memset(out_preflight, 0, sizeof(*out_preflight));
    out_preflight->valid_contract = false;
}

void runtime_scene_bridge_preflight_diag(RuntimeSceneBridgePreflight *out_preflight, const char *message) {
    if (!out_preflight || !message) return;
    snprintf(out_preflight->diagnostics,
             sizeof(out_preflight->diagnostics),
             "%s",
             message);
}

void runtime_scene_bridge_bridge_diag(char *out_diagnostics, size_t out_diagnostics_size, const char *message) {
    if (!out_diagnostics || out_diagnostics_size == 0 || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

int runtime_scene_bridge_json_array_len_or_zero(json_object *obj, const char *key) {
    json_object *array_obj = NULL;
    if (!obj || !key) return 0;
    if (!json_object_object_get_ex(obj, key, &array_obj)) return 0;
    if (!array_obj || !json_object_is_type(array_obj, json_type_array)) return 0;
    return (int)json_object_array_length(array_obj);
}

bool runtime_scene_bridge_parse_vec3(json_object *obj,
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

bool runtime_scene_bridge_parse_bool_field(json_object *obj, const char *key, bool *out_value) {
    json_object *value = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_boolean)) {
        return false;
    }
    *out_value = json_object_get_boolean(value) != 0;
    return true;
}

bool runtime_scene_bridge_parse_double_field(json_object *obj, const char *key, double *out_value) {
    json_object *value = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value) ||
        (!json_object_is_type(value, json_type_double) && !json_object_is_type(value, json_type_int))) {
        return false;
    }
    *out_value = json_object_get_double(value);
    return true;
}

const char *runtime_scene_bridge_json_string_field_or_null(json_object *obj, const char *key) {
    json_object *value = NULL;
    if (!obj || !key) return NULL;
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

int runtime_scene_bridge_clamp_color_channel(double value01) {
    if (value01 < 0.0) value01 = 0.0;
    if (value01 > 1.0) value01 = 1.0;
    return (int)lround(value01 * 255.0);
}

int runtime_scene_bridge_color_from_material_albedo(json_object *materials_array, const char *material_id) {
    size_t i = 0;
    if (!materials_array || !json_object_is_type(materials_array, json_type_array) || !material_id) {
        return 0xFFFFFF;
    }

    for (i = 0; i < json_object_array_length(materials_array); ++i) {
        json_object *mat = json_object_array_get_idx(materials_array, i);
        json_object *id = NULL;
        json_object *albedo = NULL;
        json_object *base_color = NULL;
        if (!mat || !json_object_is_type(mat, json_type_object)) continue;
        if ((!json_object_object_get_ex(mat, "material_id", &id) ||
             !json_object_is_type(id, json_type_string)) &&
            (!json_object_object_get_ex(mat, "id", &id) ||
             !json_object_is_type(id, json_type_string))) {
            continue;
        }
        if (strcmp(json_object_get_string(id), material_id) != 0) continue;
        if (json_object_object_get_ex(mat, "albedo", &albedo) &&
            json_object_is_type(albedo, json_type_array)) {
            if (json_object_array_length(albedo) < 3u) return 0xFFFFFF;
            int r = runtime_scene_bridge_clamp_color_channel(
                json_object_get_double(json_object_array_get_idx(albedo, 0)));
            int g = runtime_scene_bridge_clamp_color_channel(
                json_object_get_double(json_object_array_get_idx(albedo, 1)));
            int b = runtime_scene_bridge_clamp_color_channel(
                json_object_get_double(json_object_array_get_idx(albedo, 2)));
            return (r << 16) | (g << 8) | b;
        }
        if (json_object_object_get_ex(mat, "base_color", &base_color) &&
            json_object_is_type(base_color, json_type_object)) {
            json_object *r_obj = NULL;
            json_object *g_obj = NULL;
            json_object *b_obj = NULL;
            if (!json_object_object_get_ex(base_color, "r", &r_obj) ||
                !json_object_object_get_ex(base_color, "g", &g_obj) ||
                !json_object_object_get_ex(base_color, "b", &b_obj)) {
                return 0xFFFFFF;
            }
            {
                int r = runtime_scene_bridge_clamp_color_channel(json_object_get_double(r_obj));
                int g = runtime_scene_bridge_clamp_color_channel(json_object_get_double(g_obj));
                int b = runtime_scene_bridge_clamp_color_channel(json_object_get_double(b_obj));
                return (r << 16) | (g << 8) | b;
            }
        }
        if (json_object_object_get_ex(mat, "base_color", &base_color) &&
            json_object_is_type(base_color, json_type_array)) {
            if (json_object_array_length(base_color) < 3u) return 0xFFFFFF;
            int r = runtime_scene_bridge_clamp_color_channel(
                json_object_get_double(json_object_array_get_idx(base_color, 0)));
            int g = runtime_scene_bridge_clamp_color_channel(
                json_object_get_double(json_object_array_get_idx(base_color, 1)));
            int b = runtime_scene_bridge_clamp_color_channel(
                json_object_get_double(json_object_array_get_idx(base_color, 2)));
            return (r << 16) | (g << 8) | b;
        }
        return 0xFFFFFF;
    }
    return 0xFFFFFF;
}

bool runtime_scene_bridge_validate_root(json_object *root, RuntimeSceneBridgePreflight *out_preflight) {
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
        runtime_scene_bridge_preflight_diag(out_preflight, "missing schema_family");
        return false;
    }
    schema_family_str = json_object_get_string(schema_family);
    if (!schema_family_str || strcmp(schema_family_str, "codework_scene") != 0) {
        runtime_scene_bridge_preflight_diag(out_preflight, "schema_family must be codework_scene");
        return false;
    }

    if (!json_object_object_get_ex(root, "schema_variant", &schema_variant) ||
        !json_object_is_type(schema_variant, json_type_string)) {
        runtime_scene_bridge_preflight_diag(out_preflight, "missing schema_variant");
        return false;
    }
    schema_variant_str = json_object_get_string(schema_variant);
    if (!schema_variant_str || strcmp(schema_variant_str, "scene_runtime_v1") != 0) {
        runtime_scene_bridge_preflight_diag(out_preflight, "schema_variant must be scene_runtime_v1");
        return false;
    }

    if (!json_object_object_get_ex(root, "scene_id", &scene_id) ||
        !json_object_is_type(scene_id, json_type_string)) {
        runtime_scene_bridge_preflight_diag(out_preflight, "missing scene_id");
        return false;
    }
    scene_id_str = json_object_get_string(scene_id);
    if (!scene_id_str || !scene_id_str[0]) {
        runtime_scene_bridge_preflight_diag(out_preflight, "scene_id is empty");
        return false;
    }

    if (!json_object_object_get_ex(root, "unit_system", &unit_system) ||
        !json_object_is_type(unit_system, json_type_string)) {
        runtime_scene_bridge_preflight_diag(out_preflight, "missing unit_system");
        return false;
    }
    unit_system_str = json_object_get_string(unit_system);
    if (!unit_system_str || strcmp(unit_system_str, "meters") != 0) {
        runtime_scene_bridge_preflight_diag(out_preflight, "unit_system must be meters");
        return false;
    }

    if (!json_object_object_get_ex(root, "world_scale", &world_scale) ||
        (!json_object_is_type(world_scale, json_type_double) &&
         !json_object_is_type(world_scale, json_type_int))) {
        runtime_scene_bridge_preflight_diag(out_preflight, "missing world_scale");
        return false;
    }
    world_scale_value = json_object_get_double(world_scale);
    if (!(world_scale_value > 0.0) || !isfinite(world_scale_value)) {
        runtime_scene_bridge_preflight_diag(out_preflight, "world_scale must be finite and > 0");
        return false;
    }

    snprintf(out_preflight->scene_id, sizeof(out_preflight->scene_id), "%s", scene_id_str);
    out_preflight->object_count = runtime_scene_bridge_json_array_len_or_zero(root, "objects");
    out_preflight->material_count = runtime_scene_bridge_json_array_len_or_zero(root, "materials");
    out_preflight->light_count = runtime_scene_bridge_json_array_len_or_zero(root, "lights");
    out_preflight->camera_count = runtime_scene_bridge_json_array_len_or_zero(root, "cameras");
    out_preflight->valid_contract = true;
    runtime_scene_bridge_preflight_diag(out_preflight, "ok");
    return true;
}

bool runtime_scene_bridge_validate_root_diag(json_object *root,
                                             char *out_diagnostics,
                                             size_t out_diagnostics_size) {
    RuntimeSceneBridgePreflight preflight;
    runtime_scene_bridge_preflight_reset(&preflight);
    if (!runtime_scene_bridge_validate_root(root, &preflight)) {
        runtime_scene_bridge_bridge_diag(out_diagnostics, out_diagnostics_size, preflight.diagnostics);
        return false;
    }
    runtime_scene_bridge_bridge_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
