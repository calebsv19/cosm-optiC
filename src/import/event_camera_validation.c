#include "event_camera_internal.h"
#include <float.h>

/* Deliberately not a recursive epsilon comparison. Identities, counts, flags,
 * authored lens and dependency/content hashes retain exact comparison. These
 * bounds are in world metres (or seconds/radians for the scalar whitelist).
 * Translation magnitude may not enlarge an allowance beyond one nanometre. */
static bool near_number(json_object *a, json_object *b, bool metric) {
  double x, y;
  if (!en(a, &x) || !en(b, &y))
    return false;
  double tolerance = 1e-12;
  if (metric)
    tolerance =
        fmin(1e-9, tolerance + 32 * DBL_EPSILON * fmax(fabs(x), fabs(y)));
  return isfinite(x - y) && fabs(x - y) <= tolerance;
}
bool EventResolvedEquivalent(json_object *actual, json_object *saved) {
  if (!json_object_is_type(actual, json_type_object) ||
      !json_object_is_type(saved, json_type_object) ||
      json_object_object_length(actual) != json_object_object_length(saved))
    return false;
  json_object_object_foreach(actual, key, value) {
    json_object *other = eg(saved, key);
    if (!other)
      return false;
    if (!strcmp(key, "proposal_sha256"))
      continue; /* checked independently */
    bool vector = !strcmp(key, "focus") || !strcmp(key, "pivot") ||
                  !strcmp(key, "bounds_low") || !strcmp(key, "bounds_high") ||
                  !strcmp(key, "position") || !strcmp(key, "lead_position");
    if (vector) {
      if (!json_object_is_type(value, json_type_array) ||
          !json_object_is_type(other, json_type_array) ||
          json_object_array_length(value) != 3 ||
          json_object_array_length(other) != 3)
        return false;
      for (int i = 0; i < 3; i++)
        if (!near_number(json_object_array_get_idx(value, i),
                         json_object_array_get_idx(other, i), true))
          return false;
    } else if (!strcmp(key, "yaw") || !strcmp(key, "pitch") ||
               !strcmp(key, "lead_duration") || !strcmp(key, "lead_distance")) {
      if (!near_number(value, other, !strcmp(key, "lead_distance")))
        return false;
    } else if (!strcmp(key, "fov_y")) {
      double x, y;
      if (!en(value, &x) || !en(other, &y) || x != y)
        return false;
    } else if (json_object_get_type(value) != json_object_get_type(other) ||
               !json_object_equal(value, other))
      return false;
  }
  return true;
}
