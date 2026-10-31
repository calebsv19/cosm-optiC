#ifndef RUNTIME_CAMERA_RIG_H
#define RUNTIME_CAMERA_RIG_H
#include "animation/camera_sequence.h"
#include "animation/evaluated_scene_snapshot.h"
#include <json-c/json.h>
bool RuntimeCameraRigValidateScene(json_object *scene,char *diagnostic,size_t size);
bool RuntimeCameraRigDeclaredClock(json_object *scene,const char *id,TimelineSample sample,CameraSequenceSample *out);
bool RuntimeCameraRigTargetExists(json_object *scene,const char *target);
/* Owned physical-consumer copy: omit only validated named camera routes. */
json_object *RuntimeCameraRigPhysicalScene(json_object *scene);
void RuntimeCameraRigReset(void);
bool RuntimeCameraRigLoad(json_object *scene,double scale);
uint64_t RuntimeCameraRigRevision(void);
bool RuntimeCameraRigClock(TimelineRate *rate,TimelineRange *range);
/* handled=false means legacy scene; outputs unchanged on failure. */
bool RuntimeCameraRigEvaluate(const TimelineEvaluationContext *context,const char *id,
                             RayEvaluatedCamera *out,bool *handled);
bool RuntimeCameraRigEvaluateForAspect(const TimelineEvaluationContext*,const char*,double,RayEvaluatedCamera*,bool*);
bool RuntimeCameraRigEvaluateBase(const TimelineEvaluationContext *context,const char *id,RayEvaluatedCamera *out,bool *handled);
bool RuntimeCameraRigSnapshotMatches(const RayEvaluatedSceneSnapshot *snapshot);
#endif
