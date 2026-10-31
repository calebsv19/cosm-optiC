#pragma once
#include "app/evaluated_scene_service.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_event_planner.h"
#include "editor/scene_editor_sequence.h"
#include "editor/scene_editor_timeline.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static inline json_object *pg(json_object *o, const char *k) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, k, &v);
  return v;
}
static inline const char *ps(json_object *o, const char *k) {
  json_object *v = pg(o, k);
  return json_object_is_type(v, json_type_string) ? json_object_get_string(v)
                                                  : "";
}
static inline json_object *pc(json_object *o) {
  json_object *v = NULL;
  if (o)
    json_object_deep_copy(o, &v, NULL);
  return v;
}
static inline double pn(json_object *o, const char *k) {
  return json_object_get_double(pg(o, k));
}
static inline void pd(json_object *o, const char *k, double v) {
  json_object_object_add(o, k, json_object_new_double(v));
}
static inline void pi(json_object *o, const char *k, int64_t v) {
  json_object_object_add(o, k, json_object_new_int64(v));
}
static inline void pt(json_object *o, const char *k, const char *v) {
  json_object_object_add(o, k, json_object_new_string(v));
}
static inline json_object *pa(json_object *o, size_t i) {
  return json_object_is_type(o, json_type_array)
             ? json_object_array_get_idx(o, i)
             : NULL;
}
static inline size_t plen(json_object *o) {
  return json_object_is_type(o, json_type_array) ? json_object_array_length(o)
                                                 : 0;
}
typedef struct EventPlannerState {
  double feature_radius;
  bool open;
  int page, armed, editing, candidate;
  unsigned long long revision, proposal_revision, viewport_revision,
      viewport_token;
  char path[4096], draft[96], feedback[256], labels[72][192];
  json_object *intent, *proposals, *picked;
  SceneEditorDigestOverlayProjector projector;
  bool viewport_valid;
  TimelineSample drawn_sample;
  uint64_t drawn_tick;
} EventPlannerState;
extern EventPlannerState event_planner;
void EventPlannerSync(void);
void EventPlannerInvalidate(void);
bool EventPlannerHold(int direction);
void EventPlannerTarget(int which);
bool EventPlannerGenerate(void);
bool EventPlannerApply(void);
bool EventPlannerSeek(int phase);
json_object *EventPlannerQuery(int x, int y, char *, size_t);
void EventPlannerClearControls(void);
