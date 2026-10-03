/* Camera aim and roll share the route frame contract; legacy yaw/pitch remains
 * default. */
#include "app/evaluated_camera_orientation.h"
#include "import/runtime_scene_bridge.h"
#include "motion/scene_motion_plans.h"
#include <math.h>
#include <string.h>
#define AIM_STEPS 2048
static struct {
  bool valid;
  uint64_t paths, plans;
  double target[3];
  MotionFrame frames[AIM_STEPS + 1];
} aim;
static bool direction(double progress, const double target[3], double f[3]) {
  TimelineVec3 p;
  if (!MotionPathsRuntimeTargetPosition("camera/main", progress, &p))
    return false;
  f[0] = target[0] - p.x;
  f[1] = target[1] - p.y;
  f[2] = target[2] - p.z;
  return f[0] * f[0] + f[1] * f[1] + f[2] * f[2] > 1e-18;
}
bool EvaluatedCameraOrientation(const MotionPathBinding *b, double progress,
                                PreviewCameraSample *sample) {
  if (!b->camera_orientation)
    return true;
  MotionFrame frame;
  if (!MotionPathsRuntimeFrame("camera/main", progress, &frame)) {
    sample->orientation_fallback = true;
    return true;
  }
  sample->orientation_fallback = false;
  if (b->camera_orientation == 2) {
    RuntimeSceneBridge3DScaffoldState scaffold = {0};
    runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
    if (!scaffold.has_camera_focus_target)
      sample->orientation_fallback = true;
    else {
      double target[3] = {scaffold.camera_focus_target_x,
                          scaffold.camera_focus_target_y,
                          scaffold.camera_focus_target_z};
      uint64_t paths = MotionPathsRuntimeRevision(),
               plans = MotionPlansRuntimeRevision();
      if (!aim.valid || aim.paths != paths || aim.plans != plans ||
          memcmp(aim.target, target, sizeof(target))) {
        aim.valid = false;
        MotionFrame initial;
        double f[3];
        if (!MotionPathsRuntimeFrame("camera/main", 0, &initial))
          return false;
        MotionFrameRoll(&initial, -b->start_roll * 0.017453292519943295);
        if (direction(0, target, f)) {
          double up[3] = {b->start_up[0], b->start_up[1], b->start_up[2]};
          if (up[0] * up[0] + up[1] * up[1] + up[2] * up[2] < 1e-18)
            up[2] = 1;
          /* Focus owns forward: seed its up against the view, rather than
           * rotating a sideways dolly frame into the view and adding roll. */
          if (!MotionFrameSeed(&aim.frames[0], f, up))
            return false;
        } else
          aim.frames[0] = initial;
        for (int i = 1; i <= AIM_STEPS; ++i) {
          if (direction((double)i / AIM_STEPS, target, f))
            MotionFrameTransport(&aim.frames[i - 1], f, &aim.frames[i]);
          else
            aim.frames[i] = aim.frames[i - 1];
        }
        aim.paths = paths;
        aim.plans = plans;
        memcpy(aim.target, target, sizeof(target));
        aim.valid = true;
      }
      double t = fmax(0, fmin(1, progress)), f[3];
      int index = (int)floor(t * AIM_STEPS);
      if (direction(t, target, f))
        MotionFrameTransport(&aim.frames[index], f, &frame);
      else {
        frame = aim.frames[index];
        sample->orientation_fallback = true;
      }
      double roll = b->start_roll;
      if (b->end_roll_enabled)
        roll += (b->end_roll - b->start_roll) * t * t * (3 - 2 * t);
      MotionFrameRoll(&frame, roll * 0.017453292519943295);
    }
  }
  sample->has_orientation_frame = true;
  sample->orientation_frame = frame;
  sample->yaw_radians = atan2(frame.forward[0], -frame.forward[1]);
  sample->pitch_radians =
      atan2(frame.forward[2], hypot(frame.forward[0], frame.forward[1]));
  return true;
}
