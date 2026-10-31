#include "animation/event_camera_geometry.h"
#include "animation/event_camera_lead.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
bool MotionRouteGeometryDistanceSide(const MotionRouteGeometry *g, double d,
                                     bool left, MotionRouteFrame *out) {
  (void)g;
  (void)d;
  (void)left;
  (void)out;
  assert(!"analytic dolly must not sample cubic geometry");
  return false;
}
static unsigned checks;
#define CHECK(x)                                                               \
  do {                                                                         \
    assert(x);                                                                 \
    checks++;                                                                  \
  } while (0)
static double distance(TimelineVec3 a, TimelineVec3 b) {
  return hypot(hypot(a.x - b.x, a.y - b.y), a.z - b.z);
}
int main(void) {
  for (int trial = 0; trial < 32; trial++) {
    TimelineVec3 focus = {trial * .07, trial * .01, .4};
    RayEvaluatedCamera event = {
        .valid = true,
        .position = {focus.x + .7, focus.y - 3, focus.z + 1},
        .fov_y_degrees = 55,
        .zoom = 1,
        .aspect_ratio = 4. / 3};
    double offset[] = {trial % 2 ? .15 : 0, -.1};
    CHECK(EventCameraAim(&event, focus, offset));
    EventCameraLead request = {.event = event,
                               .focus = focus,
                               .distance_scale = 1.1 + trial * .015,
                               .duration = 2 + trial * .08,
                               .max_speed = 10,
                               .acceleration = 20,
                               .braking = 20},
                    lead;
    CHECK(EventCameraLeadBuild(&request, &lead));
    RayEvaluatedCamera c, old;
    CHECK(EventCameraLeadSample(&lead, 0, &c));
    CHECK(!memcmp(&c, &lead.start, sizeof(c)));
    CHECK(distance(c.position, focus) > distance(event.position, focus));
    CHECK(EventCameraLeadSample(&lead, lead.duration, &c));
    CHECK(!memcmp(&c, &event, sizeof(c)));
    double dt = lead.duration / 1000, prev = 0, prev_speed = 0;
    TimelineVec3 previous = lead.start.position;
    for (int i = 0; i <= 1000; i++) {
      CHECK(EventCameraLeadSample(&lead, (i == 1000 ? lead.duration : dt * i),
                                  &c));
      double moved = distance(c.position, lead.start.position);
      CHECK(moved + 1e-10 >= prev);
      CHECK(moved <= lead.length + 1e-10);
      double projected_x, projected_y, z;
      CHECK(EventCameraProject(&c, focus, &projected_x, &projected_y, &z));
      CHECK(fabs(projected_x - offset[0]) < 1e-10 &&
            fabs(projected_y - offset[1]) < 1e-10);
      double u = moved / lead.length;
      CHECK(fabs(c.position.x -
                 (lead.start.position.x +
                  u * (event.position.x - lead.start.position.x))) < 1e-10);
      CHECK(fabs(c.position.y -
                 (lead.start.position.y +
                  u * (event.position.y - lead.start.position.y))) < 1e-10);
      CHECK(fabs(c.position.z -
                 (lead.start.position.z +
                  u * (event.position.z - lead.start.position.z))) < 1e-10);
      if (i) {
        double speed = distance(c.position, previous) / dt;
        CHECK(speed <= lead.max_speed + 1e-8);
        if (i > 1)
          CHECK(fabs(speed - prev_speed) / dt <=
                fmax(lead.acceleration, lead.braking) + 1e-6);
        prev_speed = speed;
      }
      previous = c.position;
      prev = moved;
    }
    MotionTimingScheduleSample t;
    CHECK(MotionTimingScheduleSampleAt(&lead.schedule.timeline, 0, &t) ==
          MOTION_TIMING_OK);
    CHECK(t.velocity == 0);
    CHECK(MotionTimingScheduleSampleAt(&lead.schedule.timeline, lead.duration,
                                       &t) == MOTION_TIMING_OK);
    /* Fixed-arrival arithmetic may leave sub-ulp residual speed in M5.
     * Camera endpoint position is tested bit-for-bit above. */
    CHECK(fabs(t.velocity) < 1e-12);
    CHECK(EventCameraLeadSample(&lead, lead.duration * .37, &old));
    CHECK(EventCameraLeadSample(&lead, lead.duration * .9, &c));
    CHECK(EventCameraLeadSample(&lead, lead.duration * .37, &c));
    CHECK(!memcmp(&c, &old, sizeof(c)));
    EventCameraLead sentinel = lead, bad = request;
    bad.max_speed = 1e-10;
    CHECK(!EventCameraLeadBuild(&bad, &lead));
    CHECK(!memcmp(&lead, &sentinel, sizeof(lead)));
    bad = request;
    bad.distance_scale = 1;
    CHECK(!EventCameraLeadBuild(&bad, &lead));
    bad.distance_scale = 101;
    CHECK(!EventCameraLeadBuild(&bad, &lead));
    bad.distance_scale = NAN;
    CHECK(!EventCameraLeadBuild(&bad, &lead));
    bad = request;
    bad.duration = 0;
    CHECK(!EventCameraLeadBuild(&bad, &lead));
    CHECK(!EventCameraLeadSample(&lead, -.1, &c));
    CHECK(!memcmp(&c, &old, sizeof(c)));
    CHECK(!EventCameraLeadSample(&lead, INFINITY, &c));
    CHECK(!EventCameraLeadSample(&lead, lead.duration + 1, &c));
  }
  printf("Event dolly lead: %u checks passed\n", checks);
  return 0;
}
