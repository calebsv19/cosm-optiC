#include "animation/event_camera_geometry.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
/* The analytic path must not fall back to generic cubic-route sampling. */
bool MotionRouteGeometryDistanceSide(const MotionRouteGeometry *g, double d,
                                     bool left, MotionRouteFrame *out) {
  (void)g;
  (void)d;
  (void)left;
  (void)out;
  assert(!"unexpected generic route sample");
  return false;
}
static int checks;
#define CHECK(x)                                                               \
  do {                                                                         \
    assert(x);                                                                 \
    checks++;                                                                  \
  } while (0)
/* Independent screen projection uses the same published pinhole convention. */
static double dot(const double *a, const double *b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
int main(void) {
  for (int trial = 0; trial < 40; trial++) {
    TimelineVec3 target = {trial * .11 - 2, trial * .03, trial * .07 + .2},
                 low = {target.x - .4, target.y - .3, target.z - .2},
                 high = {target.x + .4, target.y + .3, target.z + .2};
    EventCameraFraming f = {.fov_y = 35 + trial % 4 * 10,
                            .aspect = 1.3333333333333333,
                            .margin = .15,
                            .offset = {trial % 2 ? .2 : 0, trial % 3 ? .1 : 0},
                            .azimuth = trial * .1,
                            .elevation = .25};
    InspectionOrbit request = {.angle =
                                   trial % 2 ? acos(-1.) / 2 : 2 * acos(-1.),
                               .duration = 4,
                               .max_speed = 50,
                               .acceleration = 100,
                               .braking = 100},
                    orbit;
    CHECK(EventCameraSolve(&f, target, target, low, high, &request, &orbit));
    for (int i = 0; i <= 100; i++) {
      RayEvaluatedCamera camera;
      CHECK(InspectionOrbitSample(&orbit, 4. * i / 100, &camera));
      CHECK(EventCameraAim(&camera, target, f.offset));
      CHECK(EventCameraContains(&camera, low, high, f.margin));
      double p[3] = {target.x - camera.position.x, target.y - camera.position.y,
                     target.z - camera.position.z};
      double z = dot(p, camera.orientation_frame.forward),
             t = tan(f.fov_y * acos(-1.) / 360);
      CHECK(fabs(dot(p, camera.orientation_frame.right) / (z * t * f.aspect) -
                 f.offset[0]) < 1e-10);
      CHECK(fabs(dot(p, camera.orientation_frame.up) / (z * t) - f.offset[1]) <
            1e-10);
      CHECK(MotionFrameValid(&camera.orientation_frame));
    }
    InspectionOrbit old = orbit, bad = request;
    bad.max_speed = 1e-9;
    CHECK(!EventCameraSolve(&f, target, target, low, high, &bad, &orbit));
    CHECK(!memcmp(&old, &orbit, sizeof(old)));
  }
  EventCameraFraming f = {.fov_y = 55,
                          .aspect = 1,
                          .margin = .15,
                          .offset = {.9, 0},
                          .elevation = .2};
  CHECK(!EventCameraFramingValid(&f));
  f.offset[0] = 0;
  CHECK(EventCameraFramingValid(&f));
  RayEvaluatedCamera c = {.valid = true,
                          .position = {0, 0, 0},
                          .fov_y_degrees = 55,
                          .aspect_ratio = 1,
                          .zoom = 1};
  CHECK(!EventCameraAim(&c, (TimelineVec3){0, 0, 0}, f.offset));
  /* Small feature views must clear the native 0.1 m near plane. */
  f = (EventCameraFraming){
      .fov_y = 55, .aspect = 4. / 3, .margin = .12, .elevation = .3};
  InspectionOrbit limits = {.angle = 2 * acos(-1.),
                            .duration = 4,
                            .max_speed = 50,
                            .acceleration = 100,
                            .braking = 100},
                  small;
  CHECK(EventCameraSolve(&f, (TimelineVec3){0, 0, 0}, (TimelineVec3){0, 0, 0},
                         (TimelineVec3){-.001, -.001, -.001},
                         (TimelineVec3){.001, .001, .001}, &limits, &small));
  double x, y, z;
  CHECK(EventCameraProject(&small.anchor, (TimelineVec3){0, 0, 0}, &x, &y, &z));
  CHECK(z > .1);
  printf("Event camera geometry: %d checks passed\n", checks);
  return 0;
}
