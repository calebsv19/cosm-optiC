#include "import/runtime_inspection.h"
#include "animation/sequence_time.h"
#include "import/runtime_camera_rig.h"
#include "import/runtime_rigid_result.h"
#include "import/sequence_clock_json.h"
#ifdef RAY_RIGID_RESULT_ENABLED
#include "runtime_rigid_result_internal.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CAP 16
static json_object *loaded_scene;
static json_object *get(json_object *o, const char *k) {
  json_object *v = NULL;
  if (o)
    json_object_object_get_ex(o, k, &v);
  return v;
}
static json_object *author(json_object *s) {
  return get(get(get(s, "extensions"), "ray_tracing"), "authoring");
}
static const char *str(json_object *o, const char *k) {
  json_object *v = get(o, k);
  return json_object_is_type(v, json_type_string) ? json_object_get_string(v)
                                                  : "";
}
static bool number(json_object *v, double *out) {
  if (!json_object_is_type(v, json_type_int) &&
      !json_object_is_type(v, json_type_double))
    return false;
  *out = json_object_get_double(v);
  return isfinite(*out);
}
static bool integer(json_object *o, const char *k, uint64_t *v) {
  json_object *j = get(o, k);
  if (!json_object_is_type(j, json_type_int) || json_object_get_int64(j) < 0)
    return false;
  *v = (uint64_t)json_object_get_int64(j);
  return true;
}
static bool vec(json_object *v, double *o, int n) {
  if (!json_object_is_type(v, json_type_array) ||
      json_object_array_length(v) != (size_t)n)
    return false;
  for (int i = 0; i < n; i++)
    if (!number(json_object_array_get_idx(v, i), o + i))
      return false;
  return true;
}
static json_object *array(const double *v, int n) {
  json_object *a = json_object_new_array();
  for (int i = 0; i < n; i++)
    json_object_array_add(a, json_object_new_double(v[i]));
  return a;
}
static bool fail(char *m, size_t n, const char *s) {
  if (m && n)
    snprintf(m, n, "Inspection: %s", s);
  return false;
}
static bool token(json_object *o, const char *key) {
  json_object *v = get(o, key);
  const char *s = str(o, key);
  size_t len = strlen(s);
  if (!json_object_is_type(v, json_type_string) || !len || len >= 64 ||
      len != (size_t)json_object_get_string_len(v))
    return false;
  for (size_t i = 0; i < len; i++)
    if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') ||
          (s[i] >= '0' && s[i] <= '9') || s[i] == '_' || s[i] == '-' ||
          s[i] == '/' || s[i] == '.'))
      return false;
  return true;
}
static bool read(json_object *e, InspectionOrbit *o, uint64_t *start,
                 uint64_t *count, uint64_t *tick) {
  memset(o, 0, sizeof(*o));
  json_object *a = get(e, "anchor");
  double p[3], frame[9];
  if (!token(e, "id") || !token(e, "camera_id") ||
      !token(e, "source_camera_id") || !token(e, "target_id") ||
      !json_object_is_type(e, json_type_object) ||
      json_object_object_length(e) != 15 ||
      strcmp(str(e, "schema"), "ray_inspection_orbit_v1") || !str(e, "id")[0] ||
      strlen(str(e, "id")) >= 64 || !integer(e, "start_frame", start) ||
      !integer(e, "frame_count", count) || *count < 2 ||
      !integer(e, "source_tick", tick) || !vec(get(e, "target_position"), p, 3))
    return false;
  o->target = (TimelineVec3){p[0], p[1], p[2]};
  if (!json_object_is_type(a, json_type_object) ||
      json_object_object_length(a) != 7 || !vec(get(a, "position"), p, 3) ||
      !number(get(a, "yaw"), &o->anchor.yaw_radians) ||
      !number(get(a, "pitch"), &o->anchor.pitch_radians) ||
      !number(get(a, "fov_y"), &o->anchor.fov_y_degrees) ||
      !number(get(a, "zoom"), &o->anchor.zoom) ||
      !json_object_is_type(get(a, "has_frame"), json_type_boolean) ||
      !vec(get(a, "frame"), frame, 9))
    return false;
  o->anchor.position = (TimelineVec3){p[0], p[1], p[2]};
  o->anchor.valid = true;
  o->anchor.has_orientation_frame =
      json_object_get_boolean(get(a, "has_frame"));
  memcpy(o->anchor.orientation_frame.forward, frame, 3 * sizeof(double));
  memcpy(o->anchor.orientation_frame.up, frame + 3, 3 * sizeof(double));
  memcpy(o->anchor.orientation_frame.right, frame + 6, 3 * sizeof(double));
  if (o->anchor.fov_y_degrees < 1 || o->anchor.fov_y_degrees > 179 ||
      o->anchor.zoom <= 0 ||
      fabs(o->anchor.pitch_radians) > 1.5707963267948966 ||
      (o->anchor.has_orientation_frame &&
       !MotionFrameValid(&o->anchor.orientation_frame)))
    return false;
  return number(get(e, "angle"), &o->angle) &&
         number(get(e, "max_speed"), &o->max_speed) &&
         number(get(e, "acceleration"), &o->acceleration) &&
         number(get(e, "braking"), &o->braking);
}
bool RuntimeInspectionValidate(json_object *scene, char *m, size_t n) {
  json_object *a = author(scene), *list = NULL;
  if (!a || !json_object_object_get_ex(a, "inspection_moves", &list))
    return true;
  if (!json_object_is_type(list, json_type_array) ||
      json_object_array_length(list) > CAP)
    return fail(m, n, "invalid move list");
  SequenceClock sequence;
  if (!SequenceClockFromJson(get(a, "sequence"), &sequence) ||
      !get(a, "camera_rig"))
    return fail(m, n, "requires saved rig and authenticated source sequence");
  bool ok = true;
  for (size_t i = 0; ok && i < json_object_array_length(list); i++) {
    json_object *e = json_object_array_get_idx(list, i);
    InspectionOrbit orbit;
    uint64_t start, count, tick;
    if (!read(e, &orbit, &start, &count, &tick) ||
        !RuntimeInspectionDependencyMatches(scene, e) ||
        start >= sequence.range.frame_count ||
        count > sequence.range.frame_count - start ||
        !RuntimeCameraRigTargetExists(scene, str(e, "camera_id")) ||
        !RuntimeCameraRigTargetExists(scene, str(e, "source_camera_id")) ||
        !str(e, "target_id")[0]) {
      ok = false;
      break;
    }
    bool hold = false;
    for (size_t k = 0; k < sequence.segment_count; k++) {
      const SequenceSegment *s = &sequence.segments[k];
      if (s->kind == SEQUENCE_HOLD && s->start_frame == start &&
          s->frame_count == count) {
        SequenceEvaluation v;
        if (SequenceClockEvaluate(
                &sequence,
                (TimelineSample){sequence.range.start_frame + (int64_t)start, 0,
                                 1},
                &v) == SEQUENCE_OK &&
            v.source_tick == tick)
          hold = true;
      }
    }
    if (!hold) {
      ok = false;
      break;
    }
    json_object *rig = get(a, "camera_rig"), *shots = get(rig, "shots");
    size_t coverage = 0;
    for (size_t j = 0; j < json_object_array_length(shots); j++) {
      json_object *shot = json_object_array_get_idx(shots, j);
      uint64_t b, c;
      if (!integer(shot, "start_frame", &b) ||
          !integer(shot, "frame_count", &c)) {
        ok = false;
        break;
      }
      if (b < start + count && b + c > start) {
        if (strcmp(str(shot, "camera_id"), str(e, "camera_id"))) {
          ok = false;
          break;
        }
        uint64_t end = (b + c < start + count) ? b + c : start + count,
                 begin = b > start ? b : start;
        coverage += (size_t)(end - begin);
      }
    }
    orbit.duration = (double)(count - 1) *
                     sequence.rate.frames_per_second_denominator /
                     sequence.rate.frames_per_second_numerator;
    if (coverage != count || !InspectionOrbitBuild(&orbit, &orbit)) {
      ok = false;
      break;
    }
    for (size_t j = 0; j < i; j++) {
      json_object *prior = json_object_array_get_idx(list, j);
      uint64_t b, c, t;
      InspectionOrbit old;
      if (!read(prior, &old, &b, &c, &t) ||
          !strcmp(str(e, "id"), str(prior, "id")) ||
          (!strcmp(str(e, "camera_id"), str(prior, "camera_id")) &&
           start < b + c && b < start + count))
        ok = false;
    }
  }
  return ok ? true
            : fail(m, n,
                   "stale dependency, unresolved anchor/target, non-hold "
                   "interval, camera ownership or infeasible motion limits");
}
bool RuntimeInspectionValidateResult(json_object *scene,
                                     const RuntimeRigidResult *result) {
  json_object *list = get(author(scene), "inspection_moves");
  if (!list)
    return true;
#ifdef RAY_RIGID_RESULT_ENABLED
  for (size_t i = 0; i < json_object_array_length(list); i++) {
    json_object *e = json_object_array_get_idx(list, i);
    InspectionOrbit o;
    uint64_t start, count, tick;
    bool found = false;
    if (!result || !read(e, &o, &start, &count, &tick) ||
        tick > result->result.endpoint_tick)
      return false;
    for (size_t b = 0; b < result->result.body_count; b++)
      if (!strcmp(str(e, "target_id"),
                  result->result.bindings[b].physical.object_id)) {
        const CoreSceneRigidPose *pose =
            &result->result.poses[tick * result->result.body_count + b];
        found = o.target.x == pose->position_m[0] &&
                o.target.y == pose->position_m[1] &&
                o.target.z == pose->position_m[2];
      }
    if (!found)
      return false;
  }
  return true;
#else
  (void)result;
  return false;
#endif
}
void RuntimeInspectionReset(void) {
  if (loaded_scene)
    json_object_put(loaded_scene);
  loaded_scene = NULL;
}
bool RuntimeInspectionLoad(json_object *scene) {
  RuntimeInspectionReset();
  if (!get(author(scene), "inspection_moves"))
    return true;
  return json_object_deep_copy(scene, &loaded_scene, NULL) == 0;
}
static bool pose_equal(const RayEvaluatedCamera *a,
                       const RayEvaluatedCamera *b) {
  return a->position.x == b->position.x && a->position.y == b->position.y &&
         a->position.z == b->position.z && a->yaw_radians == b->yaw_radians &&
         a->pitch_radians == b->pitch_radians &&
         a->fov_y_degrees == b->fov_y_degrees && a->zoom == b->zoom &&
         a->has_orientation_frame == b->has_orientation_frame &&
         (!a->has_orientation_frame ||
          !memcmp(&a->orientation_frame, &b->orientation_frame,
                  sizeof(MotionFrame)));
}
bool RuntimeInspectionApply(const TimelineEvaluationContext *context,
                            RayEvaluatedCamera *camera) {
  json_object *list = get(author(loaded_scene), "inspection_moves"),
              *entry = NULL;
  uint64_t chosen = 0;
  bool reached = false;
  for (size_t i = 0; list && i < json_object_array_length(list); i++) {
    json_object *e = json_object_array_get_idx(list, i);
    uint64_t begin;
    if (strcmp(camera->target_id, str(e, "camera_id")) ||
        !integer(e, "start_frame", &begin))
      continue;
    bool prior = begin <= context->local_frame;
    if (!entry || (prior && !reached) || (prior && reached && begin > chosen) ||
        (!prior && !reached && begin < chosen)) {
      entry = e;
      chosen = begin;
      reached = prior;
    }
  }
  if (entry) {
    json_object *e = entry;
    uint64_t start, count, tick;
    InspectionOrbit orbit;
    if (!read(e, &orbit, &start, &count, &tick))
      return false;
    TimelineEvaluationContext anchor_context;
    if (TimelineEvaluationContextBuild(
            context->rate, context->range,
            (TimelineSample){context->range.start_frame + (int64_t)start, 0, 1},
            &anchor_context) != TIMELINE_STATUS_OK)
      return false;
    RayEvaluatedCamera anchor;
    bool handled;
    if (!RuntimeCameraRigEvaluateBase(
            &anchor_context, str(e, "source_camera_id"), &anchor, &handled) ||
        !handled || !pose_equal(&anchor, &orbit.anchor))
      return false;
    orbit.duration = (double)(count - 1) *
                     context->rate.frames_per_second_denominator /
                     context->rate.frames_per_second_numerator;
    if (!InspectionOrbitBuild(&orbit, &orbit))
      return false;
    double elapsed =
        context->local_frame < start
            ? 0
            : ((double)(context->local_frame - start) + context->subframe) *
                  context->rate.frames_per_second_denominator /
                  context->rate.frames_per_second_numerator;
    if (elapsed > orbit.duration)
      elapsed = orbit.duration;
    RayEvaluatedCamera sampled;
    if (!InspectionOrbitSample(&orbit, elapsed, &sampled))
      return false;
    camera->position = sampled.position;
    camera->yaw_radians = sampled.yaw_radians;
    camera->pitch_radians = sampled.pitch_radians;
    camera->has_orientation_frame = sampled.has_orientation_frame;
    camera->orientation_frame = sampled.orientation_frame;
    camera->orientation_fallback = sampled.orientation_fallback;
    camera->fov_y_degrees = sampled.fov_y_degrees;
    camera->zoom = sampled.zoom;
    snprintf(camera->inspection_id, 64, "%s", str(e, "id"));
    uint64_t frame =
        context->local_frame < start ? 0 : context->local_frame - start;
    if (frame >= count)
      frame = count - 1;
    SequenceTime motion_frames = {frame, 1};
    if (context->local_frame >= start && frame < count - 1 &&
        SequenceTimeAdd(motion_frames,
                        (SequenceTime){context->sample.subframe_numerator,
                                       context->sample.subframe_denominator},
                        &motion_frames) != SEQUENCE_OK)
      return false;
    if (SequenceTimeMultiply(
            motion_frames,
            (SequenceTime){context->rate.frames_per_second_denominator,
                           context->rate.frames_per_second_numerator},
            &camera->inspection_time) != SEQUENCE_OK)
      return false;
  }
  return true;
}
json_object *RuntimeInspectionCapture(json_object *scene, const char *id,
                                      const char *camera, const char *target,
                                      uint64_t start, uint64_t count,
                                      double angle, double speed, double accel,
                                      double brake, char *m, size_t n) {
  SequenceClock sequence;
  if (!SequenceClockFromJson(get(author(scene), "sequence"), &sequence) ||
      start >= sequence.range.frame_count ||
      count > sequence.range.frame_count - start)
    return NULL;
  TimelineEvaluationContext c;
  if (TimelineEvaluationContextBuild(
          sequence.rate, sequence.range,
          (TimelineSample){sequence.range.start_frame + (int64_t)start, 0, 1},
          &c) != TIMELINE_STATUS_OK)
    return NULL;
  RayEvaluatedCamera anchor;
  bool present;
  if (!RuntimeCameraRigEvaluateBase(&c, camera, &anchor, &present) || !present)
    return NULL;
  RayEvaluatedObjectTransform bodies[64];
  size_t size = 0;
  if (RuntimeRigidResultCapture(&c, bodies, 64, &size) != TIMELINE_STATUS_OK)
    return NULL;
  const RayEvaluatedObjectTransform *body = NULL;
  for (size_t i = 0; i < size; i++)
    if (!strcmp(bodies[i].target_id, target))
      body = &bodies[i];
  if (!body) {
    fail(m, n, "target has no exact recorded rigid pose");
    return NULL;
  }
  json_object *e = json_object_new_object(), *a = json_object_new_object();
#define TEXT(k, v) json_object_object_add(e, k, json_object_new_string(v))
  TEXT("schema", "ray_inspection_orbit_v1");
  TEXT("id", id);
  TEXT("camera_id", camera);
  TEXT("source_camera_id", camera);
  TEXT("target_id", target);
  json_object_object_add(e, "start_frame", json_object_new_int64(start));
  json_object_object_add(e, "frame_count", json_object_new_int64(count));
  json_object_object_add(e, "source_tick",
                         json_object_new_int64(body->source_tick));
  double p[3] = {body->position.x, body->position.y, body->position.z};
  json_object_object_add(e, "target_position", array(p, 3));
  p[0] = anchor.position.x;
  p[1] = anchor.position.y;
  p[2] = anchor.position.z;
  json_object_object_add(a, "position", array(p, 3));
  json_object_object_add(a, "yaw", json_object_new_double(anchor.yaw_radians));
  json_object_object_add(a, "pitch",
                         json_object_new_double(anchor.pitch_radians));
  json_object_object_add(a, "fov_y",
                         json_object_new_double(anchor.fov_y_degrees));
  json_object_object_add(a, "zoom", json_object_new_double(anchor.zoom));
  json_object_object_add(a, "has_frame",
                         json_object_new_boolean(anchor.has_orientation_frame));
  double f[9];
  memcpy(f, anchor.orientation_frame.forward, 24);
  memcpy(f + 3, anchor.orientation_frame.up, 24);
  memcpy(f + 6, anchor.orientation_frame.right, 24);
  json_object_object_add(a, "frame", array(f, 9));
  json_object_object_add(e, "anchor", a);
  json_object_object_add(e, "angle", json_object_new_double(angle));
  json_object_object_add(e, "max_speed", json_object_new_double(speed));
  json_object_object_add(e, "acceleration", json_object_new_double(accel));
  json_object_object_add(e, "braking", json_object_new_double(brake));
  json_object_object_add(e, "dependency",
                         RuntimeInspectionDependencies(scene, e));
  return e;
}
