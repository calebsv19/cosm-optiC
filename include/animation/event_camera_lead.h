#ifndef EVENT_CAMERA_LEAD_H
#define EVENT_CAMERA_LEAD_H
#include "animation/inspection_orbit.h"
/* Radial approach to an already solved event pose. Presentation seconds and
 * world metres. Immutable M5 straight-route schedule; both endpoints at rest.
 */
typedef struct EventCameraLead {
  RayEvaluatedCamera event, start;
  TimelineVec3 focus;
  double distance_scale, duration, max_speed, acceleration, braking, length;
  MotionRouteSchedule schedule;
} EventCameraLead;
bool EventCameraLeadBuild(const EventCameraLead *, EventCameraLead *);
bool EventCameraLeadSample(const EventCameraLead *, double,
                           RayEvaluatedCamera *);
#endif
