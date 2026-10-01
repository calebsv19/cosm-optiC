/* Derived fixed-distance frames: cache construction never depends on seek
 * order. */
#include "motion/scene_motion_paths.h"
#include "motion/scene_motion_plans.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define FRAME_STEPS 2048
typedef struct {
  char target[TIMELINE_ID_CAPACITY];
  bool valid;
  MotionFrame frames[FRAME_STEPS + 1];
} FrameCache;
static FrameCache cache[MOTION_BINDING_CAPACITY];
static uint64_t path_revision, plan_revision;
static bool tangent(const char *target, double t, double f[3]) {
  for (double step = 1e-5; step <= 1.01; step *= 10) {
    TimelineVec3 a, b;
    if (!MotionPathsRuntimeTargetPosition(target, fmax(0, t - step), &a) ||
        !MotionPathsRuntimeTargetPosition(target, fmin(1, t + step), &b))
      return false;
    f[0] = b.x - a.x;
    f[1] = b.y - a.y;
    f[2] = b.z - a.z;
    if (f[0] * f[0] + f[1] * f[1] + f[2] * f[2] > 1e-24)
      return true;
  }
  return false;
}
bool MotionPathsRuntimeFrame(const char *target, double progress,
                             MotionFrame *out) {
  MotionPathBinding b;
  if (!target || !out || !isfinite(progress) ||
      !MotionPathsRuntimeBinding(target, &b))
    return false;
  uint64_t pr = MotionPathsRuntimeRevision(), mr = MotionPlansRuntimeRevision();
  if (pr != path_revision || mr != plan_revision) {
    memset(cache, 0, sizeof(cache));
    path_revision = pr;
    plan_revision = mr;
  }
  FrameCache *entry = NULL;
  for (int i = 0; i < MOTION_BINDING_CAPACITY; ++i)
    if (!strcmp(cache[i].target, target)) {
      entry = &cache[i];
      break;
    }
  if (!entry)
    for (int i = 0; i < MOTION_BINDING_CAPACITY; ++i)
      if (!cache[i].target[0]) {
        entry = &cache[i];
        snprintf(entry->target, sizeof(entry->target), "%s", target);
        double f[3], up[3] = {0, 0, 1};
        if (b.start_up[0] || b.start_up[1] || b.start_up[2])
          memcpy(up, b.start_up, sizeof(up));
        if (!tangent(target, 0, f) ||
            !MotionFrameSeed(&entry->frames[0], f, up))
          return false;
        for (int j = 1; j <= FRAME_STEPS; ++j) {
          if (!tangent(target, (double)j / FRAME_STEPS, f) ||
              !MotionFrameTransport(&entry->frames[j - 1], f,
                                    &entry->frames[j]))
            return false;
        }
        entry->valid = true;
        break;
      }
  if (!entry || !entry->valid)
    return false;
  double t = fmax(0, fmin(1, progress)), f[3];
  int index = (int)floor(t * FRAME_STEPS);
  if (!tangent(target, t, f) ||
      !MotionFrameTransport(&entry->frames[index], f, out))
    return false;
  double blend = t * t * (3 - 2 * t), roll = b.start_roll;
  if (b.end_roll_enabled)
    roll += (b.end_roll - b.start_roll) * blend;
  MotionFrameRoll(out, roll * 0.017453292519943295);
  return true;
}
bool MotionPathsRuntimeRotation(const char *target, double progress,
                                TimelineVec3 *out) {
  MotionPathBinding b;
  MotionFrame f;
  if (!out || !MotionPathsRuntimeBinding(target, &b) || !b.follow_direction ||
      b.target_id[0] || !MotionPathsRuntimeFrame(target, progress, &f))
    return false;
  double rotation[3];
  MotionFrameModelRotation(&f, b.forward_axis, b.rotation_offset, rotation);
  *out = (TimelineVec3){rotation[0], rotation[1], rotation[2]};
  return true;
}
