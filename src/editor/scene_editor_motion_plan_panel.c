/* UI drafts only. Retained commands, validation and history remain shared. */
#include "scene_editor_motion_plan_panel.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_motion_plan.h"
#include "editor/scene_editor_render_authoring.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum {
  OPEN,
  TARGET,
  RELOAD,
  PREVIEW,
  APPLY,
  RESTORE,
  PREV,
  NEXT,
  ADD,
  REMOVE,
  STOPS,
  FIXED,
  START,
  SPEED,
  ACCEL,
  BRAKE,
  PROGRESS,
  POINT_SPEED,
  HOLD,
  ARRIVAL,
  COUNT
};
static const char *names[] = {
    "plan_open",     "plan_target",      "plan_reload", "plan_preview",
    "plan_apply",    "plan_restore",     "plan_prev",   "plan_next",
    "plan_add",      "plan_remove",      "plan_stops",  "plan_fixed",
    "plan_start",    "plan_speed",       "plan_accel",  "plan_brake",
    "plan_progress", "plan_point_speed", "plan_hold",   "plan_arrival"};
static struct {
  bool open, dirty;
  int editing, point, target_index;
  unsigned long long revision;
  char path[64], target[TIMELINE_ID_CAPACITY],
      targets[MOTION_BINDING_CAPACITY][TIMELINE_ID_CAPACITY];
  size_t target_count;
  MotionPath shape;
  MotionTimingScheduleRequest request;
  SDL_Rect controls[COUNT];
  bool enabled[COUNT];
  char labels[COUNT][160], draft[64], message[300], readback[4][180];
} ui = {.editing = -1};
void SceneEditorMotionPlanPanelReset(void) {
  if (ui.editing >= 0)
    SDL_StopTextInput();
  memset(&ui, 0, sizeof(ui));
  ui.editing = -1;
}
bool SceneEditorMotionPlanPanelOpen(void) { return ui.open; }
static void load(void) {
  ui.dirty = false;
  ui.editing = -1;
  SDL_StopTextInput();
  ui.point = 0;
  ui.revision = SceneEditorDocumentRevision();
  ui.message[0] = 0;
  memset(ui.readback, 0, sizeof(ui.readback));
  if (!SceneEditorMotionPlanRead(ui.target, &ui.request)) {
    ui.request = (MotionTimingScheduleRequest){
        .max_speed = 10, .acceleration = 10, .braking = 10, .count = 2};
    ui.request.points[1].position = 1;
  }
}
static void label(SDL_Renderer *r, const char *s, int x, int y) {
  ray_tracing_text_draw_utf8_at(
      r, ray_tracing_font_runtime_get_ui_regular(r, 12, 9), s, x, y,
      SceneEditorChromeShellResolvePalette().text_primary);
}
static int button(SDL_Renderer *r, int id, const char *s, SDL_Rect clip, int x,
                  int y, int w, bool enabled) {
  snprintf(ui.labels[id], sizeof(ui.labels[id]), "%s", s);
  ui.controls[id] = (SDL_Rect){x, y, w, 28};
  ui.enabled[id] = enabled;
  SceneEditorRenderButton(r, ui.controls[id], ui.labels[id], ui.editing == id,
                          enabled);
  if (y < clip.y || y + 28 > clip.y + clip.h)
    ui.controls[id] = (SDL_Rect){0};
  return y + 34;
}
static double *field(int id) {
  MotionTimingWaypoint *p = &ui.request.points[ui.point];
  switch (id) {
  case START:
    return &ui.request.start_time;
  case SPEED:
    return &ui.request.max_speed;
  case ACCEL:
    return &ui.request.acceleration;
  case BRAKE:
    return &ui.request.braking;
  case PROGRESS:
    return &p->position;
  case POINT_SPEED:
    return &p->speed;
  case HOLD:
    return &p->hold;
  case ARRIVAL:
    return &p->arrival;
  default:
    return NULL;
  }
}
int SceneEditorMotionPlanPanelDraw(SDL_Renderer *r, const MotionPaths *paths,
                                   const MotionPath *p, SDL_Rect clip, int x,
                                   int y, int w) {
  memset(ui.controls, 0, sizeof(ui.controls));
  if (strcmp(ui.path, p ? p->id : "")) {
    SceneEditorMotionPlanPanelReset();
    snprintf(ui.path, sizeof(ui.path), "%s", p ? p->id : "");
  }
  y = button(r, OPEN, ui.open ? "< Path shape" : "Plan movement limits >", clip,
             x, y, w, p != NULL);
  if (!ui.open || !p)
    return y;
  ui.shape = *p;
  ui.target_count = 0;
  for (size_t i = 0; i < paths->binding_count; ++i) {
    const MotionPathBinding *b = &paths->bindings[i];
    if (!b->enabled || strcmp(b->path_id, p->id))
      continue;
    snprintf(ui.targets[ui.target_count++], TIMELINE_ID_CAPACITY,
             b->target_id[0] ? "%s" : "object/%s",
             b->target_id[0] ? b->target_id : b->object_id);
  }
  if (!ui.target_count) {
    label(r, "Attach a follower to plan its timing.", x, y);
    return y + 30;
  }
  if (ui.target_index >= (int)ui.target_count)
    ui.target_index = 0;
  if (strcmp(ui.target, ui.targets[ui.target_index])) {
    snprintf(ui.target, sizeof(ui.target), "%s", ui.targets[ui.target_index]);
    load();
  }
  char text[160];
  snprintf(text, sizeof(text), "Follower: %s >", ui.target);
  y = button(r, TARGET, text, clip, x, y, w, true);
  label(r,
        MotionPlansRuntimeActive(ui.target)
            ? (ui.dirty ? "APPLIED + unapplied draft edits"
                        : "APPLIED: progress keys are preserved")
            : "Draft: limits not applied",
        x, y);
  y += 26;
  if (ui.revision != SceneEditorDocumentRevision()) {
    label(r, "Scene changed: Reload before Apply.", x, y);
    y += 26;
  }
  y = button(r, RELOAD, "Reload saved plan / default draft", clip, x, y, w,
             true);
  y = button(r, PREVIEW, "Preview feasibility", clip, x, y, w, true);
  y = button(r, APPLY, "Apply plan (one undo step)", clip, x, y, w,
             ui.revision == SceneEditorDocumentRevision());
  y = button(r, RESTORE, "Restore original progress keys", clip, x, y, w,
             MotionPlansRuntimeActive(ui.target));
  const char *labels[] = {"Start (local seconds)",   "Max speed (world/s)",
                          "Acceleration (world/s2)", "Braking (world/s2)",
                          "Progress (0 to 1)",       "Waypoint speed (world/s)",
                          "Hold (seconds)",          "Arrival (local seconds)"};
  for (int id = START; id <= BRAKE; ++id) {
    if (ui.editing == id)
      snprintf(text, sizeof(text), "%s: %s_", labels[id - START], ui.draft);
    else
      snprintf(text, sizeof(text), "%s: %.8g", labels[id - START], *field(id));
    y = button(r, id, text, clip, x, y, w, true);
  }
  snprintf(ui.readback[3], sizeof(ui.readback[3]), "Waypoint %d / %zu",
           ui.point + 1, ui.request.count);
  label(r, ui.readback[3], x, y);
  y += 26;
  int half = (w - 6) / 2;
  button(r, PREV, "< Waypoint", clip, x, y, half, ui.point > 0);
  y = button(r, NEXT, "Waypoint >", clip, x + half + 6, y, half,
             ui.point + 1 < (int)ui.request.count);
  button(r, ADD, "Insert after", clip, x, y, half,
         ui.request.count < MOTION_TIMING_WAYPOINT_CAPACITY);
  y = button(r, REMOVE, "Remove", clip, x + half + 6, y, half,
             ui.request.count > 2);
  y = button(r, STOPS, "Draft stops at every route point", clip, x, y, w, true);
  for (int id = PROGRESS; id <= ARRIVAL; ++id) {
    if (ui.editing == id)
      snprintf(text, sizeof(text), "%s: %s_", labels[id - START], ui.draft);
    else
      snprintf(text, sizeof(text), "%s: %.8g", labels[id - START], *field(id));
    y = button(r, id, text, clip, x, y, w, true);
  }
  y = button(r, FIXED,
             ui.request.points[ui.point].fixed_arrival
                 ? "Arrival: fixed"
                 : "Arrival: earliest feasible",
             clip, x, y, w, true);
  label(r, "First / last speed must be zero.", x, y);
  y += 26;
  label(r, "Restore before editing route or clock.", x, y);
  y += 26;
  for (int i = 0; i < 3; ++i)
    if (ui.readback[i][0]) {
      label(r, ui.readback[i], x, y);
      y += 26;
    }
  /* Wrap feedback into persistent per-row storage; queued text owns no stack.
   */
  static char lines[8][72];
  size_t length = strlen(ui.message), offset = 0;
  int columns = (w - 8) / 7;
  if (columns < 12)
    columns = 12;
  if (columns > 70)
    columns = 70;
  for (int i = 0; i < 8 && offset < length; ++i) {
    size_t count = length - offset;
    if (count > (size_t)columns)
      count = columns;
    memcpy(lines[i], ui.message + offset, count);
    lines[i][count] = 0;
    label(r, lines[i], x, y);
    y += 23;
    offset += count;
  }
  return y;
}
static void preview(void) {
  MotionRouteSchedule p;
  ui.readback[0][0] = ui.readback[1][0] = ui.readback[2][0] = 0;
  if (SceneEditorMotionPlanPreview(ui.target, &ui.request, &p, ui.message,
                                   sizeof(ui.message))) {
    snprintf(ui.message, sizeof(ui.message),
             "Feasible. Preview does not apply. Extend timeline range first if "
             "End is outside the visible range.");
    snprintf(ui.readback[0], 180, "End %.8g s | Effective speed %.6g",
             p.timeline.end_time, p.timeline.request.max_speed);
    snprintf(ui.readback[1], 180, "Effective accel %.6g / brake %.6g",
             p.timeline.request.acceleration, p.timeline.request.braking);
    snprintf(ui.readback[2], 180, "Selected: arrive %.6g / depart %.6g",
             p.timeline.arrivals[ui.point], p.timeline.departures[ui.point]);
  }
}
bool SceneEditorMotionPlanPanelEvent(SDL_Event *e) {
  if (ui.editing >= 0 && (e->type == SDL_TEXTINPUT || e->type == SDL_KEYDOWN)) {
    if (e->type == SDL_TEXTINPUT) {
      if (strlen(ui.draft) + strlen(e->text.text) < sizeof(ui.draft))
        strcat(ui.draft, e->text.text);
      return true;
    }
    SDL_Keycode k = e->key.keysym.sym;
    if (k == SDLK_ESCAPE) {
      ui.editing = -1;
      SDL_StopTextInput();
    } else if (k == SDLK_BACKSPACE) {
      size_t n = strlen(ui.draft);
      if (n)
        ui.draft[n - 1] = 0;
    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      char *end;
      double v = strtod(ui.draft, &end);
      if (end != ui.draft && !*end && isfinite(v)) {
        *field(ui.editing) = v;
        ui.dirty = true;
        ui.editing = -1;
        SDL_StopTextInput();
        memset(ui.readback, 0, sizeof(ui.readback));
        snprintf(ui.message, sizeof(ui.message),
                 "Draft changed. Preview or Apply to validate.");
      }
    }
    return true;
  }
  if (e->type != SDL_MOUSEBUTTONDOWN || e->button.button != SDL_BUTTON_LEFT)
    return false;
  for (int id = 0; id < COUNT; ++id) {
    if (ui.controls[id].w <= 0 ||
        !SDL_PointInRect(&(SDL_Point){e->button.x, e->button.y},
                         &ui.controls[id]))
      continue;
    if (!ui.enabled[id])
      return true;
    ui.editing = -1;
    SDL_StopTextInput();
    if (id == PREV || id == NEXT || id == FIXED || id == ADD || id == REMOVE ||
        id == STOPS) {
      memset(ui.readback, 0, sizeof(ui.readback));
      if (id != PREV && id != NEXT) {
        ui.dirty = true;
        snprintf(ui.message, sizeof(ui.message),
                 "Draft changed. Preview or Apply to validate.");
      }
    }
    if (id == OPEN)
      ui.open = !ui.open;
    else if (id == TARGET) {
      ui.target_index = (ui.target_index + 1) % (int)ui.target_count;
      snprintf(ui.target, sizeof(ui.target), "%s", ui.targets[ui.target_index]);
      load();
    } else if (id == RELOAD)
      load();
    else if (id == PREVIEW)
      preview();
    else if (id == APPLY) {
      if (SceneEditorMotionPlanApply(ui.target, &ui.request, ui.revision,
                                     ui.message, sizeof(ui.message))) {
        ui.revision = SceneEditorDocumentRevision();
        ui.dirty = false;
      }
    } else if (id == RESTORE) {
      if (SceneEditorMotionPlanRestore(ui.target, SceneEditorDocumentRevision(),
                                       ui.message, sizeof(ui.message))) {
        ui.revision = SceneEditorDocumentRevision();
        ui.dirty = false;
      }
    } else if (id == PREV)
      --ui.point;
    else if (id == NEXT)
      ++ui.point;
    else if (id == FIXED)
      ui.request.points[ui.point].fixed_arrival =
          !ui.request.points[ui.point].fixed_arrival;
    else if (id == ADD) {
      size_t at = (size_t)ui.point;
      MotionTimingWaypoint p = ui.request.points[at];
      if (at + 1 < ui.request.count)
        p.position = (p.position + ui.request.points[at + 1].position) / 2;
      p.fixed_arrival = false;
      p.hold = 0;
      memmove(&ui.request.points[at + 2], &ui.request.points[at + 1],
              (ui.request.count - at - 1) * sizeof(p));
      ui.request.points[at + 1] = p;
      ++ui.request.count;
      ++ui.point;
    } else if (id == REMOVE) {
      memmove(&ui.request.points[ui.point], &ui.request.points[ui.point + 1],
              (ui.request.count - ui.point - 1) * sizeof(MotionTimingWaypoint));
      --ui.request.count;
      if (ui.point >= (int)ui.request.count)
        --ui.point;
    } else if (id == STOPS) {
      MotionRouteGeometry *g = malloc(sizeof(*g));
      if (g &&
          MotionRouteGeometryBuild(&ui.shape, SceneEditorDocumentWorldScale(),
                                   g) == MOTION_ROUTE_OK &&
          g->length > 0) {
        ui.request.count = ui.shape.count;
        for (size_t i = 0; i < ui.request.count; ++i)
          ui.request.points[i] = (MotionTimingWaypoint){
              .position = g->point_distances[i] / g->length};
        ui.point = 0;
        snprintf(ui.message, sizeof(ui.message),
                 "Draft replaced with explicit rest at every route point. "
                 "Preview before Apply.");
      }
      free(g);
    } else if (id >= START) {
      ui.editing = id;
      ui.draft[0] = 0;
      SDL_StartTextInput();
    }
    return true;
  }
  return false;
}
bool SceneEditorMotionPlanPanelControl(const char *name, SDL_Rect *out) {
  for (int i = 0; i < COUNT; ++i)
    if (!strcmp(name, names[i])) {
      *out = ui.controls[i];
      return out->w > 0;
    }
  return false;
}
