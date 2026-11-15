#pragma once
#include "cJSON.h"
#include "core_scene_rigid_result.h"
#include "import/runtime_rigid_result.h"
struct RuntimeRigidResult {
  CoreSceneRigidResult result;
  bool has_sequence;
  bool complex_bundle;
  SequenceClock sequence;
  uint64_t sequence_revision;
  char revision[65];
  bool hidden[RAY_RIGID_RESULT_MAX_BODIES];
  size_t asset_count;
  struct {
    char id[64], sha256[65];
    uint32_t child_count;
    bool approximate;
    cJSON *visible;
  } assets[RAY_RIGID_RESULT_MAX_ASSETS];
};
bool RuntimeRigidBundleRead(const char *path, const char *revision,
                            const char *current_json,
                            RuntimeRigidResult **output, char *diagnostic,
                            size_t capacity);

bool RuntimeRigidSequencePrepare(json_object *scene,RuntimeRigidResult *result,char *d,size_t n);
TimelineStatus RuntimeRigidSequenceEvaluate(const RuntimeRigidResult *result,
    const TimelineEvaluationContext *context,SequenceEvaluation *out,bool *present);
