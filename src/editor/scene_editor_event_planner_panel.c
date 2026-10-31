/* Explicit, bounded Events tab. Numeric drafts use existing shared fields. */
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_shared_fields.h"
#include "editor/scene_editor_shared_text.h"
#include "editor/scene_editor_shared_ui.h"
#include "scene_editor_event_planner_internal.h"
#include <errno.h>
#include <stdlib.h>
#define P event_planner
enum {
  TARGET_PAGE,
  VIEW_PAGE,
  REVIEW_PAGE,
  RELOAD,
  HOLD,
  BODY,
  SECOND,
  KIND,
  PICK_FOCUS,
  PICK_PIVOT,
  PIVOT,
  RADIUS,
  FOV,
  ASPECT,
  MARGIN,
  OFFSET_X,
  OFFSET_Y,
  AZIMUTH,
  ELEVATION,
  ANGLE,
  POSITION,
  AIM,
  GENERATE,
  CANDIDATE,
  APPLY,
  LEAD,
  EVENT,
  MIDDLE,
  RESUME,
  LEAD_KIND,
  LEAD_SCALE,
  N
};
static SDL_Rect controls[N];
static bool available[N];
static const char *names[] = {
    "event_target_page", "event_view_page",   "event_review_page",
    "event_reload",      "event_hold",        "event_body",
    "event_second",      "event_kind",        "event_pick_focus",
    "event_pick_pivot",  "event_pivot_focus", "event_radius",
    "event_fov",         "event_aspect",      "event_margin",
    "event_offset_x",    "event_offset_y",    "event_azimuth",
    "event_elevation",   "event_angle",       "event_resume_position",
    "event_resume_aim",  "event_generate",    "event_candidate",
    "event_apply",       "event_lead",        "event_entry",
    "event_middle",      "event_resume",      "event_lead_kind",
    "event_lead_scale"};
void EventPlannerClearControls(void) {
  memset(controls, 0, sizeof(controls));
  memset(available, 0, sizeof(available));
}
bool SceneEditorEventPlannerControl(const char *name, SDL_Rect *out) {
  if (!SceneEditorEventPlannerVisible())
    return false;
  for (int i = 0; i < N; i++)
    if (!strcmp(names[i], name)) {
      *out = controls[i];
      return out->w > 0;
    }
  return false;
}
static void text(SDL_Renderer *r, int row, const char *s, int x, int y) {
  snprintf(P.labels[row], 192, "%s", s);
  scene_editor_shared_label_at(
      r, P.labels[row], x, y,
      SceneEditorChromeShellResolvePalette().text_primary, 11);
}
static int button(SDL_Renderer *r, SDL_Rect pane, int y, int id, const char *s,
                  bool selected, bool enabled) {
  int height = pane.h < 460 ? 21 : 27;
  SDL_Rect b = {pane.x + 10, y, pane.w - 20, height};
  if (b.y >= pane.y && b.y + b.h <= pane.y + pane.h) {
    controls[id] = b;
    available[id] = enabled;
    snprintf(P.labels[id], 192, "%s", s);
    SceneEditorSharedUiPanelButton(r, 193, id, b, P.labels[id], selected,
                                   enabled);
  }
  return y + height + 4;
}
static double field(int i) {
  json_object *f = pg(P.intent, "framing");
  if (i == RADIUS)
    return P.feature_radius;
  if (i == LEAD_SCALE)
    return pn(pg(P.intent, "lead"), "distance_scale");
  if (i == OFFSET_X || i == OFFSET_Y)
    return json_object_get_double(pa(pg(f, "offset"), i - OFFSET_X));
  if (i == ANGLE)
    return pn(pg(P.intent, "orbit"), "angle") * 180 / acos(-1.);
  const char *key = i == FOV       ? "fov_y"
                    : i == ASPECT  ? "aspect"
                    : i == MARGIN  ? "margin"
                    : i == AZIMUTH ? "azimuth"
                                   : "elevation";
  double v = pn(f, key);
  return i == AZIMUTH || i == ELEVATION ? v * 180 / acos(-1.) : v;
}
static bool set_field(int i, double v) {
  if (!isfinite(v))
    return false;
  if (i == LEAD_SCALE) {
    if (v <= 1 || v > 100)
      return false;
    pd(pg(P.intent, "lead"), "distance_scale", v);
  } else if (i == RADIUS) {
    if (v <= 0 || v > 1e6)
      return false;
    P.feature_radius = v;
    json_object *f = pg(P.intent, "focus"), *p = pg(P.intent, "pivot");
    if (!strcmp(ps(f, "kind"), "body_point") ||
        !strcmp(ps(f, "kind"), "body_side") ||
        !strcmp(ps(f, "kind"), "world_point"))
      pd(f, "radius", v);
    if (!strcmp(ps(p, "kind"), "body_point"))
      pd(p, "radius", v);
  } else if (i == OFFSET_X || i == OFFSET_Y) {
    if (fabs(v) > .9)
      return false;
    json_object_array_put_idx(pg(pg(P.intent, "framing"), "offset"),
                              i - OFFSET_X, json_object_new_double(v));
  } else if (i == ANGLE) {
    if (fabs(v) < 1e-6 || fabs(v) > 1440)
      return false;
    pd(pg(P.intent, "orbit"), "angle", v * acos(-1.) / 180);
  } else {
    const char *k = i == FOV       ? "fov_y"
                    : i == ASPECT  ? "aspect"
                    : i == MARGIN  ? "margin"
                    : i == AZIMUTH ? "azimuth"
                                   : "elevation";
    if ((i == FOV && (v < 5 || v > 120)) ||
        (i == ASPECT && (v < .25 || v > 4)) ||
        (i == MARGIN && (v < .02 || v > .5)))
      return false;
    pd(pg(P.intent, "framing"), k,
       (i == AZIMUTH || i == ELEVATION) ? v * acos(-1.) / 180 : v);
  }
  EventPlannerInvalidate();
  return true;
}
static void number(SDL_Renderer *r, SDL_Rect pane, int *y, int i,
                   const char *name) {
  char s[192];
  if (P.editing == i)
    snprintf(s, sizeof(s), "%s: %s_", name, P.draft);
  else
    snprintf(s, sizeof(s), "%s: %.5g", name, field(i));
  *y = button(r, pane, *y, i, s, P.editing == i, true);
}
static double seconds(json_object *r) {
  double den = pn(r, "denominator");
  return den ? pn(r, "numerator") / den : 0;
}
static void context(SDL_Renderer *r, SDL_Rect pane) {
  SDL_Rect prior;
  SDL_bool clipped = SDL_RenderIsClipEnabled(r);
  SDL_RenderGetClipRect(r, &prior);
  SDL_RenderSetClipRect(r, &pane);
  RayTracingThemePalette palette = SceneEditorChromeShellResolvePalette();
  SDL_SetRenderDrawColor(r, palette.panel_fill.r, palette.panel_fill.g,
                         palette.panel_fill.b, palette.panel_fill.a);
  SDL_RenderFillRect(r, &pane);
  json_object *read = SceneEditorSequenceRead(), *c = pg(read, "clocks");
  int x = pane.x + 10, y = pane.y + 12;
  int line = pane.h < 460 ? 20 : 24;
  char s[192];
  text(r, 50, "Event timeline", x, y);
  y += 28;
  const char *keys[] = {"presentation_time", "simulation_time",
                        "camera_motion_time", "inspection_time"},
             *titles[] = {"Presentation", "Simulation", "Camera motion",
                          "Inspection"};
  for (int i = 0; i < 4; i++) {
    snprintf(s, sizeof(s), "%s: %s%.4f s", titles[i],
             pg(c, keys[i]) ? "" : "unbound ", seconds(pg(c, keys[i])));
    text(r, 51 + i, s, x, y);
    y += line;
  }
  snprintf(s, sizeof(s), "View: %.29s", ps(c, "camera_id"));
  text(r, 55, s, x, y);
  y += line;
  json_object *h = pg(P.intent, "hold");
  snprintf(s, sizeof(s), "Event tick: %lld",
           (long long)json_object_get_int64(pg(h, "source_tick")));
  text(r, 56, s, x, y);
  y += line;
  TimelineSample sample;
  RayEvaluatedSceneServiceResult v;
  const char *phase = "No sequence";
  if (SceneEditorTimelineCurrentSample(&sample) &&
      RayEvaluatedSceneCaptureSample(sample, &v) && v.snapshot.has_sequence) {
    uint64_t local = v.snapshot.frame.local_frame,
             start = json_object_get_uint64(pg(h, "start_frame")),
             end = start + json_object_get_uint64(pg(h, "frame_count"));
    phase = local < start ? "Lead: simulation advancing"
            : local < end ? "Hold: frozen simulation"
                          : "Resume: simulation advancing";
  }
  text(r, 57, phase, x, y);
  y += 30;
  json_object *seq = pg(read, "sequence"), *range = pg(seq, "range");
  double count = pn(range, "frame_count");
  if (count > 0) {
    double start = pn(h, "start_frame"), length = pn(h, "frame_count");
    SDL_Rect bar = {x, y, pane.w - 20, 14};
    SDL_SetRenderDrawColor(r, 74, 113, 145, 255);
    SDL_RenderFillRect(r, &bar);
    SDL_Rect hold = {bar.x + (int)(bar.w * start / count), bar.y,
                     (int)(bar.w * length / count), bar.h};
    SDL_SetRenderDrawColor(r, 214, 163, 70, 255);
    SDL_RenderFillRect(r, &hold);
    y += 23;
  }
  text(r, 58, "Blue: play   Gold: exact Hold", x, y);
  y += 28;
  const char *seeknames[] = {"Lead start", "Event / Hold entry",
                             "Inspection midpoint", "Resume entry"};
  for (int i = 0; i < 4; i++)
    y = button(r, pane, y, LEAD + i, seeknames[i], false, P.intent != NULL);
  /* Fixed per-line buffers survive queued text drawing. */
  if (P.feedback[0]) {
    size_t width = (pane.w - 20) / 7;
    if (width > 70)
      width = 70;
    if (width < 8)
      width = 8;
    size_t len = strlen(P.feedback);
    for (int i = 0; i < 4 && i * width < len; i++) {
      char line[96];
      snprintf(line, sizeof(line), "%.*s", (int)width, P.feedback + i * width);
      text(r, 64 + i, line, x, y);
      y += 20;
    }
  }
  json_object_put(read);
  SDL_RenderSetClipRect(r, clipped ? &prior : NULL);
}
void SceneEditorEventPlannerDraw(SceneEditor *editor,
                                 const SceneEditorPaneLayout *layout, int y) {
  if (!SceneEditorEventPlannerVisible()) {
    EventPlannerClearControls();
    return;
  }
  EventPlannerSync();
  EventPlannerClearControls();
  SDL_Renderer *r = editor->renderer;
  SDL_Rect pane = layout->left_content_rect;
  int x = pane.x + 10, w = pane.w - 20;
  for (int i = 0; i < 3; i++) {
    SDL_Rect b = {x + i * (w + 4) / 3, y, (w - 8) / 3, 27};
    controls[i] = b;
    available[i] = true;
    SceneEditorSharedUiPanelButton(r, 193, i, b,
                                   i == 0   ? "Target"
                                   : i == 1 ? "View"
                                            : "Review",
                                   P.page == i, true);
  }
  y += 35;
  if (!P.intent) {
    text(r, 68, "Open a recorded rigid scene first.", x, y);
    context(r, layout->right_content_rect);
    return;
  }
  if (P.page == 0) {
    y = button(r, pane, y, RELOAD, "Reload saved / fresh event", false, true);
    char s[192];
    snprintf(s, sizeof(s), "Next Hold | tick %lld",
             (long long)json_object_get_int64(
                 pg(pg(P.intent, "hold"), "source_tick")));
    y = button(r, pane, y, HOLD, s, false, true);
    json_object *f = pg(P.intent, "focus"), *ids = pg(f, "targets");
    snprintf(s, sizeof(s), "Focus: %s", ps(f, "kind"));
    y = button(r, pane, y, KIND, s, false, true);
    for (int i = 0; i < 2; i++) {
      const char *id = json_object_get_string(pa(ids, i));
      snprintf(s, sizeof(s), "%s: %.29s", i ? "Second" : "Object",
               id ? id : "none");
      y = button(r, pane, y, BODY + i, s, false,
                 i == 0 || !strncmp(ps(f, "kind"), "pair_", 5));
    }
    y = button(r, pane, y, PICK_FOCUS,
               P.armed == 1 ? "Click recorded surface / Esc"
                            : "Pick focus on mesh",
               P.armed == 1, true);
    y = button(r, pane, y, PICK_PIVOT,
               P.armed == 2 ? "Click pivot surface / Esc"
                            : "Pick independent pivot",
               P.armed == 2, true);
    y = button(r, pane, y, PIVOT, "Use focus as orbit pivot", false, true);
    number(r, pane, &y, RADIUS, "Feature radius (m)");
    bool dolly = !strcmp(ps(pg(P.intent, "lead"), "kind"), "dolly");
    y = button(r, pane, y, LEAD_KIND,
               dolly ? "Lead: moving dolly" : "Lead: stationary", dolly, true);
    if (dolly)
      number(r, pane, &y, LEAD_SCALE, "Dolly distance scale");

  } else if (P.page == 1) {
    number(r, pane, &y, FOV, "Vertical FOV");
    number(r, pane, &y, ASPECT, "Minimum aspect");
    number(r, pane, &y, MARGIN, "Framing margin");
    number(r, pane, &y, OFFSET_X, "Screen offset X");
    number(r, pane, &y, OFFSET_Y, "Screen offset Y");
    number(r, pane, &y, AZIMUTH, "View azimuth deg");
    number(r, pane, &y, ELEVATION, "View elevation deg");
    number(r, pane, &y, ANGLE, "Orbit degrees");
    char s[192];
    snprintf(s, sizeof(s), "Resume position: %s",
             ps(pg(P.intent, "resume"), "position"));
    y = button(r, pane, y, POSITION, s, false, true);
    snprintf(s, sizeof(s), "Resume aim: %s", ps(pg(P.intent, "resume"), "aim"));
    y = button(r, pane, y, AIM, s, false, true);
  } else {
    y = button(r, pane, y, GENERATE, "Generate 3 event views", false, true);
    json_object *list = pg(P.proposals, "candidates"),
                *c = pa(list, P.candidate);
    char s[192];
    snprintf(s, sizeof(s), "Candidate %d / 3 - next", P.candidate + 1);
    y = button(r, pane, y, CANDIDATE, s, false, c != NULL);
    snprintf(s, sizeof(s), "%s | blocked samples: %d",
             json_object_get_boolean(pg(c, "usable")) ? "Clear entry"
                                                      : "Entry unavailable",
             json_object_get_int(pg(c, "possible_occlusion_samples")));
    text(r, 68, s, x, y);
    y += 27;
    y = button(r, pane, y, APPLY, "Apply reviewed event camera", false,
               c && json_object_get_boolean(pg(c, "usable")));
    text(r, 39, "Apply retains intent + pose; Undo restores.", x, y);
  }
  context(r, layout->right_content_rect);
}
static void cancel_field(void) {
  if (P.editing >= 0) {
    scene_editor_shared_field_end(P.draft);
    SDL_StopTextInput();
    P.editing = -1;
  }
}
bool SceneEditorEventPlannerEvent(SDL_Event *e) {
  if (!e || !SceneEditorEventPlannerVisible())
    return false;
  if (e->type == SDL_WINDOWEVENT &&
      (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
       e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
    cancel_field();
    P.armed = 0;
    P.viewport_valid = false;
    EventPlannerClearControls();
    return false;
  }
  EventPlannerSync();
  if (P.revision != SceneEditorDocumentRevision()) {
    cancel_field();
    P.armed = 0;
  }
  if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE &&
      (P.armed || P.editing >= 0)) {
    cancel_field();
    P.armed = 0;
    return true;
  }
  if (P.editing >= 0) {
    if (scene_editor_shared_field_event(P.draft, sizeof(P.draft), e))
      return true;
    if (e->type == SDL_TEXTINPUT) {
      if (strlen(P.draft) + strlen(e->text.text) < sizeof(P.draft))
        strcat(P.draft, e->text.text);
      return true;
    }
    if (e->type == SDL_KEYDOWN) {
      if (e->key.keysym.sym == SDLK_RETURN ||
          e->key.keysym.sym == SDLK_KP_ENTER) {
        char *end;
        errno = 0;
        double v = strtod(P.draft, &end);
        if (!errno && end != P.draft && !*end && set_field(P.editing, v)) {
          cancel_field();
          P.feedback[0] = 0;
        } else
          snprintf(P.feedback, sizeof(P.feedback),
                   "Invalid value. Esc cancels.");
      } else if (e->key.keysym.sym == SDLK_BACKSPACE) {
        size_t n = strlen(P.draft);
        if (n)
          P.draft[n - 1] = 0;
      }
      return true;
    }
  }
  if (e->type != SDL_MOUSEBUTTONDOWN || e->button.button != SDL_BUTTON_LEFT)
    return false;
  SDL_Point at = {e->button.x, e->button.y};
  for (int i = 0; i < N; i++)
    if (controls[i].w && SDL_PointInRect(&at, &controls[i])) {
      if (!available[i])
        return true;
      cancel_field();
      P.armed = 0;
      SceneEditorTimelinePause();
      SceneEditorTimelineReleaseFocus();
      if (i < 3) {
        P.page = i;
        EventPlannerClearControls();
        return true;
      }
      if (i == RELOAD) {
        if (P.intent)
          json_object_put(P.intent);
        P.intent = NULL;
        SceneEditorEventPlannerOpen(true);
        return true;
      }
      if (P.revision != SceneEditorDocumentRevision()) {
        snprintf(P.feedback, sizeof(P.feedback),
                 "Scene changed. Reload event first.");
        return true;
      }
      if (i == HOLD)
        EventPlannerHold(1);
      else if (i == BODY || i == SECOND)
        EventPlannerTarget(i - BODY);
      else if (i == KIND) {
        json_object *f = pg(P.intent, "focus");
        const char *k = ps(f, "kind");
        const char *next = !strcmp(k, "body_com")   ? "pair_com"
                           : !strcmp(k, "pair_com") ? "pair_bounds"
                                                    : "body_com";
        pt(f, "kind", next);
        EventPlannerTarget(-1);
      } else if (i == PICK_FOCUS || i == PICK_PIVOT) {
        if (EventPlannerSeek(1))
          P.armed = i == PICK_FOCUS ? 1 : 2;
        snprintf(P.feedback, sizeof(P.feedback),
                 "Pick a recorded mesh surface. Alt-drag navigates.");
      } else if (i == PIVOT) {
        json_object_object_add(P.intent, "pivot", pc(pg(P.intent, "focus")));
        EventPlannerInvalidate();
      } else if (i == LEAD_KIND) {
        json_object *lead = json_object_new_object();
        bool dolly = strcmp(ps(pg(P.intent, "lead"), "kind"), "dolly") != 0;
        pi(lead, "start_frame",
           json_object_get_int64(pg(pg(P.intent, "lead"), "start_frame")));
        pt(lead, "kind", dolly ? "dolly" : "stationary");
        if (dolly) {
          pd(lead, "distance_scale", 1.5);
          pd(lead, "max_speed", 30);
          pd(lead, "acceleration", 100);
          pd(lead, "braking", 100);
        }
        json_object_object_add(P.intent, "lead", lead);
        EventPlannerInvalidate();
      } else if ((i >= RADIUS && i <= ANGLE) || i == LEAD_SCALE) {
        P.editing = i;
        snprintf(P.draft, sizeof(P.draft), "%.9g", field(i));
        scene_editor_shared_field_begin(P.draft, sizeof(P.draft), 0);
        SDL_StartTextInput();
      } else if (i == POSITION || i == AIM) {
        json_object *r = pg(P.intent, "resume");
        const char *k = i == POSITION ? "position" : "aim";
        pt(r, k,
           i == POSITION
               ? (!strcmp(ps(r, k), "endpoint") ? "entry" : "endpoint")
               : (!strcmp(ps(r, k), "track") ? "event" : "track"));
        EventPlannerInvalidate();
      } else if (i == GENERATE)
        EventPlannerGenerate();
      else if (i == CANDIDATE) {
        if (P.proposals)
          P.candidate = (P.candidate + 1) % 3;
      } else if (i == APPLY)
        EventPlannerApply();
      else if (i >= LEAD)
        EventPlannerSeek(i - LEAD);
      return true;
    }
  if (P.armed && !(SDL_GetModState() & (KMOD_ALT | KMOD_CTRL | KMOD_GUI)) &&
      P.viewport_valid && SDL_PointInRect(&at, &P.projector.viewport)) {
    json_object *hit =
        EventPlannerQuery(at.x, at.y, P.feedback, sizeof(P.feedback));
    if (hit) {
      json_object_object_add(P.intent, P.armed == 1 ? "focus" : "pivot",
                             pc(pg(hit, "feature")));
      if (P.picked)
        json_object_put(P.picked);
      P.picked = hit;
      EventPlannerInvalidate();
      P.armed = 0;
      snprintf(P.feedback, sizeof(P.feedback),
               "Recorded surface picked; review event views.");
    }
    return true;
  }
  return false;
}
