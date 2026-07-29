#include "scene_editor_light_timeline_evaluation.h"

#include <string.h>

bool scene_editor_light_timeline_evaluation_capture(
    SceneEditorLightTimelineEvaluation* evaluation,
    TimelineSample sample) {
    RayEvaluatedSceneServiceResult result = {0};
    bool captured = false;
    if (!evaluation) return false;
    captured = RayEvaluatedSceneCaptureAuthoredSample(sample, &result);
    evaluation->result = result;
    return captured;
}

bool scene_editor_light_timeline_evaluation_copy(
    const SceneEditorLightTimelineEvaluation* evaluation,
    RayEvaluatedSceneSnapshot* out_snapshot) {
    if (!evaluation || !out_snapshot || !evaluation->result.valid ||
        !evaluation->result.snapshot.valid) {
        return false;
    }
    *out_snapshot = evaluation->result.snapshot;
    return true;
}

const RayEvaluatedSceneSnapshot*
scene_editor_light_timeline_evaluation_snapshot(
    const SceneEditorLightTimelineEvaluation* evaluation) {
    if (!evaluation || !evaluation->result.valid ||
        !evaluation->result.snapshot.valid) {
        return NULL;
    }
    return &evaluation->result.snapshot;
}

void scene_editor_light_timeline_evaluation_light_position(
    const SceneEditorLightTimelineEvaluation* evaluation,
    const RuntimeSceneBridge3DLightSeedState* lights,
    int light_index,
    double* out_x,
    double* out_y,
    double* out_z) {
    const RuntimeLightSource3D* light = NULL;
    const RayEvaluatedSceneSnapshot* snapshot =
        scene_editor_light_timeline_evaluation_snapshot(evaluation);
    if (!lights || light_index < 0 || light_index >= lights->light_count ||
        !out_x || !out_y || !out_z) {
        return;
    }
    light = &lights->lights[light_index];
    *out_x = light->position.x;
    *out_y = light->position.y;
    *out_z = light->position.z;
    if (snapshot && snapshot->light.valid &&
        strcmp(snapshot->light.runtime_light_id, light->id) == 0) {
        *out_x = snapshot->light.position.x;
        *out_y = snapshot->light.position.y;
        *out_z = snapshot->light.position.z;
    }
}
