#ifndef EVENT_CAMERA_GEOMETRY_H
#define EVENT_CAMERA_GEOMETRY_H
#include "animation/inspection_orbit.h"
typedef struct EventCameraFraming {
  double fov_y, aspect, margin, offset[2], azimuth, elevation;
} EventCameraFraming;
bool EventCameraFramingValid(const EventCameraFraming *f);
bool EventCameraAim(RayEvaluatedCamera *camera, TimelineVec3 target,
                    const double offset[2]);
bool EventCameraProject(const RayEvaluatedCamera *camera, TimelineVec3 point,
                        double *x, double *y, double *depth);
bool EventCameraContains(const RayEvaluatedCamera *camera, TimelineVec3 low,
                         TimelineVec3 high, double margin);
/* Solve event framing first, then qualify a complete orbit over frozen bounds.
 * Angles are radians. Three deterministic candidate preferences are supplied by
 * the caller. No global camera, scene, clock or numerical solver is touched. */
bool EventCameraSolve(const EventCameraFraming *framing, TimelineVec3 focus,
                      TimelineVec3 pivot, TimelineVec3 low, TimelineVec3 high,
                      const InspectionOrbit *limits, InspectionOrbit *out);
#endif
