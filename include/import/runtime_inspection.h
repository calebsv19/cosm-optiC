#ifndef RUNTIME_INSPECTION_H
#define RUNTIME_INSPECTION_H
#include "animation/inspection_orbit.h"
#include "import/runtime_rigid_result.h"
#include <json-c/json.h>
bool RuntimeInspectionValidateResult(json_object *scene,
                                     const RuntimeRigidResult *result);
bool RuntimeInspectionValidate(json_object *scene, char *message, size_t size);
bool RuntimeInspectionDependencyMatches(json_object *scene, json_object *entry);
void RuntimeInspectionReset(void);
bool RuntimeInspectionLoad(json_object *scene);
bool RuntimeInspectionApply(const TimelineEvaluationContext *context,
                            RayEvaluatedCamera *camera);
/* Caller owns declaration/dependency objects. */
json_object *RuntimeInspectionDependencies(json_object *scene,
                                           json_object *entry);
json_object *RuntimeInspectionCapture(json_object *scene, const char *id,
                                      const char *camera, const char *target,
                                      uint64_t start, uint64_t count,
                                      double angle, double speed, double accel,
                                      double brake, char *message, size_t size);
#endif
