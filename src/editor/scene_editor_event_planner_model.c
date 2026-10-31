/* Transient draft adapter. The existing retained event command owns all edits.
 */
#include "editor/scene_editor_event_camera.h"
#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_sequence_panel.h"
#include "editor/scene_editor_shared_fields.h"
#include "editor/scene_editor_workspace_profile.h"
#include "import/runtime_event_camera.h"
#include "scene_editor_event_planner_internal.h"
EventPlannerState event_planner = {.editing = -1, .feature_radius = .25};
#define P event_planner
static json_object *default_intent(void) {
  return json_tokener_parse(
      "{\"schema\":\"ray_event_camera_intent_v1\",\"id\":\"ui_event\",\"hold\":"
      "{\"start_frame\":0,\"frame_count\":2,\"source_tick\":0},\"focus\":{"
      "\"kind\":\"body_com\",\"targets\":[]},\"pivot\":{\"kind\":\"body_com\","
      "\"targets\":[]},\"framing\":{\"fov_y\":55,\"aspect\":1.3333333333333333,"
      "\"margin\":0.12,\"offset\":[0,0],\"azimuth\":0.6,\"elevation\":0.45},"
      "\"orbit\":{\"angle\":4.71238898038469,\"max_speed\":30,\"acceleration\":"
      "100,\"braking\":100},\"lead\":{\"kind\":\"stationary\",\"start_frame\":"
      "0},\"resume\":{\"position\":\"endpoint\",\"aim\":\"track\",\"end_"
      "frame\":2}}");
}
static void cancel(void) {
  if (P.editing >= 0) {
    scene_editor_shared_field_end(P.draft);
    SDL_StopTextInput();
  }
  P.editing = -1;
  P.armed = 0;
}
bool SceneEditorEventPlannerVisible(void) {
  return P.open && SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_RENDER;
}
bool SceneEditorEventPlannerInteraction(void) {
  return SceneEditorEventPlannerVisible() && (P.armed || P.editing >= 0);
}
void EventPlannerInvalidate(void) {
  if (P.proposals)
    json_object_put(P.proposals);
  P.proposals = NULL;
  P.candidate = 0;
}
void SceneEditorEventPlannerReset(void) {
  cancel();
  if (P.intent)
    json_object_put(P.intent);
  if (P.picked)
    json_object_put(P.picked);
  EventPlannerInvalidate();
  memset(&P, 0, sizeof(P));
  P.editing = -1;
  P.feature_radius = .25;
  EventPlannerClearControls();
}
void EventPlannerSync(void) {
  if (strcmp(P.path, SceneEditorDocumentPath())) {
    SceneEditorEventPlannerReset();
    snprintf(P.path, sizeof(P.path), "%s", SceneEditorDocumentPath());
  }
  if (P.proposals && P.proposal_revision != SceneEditorDocumentRevision())
    EventPlannerInvalidate();
}
void SceneEditorEventPlannerOpen(bool open) {
  EventPlannerSync();
  cancel();
  P.open = open;
  P.viewport_valid = false;
  EventPlannerClearControls();
  if (open) {
    SceneEditorMotionPathPanelSelect(false);
    SceneEditorSequencePanelReset();
    SceneEditorTimelinePause();
    SceneEditorTimelineReleaseFocus();
    if (!P.intent) {
      if (P.picked) {
        json_object_put(P.picked);
        P.picked = NULL;
      }
      json_object *read = SceneEditorSequenceRead(),
                  *events = pg(read, "event_cameras"), *first = pa(events, 0);
      P.intent = pc(pg(first, "intent"));
      json_object_put(read);
      P.revision = SceneEditorDocumentRevision();
      if (!P.intent) {
        P.intent = default_intent();
        EventPlannerHold(0);
        EventPlannerTarget(-1);
      }
      P.feature_radius = pn(pg(P.intent, "focus"), "radius");
      if (P.feature_radius <= 0)
        P.feature_radius = .25;
    }
  }
}
bool EventPlannerHold(int direction) {
  json_object *read = SceneEditorSequenceRead(), *seq = pg(read, "sequence"),
              *segments = pg(seq, "segments");
  size_t count = plen(segments), selected = SIZE_MAX;
  uint64_t old =
      json_object_get_uint64(pg(pg(P.intent, "hold"), "start_frame"));
  for (size_t i = 0; i < count; i++)
    if (!strcmp(ps(pa(segments, i), "kind"), "hold") &&
        (direction == 0 ||
         json_object_get_uint64(pg(pa(segments, i), "start_frame")) > old)) {
      selected = i;
      break;
    }
  if (selected == SIZE_MAX)
    for (size_t i = 0; i < count; i++)
      if (!strcmp(ps(pa(segments, i), "kind"), "hold")) {
        selected = i;
        break;
      }
  if (selected == SIZE_MAX) {
    snprintf(P.feedback, sizeof(P.feedback),
             "Bind a rigid sequence with an exact Hold first.");
    json_object_put(read);
    return false;
  }
  json_object *s = pa(segments, selected);
  int64_t start = json_object_get_int64(pg(s, "start_frame")),
          length = json_object_get_int64(pg(s, "frame_count")),
          origin = json_object_get_int64(pg(pg(seq, "range"), "start_frame"));
  SceneEditorTimelinePause();
  RayEvaluatedSceneServiceResult v;
  if (!SceneEditorTimelineSeek(origin + start) ||
      !RayEvaluatedSceneCaptureSample((TimelineSample){origin + start, 0, 1},
                                      &v) ||
      !v.snapshot.has_sequence) {
    json_object_put(read);
    return false;
  }
  json_object *h = pg(P.intent, "hold");
  pi(h, "start_frame", start);
  pi(h, "frame_count", length);
  pi(h, "source_tick", v.snapshot.sequence.source_tick);
  pi(pg(P.intent, "resume"), "end_frame",
     json_object_get_int64(pg(pg(seq, "range"), "frame_count")));
  P.revision = SceneEditorDocumentRevision();
  EventPlannerInvalidate();
  P.viewport_valid = false;
  json_object_put(read);
  return true;
}
void EventPlannerTarget(int which) {
  bool advance = which >= 0;
  if (which < 0)
    which = 0;
  if (!P.intent)
    return;
  json_object *ref = pg(P.intent, "focus"), *ids = pg(ref, "targets");
  const char *old = json_object_get_string(pa(ids, which));
  char current[64];
  snprintf(current, sizeof(current), "%s", old ? old : "");
  RayEvaluatedSceneSnapshot v;
  if (!SceneEditorTimelineCopyEvaluated(&v))
    return;
  const char *choices[256];
  size_t count = 0, index = SIZE_MAX;
  for (size_t i = 0; i < v.object_transform_count && count < 256; i++)
    if (v.object_transforms[i].source ==
        RAY_EVALUATED_OBJECT_TRANSFORM_RIGID_RESULT) {
      choices[count] = v.object_transforms[i].target_id;
      if (!strcmp(current, choices[count]))
        index = count;
      count++;
    }
  if (!count)
    return;
  index = index == SIZE_MAX ? 0 : (advance ? (index + 1) % count : index);
  json_object *target = json_object_new_object(),
              *list = json_object_new_array();
  const char *kind = ps(ref, "kind");
  bool pair = !strcmp(kind, "pair_com") || !strcmp(kind, "pair_bounds");
  if (!pair) {
    pt(target, "kind", "body_com");
    json_object_array_add(list, json_object_new_string(choices[index]));
  } else {
    pt(target, "kind", kind);
    const char *other = json_object_get_string(pa(ids, 1 - which));
    if (!other || !strcmp(other, choices[index]))
      other = choices[(index + 1) % count];
    json_object_array_add(
        list, json_object_new_string(which ? other : choices[index]));
    json_object_array_add(
        list, json_object_new_string(which ? choices[index] : other));
  }
  json_object_object_add(target, "targets", list);
  json_object_object_add(P.intent, "focus", target);
  json_object_object_add(P.intent, "pivot", pc(target));
  if (P.picked) {
    json_object_put(P.picked);
    P.picked = NULL;
  }
  EventPlannerInvalidate();
}
bool EventPlannerGenerate(void) {
  if (!P.intent || P.revision != SceneEditorDocumentRevision()) {
    snprintf(P.feedback, sizeof(P.feedback),
             "Scene changed. Reload event or load a fresh MCP draft.");
    return false;
  }
  json_object *args = json_object_new_object(), *out = json_object_new_object();
  json_object_object_add(args, "intent", pc(P.intent));
  EventPlannerInvalidate();
  bool ok = SceneEditorEventCameraCandidates(args, out, P.feedback,
                                             sizeof(P.feedback));
  if (ok) {
    P.proposals = pc(pg(out, "event_candidates"));
    P.proposal_revision = P.revision;
    P.page = 2;
    snprintf(P.feedback, sizeof(P.feedback),
             "Review candidates; Apply saves one Undo step.");
  }
  json_object_put(args);
  json_object_put(out);
  return ok;
}
bool EventPlannerApply(void) {
  json_object *list = pg(P.proposals, "candidates"), *c = pa(list, P.candidate);
  if (!c)
    return false;
  json_object *args = json_object_new_object();
  json_object_object_add(args, "intent", pc(P.intent));
  pi(args, "candidate", P.candidate);
  pt(args, "proposal_sha256", ps(c, "proposal_sha256"));
  bool ok =
      SceneEditorEventCameraCommand("event_select", args, P.proposal_revision,
                                    P.feedback, sizeof(P.feedback));
  json_object_put(args);
  if (ok) {
    P.revision = SceneEditorDocumentRevision();
    EventPlannerInvalidate();
    snprintf(P.feedback, sizeof(P.feedback),
             "Applied event. Undo / Save use the scene owner.");
  }
  return ok;
}
bool EventPlannerSeek(int phase) {
  json_object *read = SceneEditorSequenceRead(), *seq = pg(read, "sequence"),
              *h = pg(P.intent, "hold");
  int64_t origin = json_object_get_int64(pg(pg(seq, "range"), "start_frame")),
          start = json_object_get_int64(pg(h, "start_frame")),
          count = json_object_get_int64(pg(h, "frame_count"));
  json_object_put(read);
  SceneEditorTimelinePause();
  P.armed = 0;
  P.viewport_valid = false;
  return SceneEditorTimelineSeek(origin + (phase == 0   ? 0
                                           : phase == 1 ? start
                                           : phase == 2
                                               ? start + (count - 1) / 2
                                               : start + count));
}
bool SceneEditorEventPlannerDraft(json_object *intent,
                                  unsigned long long revision, char *d,
                                  size_t n) {
  EventPlannerSync();
  if (revision != SceneEditorDocumentRevision())
    return false;
  json_object *args = json_object_new_object(), *out = json_object_new_object();
  json_object_object_add(args, "intent", pc(intent));
  bool ok = SceneEditorEventCameraCandidates(args, out, d, n);
  json_object_put(args);
  json_object_put(out);
  if (!ok)
    return false;
  cancel();
  EventPlannerInvalidate();
  if (P.intent)
    json_object_put(P.intent);
  P.intent = pc(intent);
  if (P.picked) {
    json_object_put(P.picked);
    P.picked = NULL;
  }
  P.feature_radius = pn(pg(P.intent, "focus"), "radius");
  if (P.feature_radius <= 0)
    P.feature_radius = .25;
  P.revision = revision;
  SceneEditorEventPlannerOpen(true);
  return true;
}
json_object *SceneEditorEventPlannerRead(void) {
  EventPlannerSync();
  json_object *o = json_object_new_object(), *read = SceneEditorSequenceRead();
  json_object_object_add(
      o, "open", json_object_new_boolean(SceneEditorEventPlannerVisible()));
  pi(o, "page", P.page);
  pi(o, "armed", P.armed);
  pi(o, "draft_revision", P.revision);
  json_object_object_add(o, "intent", pc(P.intent));
  json_object_object_add(o, "clocks", pc(pg(read, "clocks")));
  json_object_object_add(o, "candidates", pc(pg(P.proposals, "candidates")));
  pi(o, "viewport_token", P.viewport_valid ? P.viewport_token : 0);
  pi(o, "viewport_frame", P.drawn_sample.absolute_frame);
  pi(o, "viewport_tick", P.drawn_tick);
  pt(o, "coordinate_space", "logical_window_pixels");
  json_object *viewport = json_object_new_object();
  pi(viewport, "x", P.projector.viewport.x);
  pi(viewport, "y", P.projector.viewport.y);
  pi(viewport, "width", P.projector.viewport.w);
  pi(viewport, "height", P.projector.viewport.h);
  json_object_object_add(o, "viewport", P.viewport_valid ? viewport : NULL);
  if (!P.viewport_valid)
    json_object_put(viewport);

  json_object_object_add(o, "picked", pc(P.picked));
  json_object_put(read);
  return o;
}
