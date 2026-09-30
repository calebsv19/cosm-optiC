/* Versioned retained intent and immutable derived caches. Legacy scenes do not
 * enter this planner; no generated keys or second transform owner are added. */
#include "motion/scene_motion_plans.h"
#include "import/scene_timeline_document_io.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static json_object *get(json_object *o, const char *k) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, k, &v);
  return v;
}
static const char *str(json_object *o, const char *k) {
  json_object *v = get(o, k);
  return json_object_is_type(v, json_type_string) ? json_object_get_string(v)
                                                  : "";
}
static bool fail(char *m, size_t n, const char *why) {
  if (m && n)
    snprintf(m, n, "Movement plan: %s", why);
  return false;
}
json_object *MotionPlansAuthor(json_object *s) {
  return get(get(get(s, "extensions"), "ray_tracing"), "authoring");
}
json_object *MotionPlanFind(json_object *a, const char *target) {
  json_object *list = get(a, "motion_plans");
  for (size_t i = 0; json_object_is_type(list, json_type_array) &&
                     i < json_object_array_length(list);
       ++i) {
    json_object *e = json_object_array_get_idx(list, i);
    if (!strcmp(str(e, "target"), target))
      return e;
  }
  return NULL;
}
static bool number(json_object *o, const char *k, double *v) {
  json_object *j = get(o, k);
  if (!json_object_is_type(j, json_type_double) &&
      !json_object_is_type(j, json_type_int))
    return false;
  *v = json_object_get_double(j);
  return isfinite(*v);
}
bool MotionPlanReadRequest(json_object *e, MotionTimingScheduleRequest *out) {
  MotionTimingScheduleRequest r = {0};
  json_object *a = get(e, "waypoints");
  if (strcmp(str(e, "schema"), "ray_motion_plan_v1") ||
      !json_object_is_type(a, json_type_array) ||
      !number(e, "start_time", &r.start_time) ||
      !number(e, "max_speed", &r.max_speed) ||
      !number(e, "acceleration", &r.acceleration) ||
      !number(e, "braking", &r.braking))
    return false;
  r.count = json_object_array_length(a);
  if (r.count < 2 || r.count > MOTION_TIMING_WAYPOINT_CAPACITY)
    return false;
  for (size_t i = 0; i < r.count; ++i) {
    json_object *p = json_object_array_get_idx(a, i),
                *fixed = get(p, "fixed_arrival");
    MotionTimingWaypoint *w = &r.points[i];
    if (!number(p, "progress", &w->position) || w->position < 0 ||
        w->position > 1 || !number(p, "speed", &w->speed) ||
        !number(p, "hold", &w->hold) || !number(p, "arrival", &w->arrival) ||
        !json_object_is_type(fixed, json_type_boolean))
      return false;
    w->fixed_arrival = json_object_get_boolean(fixed);
  }
  *out = r;
  return true;
}
static void num(json_object *o, const char *k, double v) {
  json_object_object_add(o, k, json_object_new_double(v));
}
/* Dependency stores semantic values, not unstable byte padding or JSON order.
 * Only the selected route/channel/clock/scale are locked. Focus, FOV, lighting,
 * other paths and unrelated channels remain independently editable. */
static json_object *dependency(json_object *a, double scale, const char *target,
                               MotionPath *path, char *m, size_t n) {
  MotionPaths *paths = calloc(1, sizeof(*paths));
  TimelineDocument *d = malloc(sizeof(*d));
  json_object *dep = NULL;
  if (!paths || !d) {
    fail(m, n, "allocation failed");
    goto done;
  }
  if (!MotionPathsParse(a, paths, m, n) ||
      SceneTimelineDocumentFromJson(get(a, "scene_timeline"), d) !=
          TIMELINE_STATUS_OK) {
    fail(m, n, "route or timeline missing");
    goto done;
  }
  const MotionPathBinding *binding = NULL;
  const TimelineTrack *track = NULL;
  for (size_t i = 0; i < paths->binding_count; ++i) {
    const MotionPathBinding *b = &paths->bindings[i];
    char id[TIMELINE_ID_CAPACITY];
    snprintf(id, sizeof(id), "object/%s", b->object_id);
    if (b->enabled && !strcmp(target, b->target_id[0] ? b->target_id : id))
      binding = b;
  }
  if (!binding) {
    fail(m, n, "attach this follower first, or Restore before detach");
    goto done;
  }
  for (size_t i = 0; i < d->track_count; ++i) {
    const TimelineTrack *t = &d->tracks[i];
    if (t->enabled && !strcmp(target, t->target_id) &&
        (!strcmp(t->property_id, MOTION_PROGRESS_PROPERTY) ||
         !strcmp(t->property_id, MOTION_CAMERA_PROGRESS_PROPERTY) ||
         !strcmp(t->property_id, MOTION_LIGHT_PROGRESS_PROPERTY)))
      track = t;
  }
  if (!track) {
    fail(m, n, "enabled progress source is missing");
    goto done;
  }
  bool found = false;
  for (size_t i = 0; i < paths->count; ++i)
    if (!strcmp(paths->paths[i].id, binding->path_id)) {
      *path = paths->paths[i];
      found = true;
      break;
    }
  if (!found) {
    fail(m, n, "route missing");
    goto done;
  }
  TimelineTrack saved = *track;
  d->track_count = 1;
  d->tracks[0] = saved;
  paths->count = 1;
  paths->paths[0] = *path;
  paths->binding_count = 0;
  dep = json_object_new_object();
  num(dep, "world_scale", scale);
  json_object_object_add(dep, "route", MotionPathsToJson(paths));
  json_object_object_add(dep, "source", SceneTimelineDocumentToJson(d));
done:
  free(paths);
  free(d);
  return dep;
}
static bool compile(json_object *a, double scale, json_object *e, bool check,
                    MotionRouteGeometry *g, MotionRouteSchedule *s, char *m,
                    size_t n) {
  MotionTimingScheduleRequest r;
  MotionPath p;
  if (!MotionPlanReadRequest(e, &r))
    return fail(m, n, "invalid v1 intent");
  json_object *dep = dependency(a, scale, str(e, "target"), &p, m, n);
  if (!dep)
    return false;
  bool same = !check || json_object_equal(dep, get(e, "dependency"));
  json_object_put(dep);
  if (!same)
    return fail(m, n,
                "route, progress keys, scale or clock changed; Restore plan "
                "before editing, then Apply again");
  if (r.start_time < 0 || r.points[0].speed != 0 ||
      r.points[r.count - 1].speed != 0)
    return fail(m, n,
                "timeline plan must begin and end at rest, with nonnegative "
                "start time");
  if (MotionRouteGeometryBuild(&p, scale, g) != MOTION_ROUTE_OK ||
      !(g->length > 0))
    return fail(m, n, "route geometry cannot be certified");
  for (size_t i = 0; i < r.count; ++i) {
    double progress = r.points[i].position;
    r.points[i].position *= g->length;
    for (size_t k = 0; k < g->path.count; ++k)
      if (progress == g->point_distances[k] / g->length)
        r.points[i].position = g->point_distances[k];
  }
  MotionTimingConflict conflict = {0};
  MotionTimingStatus status = MotionRouteScheduleBuild(g, &r, s, &conflict);
  if (status != MOTION_TIMING_OK) {
    if (m && n)
      snprintf(m, n,
               "Movement plan: waypoint %zu: %s (arrival %.6g; feasible %.6g "
               "to %.6g). Change duration, limits or stop.",
               conflict.waypoint + 1, MotionTimingConflictLabel(conflict.kind),
               conflict.requested_arrival, conflict.earliest_arrival,
               conflict.latest_arrival);
    return false;
  }
  json_object *timeline = get(a, "scene_timeline"),
              *range = get(timeline, "range"), *rate = get(timeline, "rate");
  double last = (json_object_get_double(get(range, "frame_count")) - 1) *
                json_object_get_double(get(rate, "denominator")) /
                json_object_get_double(get(rate, "numerator"));
  if (s->timeline.end_time > last + 1e-12 * fmax(1, last)) {
    if (m && n)
      snprintf(m, n,
               "Movement plan ends at %.8g s, beyond timeline end %.8g s. "
               "Extend the timeline range before Apply.",
               s->timeline.end_time, last);
    return false;
  }
  return true;
}
json_object *MotionPlanCreate(json_object *a, double scale, const char *target,
                              const MotionTimingScheduleRequest *r,
                              MotionRouteSchedule *preview, char *m, size_t n) {
  if (!r || r->count < 2 || r->count > MOTION_TIMING_WAYPOINT_CAPACITY ||
      !target || !*target)
    return NULL;
  json_object *e = json_object_new_object(), *points = json_object_new_array();
  json_object_object_add(e, "schema",
                         json_object_new_string("ray_motion_plan_v1"));
  json_object_object_add(e, "target", json_object_new_string(target));
  num(e, "start_time", r->start_time);
  num(e, "max_speed", r->max_speed);
  num(e, "acceleration", r->acceleration);
  num(e, "braking", r->braking);
  json_object_object_add(e, "waypoints", points);
  for (size_t i = 0; i < r->count; ++i) {
    const MotionTimingWaypoint *w = &r->points[i];
    json_object *p = json_object_new_object();
    num(p, "progress", w->position);
    num(p, "speed", w->speed);
    num(p, "hold", w->hold);
    num(p, "arrival", w->arrival);
    json_object_object_add(p, "fixed_arrival",
                           json_object_new_boolean(w->fixed_arrival));
    json_object_array_add(points, p);
  }
  MotionPath path;
  json_object *dep = dependency(a, scale, target, &path, m, n);
  if (!dep) {
    json_object_put(e);
    return NULL;
  }
  json_object_object_add(e, "dependency", dep);
  MotionRouteGeometry *g = malloc(sizeof(*g));
  MotionRouteSchedule s;
  bool ok = g && compile(a, scale, e, true, g, &s, m, n);
  free(g);
  if (!ok) {
    json_object_put(e);
    return NULL;
  }
  if (preview)
    *preview = s;
  return e;
}
static bool list_valid(json_object *a, char *m, size_t n) {
  json_object *list = NULL;
  if (!a || !json_object_object_get_ex(a, "motion_plans", &list))
    return true;
  if (!json_object_is_type(list, json_type_array) ||
      json_object_array_length(list) > MOTION_BINDING_CAPACITY)
    return fail(m, n, "invalid plan list/capacity");
  for (size_t i = 0; i < json_object_array_length(list); ++i) {
    const char *target = str(json_object_array_get_idx(list, i), "target");
    if (!*target || strlen(target) >= TIMELINE_ID_CAPACITY)
      return fail(m, n, "invalid target");
    for (size_t j = 0; j < i; ++j)
      if (!strcmp(target, str(json_object_array_get_idx(list, j), "target")))
        return fail(m, n, "duplicate target plan");
  }
  return true;
}
bool MotionPlansValidate(json_object *a, double scale, char *m, size_t n) {
  if (!list_valid(a, m, n))
    return false;
  json_object *list = get(a, "motion_plans");
  if (!list || !json_object_array_length(list))
    return true;
  MotionRouteGeometry *g = malloc(sizeof(*g));
  if (!g)
    return fail(m, n, "allocation failed");
  bool ok = true;
  MotionRouteSchedule s;
  for (size_t i = 0; ok && i < json_object_array_length(list); ++i)
    ok = compile(a, scale, json_object_array_get_idx(list, i), true, g, &s, m,
                 n);
  free(g);
  return ok;
}
typedef struct CachedPlan {
  char target[TIMELINE_ID_CAPACITY];
  MotionRouteGeometry *geometry;
  MotionRouteSchedule schedule;
} CachedPlan;
static CachedPlan cache[MOTION_BINDING_CAPACITY];
static size_t count;
static uint64_t revision;
void MotionPlansRuntimeReset(void) {
  for (size_t i = 0; i < count; ++i)
    free(cache[i].geometry);
  memset(cache, 0, sizeof(cache));
  count = 0;
  revision = 0;
}
bool MotionPlansRuntimeLoad(json_object *a, double scale) {
  MotionPlansRuntimeReset();
  if (!list_valid(a, NULL, 0))
    return false;
  json_object *list = get(a, "motion_plans");
  if (!list)
    return true;
  for (size_t i = 0; i < json_object_array_length(list); ++i) {
    json_object *e = json_object_array_get_idx(list, i);
    CachedPlan *c = &cache[count++];
    c->geometry = malloc(sizeof(*c->geometry));
    if (!c->geometry ||
        !compile(a, scale, e, true, c->geometry, &c->schedule, NULL, 0)) {
      MotionPlansRuntimeReset();
      return false;
    }
    snprintf(c->target, sizeof(c->target), "%s", str(e, "target"));
  }
  if (count) {
    revision = 1469598103934665603ULL;
    const char *p =
        json_object_to_json_string_ext(list, JSON_C_TO_STRING_PLAIN);
    for (; *p; ++p) {
      revision ^= (unsigned char)*p;
      revision *= 1099511628211ULL;
    }
  }
  return true;
}
uint64_t MotionPlansRuntimeRevision(void) { return revision; }
static CachedPlan *find(const char *target) {
  for (size_t i = 0; i < count; ++i)
    if (!strcmp(cache[i].target, target))
      return &cache[i];
  return NULL;
}
bool MotionPlansRuntimeActive(const char *target) {
  return find(target) != NULL;
}
bool MotionPlansRuntimeGeometry(const char *target, double progress,
                                TimelineVec3 *out, double *length,
                                double *parameter) {
  CachedPlan *c = find(target);
  if (!c || !isfinite(progress))
    return false;
  MotionRouteFrame f;
  if (!MotionRouteGeometryDistance(
          c->geometry, fmax(0, fmin(1, progress)) * c->geometry->length, &f))
    return false;
  *out = (TimelineVec3){f.position[0], f.position[1], f.position[2]};
  if (length)
    *length = c->geometry->length;
  if (parameter)
    *parameter = f.parameter;
  return true;
}
bool MotionPlansRuntimeReadback(const char *target, double seconds,
                                MotionTimingScheduleSample *out,
                                const MotionRouteSchedule **schedule) {
  CachedPlan *c = find(target);
  if (!c || !isfinite(seconds))
    return false;
  double start = c->schedule.timeline.request.start_time,
         end = c->schedule.timeline.end_time;
  bool outside = seconds < start || seconds > end;
  if (MotionTimingScheduleSampleAt(&c->schedule.timeline,
                                   fmax(start, fmin(end, seconds)),
                                   out) != MOTION_TIMING_OK)
    return false;
  if (outside) {
    out->velocity = out->acceleration = 0;
    out->stationary = true;
  }
  if (schedule)
    *schedule = &c->schedule;
  return true;
}
bool MotionPlansRuntimeEvaluate(const TimelineEvaluationContext *context,
                                TimelineEvaluationResult *r) {
  if (strcmp(r->property_id, MOTION_PROGRESS_PROPERTY) &&
      strcmp(r->property_id, MOTION_CAMERA_PROGRESS_PROPERTY) &&
      strcmp(r->property_id, MOTION_LIGHT_PROGRESS_PROPERTY))
    return true;
  CachedPlan *c = find(r->target_id);
  if (!c)
    return true;
  MotionTimingScheduleSample sample;
  if (!MotionPlansRuntimeReadback(r->target_id, context->local_time_seconds,
                                  &sample, NULL))
    return false;
  r->value = TimelineValueScalar(sample.position / c->geometry->length);
  r->derivative_per_frame =
      sample.velocity / c->geometry->length *
      (double)context->rate.frames_per_second_denominator /
      context->rate.frames_per_second_numerator;
  r->derivative_valid = true;
  r->held = sample.stationary;
  r->exact_key = false;
  return true;
}
bool MotionPlansRuntimeSnapshot(TimelineFrameSnapshot *s) {
  for (size_t i = 0; i < s->property_count; ++i)
    if (!MotionPlansRuntimeEvaluate(&s->context, &s->properties[i].track))
      return false;
  return true;
}
