#include "editor/scene_editor_sequence_panel.h"
#include "animation/sequence_time.h"
#include "app/evaluated_scene_service.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_sequence.h"
#include "editor/scene_editor_shared_fields.h"
#include "editor/scene_editor_shared_text.h"
#include "editor/scene_editor_shared_ui.h"
#include "editor/scene_editor_timeline.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum {
  OPEN,
  SETUP,
  PREV,
  NEXT,
  DUPLICATE,
  REMOVE,
  POLICY,
  SPEED,
  OFFSET,
  SHOT_PREV,
  SHOT_NEXT,
  ASSIGN,
  SPLIT,
  TARGET,
  ANGLE,
  LIMIT,
  ACCEL,
  BRAKE,
  INSPECT,
  REMOVE_INSPECTION,
  SOURCE_SETUP,
  SEGMENT_NEXT,
  SEGMENT_MODE,
  SEGMENT_SPLIT,
  POSE_X,
  POSE_Y,
  POSE_Z,
  YAW,
  PITCH,
  FOV,
  CLIP,
  SHOT_END,
  SEGMENT_END,
  N
};
static SDL_Rect controls[N];
static char labels[N][192], draft[64], feedback[256], path[4096], camera_id[64],
    target_id[64];
static int editing = -1;
static bool expanded;
static size_t camera_index, shot_index, target_index, segment_index;
static unsigned long long revision, shown_revision;
static const char *names[] = {"sequence_open",
                              "sequence_setup",
                              "sequence_camera_previous",
                              "sequence_camera_next",
                              "sequence_camera_duplicate",
                              "sequence_camera_remove",
                              "sequence_camera_policy",
                              "sequence_camera_speed",
                              "sequence_camera_offset",
                              "sequence_shot_previous",
                              "sequence_shot_next",
                              "sequence_shot_assign",
                              "sequence_shot_split",
                              "sequence_target",
                              "sequence_angle",
                              "sequence_speed_limit",
                              "sequence_acceleration",
                              "sequence_braking",
                              "sequence_inspect",
                              "sequence_inspection_remove",
                              "sequence_source_setup",
                              "sequence_segment_next",
                              "sequence_segment_mode",
                              "sequence_segment_split",
                              "sequence_camera_x",
                              "sequence_camera_y",
                              "sequence_camera_z",
                              "sequence_camera_yaw",
                              "sequence_camera_pitch",
                              "sequence_camera_fov",
                              "sequence_clip_start",
                              "sequence_shot_end",
                              "sequence_segment_end"};
static char clock_labels[4][192];
static double angle = 360, limit = 20, acceleration = 100, braking = 100;
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
static json_object *clone(json_object *o) {
  json_object *c = NULL;
  if (o)
    json_object_deep_copy(o, &c, NULL);
  return c;
}
static json_object *ratio(double value) {
  json_object *o = json_object_new_object();
  json_object_object_add(o, "numerator",
                         json_object_new_int64(llround(value * 1000000)));
  json_object_object_add(o, "denominator", json_object_new_int(1000000));
  return o;
}
static double seconds(json_object *r) {
  double d = json_object_get_double(get(r, "denominator"));
  return d ? json_object_get_double(get(r, "numerator")) / d : 0;
}
bool SceneEditorSequencePanelActive(void) { return editing >= 0; }
void SceneEditorSequencePanelReset(void) {
  if (editing >= 0) {
    scene_editor_shared_field_end(draft);
    SDL_StopTextInput();
  }
  editing = -1;
  expanded = false;
  memset(controls, 0, sizeof(controls));
  feedback[0] = 0;
  camera_index = shot_index = target_index = 0;
}
static void label(SDL_Renderer *r, const char *s, int x, int y) {
  scene_editor_shared_label_at(
      r, s, x, y, SceneEditorChromeShellResolvePalette().text_primary, 11);
}
int SceneEditorSequencePanelDraw(SDL_Renderer *r, SDL_Rect pane, int y) {
  if (strcmp(path, SceneEditorDocumentPath())) {
    SceneEditorSequencePanelReset();
    snprintf(path, sizeof(path), "%s", SceneEditorDocumentPath());
  }
  shown_revision = SceneEditorDocumentRevision();
  memset(controls, 0, sizeof(controls));
  json_object *read = SceneEditorSequenceRead(), *rig = get(read, "camera_rig"),
              *cameras = get(rig, "cameras"), *shots = get(rig, "shots");
  int x = pane.x + 10, w = pane.w - 20;
  controls[OPEN] = (SDL_Rect){x, y, w, 30};
  SceneEditorSharedUiPanelButton(r, 188, 0, controls[OPEN],
                                 "Sequence / cameras / inspection", expanded,
                                 true);
  y += 36;
  if (!expanded)
    goto done;
  static char active_labels[2][192];
  json_object *active = get(read, "clocks");
  snprintf(active_labels[0], 192, "Active view: %s", str(active, "camera_id"));
  snprintf(active_labels[1], 192, "Shot: %s", str(active, "shot_id"));
  for (int i = 0; i < 2; i++) {
    label(r, active_labels[i], x, y);
    y += 22;
  }
  {
    json_object *c = get(read, "clocks");
    const char *keys[] = {"presentation_time", "simulation_time",
                          "camera_motion_time", "inspection_time"};
    const char *titles[] = {"Presentation", "Simulation", "Camera motion",
                            "Inspection"};
    for (int i = 0; i < 4; i++) {
      json_object *v = get(c, keys[i]);
      snprintf(clock_labels[i], 192, "%s: %s%.6g s", titles[i],
               v ? "" : "unbound ", seconds(v));
      label(r, clock_labels[i], x, y);
      y += 22;
    }
  }
  if (!rig) {
    controls[SETUP] = (SDL_Rect){x, y, w, 30};
    SceneEditorSharedUiPanelButton(r, 188, 1, controls[SETUP],
                                   "Set up named cameras", false, true);
    y += 36;
    goto footer;
  }
  size_t count = json_object_array_length(cameras);
  if (camera_index >= count)
    camera_index = 0;
  json_object *camera = json_object_array_get_idx(cameras, camera_index),
              *clock = get(camera, "clock");
  snprintf(camera_id, 64, "%s", str(camera, "id"));
  snprintf(labels[PREV], 192, "Previous camera");
  snprintf(labels[NEXT], 192, "Next: %s", camera_id);
  snprintf(labels[DUPLICATE], 192, "Duplicate selected camera");
  snprintf(labels[REMOVE], 192, "Remove camera (references must be clear)");
  snprintf(labels[POLICY], 192, "Motion clock: %s", str(clock, "mode"));
  snprintf(labels[SPEED], 192, "Camera speed: %.6g",
           seconds(get(clock, "speed")));
  snprintf(labels[OFFSET], 192, "Camera offset seconds: %.6g",
           seconds(get(clock, "offset")));
  if (shot_index >= json_object_array_length(shots))
    shot_index = 0;
  json_object *shot = json_object_array_get_idx(shots, shot_index);
  snprintf(labels[SHOT_PREV], 192, "Previous shot");
  snprintf(labels[SHOT_NEXT], 192, "Next shot: %.64s", str(shot, "id"));
  snprintf(labels[ASSIGN], 192, "Assign %s to this shot", camera_id);
  snprintf(labels[SPLIT], 192, "Split selected shot at playhead");
  TimelineSample sample;
  RayEvaluatedSceneServiceResult state;
  size_t targets = 0;
  if (SceneEditorTimelineCurrentSample(&sample) &&
      RayEvaluatedSceneCaptureSample(sample, &state)) {
    for (size_t i = 0; i < state.snapshot.object_transform_count; i++)
      if (state.snapshot.object_transforms[i].source ==
          RAY_EVALUATED_OBJECT_TRANSFORM_RIGID_RESULT)
        targets++;
    if (target_index >= targets)
      target_index = 0;
    size_t index = 0;
    for (size_t i = 0; i < state.snapshot.object_transform_count; i++)
      if (state.snapshot.object_transforms[i].source ==
              RAY_EVALUATED_OBJECT_TRANSFORM_RIGID_RESULT &&
          index++ == target_index)
        snprintf(target_id, 64, "%s",
                 state.snapshot.object_transforms[i].target_id);
  }
  snprintf(labels[TARGET], 192, "Inspection target: %s",
           targets ? target_id : "no recorded bodies");
  snprintf(labels[ANGLE], 192, "Orbit degrees: %.6g", angle);
  snprintf(labels[LIMIT], 192, "Max camera speed: %.6g", limit);
  snprintf(labels[ACCEL], 192, "Acceleration: %.6g", acceleration);
  snprintf(labels[BRAKE], 192, "Braking: %.6g", braking);
  snprintf(labels[INSPECT], 192, "Orbit selected view through current hold");
  snprintf(labels[REMOVE_INSPECTION], 192,
           "Remove last inspection (unlock edits)");
  json_object *segments = get(get(read, "sequence"), "segments");
  size_t sc = json_object_is_type(segments, json_type_array)
                  ? json_object_array_length(segments)
                  : 0;
  if (segment_index >= sc)
    segment_index = 0;
  json_object *segment =
      sc ? json_object_array_get_idx(segments, segment_index) : NULL;
  snprintf(labels[SOURCE_SETUP], 192, "Bind recorded result to sequence");
  snprintf(labels[SEGMENT_NEXT], 192, "Next source segment: %s",
           str(segment, "id"));
  snprintf(labels[SEGMENT_MODE], 192, "Toggle %s: %s", str(segment, "id"),
           str(segment, "kind"));
  snprintf(labels[SEGMENT_SPLIT], 192, "Split source segment at playhead");
  json_object *position = get(camera, "position");
  double scale = SceneEditorDocumentWorldScale();
  for (int i = 0; i < 3; i++)
    snprintf(labels[POSE_X + i], 192, "Camera %c (world): %.6g", 'X' + i,
             json_object_get_double(json_object_array_get_idx(position, i)) *
                 scale);
  snprintf(labels[YAW], 192, "Yaw degrees: %.6g",
           json_object_get_double(get(camera, "yaw")) * 180 / acos(-1));
  snprintf(labels[PITCH], 192, "Pitch degrees: %.6g",
           json_object_get_double(get(camera, "pitch")) * 180 / acos(-1));
  snprintf(labels[FOV], 192, "Vertical FOV: %.6g",
           json_object_get_double(get(camera, "fov_y")));
  snprintf(labels[CLIP], 192, "Shot clip start seconds: %.6g",
           seconds(get(shot, "clip_start")));
  snprintf(labels[SHOT_END], 192, "Shot end (local frame): %lld",
           (long long)(json_object_get_int64(get(shot, "start_frame")) +
                       json_object_get_int64(get(shot, "frame_count"))));
  snprintf(labels[SEGMENT_END], 192, "Source end (local frame): %lld",
           (long long)(json_object_get_int64(get(segment, "start_frame")) +
                       json_object_get_int64(get(segment, "frame_count"))));
  for (int i = PREV; i < N; i++) {
    if (i == TARGET && !targets)
      target_id[0] = 0;
    controls[i] = (SDL_Rect){x, y, w, 28};
    if (editing == i)
      snprintf(labels[i], 192, "Enter number: %s_", draft);
    SceneEditorSharedUiPanelButton(r, 188, 128 + i, controls[i], labels[i],
                                   editing == i, !(i == INSPECT && !targets));
    y += 32;
  }
footer:
  if (feedback[0]) {
    label(r, feedback, x, y);
    y += 25;
  }

done:
  for (int i = 0; i < N; i++)
    if (controls[i].y < pane.y ||
        controls[i].y + controls[i].h > pane.y + pane.h)
      controls[i] = (SDL_Rect){0};
  json_object_put(read);
  return y;
}
static bool command(const char *action, json_object *p) {
  bool ok = SceneEditorSequenceCommand(action, p, shown_revision, feedback,
                                       sizeof(feedback));
  json_object_put(p);
  if (ok)
    snprintf(feedback, sizeof(feedback),
             "Applied. Shared Undo restores this edit.");
  return ok;
}
static void click(int control) {
  json_object *read = SceneEditorSequenceRead(), *rig = get(read, "camera_rig"),
              *cameras = get(rig, "cameras"), *shots = get(rig, "shots"),
              *p = json_object_new_object();
  size_t count = json_object_is_type(cameras, json_type_array)
                     ? json_object_array_length(cameras)
                     : 0;
  json_object *camera =
      count ? json_object_array_get_idx(cameras, camera_index % count) : NULL;
  if (control == OPEN)
    expanded = !expanded;
  else if (control == SETUP)
    command("setup", json_object_new_object());
  else if (control == PREV && count)
    camera_index = (camera_index + count - 1) % count;
  else if (control == NEXT && count)
    camera_index = (camera_index + 1) % count;
  else if (control == SHOT_PREV && shots)
    shot_index = (shot_index + json_object_array_length(shots) - 1) %
                 json_object_array_length(shots);
  else if (control == SHOT_NEXT && shots)
    shot_index = (shot_index + 1) % json_object_array_length(shots);
  else if (control == TARGET)
    target_index++;
  else if (control == DUPLICATE && camera) {
    json_object *c = clone(camera);
    char name[64];
    snprintf(name, 64, "camera/copy_%llu", SceneEditorDocumentRevision());
    json_object_object_add(c, "id", json_object_new_string(name));
    json_object_object_add(p, "camera", c);
    command("camera_upsert", json_object_get(p));
  } else if (control == REMOVE) {
    json_object_object_add(p, "camera_id", json_object_new_string(camera_id));
    command("camera_remove", json_object_get(p));
  } else if (control == POLICY && camera) {
    json_object *c = clone(get(camera, "clock"));
    const char *mode = str(c, "mode");
    json_object_object_add(
        c, "mode",
        json_object_new_string(!strcmp(mode, "continuous") ? "selected"
                               : !strcmp(mode, "selected") ? "clip"
                                                           : "continuous"));
    json_object_object_add(p, "camera_id", json_object_new_string(camera_id));
    json_object_object_add(p, "clock", c);
    command("clock_set", json_object_get(p));
  } else if ((control == ASSIGN || control == SPLIT) && shots) {
    json_object *list = clone(shots),
                *s = json_object_array_get_idx(list, shot_index);
    TimelineSample sample;
    if (control == ASSIGN) {
      json_object_object_add(s, "camera_id", json_object_new_string(camera_id));
      json_object_object_add(s, "clip_start", ratio(0));
    } else if (SceneEditorTimelineCurrentSample(&sample)) {
      int64_t origin =
          json_object_get_int64(get(get(rig, "range"), "start_frame"));
      uint64_t local = (uint64_t)(sample.absolute_frame - origin),
               start = json_object_get_uint64(get(s, "start_frame")),
               length = json_object_get_uint64(get(s, "frame_count"));
      if (local > start && local < start + length) {
        json_object *tail = clone(s);
        char name[64];
        snprintf(name, 64, "shot_%llu", SceneEditorDocumentRevision());
        json_object_object_add(tail, "id", json_object_new_string(name));
        json_object_object_add(tail, "start_frame",
                               json_object_new_int64(local));
        json_object_object_add(tail, "frame_count",
                               json_object_new_int64(start + length - local));
        SequenceTime first = {json_object_get_uint64(
                                  get(get(s, "clip_start"), "numerator")),
                              json_object_get_uint64(
                                  get(get(s, "clip_start"), "denominator"))},
                     delta = {(local - start) *
                                  json_object_get_uint64(
                                      get(get(rig, "rate"), "denominator")),
                              json_object_get_uint64(
                                  get(get(rig, "rate"), "numerator"))},
                     sum;
        const char *shot_camera = str(s, "camera_id");
        json_object *shot_clock = NULL;
        for (size_t ci = 0; ci < json_object_array_length(cameras); ci++) {
          json_object *cc = json_object_array_get_idx(cameras, ci);
          if (!strcmp(str(cc, "id"), shot_camera))
            shot_clock = get(cc, "clock");
        }
        if (!strcmp(str(shot_clock, "mode"), "clip") &&
            SequenceTimeAdd(first, delta, &sum) == SEQUENCE_OK) {
          json_object *time = json_object_new_object();
          json_object_object_add(time, "numerator",
                                 json_object_new_int64(sum.numerator));
          json_object_object_add(time, "denominator",
                                 json_object_new_int64(sum.denominator));
          json_object_object_add(tail, "clip_start", time);
        }
        json_object_object_add(s, "frame_count",
                               json_object_new_int64(local - start));
        json_object *ordered = json_object_new_array();
        for (size_t i = 0; i < json_object_array_length(list); i++) {
          json_object_array_add(
              ordered, json_object_get(json_object_array_get_idx(list, i)));
          if (i == shot_index)
            json_object_array_add(ordered, tail);
        }
        json_object_put(list);
        list = ordered;
      } else
        snprintf(feedback, sizeof(feedback),
                 "Playhead must be inside the selected shot.");
    }
    json_object_object_add(p, "shots", list);
    command("shots_set", json_object_get(p));
  } else if (control == INSPECT) {
    json_object *sequence = get(read, "sequence"),
                *segments = get(sequence, "segments");
    TimelineSample sample;
    if (SceneEditorTimelineCurrentSample(&sample)) {
      int64_t local =
          sample.absolute_frame -
          json_object_get_int64(get(get(sequence, "range"), "start_frame"));
      for (size_t i = 0; segments && i < json_object_array_length(segments);
           i++) {
        json_object *s = json_object_array_get_idx(segments, i);
        int64_t b = json_object_get_int64(get(s, "start_frame")),
                c = json_object_get_int64(get(s, "frame_count"));
        if (!strcmp(str(s, "kind"), "hold") && local >= b && local < b + c) {
          char id[40];
          snprintf(id, 40, "orbit_%llu", SceneEditorDocumentRevision());
          json_object_object_add(p, "id", json_object_new_string(id));
          json_object_object_add(p, "camera_id",
                                 json_object_new_string(camera_id));
          json_object_object_add(p, "target_id",
                                 json_object_new_string(target_id));
          json_object_object_add(p, "start_frame", json_object_new_int64(b));
          json_object_object_add(p, "frame_count", json_object_new_int64(c));
          json_object_object_add(
              p, "angle", json_object_new_double(angle * acos(-1) / 180));
          json_object_object_add(p, "max_speed", json_object_new_double(limit));
          json_object_object_add(p, "acceleration",
                                 json_object_new_double(acceleration));
          json_object_object_add(p, "braking", json_object_new_double(braking));
          command("inspect", json_object_get(p));
          break;
        }
      }
      if (!json_object_object_length(p))
        snprintf(feedback, sizeof(feedback),
                 "Place the playhead inside an exact source Hold first.");
    }
  } else if (control == SOURCE_SETUP)
    command("source_setup", json_object_new_object());
  else if (control == SEGMENT_NEXT)
    segment_index++;
  else if (control == SEGMENT_MODE) {
    json_object *list = get(get(read, "sequence"), "segments");
    if (list && segment_index < json_object_array_length(list)) {
      json_object *s = json_object_array_get_idx(list, segment_index);
      json_object_object_add(p, "id", json_object_get(get(s, "id")));
      json_object_object_add(
          p, "kind",
          json_object_new_string(!strcmp(str(s, "kind"), "hold") ? "play"
                                                                 : "hold"));
      command("segment_mode", json_object_get(p));
    }
  } else if (control == SEGMENT_SPLIT) {
    TimelineSample sample;
    if (SceneEditorTimelineCurrentSample(&sample)) {
      json_object_object_add(
          p, "frame",
          json_object_new_int64(
              sample.absolute_frame -
              json_object_get_int64(
                  get(get(get(read, "sequence"), "range"), "start_frame"))));
      command("segment_split", json_object_get(p));
    }
  } else if (control == REMOVE_INSPECTION) {
    json_object *list = get(read, "inspection_moves");
    if (list && json_object_array_length(list)) {
      json_object_object_add(
          p, "id",
          json_object_get(get(json_object_array_get_idx(
                                  list, json_object_array_length(list) - 1),
                              "id")));
      command("inspection_remove", json_object_get(p));
    }
  } else if (control == SPEED || control == OFFSET || control == ANGLE ||
             control == LIMIT || control == ACCEL || control == BRAKE ||
             (control >= POSE_X && control <= SEGMENT_END)) {
    editing = control;
    draft[0] = 0;
    revision = shown_revision;
    scene_editor_shared_field_begin(draft, sizeof(draft), 0);
    SDL_StartTextInput();
  }
  json_object_put(p);
  json_object_put(read);
}
bool SceneEditorSequencePanelEvent(SDL_Event *e) {
  if (!e)
    return false;
  if (e->type == SDL_WINDOWEVENT &&
      (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
       e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
    SceneEditorSequencePanelReset();
    return false;
  }
  if (editing >= 0) {
    if (revision != SceneEditorDocumentRevision()) {
      SceneEditorSequencePanelReset();
      return false;
    }
    if (scene_editor_shared_field_event(draft, sizeof(draft), e))
      return true;
    if (e->type == SDL_TEXTINPUT) {
      if (strlen(draft) + strlen(e->text.text) < sizeof(draft))
        strcat(draft, e->text.text);
      return true;
    }
    if (e->type == SDL_KEYDOWN) {
      SDL_Keycode k = e->key.keysym.sym;
      if (k == SDLK_BACKSPACE) {
        if (draft[0])
          draft[strlen(draft) - 1] = 0;
        return true;
      }
      if (k == SDLK_ESCAPE || k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        int field = editing;
        editing = -1;
        scene_editor_shared_field_end(draft);
        SDL_StopTextInput();
        if (k != SDLK_ESCAPE) {
          char *end;
          errno = 0;
          double value = strtod(draft, &end);
          if (errno || end == draft || *end || !isfinite(value) ||
              fabs(value) > 1e8)
            snprintf(feedback, sizeof(feedback),
                     "Enter a finite bounded number.");
          else if (field == ANGLE)
            angle = value;
          else if (field == LIMIT)
            limit = value;
          else if (field == ACCEL)
            acceleration = value;
          else if (field == BRAKE)
            braking = value;
          else if (field >= POSE_X && field <= SEGMENT_END) {
            json_object *r = SceneEditorSequenceRead(),
                        *g = get(r, "camera_rig"),
                        *p = json_object_new_object();
            if (field <= FOV) {
              json_object *c = clone(
                  json_object_array_get_idx(get(g, "cameras"), camera_index));
              if (field <= POSE_Z)
                json_object_array_put_idx(
                    get(c, "position"), field - POSE_X,
                    json_object_new_double(value /
                                           SceneEditorDocumentWorldScale()));
              else
                json_object_object_add(
                    c,
                    field == YAW     ? "yaw"
                    : field == PITCH ? "pitch"
                                     : "fov_y",
                    json_object_new_double(
                        field == FOV ? value : value * acos(-1) / 180));
              json_object_object_add(p, "camera", c);
              command("camera_upsert", json_object_get(p));
            } else if (field == SEGMENT_END) {
              json_object *s = json_object_array_get_idx(
                  get(get(r, "sequence"), "segments"), segment_index);
              if (value == floor(value) && value > 0) {
                json_object_object_add(p, "id", json_object_get(get(s, "id")));
                json_object_object_add(p, "end_frame",
                                       json_object_new_int64((int64_t)value));
                command("segment_boundary", json_object_get(p));
              }
            } else {
              json_object *list = clone(get(g, "shots")),
                          *s = json_object_array_get_idx(list, shot_index);
              if (field == CLIP && value >= 0)
                json_object_object_add(s, "clip_start", ratio(value));
              else if (field == SHOT_END && value == floor(value) &&
                       shot_index + 1 < json_object_array_length(list)) {
                json_object *next =
                    json_object_array_get_idx(list, shot_index + 1);
                int64_t b = json_object_get_int64(get(s, "start_frame")),
                        end = json_object_get_int64(get(next, "start_frame")) +
                              json_object_get_int64(get(next, "frame_count")),
                        v = (int64_t)value;
                if (v > b && v < end) {
                  json_object_object_add(s, "frame_count",
                                         json_object_new_int64(v - b));
                  json_object_object_add(next, "start_frame",
                                         json_object_new_int64(v));
                  json_object_object_add(next, "frame_count",
                                         json_object_new_int64(end - v));
                }
              }
              json_object_object_add(p, "shots", list);
              command("shots_set", json_object_get(p));
            }
            json_object_put(p);
            json_object_put(r);
          } else {
            json_object *read = SceneEditorSequenceRead(),
                        *list = get(get(read, "camera_rig"), "cameras"),
                        *c = json_object_array_get_idx(list, camera_index),
                        *p = json_object_new_object(),
                        *clock = clone(get(c, "clock"));
            if (value < 0 || (field == SPEED && value == 0))
              snprintf(feedback, sizeof(feedback),
                       "Camera time must be nonnegative and speed positive.");
            else {
              json_object_object_add(clock, field == SPEED ? "speed" : "offset",
                                     ratio(value));
              json_object_object_add(p, "camera_id",
                                     json_object_new_string(camera_id));
              json_object_object_add(p, "clock", json_object_get(clock));
              command("clock_set", json_object_get(p));
            }
            json_object_put(clock);
            json_object_put(p);
            json_object_put(read);
          }
        }
        return true;
      }
      return true;
    }
  }
  if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT)
    for (int i = 0; i < N; i++)
      if (controls[i].w > 0 &&
          SDL_PointInRect(&(SDL_Point){e->button.x, e->button.y},
                          &controls[i])) {
        click(i);
        return true;
      }
  return false;
}
bool SceneEditorSequencePanelControl(const char *name, SDL_Rect *out) {
  for (int i = 0; i < N; i++)
    if (!strcmp(name, names[i])) {
      *out = controls[i];
      return out->w > 0;
    }
  return false;
}
