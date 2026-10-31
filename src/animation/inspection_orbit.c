#include "animation/inspection_orbit.h"
#include <math.h>
bool InspectionOrbitBuild(const InspectionOrbit *r, InspectionOrbit *out) {
  if (!r || !out || !r->anchor.valid || !isfinite(r->angle) ||
      fabs(r->angle) < 1e-9 || fabs(r->angle) > 8 * acos(-1.0) ||
      !isfinite(r->duration) || r->duration <= 0)
    return false;
  InspectionOrbit v = *r;
  v.radius = hypot(r->anchor.position.x - r->target.x,
                   r->anchor.position.y - r->target.y);
  v.length = v.radius * fabs(v.angle);
  if (!isfinite(v.radius) || v.radius < 1e-8 || !isfinite(v.length) ||
      !isfinite(r->target.z))
    return false;
  MotionRouteGeometry geometry = {0};
  geometry.length = v.length;
  geometry.regular_curvature_bound = 1 / v.radius;
  geometry.stopping_certificate = true;
  MotionTimingScheduleRequest request = {.max_speed = r->max_speed,
                                         .acceleration = r->acceleration,
                                         .braking = r->braking,
                                         .count = 2};
  request.points[1] = (MotionTimingWaypoint){
      .position = v.length, .fixed_arrival = true, .arrival = v.duration};
  if (MotionRouteScheduleBuild(&geometry, &request, &v.schedule, NULL) !=
      MOTION_TIMING_OK)
    return false;
  *out = v;
  return true;
}
static void rotate(double v[3], double c, double s) {
  double x = v[0], y = v[1];
  v[0] = c * x - s * y;
  v[1] = s * x + c * y;
}
bool InspectionOrbitSample(const InspectionOrbit *o, double seconds,
                           RayEvaluatedCamera *out) {
  if (!o || !out || !isfinite(seconds) || seconds < 0 || seconds > o->duration)
    return false;
  MotionTimingScheduleSample timing;
  if (MotionTimingScheduleSampleAt(&o->schedule.timeline, seconds, &timing) !=
      MOTION_TIMING_OK)
    return false;
  double angle = o->angle * timing.position / o->length, c = cos(angle),
         s = sin(angle);
  RayEvaluatedCamera v = o->anchor;
  if (seconds != 0) {
    double p[3] = {v.position.x - o->target.x, v.position.y - o->target.y, 0};
    rotate(p, c, s);
    v.position.x = o->target.x + p[0];
    v.position.y = o->target.y + p[1];
    v.yaw_radians += angle;
    if (v.has_orientation_frame) {
      rotate(v.orientation_frame.forward, c, s);
      rotate(v.orientation_frame.up, c, s);
      rotate(v.orientation_frame.right, c, s);
    }
  }
  /* Closed-orbit endpoint reproduces the retained pose exactly. */
  if (seconds == o->duration &&
      fabs(remainder(o->angle, 2 * acos(-1.0))) < 1e-12)
    v = o->anchor;
  *out = v;
  return true;
}
