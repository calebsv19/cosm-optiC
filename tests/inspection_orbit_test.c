#include "animation/inspection_orbit.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Analytic orbit sampling never calls generic cubic-route sampling. Trap an
 * accidental dependency rather than supplying substitute geometry. */
bool MotionRouteGeometryDistanceSide(const MotionRouteGeometry *g, double d,
                                     bool left, MotionRouteFrame *out) {
  (void)g;
  (void)d;
  (void)left;
  (void)out;
  assert(!"unexpected generic geometry sample");
  return false;
}
int main(void) {
  int checks = 0;
  for (int trial = 0; trial < 30; trial++) {
    InspectionOrbit request = {.anchor = {.valid = true,
                                          .position = {3 + trial * .1, -4, 5},
                                          .yaw_radians = .2,
                                          .pitch_radians = -.3,
                                          .fov_y_degrees = 55,
                                          .zoom = 1},
                               .target = {1, 1, 2},
                               .angle = 2 * acos(-1.0) * (trial % 2 ? -1 : 1),
                               .duration = 8 + trial * .1,
                               .max_speed = 30,
                               .acceleration = 30,
                               .braking = 30},
                    o;
    assert(InspectionOrbitBuild(&request, &o));
    checks++;
    for (int i = 0; i <= 100; i++) {
      RayEvaluatedCamera camera;
      assert(InspectionOrbitSample(&o, o.duration * i / 100, &camera));
      assert(fabs(hypot(camera.position.x - o.target.x,
                        camera.position.y - o.target.y) -
                  o.radius) < 1e-12);
      assert(camera.position.z == request.anchor.position.z &&
             camera.fov_y_degrees == 55 && camera.zoom == 1);
      MotionTimingScheduleSample s;
      assert(MotionTimingScheduleSampleAt(&o.schedule.timeline,
                                          o.duration * i / 100,
                                          &s) == MOTION_TIMING_OK);
      assert(hypot(s.acceleration, s.velocity * s.velocity / o.radius) <=
             30 * (1 + 1e-10));
      checks += 4;
    }
    RayEvaluatedCamera end;
    assert(InspectionOrbitSample(&o, o.duration, &end));
    assert(!memcmp(&end, &o.anchor, sizeof(end)));
    checks += 2;
    InspectionOrbit before = o;
    request.duration = .001;
    assert(!InspectionOrbitBuild(&request, &o));
    assert(!memcmp(&o, &before, sizeof(o)));
    checks += 2;
  }
  printf("Inspection orbit: %d checks passed\n", checks);
  return 0;
}
