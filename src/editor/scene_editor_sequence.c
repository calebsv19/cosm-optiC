#include "editor/scene_editor_event_planner.h"
#include "editor/scene_editor_event_camera.h"
#include "editor/scene_editor_sequence.h"
#include "animation/sequence_time.h"
#include "app/evaluated_scene_service.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_timeline.h"
#include "import/runtime_camera_rig.h"
#include "import/runtime_inspection.h"
#include "import/scene_timeline_document_io.h"
#include "import/sequence_clock_json.h"
#include "scene_editor_document_transaction.h"
#include "scene_editor_sequence_segments.h"
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
static json_object *copy(json_object *o) {
  json_object *c = NULL;
  if (o)
    json_object_deep_copy(o, &c, NULL);
  return c;
}
static json_object *author(void) {
  return get(get(get(document_authoring_root(), "extensions"), "ray_tracing"),
             "authoring");
}
static bool fail(char *m, size_t n, const char *s) {
  if (m && n)
    snprintf(m, n, "%s", s);
  return false;
}
static json_object *ratio(uint64_t a, uint64_t b) {
  json_object *r = json_object_new_object();
  json_object_object_add(r, "numerator", json_object_new_int64(a));
  json_object_object_add(r, "denominator", json_object_new_int64(b));
  return r;
}
static json_object *base(const RayEvaluatedCamera *c, const char *id,
                         uint64_t count, double scale) {
  json_object *o = json_object_new_object(), *p = json_object_new_array(),
              *clock = json_object_new_object();
  double xyz[3] = {c->position.x / scale, c->position.y / scale,
                   c->position.z / scale};
  for (int i = 0; i < 3; i++)
    json_object_array_add(p, json_object_new_double(xyz[i]));
  json_object_object_add(o, "id", json_object_new_string(id));
  json_object_object_add(o, "position", p);
  json_object_object_add(o, "yaw", json_object_new_double(c->yaw_radians));
  json_object_object_add(o, "pitch", json_object_new_double(c->pitch_radians));
  json_object_object_add(o, "fov_y", json_object_new_double(c->fov_y_degrees));
  json_object_object_add(o, "zoom", json_object_new_double(c->zoom));
  json_object_object_add(clock, "mode", json_object_new_string("continuous"));
  json_object_object_add(clock, "offset", ratio(0, 1));
  json_object_object_add(clock, "speed", ratio(1, 1));
  json_object_object_add(clock, "duration_frames",
                         json_object_new_int64(count));
  json_object_object_add(clock, "end_behavior", json_object_new_string("hold"));
  json_object_object_add(o, "clock", clock);
  return o;
}
json_object *SceneEditorSequenceRead(void) {
  json_object *o = json_object_new_object(), *a = author();
  const char *keys[] = {"sequence", "camera_rig", "inspection_moves", "sequence_volume", "event_cameras"};
  for (int i = 0; i < 5; i++)
    json_object_object_add(o, keys[i], copy(get(a, keys[i])));
  const char *locks[] = {"inspection_moves", "event_cameras"};
  for(size_t k=0;k<2;k++) {
    json_object *moves=get(o,locks[k]);
    for(size_t i=0;moves&&i<json_object_array_length(moves);i++) {
      json_object *entry=json_object_array_get_idx(moves,i);
      json_object_object_del(entry,"dependency");
      json_object_object_add(entry,"locked",json_object_new_boolean(true));
    }
  }
  TimelineSample sample;
  RayEvaluatedSceneServiceResult r;
  if (SceneEditorTimelineCurrentSample(&sample) &&
      RayEvaluatedSceneCaptureSample(sample, &r)) {
    json_object *c = json_object_new_object();
    json_object_object_add(c, "frame",
                           json_object_new_int64(sample.absolute_frame));
    json_object_object_add(c, "camera_id",
                           json_object_new_string(r.snapshot.camera.target_id));
    json_object_object_add(c, "shot_id",
                           json_object_new_string(r.snapshot.camera.shot_id));
    json_object_object_add(
        c, "presentation_time",
        ratio(r.snapshot.has_sequence
                  ? r.snapshot.sequence.presentation_time.numerator
                  : r.snapshot.frame.local_frame *
                        r.snapshot.frame.rate.frames_per_second_denominator,
              r.snapshot.has_sequence
                  ? r.snapshot.sequence.presentation_time.denominator
                  : r.snapshot.frame.rate.frames_per_second_numerator));
    json_object_object_add(
        c, "simulation_time",
        r.snapshot.has_sequence
            ? ratio(r.snapshot.sequence.simulation_time.numerator,
                    r.snapshot.sequence.simulation_time.denominator)
            : NULL);
    json_object_object_add(
        c, "camera_motion_time",
        r.snapshot.camera.rig_revision
            ? ratio(r.snapshot.camera.motion_time.numerator,
                    r.snapshot.camera.motion_time.denominator)
            : NULL);
    json_object_object_add(
        c, "inspection_id",
        json_object_new_string(r.snapshot.camera.inspection_id));
    json_object_object_add(
        c, "inspection_time",
        r.snapshot.camera.inspection_id[0]
            ? ratio(r.snapshot.camera.inspection_time.numerator,
                    r.snapshot.camera.inspection_time.denominator)
            : NULL);
    json_object_object_add(o, "clocks", c);
  }
  return o;
}
static json_object *camera(json_object *rig, const char *id) {
  json_object *list = get(rig, "cameras");
  for (size_t i = 0; list && i < json_object_array_length(list); i++) {
    json_object *c = json_object_array_get_idx(list, i);
    if (!strcmp(str(c, "id"), id))
      return c;
  }
  return NULL;
}
static bool replace_camera(json_object *rig, json_object *entry,
                           const char *id) {
  json_object *list = get(rig, "cameras");
  if (!list)
    return false;
  for (size_t i = 0; i < json_object_array_length(list); i++)
    if (!strcmp(str(json_object_array_get_idx(list, i), "id"), id)) {
      if (entry)
        json_object_array_put_idx(list, i, copy(entry));
      else
        json_object_array_del_idx(list, i, 1);
      return true;
    }
  if (!entry)
    return false;
  return json_object_array_add(list, copy(entry)) == 0;
}
static json_object *shot(json_object *old, uint64_t start, uint64_t count,
                         const char *id, const char *camera_id) {
  json_object *s = copy(old);
  json_object_object_add(s, "id", json_object_new_string(id));
  json_object_object_add(s, "start_frame", json_object_new_int64(start));
  json_object_object_add(s, "frame_count", json_object_new_int64(count));
  if (camera_id) {
    json_object_object_add(s, "camera_id", json_object_new_string(camera_id));
    json_object_object_add(s, "clip_start", ratio(0, 1));
  }
  return s;
}
bool SceneEditorSequenceCommand(const char *action, json_object *p,
                                unsigned long long revision, char *m,
                                size_t n) {
  if(!strcmp(action,"event_draft"))return json_object_is_type(p,json_type_object)&&json_object_object_length(p)==1&&SceneEditorEventPlannerDraft(get(p,"intent"),revision,m,n);
  if(!strcmp(action,"event_select")||!strcmp(action,"event_remove"))return SceneEditorEventCameraCommand(action,p,revision,m,n);
  const char *expected =
      !strcmp(action, "apply")                                        ? NULL
      : (!strcmp(action, "setup") || !strcmp(action, "source_setup")) ? ""
      : !strcmp(action, "segment_split")     ? "|frame|"
      : !strcmp(action, "segment_mode")      ? "|id||kind|"
      : !strcmp(action, "segment_boundary")  ? "|id||end_frame|"
      : !strcmp(action, "camera_upsert")     ? "|camera|"
      : !strcmp(action, "camera_remove")     ? "|camera_id|"
      : !strcmp(action, "clock_set")         ? "|camera_id||clock|"
      : !strcmp(action, "shots_set")         ? "|shots|"
      : !strcmp(action, "inspection_remove") ? "|id|"
      : !strcmp(action, "inspect")
          ? "|id||camera_id||target_id||start_frame||frame_count||angle||max_"
            "speed||acceleration||braking|"
          : "INVALID";
  if (!json_object_is_type(p, json_type_object))
    return fail(m, n, "Expected typed command parameters.");
  if (expected) {
    size_t count = 0;
    for (const char *c = expected; *c; c++)
      count += *c == '|';
    if (json_object_object_length(p) != (int)(count / 2))
      return fail(m, n, "Unknown or missing command fields.");
    json_object_object_foreach(p, key, value) {
      (void)value;
      char token[96];
      snprintf(token, sizeof(token), "|%s|", key);
      if (!strstr(expected, token))
        return fail(m, n, "Unknown command field.");
    }
  }
  if (!strcmp(action, "inspect")) {
    const char *numbers[] = {"angle", "max_speed", "acceleration", "braking"};
    for (int i = 0; i < 4; i++) {
      json_object *v = get(p, numbers[i]);
      if ((!json_object_is_type(v, json_type_int) &&
           !json_object_is_type(v, json_type_double)) ||
          !isfinite(json_object_get_double(v)))
        return fail(m, n, "Invalid inspection limit.");
    }
    const char *ints[] = {"start_frame", "frame_count"};
    for (int i = 0; i < 2; i++)
      if (!json_object_is_type(get(p, ints[i]), json_type_int) ||
          json_object_get_int64(get(p, ints[i])) < 0)
        return fail(m, n, "Invalid inspection interval.");
  }
  if (!SceneEditorDocumentIsOpen() ||
      revision != SceneEditorDocumentRevision() ||
      !json_object_is_type(p, json_type_object))
    return fail(m, n, "Scene changed; reload the sequence draft.");
  json_object *a = author();
  if (!a)
    return fail(m, n, "Set up the scene timeline first.");
  if (!document_begin_command(m, n))
    return false;
  a = author();
  json_object *rig = get(a, "camera_rig");
  bool ok = false;
  if (!strcmp(action, "source_setup") || !strcmp(action, "segment_split") ||
      !strcmp(action, "segment_mode") || !strcmp(action, "segment_boundary")) {
    ok = SceneEditorSequenceSegments(document_authoring_root(), a, action, p,
                                     revision);
  } else if (!strcmp(action, "apply")) {
    const char *keys[] = {"sequence",       "camera_rig",   "inspection_moves",
                          "scene_timeline", "motion_paths", "motion_plans", "sequence_volume", "event_cameras"};
    ok = json_object_object_length(p) > 0;
    json_object_object_foreach(p, key, value) {
      bool allowed = false;
      for (int i = 0; i < 8; i++)
        if (!strcmp(key, keys[i]))
          allowed = true;
      if (!allowed) {
        ok = false;
        break;
      }
      if (value)
        json_object_object_add(a, key, copy(value));
      else
        json_object_object_del(a, key);
    }
  } else if (!strcmp(action, "setup")) {
    TimelineDocument *d = malloc(sizeof(*d));
    RayEvaluatedSceneServiceResult snapshot;
    TimelineRate rate;
    TimelineRange range;
    if (!rig && d && SceneEditorDocumentGetTimeline(d) == TIMELINE_STATUS_OK &&
        RayEvaluatedSceneResolveTimelineClock(&rate, &range) ==
            TIMELINE_STATUS_OK &&
        RayEvaluatedSceneCaptureSample(
            (TimelineSample){range.start_frame, 0, 1}, &snapshot)) {
      rig = json_object_new_object();
      json_object_object_add(rig, "schema",
                             json_object_new_string("ray_camera_rig_v1"));
      json_object_object_add(rig, "version", json_object_new_int(1));
      json_object_object_add(rig, "rate",
                             ratio(rate.frames_per_second_numerator,
                                   rate.frames_per_second_denominator));
      json_object *r = json_object_new_object();
      json_object_object_add(r, "start_frame",
                             json_object_new_int64(range.start_frame));
      json_object_object_add(r, "frame_count",
                             json_object_new_int64(range.frame_count));
      json_object_object_add(rig, "range", r);
      json_object *list = json_object_new_array();
      json_object_array_add(list, base(&snapshot.snapshot.camera, "camera/main",
                                       range.frame_count,
                                       SceneEditorDocumentWorldScale()));
      json_object_object_add(rig, "cameras", list);
      list = json_object_new_array();
      json_object *s = json_tokener_parse(
          "{\"id\":\"main\",\"camera_id\":\"camera/"
          "main\",\"start_frame\":0,\"frame_count\":1,\"clip_start\":{"
          "\"numerator\":0,\"denominator\":1}}");
      json_object_object_add(s, "frame_count",
                             json_object_new_int64(range.frame_count));
      json_object_array_add(list, s);
      json_object_object_add(rig, "shots", list);
      json_object_object_add(a, "camera_rig", rig);
      ok = true;
    }
    free(d);
  } else if (rig && !strcmp(action, "camera_upsert")) {
    json_object *c = get(p, "camera");
    ok = c && replace_camera(rig, c, str(c, "id"));
  } else if (rig && !strcmp(action, "camera_remove"))
    ok = replace_camera(rig, NULL, str(p, "camera_id"));
  else if (rig && !strcmp(action, "clock_set")) {
    json_object *c = camera(rig, str(p, "camera_id")), *clock = get(p, "clock");
    if (c && clock) {
      json_object_object_add(c, "clock", copy(clock));
      ok = true;
    }
  } else if (rig && !strcmp(action, "shots_set")) {
    json_object *list = get(p, "shots");
    if (list) {
      json_object_object_add(rig, "shots", copy(list));
      ok = true;
    }
  } else if (!strcmp(action, "inspection_remove")) {
    json_object *list = get(a, "inspection_moves");
    for (size_t i = 0; list && i < json_object_array_length(list); i++)
      if (!strcmp(str(json_object_array_get_idx(list, i), "id"),
                  str(p, "id"))) {
        json_object_array_del_idx(list, i, 1);
        if (!json_object_array_length(list))
          json_object_object_del(a, "inspection_moves");
        ok = true;
        break;
      }
  } else if (rig && !strcmp(action, "inspect")) {
    uint64_t start = json_object_get_uint64(get(p, "start_frame")),
             count = json_object_get_uint64(get(p, "frame_count"));
    const char *id = str(p, "id"), *source = str(p, "camera_id"),
               *target = str(p, "target_id");
    json_object *entry = RuntimeInspectionCapture(
        document_authoring_root(), id, source, target, start, count,
        json_object_get_double(get(p, "angle")),
        json_object_get_double(get(p, "max_speed")),
        json_object_get_double(get(p, "acceleration")),
        json_object_get_double(get(p, "braking")), m, n);
    if (entry && id[0] && strlen(id) < 40) {
      char name[64];
      snprintf(name, 64, "camera/inspect_%s", id);
      json_object *anchor = get(entry, "anchor");
      RayEvaluatedCamera c = {.valid = true};
      json_object *v = get(anchor, "position");
      c.position = (TimelineVec3){
          json_object_get_double(json_object_array_get_idx(v, 0)),
          json_object_get_double(json_object_array_get_idx(v, 1)),
          json_object_get_double(json_object_array_get_idx(v, 2))};
      c.yaw_radians = json_object_get_double(get(anchor, "yaw"));
      c.pitch_radians = json_object_get_double(get(anchor, "pitch"));
      c.fov_y_degrees = json_object_get_double(get(anchor, "fov_y"));
      c.zoom = json_object_get_double(get(anchor, "zoom"));
      if (!camera(rig, name)) {
        json_object *cb =
            base(&c, name,
                 json_object_get_uint64(get(get(rig, "range"), "frame_count")),
                 SceneEditorDocumentWorldScale());
        replace_camera(rig, cb, name);
        json_object_put(cb);
        json_object *old = get(rig, "shots"), *list = json_object_new_array();
        for (size_t i = 0; i < json_object_array_length(old); i++) {
          json_object *s = json_object_array_get_idx(old, i);
          uint64_t b = json_object_get_uint64(get(s, "start_frame")),
                   z = b + json_object_get_uint64(get(s, "frame_count"));
          char sid[64];
          if (z <= start || b >= start + count)
            json_object_array_add(list, copy(s));
          else {
            if (b < start) {
              snprintf(sid, 64, "%.48s_before", str(s, "id"));
              json_object_array_add(list, shot(s, b, start - b, sid, NULL));
            }
            if (b <= start && z > start)
              json_object_array_add(list, shot(s, start, count, id, name));
            if (z > start + count) {
              snprintf(sid, 64, "%.48s_after", str(s, "id"));
              json_object *tail =
                  shot(s, start + count, z - start - count, sid, NULL);
              json_object *old_clock =
                  get(camera(rig, str(s, "camera_id")), "clock");
              if (!strcmp(str(old_clock, "mode"), "clip")) {
                json_object *old = get(s, "clip_start"),
                            *rate = get(rig, "rate");
                SequenceTime delta, sum;
                if (SequenceTimeMultiply(
                        (SequenceTime){start + count - b, 1},
                        (SequenceTime){
                            json_object_get_uint64(get(rate, "denominator")),
                            json_object_get_uint64(get(rate, "numerator"))},
                        &delta) == SEQUENCE_OK &&
                    SequenceTimeAdd(
                        (SequenceTime){
                            json_object_get_uint64(get(old, "numerator")),
                            json_object_get_uint64(get(old, "denominator"))},
                        delta, &sum) == SEQUENCE_OK)
                  json_object_object_add(tail, "clip_start",
                                         ratio(sum.numerator, sum.denominator));
                else
                  json_object_object_add(tail, "clip_start", NULL);
              }
              json_object_array_add(list, tail);
            }
          }
        }
        json_object_object_add(rig, "shots", list);
        json_object_object_add(entry, "camera_id",
                               json_object_new_string(name));
        json_object_object_add(
            entry, "dependency",
            RuntimeInspectionDependencies(document_authoring_root(), entry));
        list = get(a, "inspection_moves");
        if (!list) {
          list = json_object_new_array();
          json_object_object_add(a, "inspection_moves", list);
        }
        json_object_array_add(list, entry);
        entry = NULL;
        ok = true;
      }
    }
    if (entry)
      json_object_put(entry);
  }
  if (!ok) {
    document_rollback_command();
    return fail(
        m, n,
        "Invalid sequence command, unresolved camera/target or unsupported "
        "setup. Convert a legacy camera path before enabling a rig.");
  }
  if (!document_finish_command(m, n))
    return false;
  SceneEditorTimelinePause();
  return true;
}
bool SceneEditorSequenceReadOnly(const char *name) {
  return !strcmp(name,"sequence.event_planner_read") || !strcmp(name,"sequence.event_pick") || !strcmp(name, "sequence.read") ||
         !strcmp(name, "sequence.read_section") || !strcmp(name,"sequence.event_candidates");
}
bool SceneEditorSequenceDispatch(const char *name, json_object *params,
                                 json_object *response, char *m, size_t n) {
  if(!strcmp(name,"sequence.event_planner_read")){if(!json_object_is_type(params,json_type_object)||json_object_object_length(params))return false;json_object_object_add(response,"event_planner",SceneEditorEventPlannerRead());return true;}
  if(!strcmp(name,"sequence.event_pick"))return SceneEditorEventPlannerPick(params,response,m,n);
  if(!strcmp(name,"sequence.event_candidates"))return SceneEditorEventCameraCandidates(params,response,m,n);
  if (SceneEditorSequenceReadOnly(name)) {
    bool section = !strcmp(name, "sequence.read_section");
    if (!json_object_is_type(params, json_type_object) ||
        json_object_object_length(params) != (section ? 1 : 0))
      return false;
    json_object *read = SceneEditorSequenceRead();
    if (section) {
      const char *key = str(params, "section");
      if (strcmp(key, "sequence") && strcmp(key, "camera_rig") &&
          strcmp(key, "inspection_moves") && strcmp(key, "sequence_volume") && strcmp(key,"event_cameras") && strcmp(key, "clocks")) {
        json_object_put(read);
        return false;
      }
      json_object *part = json_object_new_object();
      json_object_object_add(part, key, copy(get(read, key)));
      json_object_put(read);
      read = part;
    }
    if (strlen(json_object_to_json_string_ext(read, JSON_C_TO_STRING_PLAIN)) >
        48000) {
      json_object_put(read);
      return fail(
          m, n,
          "Readback exceeds control capacity; use sequence_read_section.");
    }
    json_object_object_add(response, "sequence", read);
    return true;
  }
  if (strncmp(name, "sequence.", 9))
    return false;
  json_object *args = copy(params);
  json_object *expected = get(args, "expected_revision");
  unsigned long long rev = json_object_get_uint64(expected);
  if (!json_object_is_type(expected, json_type_int)) {
    json_object_put(args);
    return false;
  }
  json_object_object_del(args, "expected_revision");
  bool ok = SceneEditorSequenceCommand(
      name + 9, !strcmp(name, "sequence.apply") ? get(args, "changes") : args,
      rev, m, n);
  json_object_put(args);
  return ok;
}
