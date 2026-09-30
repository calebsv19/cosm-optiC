#ifndef SCENE_MOTION_PLANS_H
#define SCENE_MOTION_PLANS_H
#include "animation/timeline_frame_snapshot.h"
#include "motion/motion_route_schedule.h"
/* Authored positions are normalized arc distances; limits are world units/s.
 * start/arrival/hold use the existing timeline's local seconds. */
json_object *MotionPlansAuthor(json_object *scene);
json_object *MotionPlanFind(json_object *author, const char *target);
bool MotionPlanReadRequest(json_object *entry,
                           MotionTimingScheduleRequest *out);
json_object *MotionPlanCreate(json_object *author, double scale,
                              const char *target,
                              const MotionTimingScheduleRequest *,
                              MotionRouteSchedule *preview, char *, size_t);
bool MotionPlansValidate(json_object *author, double scale, char *, size_t);
void MotionPlansRuntimeReset(void);
bool MotionPlansRuntimeLoad(json_object *author, double scale);
uint64_t MotionPlansRuntimeRevision(void);
bool MotionPlansRuntimeActive(const char *target);
bool MotionPlansRuntimeGeometry(const char *target, double progress,
                                TimelineVec3 *, double *, double *);
bool MotionPlansRuntimeEvaluate(const TimelineEvaluationContext *,
                                TimelineEvaluationResult *);
bool MotionPlansRuntimeSnapshot(TimelineFrameSnapshot *);
bool MotionPlansRuntimeReadback(const char *target, double seconds,
                                MotionTimingScheduleSample *,
                                const MotionRouteSchedule **);
#endif
