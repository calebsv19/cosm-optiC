#ifndef RUNTIME_SCENE_BRIDGE_AUTHORING_ENVIRONMENT_H
#define RUNTIME_SCENE_BRIDGE_AUTHORING_ENVIRONMENT_H

#include <json-c/json.h>

void runtime_scene_bridge_apply_ray_authoring_environment_settings(json_object *authoring);
void runtime_scene_bridge_apply_ray_authoring_light_settings(json_object *authoring,
                                                             double world_scale);

#endif
