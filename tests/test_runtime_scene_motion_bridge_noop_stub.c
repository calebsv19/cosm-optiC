#include "import/runtime_scene_motion_bridge.h"

#include <string.h>

void runtime_scene_motion_bridge_reset(void) {
}

void runtime_scene_motion_bridge_apply_authoring(json_object* authoring,
                                                 double world_scale) {
    (void)authoring;
    (void)world_scale;
}

void runtime_scene_motion_bridge_get_last_summary(
    RuntimeMotionTrack3DSummary* out_summary) {
    if (!out_summary) return;
    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->valid = true;
}

bool runtime_scene_motion_bridge_sample_object(const char* object_id,
                                               double normalized_t,
                                               RuntimeMotionTrack3DSample* out_sample) {
    (void)object_id;
    (void)normalized_t;
    if (out_sample) memset(out_sample, 0, sizeof(*out_sample));
    return false;
}

bool runtime_scene_motion_bridge_has_executable_motion(void) {
    return false;
}
