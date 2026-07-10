#ifndef RUNTIME_SCENE_MOTION_BRIDGE_H
#define RUNTIME_SCENE_MOTION_BRIDGE_H

#include <json-c/json.h>

#include "motion/runtime_motion_track_3d.h"

void runtime_scene_motion_bridge_reset(void);
void runtime_scene_motion_bridge_apply_authoring(json_object *authoring);
void runtime_scene_motion_bridge_get_last_summary(RuntimeMotionTrack3DSummary *out_summary);

#endif
