#pragma once
#include "animation/evaluated_scene_snapshot.h"
#include "import/runtime_mesh_asset_loader.h"
#include <json-c/json.h>
/* Consumer policy, separate from shared rigid-scene meaning. Matches the
 * bounded native convex-compound profile; distinct asset capacity is separate. */
enum {
  RAY_RIGID_RESULT_MAX_BODIES = 32,
  RAY_RIGID_RESULT_MAX_PAIRS =
      RAY_RIGID_RESULT_MAX_BODIES * (RAY_RIGID_RESULT_MAX_BODIES - 1) / 2,
  RAY_RIGID_RESULT_MAX_CHILDREN = 64,
  RAY_RIGID_RESULT_MAX_ASSETS = 32
};
typedef struct RuntimeRigidResult RuntimeRigidResult;
/* Pure preparation: no current scene/cache mutation. NULL candidate means
 * absent. */
bool RuntimeRigidResultPrepare(json_object *scene, const char *scene_path,
                               RuntimeRigidResult **candidate, char *diagnostic,
                               size_t capacity);
bool RuntimeRigidResultMeshesMatch(const RuntimeRigidResult *candidate,
                                   const RayTracingRuntimeMeshAssetSet *meshes,
                                   char *diagnostic, size_t capacity);
void RuntimeRigidResultFree(RuntimeRigidResult *result);
void RuntimeRigidResultCommit(RuntimeRigidResult *candidate);
bool RuntimeRigidResultActive(void);
bool RuntimeRigidResultClock(TimelineRate *rate, TimelineRange *range);
TimelineStatus
RuntimeRigidResultCapture(const TimelineEvaluationContext *context,
                          RayEvaluatedObjectTransform *transforms,
                          size_t capacity, size_t *count);
/* Returns false for invalid time. Unbound instances remain unchanged. */
bool RuntimeRigidResultInstanceAtT(RayTracingRuntimeMeshAssetInstance *instance,
                                   double t);
bool RuntimeRigidResultValidateReference(json_object *scene, char *diagnostic,
                                         size_t capacity);

bool RuntimeRigidResultValidateScene(json_object *scene, char *diagnostic,
                                     size_t capacity);

bool RuntimeRigidResultSnapshotMatches(
    const RayEvaluatedSceneSnapshot *snapshot);
bool RuntimeRigidResultStrictJSON(const char *text);

/* Owned inspection copy; no mutable cache/solver access. Caller puts JSON. */
json_object *RuntimeRigidResultInspection(void);

TimelineStatus RuntimeRigidResultSequence(const TimelineEvaluationContext *context,
    SequenceEvaluation *out,bool *present);
uint64_t RuntimeRigidResultSequenceRevision(void);
