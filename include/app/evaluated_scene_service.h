#ifndef RAY_TRACING_EVALUATED_SCENE_SERVICE_H
#define RAY_TRACING_EVALUATED_SCENE_SERVICE_H

#include <stdbool.h>

#include "animation/evaluated_scene_snapshot.h"
#include "motion/runtime_motion_track_3d.h"

typedef struct RayEvaluatedSceneServiceResult {
    bool valid;
    TimelineStatus status;
    RayEvaluatedSceneSnapshot snapshot;
    char status_line[RAY_EVALUATED_SCENE_DIAGNOSTICS_CAPACITY];
} RayEvaluatedSceneServiceResult;

TimelineStatus RayEvaluatedSceneCaptureCompatibilityTransforms(
    const RuntimeMotionTrack3DSummary* motion,
    const TimelineEvaluationContext* frame,
    RayEvaluatedObjectTransform* out_transforms,
    size_t capacity,
    size_t* out_count);
bool RayEvaluatedSceneCaptureForElapsed(
    double elapsed_seconds,
    RayEvaluatedSceneServiceResult* out_result);
bool RayEvaluatedSceneCaptureAuthoredSample(
    TimelineSample sample,
    RayEvaluatedSceneServiceResult* out_result);
bool RayEvaluatedSceneCaptureSample(
    TimelineSample sample,
    RayEvaluatedSceneServiceResult* out_result);

#endif
