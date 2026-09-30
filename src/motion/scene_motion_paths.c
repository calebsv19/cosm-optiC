#include "motion/scene_motion_plans.h"
#include "motion/scene_motion_paths.h"
#include "import/runtime_scene_object_timeline.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static json_object *member(json_object *o, const char *k) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, k, &v);
  return v;
}
static const char *text(json_object *o, const char *k) {
  json_object *v = member(o, k);
  return json_object_is_type(v, json_type_string) ? json_object_get_string(v)
                                                  : "";
}
static bool fail(char *m, size_t n, const char *why) {
  if (m && n)
    snprintf(m, n, "Motion paths: %s", why);
  return false;
}
static bool string_field(json_object *o, const char *k, char *out, size_t n) {
  const char *s = text(o, k);
  if (!*s || strlen(s) >= n)
    return false;
  snprintf(out, n, "%s", s);
  return true;
}
static bool vector(json_object *o, const char *k, double out[3]) {
  json_object *a = member(o, k);
  if (!json_object_is_type(a, json_type_array) ||
      json_object_array_length(a) != 3)
    return false;
  for (int i = 0; i < 3; ++i) {
    json_object *v = json_object_array_get_idx(a, i);
    if (!json_object_is_type(v, json_type_int) &&
        !json_object_is_type(v, json_type_double))
      return false;
    out[i] = json_object_get_double(v);
    if (!isfinite(out[i]) || fabs(out[i]) > 1e12)
      return false;
  }
  return true;
}
bool MotionPathsParse(json_object *a, MotionPaths *out, char *m, size_t n) {
  memset(out, 0, sizeof(*out));
  json_object *root = member(a, "motion_paths");
  if (!root)
    return true;
  if (strcmp(text(root, "schema"), "ray_motion_paths_v1"))
    return fail(m, n, "unsupported schema");
  json_object *paths = member(root, "paths"),
              *bindings = member(root, "bindings");
  if (!json_object_is_type(paths, json_type_array) ||
      !json_object_is_type(bindings, json_type_array) ||
      json_object_array_length(paths) > MOTION_PATH_CAPACITY ||
      json_object_array_length(bindings) > MOTION_BINDING_CAPACITY)
    return fail(m, n, "invalid arrays or capacity");
  out->count = json_object_array_length(paths);
  out->binding_count = json_object_array_length(bindings);
  for (size_t i = 0; i < out->count; ++i) {
    MotionPath *p = &out->paths[i];
    json_object *o = json_object_array_get_idx(paths, i),
                *points = member(o, "points");
    if (!string_field(o, "id", p->id, sizeof(p->id)) ||
        !string_field(o, "name", p->name, sizeof(p->name)) ||
        !json_object_is_type(points, json_type_array) ||
        json_object_array_length(points) < 2 ||
        json_object_array_length(points) > MOTION_POINT_CAPACITY)
      return fail(m, n, "path needs identity, name and 2-32 points");
    for (size_t j = 0; j < i; ++j)
      if (!strcmp(p->id, out->paths[j].id))
        return fail(m, n, "duplicate path identity");
    p->count = json_object_array_length(points);
    for (size_t j = 0; j < p->count; ++j) {
      MotionPathPoint *v = &p->points[j];
      json_object *point = json_object_array_get_idx(points, j);
      if (!string_field(point, "id", v->id, sizeof(v->id)) ||
          !vector(point, "position", v->position) ||
          !vector(point, "incoming", v->incoming) ||
          !vector(point, "outgoing", v->outgoing))
        return fail(m, n, "invalid point identity or coordinates");
      const char *mode = text(point, "segment");
      if (strcmp(mode, "line") && strcmp(mode, "cubic"))
        return fail(m, n, "segment must be line or cubic");
      v->linear = !strcmp(mode, "line");
      json_object *handle_mode = member(point, "handle_mode");
      if (handle_mode) {
        const char *h = text(point, "handle_mode");
        if (!strcmp(h,"independent")) v->handle_mode=MOTION_HANDLE_INDEPENDENT;
        else if (!strcmp(h,"linked")) v->handle_mode=MOTION_HANDLE_LINKED;
        else if (!strcmp(h,"corner")) v->handle_mode=MOTION_HANDLE_CORNER;
        else return fail(m,n,"invalid handle mode");
      }
      for (size_t k = 0; k < j; ++k)
        if (!strcmp(v->id, p->points[k].id))
          return fail(m, n, "duplicate point identity");
    }
  }
  for (size_t i = 0; i < out->binding_count; ++i) {
    MotionPathBinding *b = &out->bindings[i];
    json_object *o = json_object_array_get_idx(bindings, i),
                *enabled = member(o, "enabled");
    bool camera = member(o, "target_id") != NULL;
    if ((camera ? (!string_field(o, "target_id", b->target_id, sizeof(b->target_id)) ||
                    (strcmp(b->target_id, "camera/main") && (strncmp(b->target_id, "light/", 6) || !b->target_id[6])) || member(o, "object_id"))
                : !string_field(o, "object_id", b->object_id, sizeof(b->object_id))) ||
        strlen(b->object_id) + 7 >= TIMELINE_ID_CAPACITY ||
        !string_field(o, "path_id", b->path_id, sizeof(b->path_id)) ||
        strcmp(text(o, "placement"), "on_path") ||
        !json_object_is_type(enabled, json_type_boolean))
      return fail(m, n, "invalid binding; placement must be on_path");
    b->enabled = json_object_get_boolean(enabled);
    json_object *orientation=member(o,"follow_direction");
    if(orientation) {
      json_object *axis=member(o,"forward_axis");
      if(camera || !json_object_is_type(orientation,json_type_boolean) ||
         !json_object_is_type(axis,json_type_int) || json_object_get_int(axis)<0 || json_object_get_int(axis)>5 ||
         !vector(o,"rotation_offset",b->rotation_offset)) return fail(m,n,"invalid object orientation settings");
      b->follow_direction=json_object_get_boolean(orientation);b->forward_axis=json_object_get_int(axis);
    }
    json_object *focus = member(o, "use_focus_target");
    if (focus) {
      if (strcmp(b->target_id, "camera/main") || !json_object_is_type(focus, json_type_boolean))
        return fail(m, n, "focus target is a camera-only boolean");
      b->use_focus_target = json_object_get_boolean(focus);
    }
    json_object *restore = member(o, camera ? "restore_position_tracks" : "restore_xyz_tracks");
    if (restore) {
      if (!json_object_is_type(restore, json_type_array) ||
          json_object_array_length(restore) > 3)
        return fail(m, n, "invalid prior XYZ track identities");
      b->restore_known = true;
      b->restore_count = json_object_array_length(restore);
      for (size_t j = 0; j < b->restore_count; ++j) {
        json_object *entry = json_object_array_get_idx(restore, j);
        if (!json_object_is_type(entry, json_type_string))
          return fail(m, n, "invalid prior XYZ track identity");
        const char *id = json_object_get_string(entry);
        if (!*id || strlen(id) >= TIMELINE_ID_CAPACITY)
          return fail(m, n, "invalid prior XYZ track identity");
        snprintf(b->restore_xyz_tracks[j], TIMELINE_ID_CAPACITY, "%s", id);
        for (size_t k = 0; k < j; ++k)
          if (!strcmp(id, b->restore_xyz_tracks[k]))
            return fail(m, n, "duplicate prior XYZ track identity");
      }
    }
    bool found = false;
    for (size_t j = 0; j < out->count; ++j)
      if (!strcmp(b->path_id, out->paths[j].id))
        found = true;
    if (!found)
      return fail(m, n, "binding references a missing path");
    for (size_t j = 0; j < i; ++j)
      if (!strcmp(b->target_id, out->bindings[j].target_id) &&
          !strcmp(b->object_id, out->bindings[j].object_id))
        return fail(m, n, "duplicate object binding");
  }
  return true;
}
static json_object *vec(const double v[3]) {
  json_object *a = json_object_new_array();
  for (int i = 0; i < 3; ++i)
    json_object_array_add(a, json_object_new_double(v[i]));
  return a;
}
json_object *MotionPathsToJson(const MotionPaths *d) {
  if (!d || d->count > MOTION_PATH_CAPACITY ||
      d->binding_count > MOTION_BINDING_CAPACITY)
    return NULL;
  for (size_t i = 0; i < d->count; ++i)
    if (d->paths[i].count < 2 || d->paths[i].count > MOTION_POINT_CAPACITY)
      return NULL;
  json_object *root = json_object_new_object(),
              *paths = json_object_new_array(),
              *bindings = json_object_new_array();
  json_object_object_add(root, "schema",
                         json_object_new_string("ray_motion_paths_v1"));
  json_object_object_add(root, "paths", paths);
  json_object_object_add(root, "bindings", bindings);
  for (size_t i = 0; i < d->count; ++i) {
    const MotionPath *p = &d->paths[i];
    json_object *o = json_object_new_object(),
                *points = json_object_new_array();
    json_object_array_add(paths, o);
    json_object_object_add(o, "id", json_object_new_string(p->id));
    json_object_object_add(o, "name", json_object_new_string(p->name));
    json_object_object_add(o, "points", points);
    for (size_t j = 0; j < p->count; ++j) {
      const MotionPathPoint *v = &p->points[j];
      if(v->handle_mode<MOTION_HANDLE_INDEPENDENT || v->handle_mode>MOTION_HANDLE_CORNER) {
        json_object_put(root); return NULL;
      }
      json_object *point = json_object_new_object();
      json_object_array_add(points, point);
      json_object_object_add(point, "id", json_object_new_string(v->id));
      json_object_object_add(point, "position", vec(v->position));
      json_object_object_add(point, "incoming", vec(v->incoming));
      json_object_object_add(point, "outgoing", vec(v->outgoing));
      json_object_object_add(point, "handle_mode", json_object_new_string(
          v->handle_mode==MOTION_HANDLE_LINKED?"linked":
          v->handle_mode==MOTION_HANDLE_CORNER?"corner":"independent"));
      json_object_object_add(
          point, "segment",
          json_object_new_string(v->linear ? "line" : "cubic"));
    }
  }
  for (size_t i = 0; i < d->binding_count; ++i) {
    const MotionPathBinding *b = &d->bindings[i];
    if (b->restore_count > 3) {
      json_object_put(root);
      return NULL;
    }
    json_object *o = json_object_new_object();
    json_object_array_add(bindings, o);
    if (b->target_id[0])
      json_object_object_add(o, "target_id", json_object_new_string(b->target_id));
    else
      json_object_object_add(o, "object_id", json_object_new_string(b->object_id));
    json_object_object_add(o, "path_id", json_object_new_string(b->path_id));
    json_object_object_add(o, "enabled", json_object_new_boolean(b->enabled));
    json_object_object_add(o, "placement", json_object_new_string("on_path"));
    if (!strcmp(b->target_id, "camera/main"))
      json_object_object_add(o, "use_focus_target", json_object_new_boolean(b->use_focus_target));
    if (!b->target_id[0]) {
      json_object_object_add(o,"follow_direction",json_object_new_boolean(b->follow_direction));
      json_object_object_add(o,"forward_axis",json_object_new_int(b->forward_axis));
      json_object_object_add(o,"rotation_offset",vec(b->rotation_offset));
    }
    if (b->restore_known) {
      json_object *restore = json_object_new_array();
      for (size_t j = 0; j < b->restore_count; ++j)
        json_object_array_add(restore,
            json_object_new_string(b->restore_xyz_tracks[j]));
      json_object_object_add(o, b->target_id[0] ? "restore_position_tracks" : "restore_xyz_tracks", restore);
    }
  }
  return root;
}
void MotionPathPointAt(const MotionPath *p, double t, double out[3]) {
  t = fmax(0, fmin((double)p->count - 1, t));
  size_t i = (size_t)t;
  if (i >= p->count - 1)
    i = p->count - 2;
  double u = t - i, v = 1 - u;
  const MotionPathPoint *a = &p->points[i], *b = &p->points[i + 1];
  for (int k = 0; k < 3; ++k)
    out[k] = a->linear ? v * a->position[k] + u * b->position[k]
                       : v * v * v * a->position[k] +
                             3 * v * v * u * (a->position[k] + a->outgoing[k]) +
                             3 * v * u * u * (b->position[k] + b->incoming[k]) +
                             u * u * u * b->position[k];
}
bool MotionPathsValidateScene(json_object *scene,
                              const TimelineDocument *timeline, char *m,
                              size_t n) {
  json_object *author =
      member(member(member(scene, "extensions"), "ray_tracing"), "authoring");
  MotionPaths *d = malloc(sizeof(*d));
  if (!d)
    return fail(m, n, "allocation failed");
  bool ok = MotionPathsParse(author, d, m, n);
  json_object *objects = member(scene, "objects");
  for (size_t i = 0; ok && i < d->binding_count; ++i) {
    MotionPathBinding *b = &d->bindings[i];
    size_t found = 0;
    for (size_t j = 0; json_object_is_type(objects, json_type_array) &&
                       j < json_object_array_length(objects);
         ++j)
      if (!strcmp(text(json_object_array_get_idx(objects, j), "object_id"),
                  b->object_id))
        ++found;
    if (b->target_id[0]) found = !strcmp(b->target_id, "camera/main");
    if (!strncmp(b->target_id, "light/", 6)) {
      json_object *lights = member(scene, "lights");
      for (size_t j = 0; json_object_is_type(lights, json_type_array) && j < json_object_array_length(lights); ++j) {
        json_object *light = json_object_array_get_idx(lights, j);
        const char *id = text(light, "id");
        if (!*id) id = text(light, "light_id");
        if (!*id) id = text(light, "object_id");
        if (!strcmp(id, b->target_id + 6)) ++found;
      }
    }
    bool light = !strncmp(b->target_id, "light/", 6);
    if (found != 1) {
      ok = fail(m, n, "binding object is missing or ambiguous");
      break;
    }
    if (!b->enabled)
      continue;
    if (b->target_id[0]) {
      if (!b->restore_known || b->restore_count > 1 || (light && b->restore_count != 1)) {
        ok = fail(m, n, "typed binding requires saved prior source");
        break;
      }
      for (size_t k = 0; ok && k < b->restore_count; ++k) {
        bool prior = false;
        for (size_t j = 0; timeline && j < timeline->track_count; ++j) {
          const TimelineTrack *t = &timeline->tracks[j];
          if (!strcmp(t->track_id, b->restore_xyz_tracks[k]) &&
              !strcmp(t->target_id, b->target_id) && !t->enabled &&
              (light ? !strcmp(t->property_id, "light/path_progress") :
               (!strcmp(t->property_id, "camera/path_progress") || !strcmp(t->property_id, "camera/position")))) prior = true;
        }
        if (!prior) ok = fail(m, n, "prior position source is missing or active");
      }
    }
    size_t progress = 0;
    for (size_t j = 0; timeline && j < timeline->track_count; ++j) {
      const TimelineTrack *t = &timeline->tracks[j];
      bool camera = b->target_id[0] != 0;
      if (!t->enabled || (camera ? strcmp(t->target_id, b->target_id)
          : (strncmp(t->target_id, "object/", 7) || strcmp(t->target_id + 7, b->object_id))))
        continue;
      if (camera ? (light ? (!strcmp(t->property_id, "light/position") || !strcmp(t->property_id, "light/path_progress")) :
                    (!strcmp(t->property_id, "camera/position") || !strcmp(t->property_id, "camera/path_progress")))
                 : RuntimeObjectTimelineAxis(t->property_id) >= 0) {
        ok = fail(m, n, "path and XYZ cannot both own position");
        break;
      }
      if (!strcmp(t->property_id, camera ? (light ? MOTION_LIGHT_PROGRESS_PROPERTY : MOTION_CAMERA_PROGRESS_PROPERTY) : MOTION_PROGRESS_PROPERTY))
        ++progress;
    }
    if (ok && progress != 1)
      ok = fail(m, n, "enabled binding requires one enabled progress channel");
  }
  for (size_t i = 0; ok && timeline && i < timeline->track_count; ++i) {
    const TimelineTrack *t = &timeline->tracks[i];
    bool camera = !strcmp(t->property_id, MOTION_CAMERA_PROGRESS_PROPERTY) || !strcmp(t->property_id, MOTION_LIGHT_PROGRESS_PROPERTY);
    if (!t->enabled || (!camera && strcmp(t->property_id, MOTION_PROGRESS_PROPERTY)))
      continue;
    bool found = false;
    for (size_t j = 0; j < d->binding_count; ++j)
      if (d->bindings[j].enabled &&
          (camera ? !strcmp(t->target_id, d->bindings[j].target_id)
                  : (!d->bindings[j].target_id[0] && !strcmp(t->target_id + 7, d->bindings[j].object_id))))
        found = true;
    if (!found)
      ok = fail(m, n, "progress channel has no active path binding");
  }
  free(d);
  return ok;
}
/* Immutable derived arc tables, rebuilt on scene hydration, shared by all
 * followers. 256 chords per cubic segment; invert distance then evaluate the
 * actual cubic. */
#define PATH_STEPS 256
#define PATH_SAMPLES ((MOTION_POINT_CAPACITY - 1) * PATH_STEPS + 1)
static MotionPaths runtime;
static double lengths[MOTION_PATH_CAPACITY][PATH_SAMPLES], scale = 1;
static uint64_t revision;
void MotionPathsRuntimeReset(void) {
  memset(&runtime, 0, sizeof(runtime));
  revision = 0;
}
uint64_t MotionPathsRuntimeRevision(void) { return revision; }
bool MotionPathsRuntimeLoad(json_object *author, double world_scale) {
  MotionPathsRuntimeReset();
  if (!MotionPathsParse(author, &runtime, NULL, 0)) {
    MotionPathsRuntimeReset();
    return false;
  }
  scale = world_scale;
  if (!isfinite(scale) || scale <= 0) {
    MotionPathsRuntimeReset();
    return false;
  }
  json_object *root = member(author, "motion_paths");
  const char *bytes =
      root ? json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN) : "";
  revision = 1469598103934665603ULL;
  for (; *bytes; ++bytes) {
    revision ^= (unsigned char)*bytes;
    revision *= 1099511628211ULL;
  }
  for (size_t p = 0; p < runtime.count; ++p) {
    double last[3];
    MotionPathPointAt(&runtime.paths[p], 0, last);
    lengths[p][0] = 0;
    size_t count = (runtime.paths[p].count - 1) * PATH_STEPS;
    for (size_t j = 1; j <= count; ++j) {
      double v[3], distance = 0;
      MotionPathPointAt(&runtime.paths[p], (double)j / PATH_STEPS, v);
      for (int k = 0; k < 3; ++k) {
        double d = v[k] - last[k];
        distance += d * d;
        last[k] = v[k];
      }
      lengths[p][j] = lengths[p][j - 1] + sqrt(distance);
    }
  }
  return true;
}
bool MotionPathsRuntimeBinding(const char *target, MotionPathBinding *out) {
  for (size_t i = 0; i < runtime.binding_count; ++i) {
    const MotionPathBinding *b = &runtime.bindings[i];
    bool match = b->target_id[0] ? !strcmp(target, b->target_id)
        : (!strncmp(target, "object/", 7) && !strcmp(target + 7, b->object_id));
    if (b->enabled && match) { if (out) *out = *b; return true; }
  }
  return false;
}
bool MotionPathsRuntimePosition(const char *id, double progress, TimelineVec3 *out) {
  char target[TIMELINE_ID_CAPACITY];
  if (!id || snprintf(target, sizeof(target), "object/%s", id) >= (int)sizeof(target)) return false;
  return MotionPathsRuntimeTargetPosition(target, progress, out);
}
bool MotionPathsRuntimeTargetPosition(const char *target, double progress,
                                     TimelineVec3 *out) {
  return MotionPathsRuntimeTargetSample(target, progress, out, NULL, NULL);
}
bool MotionPathsRuntimeTargetSample(const char *target, double progress,
    TimelineVec3 *out, double *length, double *parameter) {
  if(MotionPlansRuntimeActive(target)) return MotionPlansRuntimeGeometry(target,progress,out,length,parameter);
  MotionPathBinding binding;
  if (!isfinite(progress) || !MotionPathsRuntimeBinding(target, &binding)) return false;
  const char *path = binding.path_id;
  if (!path)
    return false;
  for (size_t i = 0; i < runtime.count; ++i)
    if (!strcmp(runtime.paths[i].id, path)) {
      if (length) *length = lengths[i][(runtime.paths[i].count - 1) * PATH_STEPS] * scale;
      if (progress <= 0 || progress >= 1) {
        if (parameter) *parameter = progress <= 0 ? 0 : runtime.paths[i].count - 1;
        double v[3];
        MotionPathPointAt(&runtime.paths[i],
                          progress <= 0 ? 0 : runtime.paths[i].count - 1, v);
        *out = (TimelineVec3){v[0] * scale, v[1] * scale, v[2] * scale};
        return true;
      }
      size_t end = (runtime.paths[i].count - 1) * PATH_STEPS, lo = 0, hi = end;
      double distance = fmax(0, fmin(1, progress)) * lengths[i][end];
      while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (lengths[i][mid] < distance)
          lo = mid;
        else
          hi = mid;
      }
      double span = lengths[i][hi] - lengths[i][lo],
             t = span > 1e-14 ? lo + (distance - lengths[i][lo]) / span : 0,
             v[3];
      if (parameter) *parameter = t / PATH_STEPS;
      MotionPathPointAt(&runtime.paths[i], t / PATH_STEPS, v);
      *out = (TimelineVec3){v[0] * scale, v[1] * scale, v[2] * scale};
      return true;
    }
  return false;
}
