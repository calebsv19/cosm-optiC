#include "import/runtime_scene_light_timeline_bridge.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

TimelineStatus RuntimeSceneLightTimelineResolveTarget(
    const RuntimeLightSource3D* lights,
    size_t light_count,
    const char* timeline_target_id,
    RuntimeSceneLightTimelineTarget* out_target) {
    RuntimeSceneLightTimelineTarget target;
    const char* runtime_id = NULL;
    size_t match_count = 0u;
    size_t match_index = 0u;
    if (!lights || light_count == 0u || !timeline_target_id || !out_target ||
        strncmp(timeline_target_id, "light/", 6u) != 0 ||
        timeline_target_id[6] == '\0') {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    runtime_id = timeline_target_id + 6u;
    memset(&target, 0, sizeof(target));
    for (size_t i = 0u; i < light_count; ++i) {
        if (lights[i].id[0] != '\0' && strcmp(lights[i].id, runtime_id) == 0) {
            match_count += 1u;
            match_index = i;
        }
    }
    if (match_count == 0u) return TIMELINE_STATUS_TARGET_NOT_FOUND;
    if (match_count > 1u) return TIMELINE_STATUS_DUPLICATE_ID;
    target.valid = true;
    target.light_index = match_index;
    snprintf(target.timeline_target_id, sizeof(target.timeline_target_id), "%s",
             timeline_target_id);
    snprintf(target.runtime_light_id, sizeof(target.runtime_light_id), "%s",
             runtime_id);
    *out_target = target;
    return TIMELINE_STATUS_OK;
}

TimelineStatus RuntimeSceneLightTimelineApplyMotion(
    RuntimeLightSource3D* lights,
    size_t light_count,
    const TimelineLightMotionSample* motion,
    RuntimeSceneLightTimelineTarget* out_target) {
    RuntimeSceneLightTimelineTarget target;
    TimelineStatus status;
    if (!lights || !motion || !motion->valid || !out_target ||
        !isfinite(motion->position.x) || !isfinite(motion->position.y) ||
        !isfinite(motion->position.z)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    memset(&target, 0, sizeof(target));
    status = RuntimeSceneLightTimelineResolveTarget(
        lights, light_count, motion->target_id, &target);
    if (status != TIMELINE_STATUS_OK) return status;
    lights[target.light_index].position = vec3(
        motion->position.x, motion->position.y, motion->position.z);
    *out_target = target;
    return TIMELINE_STATUS_OK;
}
