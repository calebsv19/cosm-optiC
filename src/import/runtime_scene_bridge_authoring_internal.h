#ifndef RUNTIME_SCENE_BRIDGE_AUTHORING_INTERNAL_H
#define RUNTIME_SCENE_BRIDGE_AUTHORING_INTERNAL_H

#include <stdbool.h>
#include <json-c/json.h>

#include "config/config_scene_path_io.h"

double runtime_scene_bridge_authoring_zero_length(void);
double runtime_scene_bridge_authoring_scale_scene_length(
    double scene_length [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double world_scale);
bool runtime_scene_bridge_parse_position_or_transform_position(json_object *obj,
                                                               double *out_x,
                                                               double *out_y,
                                                               double *out_z);
bool runtime_scene_bridge_parse_camera_seed_yaw(json_object *obj, double *out_yaw);
bool runtime_scene_bridge_parse_camera_seed_pitch(json_object *obj, double *out_pitch);
bool runtime_scene_bridge_parse_focus_target(json_object *obj,
                                             double world_scale,
                                             double *out_x,
                                             double *out_y,
                                             double *out_z);
bool runtime_scene_bridge_apply_authoring_path_scaled(json_object *authoring_obj,
                                                      const char *key,
                                                      Path *target_path,
                                                      double world_scale,
                                                      bool allow_empty);

#endif
