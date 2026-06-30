#ifndef RUNTIME_SCENE_BRIDGE_INTERNAL_H
#define RUNTIME_SCENE_BRIDGE_INTERNAL_H

#include <json-c/json.h>

#include "import/runtime_scene_bridge.h"

extern RuntimeSceneBridge3DScaffoldState g_last_3d_scaffold;
extern RuntimeSceneBridge3DDigestState g_last_3d_digest;
extern RuntimeSceneBridge3DPrimitiveSeedState g_last_3d_primitive_seeds;
extern RuntimeSceneBridge3DLightSeedState g_last_3d_light_seeds;
extern char g_last_runtime_object_ids[MAX_OBJECTS][64];
extern int g_last_runtime_object_id_count;

void runtime_scene_bridge_apply_scene3d_extension_digest(json_object *root,
                                                         double world_scale);
void runtime_scene_bridge_apply_space_mode(json_object *root);
void runtime_scene_bridge_apply_light_seed_scaled(json_object *lights_array,
                                                  double world_scale);
void runtime_scene_bridge_apply_camera_seed_scaled(json_object *cameras_array,
                                                   double world_scale);
void runtime_scene_bridge_apply_ray_authoring_paths(json_object *root,
                                                    double world_scale);

#endif
