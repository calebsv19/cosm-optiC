#include "../src/import/event_camera_internal.h"
#include <assert.h>
#include <stdlib.h>
static int checks;
static void check(bool b) {
  assert(b);
  checks++;
}
int main(void) {
  json_object *a = json_tokener_parse(
      "{\"index\":2,\"usable\":true,\"visibility_samples\":17,\"focus\":[1,2,3]"
      ",\"bounds_low\":[-2.3718831743057343,0,0],\"yaw\":0.3,\"fov_y\":75.0,"
      "\"proposal_sha256\":\"original\"}");
  json_object *b = ec(a);
  check(EventResolvedEquivalent(a, b));
  json_object_array_put_idx(eg(b, "bounds_low"), 0,
                            json_object_new_double(-2.371883174305734));
  json_object_object_add(b, "proposal_sha256",
                         json_object_new_string("native"));
  check(EventResolvedEquivalent(a, b)); /* hash integrity has its own gate */
  check(!EventEqual(a, b));
  json_object_array_put_idx(eg(b, "bounds_low"), 0,
                            json_object_new_double(-2.3718831743));
  check(!EventResolvedEquivalent(a, b));
  json_object_put(b);
  b = ec(a);
  json_object_object_add(b, "fov_y", json_object_new_double(75.0 + 1e-13));
  check(!EventResolvedEquivalent(a, b));
  const char *keys[] = {"index", "visibility_samples", "usable"};
  for (int i = 0; i < 3; i++) {
    json_object_put(b);
    b = ec(a);
    json_object_object_add(b, keys[i],
                           json_object_new_string(i == 0   ? "2"
                                                  : i == 1 ? "17"
                                                           : "true"));
    check(!EventResolvedEquivalent(a, b));
  }
  double bad[] = {NAN, INFINITY, -INFINITY, 1e100};
  for (int i = 0; i < 4; i++) {
    json_object_put(b);
    b = ec(a);
    json_object_array_put_idx(eg(b, "focus"), 0,
                              json_object_new_double(bad[i]));
    check(!EventResolvedEquivalent(a, b));
  }
  json_object_put(b);
  b = ec(a);
  json_object_object_del(b, "index");
  check(!EventResolvedEquivalent(a, b));
  json_object_put(b);
  b = ec(a);
  json_object_object_add(b, "unknown", json_object_new_int(1));
  check(!EventResolvedEquivalent(a, b));
  json_object_put(b);
  b = ec(a);
  json_object_array_add(eg(b, "focus"), json_object_new_int(4));
  check(!EventResolvedEquivalent(a, b));
  json_object_put(b);
  json_object_put(a);
  printf("Portable event comparison PASS %d checks\n", checks);
  return 0;
}
