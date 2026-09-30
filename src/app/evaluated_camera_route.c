#include "app/evaluated_camera_route.h"
#include "motion/scene_motion_paths.h"
#include "import/runtime_scene_timeline.h"
#include <string.h>
#include <math.h>
#include "import/runtime_scene_bridge.h"
bool EvaluatedCameraRouteSample(const Camera *camera, double z, const Path *path,
    const CameraPath3D *depth, double normalized_t, int width, int height,
    const TimelineFrameSnapshot *snapshot, PreviewCameraSample *out) {
  MotionPathBinding binding;
  if (!MotionPathsRuntimeBinding("camera/main", &binding))
    return PreviewCameraSampleEvaluateTimeline(camera, z, path, depth, normalized_t,
        width, height, snapshot, "camera/main", out);
  TimelineFrameSnapshot orientation = *snapshot;
  bool has_route = false; double progress = 0;
  orientation.property_count = 0;
  for (size_t i = 0; i < snapshot->property_count; ++i) {
    const TimelinePropertyEvaluationResult *p = &snapshot->properties[i];
    if (!strcmp(p->track.target_id, "camera/main") &&
        !strcmp(p->track.property_id, MOTION_CAMERA_PROGRESS_PROPERTY)) {
      has_route = true; progress = p->track.value.as.scalar;
    } else orientation.properties[orientation.property_count++] = *p;
  }
  if (!has_route || !binding.restore_known) return false;
  const TimelineDocument *doc = RuntimeSceneTimelineRead();
  for (size_t j = 0; j < binding.restore_count; ++j) {
    bool found = false;
    for (size_t i = 0; doc && i < doc->track_count; ++i) {
      if (strcmp(doc->tracks[i].track_id, binding.restore_xyz_tracks[j])) continue;
      TimelineTrack source = doc->tracks[i];
      source.enabled = true; /* copied evaluation only; never reactivate authoring */
      if (orientation.property_count == TIMELINE_FRAME_SNAPSHOT_PROPERTY_CAPACITY) return false;
      TimelinePropertyEvaluationResult p = {0};
      if (TimelineTrackEvaluate(&source, &snapshot->context, &p.track) != TIMELINE_STATUS_OK) return false;
      p.target_kind = TIMELINE_PROPERTY_TARGET_CAMERA;
      p.unit = source.unit;
      p.access = TIMELINE_PROPERTY_ACCESS_AUTHORABLE;
      p.invalidation_domains = TIMELINE_INVALIDATION_CAMERA;
      orientation.properties[orientation.property_count++] = p;
      found = true; break;
    }
    if (!found) return false;
  }
  PreviewCameraSample result;
  TimelineVec3 position;
  if (!PreviewCameraSampleEvaluateTimeline(camera, z, path, depth, normalized_t,
          width, height, &orientation, "camera/main", &result) ||
      !MotionPathsRuntimeTargetPosition("camera/main", progress, &position)) return false;
  result.position_x = position.x; result.position_y = position.y; result.position_z = position.z;
  result.uses_authored_path = true;
  *out = result;
  return true;
}

/* Preserve the established focus-target precedence after final translation.
 * Doing this before route placement aims from the wrong camera position. */
void EvaluatedCameraApplyFocusTarget(PreviewCameraSample *sample) {
  MotionPathBinding binding;
  if (!MotionPathsRuntimeBinding("camera/main", &binding) || !binding.use_focus_target) return;
  RuntimeSceneBridge3DScaffoldState scaffold = {0};
  runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
  if (!sample || !scaffold.has_camera_focus_target) return;
  double dx=scaffold.camera_focus_target_x-sample->position_x;
  double dy=scaffold.camera_focus_target_y-sample->position_y;
  double dz=scaffold.camera_focus_target_z-sample->position_z;
  double horizontal=hypot(dx,dy),limit=70.0*acos(-1.0)/180.0;
  if (!(horizontal>1e-9) && !(fabs(dz)>1e-9)) return;
  if (horizontal>1e-9) sample->yaw_radians=atan2(dx,-dy);
  sample->pitch_radians=fmax(-limit,fmin(limit,atan2(dz,horizontal)));
}
