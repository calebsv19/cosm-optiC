#ifndef RAY_TRACING_EVALUATED_SCENE_SERVICE_H
#define RAY_TRACING_EVALUATED_SCENE_SERVICE_H

#include <stdbool.h>

#include "animation/evaluated_scene_snapshot.h"

typedef struct RayEvaluatedSceneServiceResult {
    bool valid;
    TimelineStatus status;
    RayEvaluatedSceneSnapshot snapshot;
    char status_line[RAY_EVALUATED_SCENE_DIAGNOSTICS_CAPACITY];
} RayEvaluatedSceneServiceResult;

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
