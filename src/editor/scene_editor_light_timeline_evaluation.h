#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_EVALUATION_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_EVALUATION_H

#include <stdbool.h>

#include "animation/evaluated_scene_snapshot.h"
#include "animation/timeline_clock.h"
#include "app/evaluated_scene_service.h"
#include "import/runtime_scene_bridge.h"

typedef struct SceneEditorLightTimelineEvaluation {
    RayEvaluatedSceneServiceResult result;
} SceneEditorLightTimelineEvaluation;

bool scene_editor_light_timeline_evaluation_capture(
    SceneEditorLightTimelineEvaluation* evaluation,
    TimelineSample sample);

bool scene_editor_light_timeline_evaluation_copy(
    const SceneEditorLightTimelineEvaluation* evaluation,
    RayEvaluatedSceneSnapshot* out_snapshot);

const RayEvaluatedSceneSnapshot*
scene_editor_light_timeline_evaluation_snapshot(
    const SceneEditorLightTimelineEvaluation* evaluation);

void scene_editor_light_timeline_evaluation_light_position(
    const SceneEditorLightTimelineEvaluation* evaluation,
    const RuntimeSceneBridge3DLightSeedState* lights,
    int light_index,
    double* out_x,
    double* out_y,
    double* out_z);

#endif
