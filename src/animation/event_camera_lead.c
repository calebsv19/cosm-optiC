#include "animation/event_camera_lead.h"
#include <math.h>
static bool finite3(TimelineVec3 v) {
  return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}
bool EventCameraLeadBuild(const EventCameraLead *r, EventCameraLead *out) {
  if (!r || !out || !r->event.valid || !finite3(r->event.position) ||
      !finite3(r->focus) || !r->event.has_orientation_frame ||
      !MotionFrameValid(&r->event.orientation_frame) ||
      !isfinite(r->distance_scale) || r->distance_scale <= 1 ||
      r->distance_scale > 100 || !isfinite(r->duration) || r->duration <= 0)
    return false;
  EventCameraLead v = *r;
  v.start = v.event;
  double dx = v.event.position.x - v.focus.x,
         dy = v.event.position.y - v.focus.y,
         dz = v.event.position.z - v.focus.z;
  v.length = (v.distance_scale - 1) * hypot(hypot(dx, dy), dz);
  v.start.position = (TimelineVec3){v.focus.x + v.distance_scale * dx,
                                    v.focus.y + v.distance_scale * dy,
                                    v.focus.z + v.distance_scale * dz};
  if (!finite3(v.start.position) || !isfinite(v.length) || v.length < 1e-8)
    return false;
  MotionRouteGeometry geometry = {.length = v.length,
                                  .stopping_certificate = true};
  MotionTimingScheduleRequest request = {.max_speed = v.max_speed,
                                         .acceleration = v.acceleration,
                                         .braking = v.braking,
                                         .count = 2};
  request.points[1] = (MotionTimingWaypoint){
      .position = v.length, .fixed_arrival = true, .arrival = v.duration};
  if (MotionRouteScheduleBuild(&geometry, &request, &v.schedule, NULL) !=
      MOTION_TIMING_OK)
    return false;
  *out = v;
  return true;
}
bool EventCameraLeadSample(const EventCameraLead *l, double seconds,
                           RayEvaluatedCamera *out) {
  if (!l || !out || !isfinite(seconds) || seconds < 0 ||
      seconds > l->duration || !isfinite(l->length) || l->length < 1e-8)
    return false;
  MotionTimingScheduleSample t;
  if (MotionTimingScheduleSampleAt(&l->schedule.timeline, seconds, &t) !=
      MOTION_TIMING_OK)
    return false;
  RayEvaluatedCamera value = l->event;
  if (seconds == 0)
    value = l->start;
  else if (seconds != l->duration) {
    double u = t.position / l->length;
    value.position = (TimelineVec3){
        l->start.position.x + u * (l->event.position.x - l->start.position.x),
        l->start.position.y + u * (l->event.position.y - l->start.position.y),
        l->start.position.z + u * (l->event.position.z - l->start.position.z)};
  }
  *out = value;
  return true;
}
