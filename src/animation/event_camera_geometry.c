#include "animation/event_camera_geometry.h"
#include <math.h>
#include <string.h>
static double dot(const double *a, const double *b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
static bool finite3(TimelineVec3 a) {
  return isfinite(a.x) && isfinite(a.y) && isfinite(a.z);
}
static void rotate(const double *v, const double *axis, double c, double s,
                   double *out) {
  double d = dot(v, axis), cross[3] = {axis[1] * v[2] - axis[2] * v[1],
                                       axis[2] * v[0] - axis[0] * v[2],
                                       axis[0] * v[1] - axis[1] * v[0]};
  for (int i = 0; i < 3; i++)
    out[i] = v[i] * c + cross[i] * s + axis[i] * d * (1 - c);
}
bool EventCameraFramingValid(const EventCameraFraming *f) {
  return f && isfinite(f->fov_y) && f->fov_y >= 5 && f->fov_y <= 120 &&
         isfinite(f->aspect) && f->aspect >= .25 && f->aspect <= 4 &&
         isfinite(f->margin) && f->margin >= .02 && f->margin <= .5 &&
         isfinite(f->offset[0]) && isfinite(f->offset[1]) &&
         fabs(f->offset[0]) + f->margin < .95 &&
         fabs(f->offset[1]) + f->margin < .95 && isfinite(f->azimuth) &&
         fabs(f->azimuth) <= 8 * acos(-1.) && isfinite(f->elevation) &&
         f->elevation >= -.9 && f->elevation <= 1.3;
}
bool EventCameraAim(RayEvaluatedCamera *camera, TimelineVec3 target,
                    const double offset[2]) {
  if (!camera || !offset || !finite3(target) || !finite3(camera->position) ||
      !isfinite(offset[0]) || !isfinite(offset[1]) ||
      !isfinite(camera->fov_y_degrees) || camera->fov_y_degrees <= 0 ||
      camera->fov_y_degrees >= 179 || !isfinite(camera->aspect_ratio) ||
      camera->aspect_ratio <= 0 || !isfinite(camera->zoom) || camera->zoom <= 0)
    return false;
  double forward[3] = {target.x - camera->position.x,
                       target.y - camera->position.y,
                       target.z - camera->position.z},
         up[3] = {0, 0, 1};
  if (dot(forward, forward) < 1e-18)
    return false;
  MotionFrame base;
  if (!MotionFrameSeed(&base, forward, up))
    return false;
  double tangent = tan(camera->fov_y_degrees * acos(-1.) / 360) / camera->zoom;
  double ray[3] = {offset[0] * tangent * camera->aspect_ratio,
                   offset[1] * tangent, 1},
         len = sqrt(dot(ray, ray));
  for (int i = 0; i < 3; i++)
    ray[i] /= len;
  double axis[3] = {ray[1], -ray[0], 0}, s = hypot(axis[0], axis[1]),
         c = ray[2];
  MotionFrame result = base;
  if (s > 1e-15) {
    axis[0] /= s;
    axis[1] /= s;
    double *columns[3] = {result.right, result.up, result.forward};
    for (int k = 0; k < 3; k++) {
      double v[3] = {0}, r[3];
      v[k] = 1;
      rotate(v, axis, c, s, r);
      for (int j = 0; j < 3; j++)
        columns[k][j] =
            base.right[j] * r[0] + base.up[j] * r[1] + base.forward[j] * r[2];
    }
  }
  if (!MotionFrameValid(&result))
    return false;
  RayEvaluatedCamera out = *camera;
  out.orientation_frame = result;
  out.has_orientation_frame = true;
  out.orientation_fallback = false;
  out.yaw_radians = atan2(result.forward[0], -result.forward[1]);
  out.pitch_radians = asin(fmax(-1, fmin(1, result.forward[2])));
  out.valid = true;
  *camera = out;
  return true;
}
bool EventCameraProject(const RayEvaluatedCamera *c, TimelineVec3 p, double *x,
                        double *y, double *depth) {
  if (!c || !x || !y || !depth || !c->has_orientation_frame ||
      !MotionFrameValid(&c->orientation_frame) || !finite3(p))
    return false;
  double d[3] = {p.x - c->position.x, p.y - c->position.y, p.z - c->position.z};
  double z = dot(d, c->orientation_frame.forward),
         t = tan(c->fov_y_degrees * acos(-1.) / 360) / c->zoom;
  if (!isfinite(z) || z <= 1e-9 || !isfinite(t) || t <= 0 ||
      c->aspect_ratio <= 0)
    return false;
  *depth = z;
  *x = dot(d, c->orientation_frame.right) / (z * t * c->aspect_ratio);
  *y = dot(d, c->orientation_frame.up) / (z * t);
  return isfinite(*x) && isfinite(*y);
}
bool EventCameraContains(const RayEvaluatedCamera *c, TimelineVec3 low,
                         TimelineVec3 high, double margin) {
  if (!finite3(low) || !finite3(high) || low.x > high.x || low.y > high.y ||
      low.z > high.z)
    return false;
  for (int i = 0; i < 8; i++) {
    double x, y, z;
    TimelineVec3 p = {i & 1 ? high.x : low.x, i & 2 ? high.y : low.y,
                      i & 4 ? high.z : low.z};
    if (!EventCameraProject(c, p, &x, &y, &z) || z <= .100001 ||
        fabs(x) > 1 - margin + 1e-10 || fabs(y) > 1 - margin + 1e-10)
      return false;
  }
  return true;
}
bool EventCameraSolve(const EventCameraFraming *f, TimelineVec3 focus,
                      TimelineVec3 pivot, TimelineVec3 low, TimelineVec3 high,
                      const InspectionOrbit *limits, InspectionOrbit *out) {
  if (!EventCameraFramingValid(f) || !limits || !out || !finite3(focus) ||
      !finite3(pivot) || !finite3(low) || !finite3(high))
    return false;
  double dx = high.x - low.x, dy = high.y - low.y, dz = high.z - low.z,
         d = fmax(.01, sqrt(dx * dx + dy * dy + dz * dz));
  for (int attempt = 0; attempt < 64; attempt++, d *= 1.15) {
    InspectionOrbit orbit = *limits;
    orbit.target = pivot;
    orbit.anchor = (RayEvaluatedCamera){
        .valid = true,
        .position = {focus.x + d * sin(f->azimuth) * cos(f->elevation),
                     focus.y - d * cos(f->azimuth) * cos(f->elevation),
                     focus.z + d * sin(f->elevation)},
        .fov_y_degrees = f->fov_y,
        .aspect_ratio = f->aspect,
        .zoom = 1};
    if (!EventCameraAim(&orbit.anchor, focus, f->offset))
      return false;
    if (!InspectionOrbitBuild(&orbit, &orbit))
      continue;
    bool fits = true;
    for (int i = 0; i <= 64 && fits; i++) {
      RayEvaluatedCamera c;
      if (!InspectionOrbitSample(&orbit, orbit.duration * i / 64, &c) ||
          !EventCameraAim(&c, focus, f->offset) ||
          !EventCameraContains(&c, low, high, f->margin))
        fits = false;
    }
    if (fits) {
      *out = orbit;
      return true;
    }
  }
  return false;
}
