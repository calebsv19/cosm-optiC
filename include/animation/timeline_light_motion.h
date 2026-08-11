#ifndef RAY_TRACING_TIMELINE_LIGHT_MOTION_H
#define RAY_TRACING_TIMELINE_LIGHT_MOTION_H

#include <stdbool.h>

#include "animation/timeline_track.h"
#include "camera/camera_path_3d.h"
#include "path/path_system.h"

typedef struct TimelineLightMotionSample {
    bool valid;
    bool speed_valid;
    char target_id[TIMELINE_ID_CAPACITY];
    double progress;
    double progress_per_frame;
    double path_length_world;
    double world_speed_per_second;
    double global_path_t;
    TimelineVec3 position;
    uint32_t invalidation_domains;
} TimelineLightMotionSample;

TimelineStatus TimelineLightMotionValidateProgressTrack(
    const TimelineTrack* progress_track,
    const TimelineRange* range);

TimelineStatus TimelineLightMotionEvaluate(
    const TimelineTrack* progress_track,
    const Path* path,
    const CameraPath3D* path3d,
    const TimelineEvaluationContext* context,
    TimelineLightMotionSample* out_sample);

#endif
