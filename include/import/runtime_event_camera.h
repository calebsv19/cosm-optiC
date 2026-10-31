#ifndef RUNTIME_EVENT_CAMERA_H
#define RUNTIME_EVENT_CAMERA_H
#include "import/runtime_rigid_result.h"
#include <json-c/json.h>
#define RAY_EVENT_GEOMETRY_VALIDATION_POLICY "ray_event_geometry_validation_v1"
bool RuntimeEventCameraValidate(json_object *, char *, size_t);
bool RuntimeEventCameraValidateResult(json_object *, const RuntimeRigidResult *,
                                      char *, size_t);
bool RuntimeEventCameraLoad(json_object *);
void RuntimeEventCameraReset(void);
bool RuntimeEventCameraOwns(const char*);
double RuntimeEventCameraMinimumAspect(const char*);
bool RuntimeEventCameraApply(const TimelineEvaluationContext *,
                             RayEvaluatedCamera *);
/* Owned proposals and retained entry; candidate hash binds the reviewed intent,
 * resolved geometry and source. No solver or scene mutation. */
json_object *RuntimeEventCameraPropose(json_object *, json_object *, char *,
                                       size_t);
json_object *RuntimeEventCameraSelect(json_object *, json_object *, int,
                                      const char *, char *, size_t);
bool RuntimeEventCameraAttach(json_object*,json_object*,char*,size_t);
json_object *RuntimeEventCameraDependencies(json_object *, json_object *);
json_object *RuntimeEventCameraPickFeature(json_object *, uint64_t, const double[3], const double[3], double, char *, size_t);
bool RuntimeEventCameraRecordedPoint(const char *,uint64_t,const double[3],double[3]);
#endif
