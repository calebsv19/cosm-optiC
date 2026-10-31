#ifndef EVENT_CAMERA_INTERNAL_H
#define EVENT_CAMERA_INTERNAL_H
#include "animation/event_camera_geometry.h"
#include "animation/event_camera_lead.h"
#include "import/runtime_event_camera.h"
#include "import/sequence_clock_json.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static inline json_object *eg(json_object *o, const char *k) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, k, &v);
  return v;
}
static inline json_object *ea(json_object *s) {
  return eg(eg(eg(s, "extensions"), "ray_tracing"), "authoring");
}
static inline const char *es(json_object *o, const char *k) {
  json_object *v = eg(o, k);
  return json_object_is_type(v, json_type_string) &&
                 (size_t)json_object_get_string_len(v) ==
                     strlen(json_object_get_string(v))
             ? json_object_get_string(v)
             : "";
}
static inline bool en(json_object *v, double *d) {
  if (!json_object_is_type(v, json_type_double) &&
      !json_object_is_type(v, json_type_int))
    return false;
  *d = json_object_get_double(v);
  return isfinite(*d);
}
static inline bool eu(json_object *o, const char *k, uint64_t *d) {
  json_object *v = eg(o, k);
  if (!json_object_is_type(v, json_type_int) || json_object_get_int64(v) < 0)
    return false;
  *d = json_object_get_uint64(v);
  return true;
}
static inline bool eo(json_object *o, size_t n) {
  return json_object_is_type(o, json_type_object) &&
         (size_t)json_object_object_length(o) == n;
}
static inline bool ev(json_object *v, double *d, int n) {
  if (!json_object_is_type(v, json_type_array) ||
      json_object_array_length(v) != (size_t)n)
    return false;
  for (int i = 0; i < n; i++)
    if (!en(json_object_array_get_idx(v, i), d + i))
      return false;
  return true;
}
static inline bool ef(char *d, size_t n, const char *s) {
  if (d && n)
    snprintf(d, n, "Event camera: %s", s);
  return false;
}
static inline json_object *ec(json_object *o) {
  json_object *c = NULL;
  if (o)
    json_object_deep_copy(o, &c, NULL);
  return c;
}
static inline json_object *earr(const double *v, int n) {
  json_object *a = json_object_new_array();
  for (int i = 0; i < n; i++)
    json_object_array_add(a, json_object_new_double(v[i]));
  return a;
}
static inline json_object *evec(TimelineVec3 v) {
  double a[3] = {v.x, v.y, v.z};
  return earr(a, 3);
}
static inline TimelineVec3 etvec(const double *v) {
  return (TimelineVec3){v[0], v[1], v[2]};
}
static inline bool etoken(const char *s, size_t cap) {
  size_t n = strlen(s);
  if (!n || n >= cap)
    return false;
  for (; *s; s++)
    if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
          (*s >= '0' && *s <= '9') || *s == '_' || *s == '-'))
      return false;
  return true;
}
json_object *EventCanonical(json_object *);
bool EventEqual(json_object *, json_object *);
bool EventHash(json_object *, char[65]);
typedef struct EventFocus {
  TimelineVec3 point, low, high;
  int bodies[2];
  size_t body_count;
} EventFocus;
typedef struct EventRequest {
  char id[32], camera_id[64];
  uint64_t start, count, tick, lead, end;
  bool track, entry, dolly;
  EventCameraLead approach;
  EventCameraFraming framing;
  InspectionOrbit limits;
  json_object *focus, *pivot;
} EventRequest;
bool EventLeadRead(json_object *,EventRequest *,char *,size_t);
bool EventLeadBuild(const EventRequest *,const InspectionOrbit *,const EventFocus *,EventCameraLead *);
bool EventRequestRead(json_object *, json_object *, EventRequest *, char *,
                      size_t);
bool EventFocusResolve(const RuntimeRigidResult *, uint64_t, json_object *,
                       EventFocus *);
bool EventFocusOccluded(const RuntimeRigidResult *, uint64_t,
                        const EventFocus *, TimelineVec3);
bool EventResolve(json_object *, const RuntimeRigidResult *, json_object *, int,
                  EventRequest *, InspectionOrbit *, EventFocus *, EventFocus *,
                  json_object **, char *, size_t);
#ifdef RAY_RIGID_RESULT_ENABLED
const RuntimeRigidResult *RuntimeRigidResultCurrent(void);
#endif
#endif
