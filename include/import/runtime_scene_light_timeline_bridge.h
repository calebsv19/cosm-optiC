#ifndef RAY_TRACING_RUNTIME_SCENE_LIGHT_TIMELINE_BRIDGE_H
#define RAY_TRACING_RUNTIME_SCENE_LIGHT_TIMELINE_BRIDGE_H

#include <stddef.h>

#include "animation/timeline_light_motion.h"
#include "render/runtime_light_set_3d.h"

typedef struct RuntimeSceneLightTimelineTarget {
    bool valid;
    size_t light_index;
    char timeline_target_id[TIMELINE_ID_CAPACITY];
    char runtime_light_id[RUNTIME_LIGHT_SOURCE_3D_MAX_ID];
} RuntimeSceneLightTimelineTarget;

TimelineStatus RuntimeSceneLightTimelineResolveTarget(
    const RuntimeLightSource3D* lights,
    size_t light_count,
    const char* timeline_target_id,
    RuntimeSceneLightTimelineTarget* out_target);

TimelineStatus RuntimeSceneLightTimelineApplyMotion(
    RuntimeLightSource3D* lights,
    size_t light_count,
    const TimelineLightMotionSample* motion,
    RuntimeSceneLightTimelineTarget* out_target);

#endif
