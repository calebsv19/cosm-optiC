#ifndef RUNTIME_SCENE_OBJECT_TIMELINE_H
#define RUNTIME_SCENE_OBJECT_TIMELINE_H
#include "animation/timeline_document.h"
#include "animation/evaluated_scene_snapshot.h"
#include <json-c/json.h>
int RuntimeObjectTimelineAxis(const char* property);
bool RuntimeObjectTimelineValidate(json_object* scene,const TimelineDocument* doc,char* message,size_t size);
bool RuntimeObjectTimelineHasMotion(void);
TimelineStatus RuntimeObjectTimelinePosition(const char* id,const TimelineEvaluationContext* context,TimelineVec3* out);
bool RuntimeObjectTimelinePositionAtT(const char* id,double t,TimelineVec3* out);
TimelineStatus RuntimeObjectTimelineCapture(const TimelineEvaluationContext* context,RayEvaluatedObjectTransform* transforms,size_t capacity,size_t* count);
#endif
