#ifndef INSPECTION_ORBIT_H
#define INSPECTION_ORBIT_H
#include "animation/evaluated_scene_snapshot.h"
#include "motion/motion_route_schedule.h"
typedef struct InspectionOrbit {
  RayEvaluatedCamera anchor;
  TimelineVec3 target;
  double angle, duration, max_speed, acceleration, braking;
  double radius, length;
  MotionRouteSchedule schedule;
} InspectionOrbit;
/* Exact analytic circular geometry feeds the existing M5 curvature budget. */
bool InspectionOrbitBuild(const InspectionOrbit *request, InspectionOrbit *out);
bool InspectionOrbitSample(const InspectionOrbit *orbit, double seconds,
                           RayEvaluatedCamera *out);
#endif
