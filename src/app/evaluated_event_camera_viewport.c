#include "app/evaluated_scene_service.h"
#include "import/runtime_camera_rig.h"
#include "import/runtime_event_camera.h"
bool RayEvaluatedSceneBindViewport(const RayEvaluatedSceneSnapshot *source,
                                   int width, int height,
                                   RayEvaluatedSceneSnapshot *out) {
  if (!source || !out || width <= 0 || height <= 0 ||
      RayEvaluatedSceneSnapshotValidate(source) != TIMELINE_STATUS_OK)
    return false;
  RayEvaluatedSceneSnapshot value = *source;
  if (RuntimeEventCameraOwns(value.camera.target_id)) {
    if (!RuntimeCameraRigSnapshotMatches(source))
      return false;
    bool handled = false;
    if (!RuntimeCameraRigEvaluateForAspect(&value.frame, value.camera.target_id,
                                           (double)width / height,
                                           &value.camera, &handled) ||
        !handled ||
        RayEvaluatedSceneSnapshotValidate(&value) != TIMELINE_STATUS_OK)
      return false;
  }
  *out = value;
  return true;
}
