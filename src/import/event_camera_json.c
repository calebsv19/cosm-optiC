#include "app/ray_tracing_sha256.h"
#include "event_camera_internal.h"
#include <inttypes.h>
#include <stdlib.h>
static int compare(const void *a, const void *b) {
  return strcmp(*(const char *const *)a, *(const char *const *)b);
}
json_object *EventCanonical(json_object *o) {
  if (!o)
    return NULL;
  enum json_type t = json_object_get_type(o);
  if (t == json_type_int) {
    char s[64];
    snprintf(s, sizeof(s), "%" PRId64, json_object_get_int64(o));
    return json_object_new_string(s);
  }
  if (t == json_type_double) {
    char s[64];
    double d = json_object_get_double(o);
    snprintf(s, sizeof(s), "%.17g", d == 0 ? 0 : d);
    return json_object_new_string(s);
  }
  if (t == json_type_array) {
    json_object *a = json_object_new_array();
    for (size_t i = 0; i < json_object_array_length(o); i++)
      json_object_array_add(a, EventCanonical(json_object_array_get_idx(o, i)));
    return a;
  }
  if (t == json_type_object) {
    size_t n = json_object_object_length(o), i = 0;
    const char **keys = calloc(n ? n : 1, sizeof(*keys));
    if (!keys)
      return NULL;
    json_object_object_foreach(o, k, v) {
      (void)v;
      keys[i++] = k;
    }
    qsort(keys, n, sizeof(*keys), compare);
    json_object *r = json_object_new_object();
    for (i = 0; i < n; i++)
      json_object_object_add(r, keys[i], EventCanonical(eg(o, keys[i])));
    free(keys);
    return r;
  }
  return ec(o);
}
bool EventEqual(json_object *a, json_object *b) {
  json_object *x = EventCanonical(a), *y = EventCanonical(b);
  bool ok = json_object_equal(x, y);
  if (x)
    json_object_put(x);
  if (y)
    json_object_put(y);
  return ok;
}
bool EventHash(json_object *a, char out[65]) {
  json_object *c = EventCanonical(a);
  if (!c)
    return false;
  const char *s = json_object_to_json_string_ext(c, JSON_C_TO_STRING_PLAIN);
  bool ok = ray_tracing_sha256_bytes(s, strlen(s), out);
  json_object_put(c);
  return ok;
}
