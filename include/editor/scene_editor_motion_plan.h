#ifndef SCENE_EDITOR_MOTION_PLAN_H
#define SCENE_EDITOR_MOTION_PLAN_H
#include "motion/scene_motion_plans.h"
bool SceneEditorMotionPlanRead(const char *target,
                               MotionTimingScheduleRequest *);
bool SceneEditorMotionPlanPreview(const char *target,
                                  const MotionTimingScheduleRequest *,
                                  MotionRouteSchedule *, char *, size_t);
bool SceneEditorMotionPlanApply(const char *target,
                                const MotionTimingScheduleRequest *,
                                unsigned long long revision, char *, size_t);
bool SceneEditorMotionPlanRestore(const char *target,
                                  unsigned long long revision, char *, size_t);
#endif
