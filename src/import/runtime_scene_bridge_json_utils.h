#ifndef RUNTIME_SCENE_BRIDGE_JSON_UTILS_H
#define RUNTIME_SCENE_BRIDGE_JSON_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#include <json-c/json.h>

#include "import/runtime_scene_bridge.h"

void runtime_scene_bridge_preflight_reset(RuntimeSceneBridgePreflight *out_preflight);
void runtime_scene_bridge_preflight_diag(RuntimeSceneBridgePreflight *out_preflight, const char *message);
void runtime_scene_bridge_bridge_diag(char *out_diagnostics, size_t out_diagnostics_size, const char *message);
int runtime_scene_bridge_json_array_len_or_zero(json_object *obj, const char *key);
bool runtime_scene_bridge_parse_vec3(json_object *obj,
                                     const char *key,
                                     double *out_x,
                                     double *out_y,
                                     double *out_z);
bool runtime_scene_bridge_parse_bool_field(json_object *obj, const char *key, bool *out_value);
bool runtime_scene_bridge_parse_double_field(json_object *obj, const char *key, double *out_value);
const char *runtime_scene_bridge_json_string_field_or_null(json_object *obj, const char *key);
int runtime_scene_bridge_clamp_color_channel(double value01);
int runtime_scene_bridge_color_from_material_albedo(json_object *materials_array, const char *material_id);
bool runtime_scene_bridge_validate_root(json_object *root, RuntimeSceneBridgePreflight *out_preflight);
bool runtime_scene_bridge_validate_root_diag(json_object *root,
                                             char *out_diagnostics,
                                             size_t out_diagnostics_size);

#endif
