#ifndef MOTION_ROUTE_SCHEDULE_H
#define MOTION_ROUTE_SCHEDULE_H
#include "motion/motion_route_geometry.h"
#include "motion/motion_timing_schedule.h"
typedef struct MotionRouteSchedule {
    MotionTimingSchedule timeline;
    double requested_speed,requested_acceleration,requested_braking;
    double curvature_bound,stop_curvature_factor;
} MotionRouteSchedule;
/* Waypoint positions are world arc distances on geometry, not XYZ. Stops at
 * required path points must be explicit waypoints with speed zero. Curvature
 * budgets conservatively reduce effective limits, never prescribed speeds. */
MotionTimingStatus MotionRouteScheduleBuild(const MotionRouteGeometry *,const MotionTimingScheduleRequest *,MotionRouteSchedule *,MotionTimingConflict *);
typedef struct MotionRouteSample {double position[3],velocity[3],acceleration[3];bool stationary;} MotionRouteSample;
bool MotionRouteScheduleSample(const MotionRouteGeometry *,const MotionRouteSchedule *,double seconds,MotionRouteSample *);
#endif
