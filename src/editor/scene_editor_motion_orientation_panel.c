#include "app/evaluated_camera_orientation.h"
#include "editor/scene_editor_render_authoring.h"
/* Follower-local up/roll controls. Drag previews the handle; release is one
 * undo command. */
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_timeline.h"
#include "import/runtime_scene_bridge.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "scene_editor_motion_orientation_panel.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { START, END_ENABLE, END, RESET, CAMERA_MODE, COUNT };
static struct {
  char target[TIMELINE_ID_CAPACITY], draft[80], labels[COUNT][128];
  SDL_Rect controls[COUNT], viewport;
  int editing, drag;
  bool visible, dragging;
  unsigned long long revision;
  MotionPathBinding binding;
  double cx[2], cy[2], ux[2], uy[2], rx[2], ry[2], last, delta;
  bool projected[2];
} panel = {.editing = -1};
void MotionOrientationPanelBeginFrame(void) {
  memset(panel.controls, 0, sizeof(panel.controls));
  panel.visible = false;
}
static bool matches(const MotionPathBinding *b, const char *target) {
  return b->target_id[0] ? !strcmp(b->target_id, target)
                         : !strncmp(target, "object/", 7) &&
                               !strcmp(b->object_id, target + 7);
}
static MotionPathBinding *binding(MotionPaths *p) {
  for (size_t i = 0; i < p->binding_count; ++i)
    if (p->bindings[i].enabled && matches(&p->bindings[i], panel.target))
      return &p->bindings[i];
  return NULL;
}
void MotionOrientationPanelReset(void) {
  if (panel.editing >= 0)
    SDL_StopTextInput();
  if (panel.dragging)
    SDL_CaptureMouse(SDL_FALSE);
  memset(&panel, 0, sizeof(panel));
  panel.editing = -1;
}
static void button(SDL_Renderer *r, int i, SDL_Rect clip, int x, int *y, int w,
                   const char *text) {
  snprintf(panel.labels[i], sizeof(panel.labels[i]), "%s", text);
  panel.controls[i] = (SDL_Rect){x, *y, w, 26};
  SceneEditorRenderButton(r, panel.controls[i], panel.labels[i], false, true);
  if (*y < clip.y || *y + 26 > clip.y + clip.h)
    panel.controls[i] = (SDL_Rect){0};
  *y += 30;
}
int MotionOrientationPanelDraw(SDL_Renderer *r, const MotionPaths *paths,
                               const char *target, SDL_Rect clip, int x, int y,
                               int w) {
  if (strcmp(panel.target, target)) {
    MotionOrientationPanelReset();
    snprintf(panel.target, sizeof(panel.target), "%s", target);
  }
  memset(panel.controls, 0, sizeof(panel.controls));
  panel.visible = false;
  const MotionPathBinding *b = NULL;
  for (size_t i = 0; i < paths->binding_count; ++i)
    if (paths->bindings[i].enabled && matches(&paths->bindings[i], target))
      b = &paths->bindings[i];
  if (!b)
    return y;
  bool camera = !strcmp(target, "camera/main");
  char text[128];
  if (camera) {
    const char *modes[] = {"Authored / legacy focus", "Follow route",
                           "Stable focus target"};
    snprintf(text, sizeof(text), "Aim: %s", modes[b->camera_orientation]);
    button(r, CAMERA_MODE, clip, x, &y, w, text);
  }
  if ((camera && !b->camera_orientation) || (!camera && !b->follow_direction))
    return y;
  panel.visible = true;
  if (!panel.dragging)
    panel.binding = *b;
  if (camera && b->camera_orientation == 2) {
    RuntimeSceneBridge3DScaffoldState scaffold = {0};
    runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
    RayEvaluatedSceneSnapshot evaluated;
    bool fallback = SceneEditorTimelineCopyEvaluated(&evaluated) &&
                    evaluated.camera.orientation_fallback;
    if (!scaffold.has_camera_focus_target || fallback) {
      ray_tracing_text_draw_utf8_at(
          r, ray_tracing_font_runtime_get_ui_regular(r, 11, 8),
          scaffold.has_camera_focus_target
              ? "Aim undefined here: retained stable heading."
              : "No focus target: using route direction.",
          x, y, (SDL_Color){255, 200, 120, 255});
      y += 24;
    }
  }

  if (panel.editing == START)
    snprintf(text, sizeof(text), "Start roll: %s_", panel.draft);
  else
    snprintf(text, sizeof(text), "Start up / roll: %.3g deg", b->start_roll);
  button(r, START, clip, x, &y, w, text);
  button(r, END_ENABLE, clip, x, &y, w,
         b->end_roll_enabled ? "End roll: enabled" : "End roll: automatic");
  if (b->end_roll_enabled) {
    if (panel.editing == END)
      snprintf(text, sizeof(text), "End roll: %s_", panel.draft);
    else
      snprintf(text, sizeof(text), "End roll: %.3g deg", b->end_roll);
    button(r, END, clip, x, &y, w, text);
  }
  button(r, RESET, clip, x, &y, w, "Reset up / roll (+Z reference)");
  return y;
}
static bool apply(double value, int field, char *message, size_t size) {
  MotionPaths paths;
  if (!SceneEditorMotionPathsRead(&paths))
    return false;
  MotionPathBinding *b = binding(&paths);
  if (!b)
    return false;
  if (field == START)
    b->start_roll = value;
  else {
    b->end_roll = value;
    b->end_roll_enabled = true;
  }
  return SceneEditorMotionPathsSet(&paths, panel.revision, message, size);
}
static bool angle(int k, int x, int y, double *out) {
  double a = panel.ux[k], b = panel.rx[k], c = panel.uy[k], d = panel.ry[k],
         det = a * d - b * c;
  if (fabs(det) < 1e-3)
    return false;
  double dx = x - panel.cx[k], dy = y - panel.cy[k];
  *out = atan2(-(a * dy - c * dx) / det, (d * dx - b * dy) / det);
  return true;
}
bool MotionOrientationPanelEvent(SDL_Event *e, char *message, size_t size) {
  if ((panel.dragging || panel.editing >= 0) &&
      panel.revision != SceneEditorDocumentRevision()) {
    MotionOrientationPanelReset();
    snprintf(message, size, "Orientation edit cancelled: scene changed.");
    return false;
  }
  if (e->type == SDL_WINDOWEVENT &&
      e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
    MotionOrientationPanelReset();
    return false;
  }
  if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE &&
      (panel.dragging || panel.editing >= 0)) {
    MotionOrientationPanelReset();
    SDL_StopTextInput();
    return true;
  }
  if (panel.editing >= 0) {
    if (e->type == SDL_TEXTINPUT) {
      if (strlen(panel.draft) + strlen(e->text.text) < sizeof(panel.draft))
        strcat(panel.draft, e->text.text);
      return true;
    }
    if (e->type == SDL_KEYDOWN) {
      if (e->key.keysym.sym == SDLK_BACKSPACE) {
        size_t n = strlen(panel.draft);
        if (n)
          panel.draft[n - 1] = 0;
      }
      if (e->key.keysym.sym == SDLK_RETURN) {
        char *end;
        double value = strtod(panel.draft, &end);
        if (end != panel.draft && !*end && isfinite(value) &&
            fabs(value) <= 36000 &&
            apply(value, panel.editing, message, size)) {
          panel.editing = -1;
          SDL_StopTextInput();
        } else
          snprintf(message, size,
                   "Enter roll degrees between -36000 and 36000.");
      }
      return true;
    }
  }
  if (panel.dragging) {
    if (SDL_GetModState() & (KMOD_ALT | KMOD_CTRL | KMOD_GUI)) {
      MotionOrientationPanelReset();
      return false;
    }
    if (e->type == SDL_MOUSEMOTION) {
      double a;
      if (angle(panel.drag, e->motion.x, e->motion.y, &a)) {
        double step = remainder(a - panel.last, 6.283185307179586);
        panel.delta += step;
        panel.last = a;
      }
      return true;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
      double base = panel.drag && panel.binding.end_roll_enabled
                        ? panel.binding.end_roll
                        : panel.binding.start_roll;
      double value = base + panel.delta * 57.29577951308232;
      if (fabs(value) <= 36000)
        apply(value, panel.drag ? END : START, message, size);
      panel.dragging = false;
      SDL_CaptureMouse(SDL_FALSE);
      return true;
    }
  }
  if (e->type != SDL_MOUSEBUTTONDOWN || e->button.button != SDL_BUTTON_LEFT ||
      (SDL_GetModState() & (KMOD_ALT | KMOD_CTRL | KMOD_GUI | KMOD_SHIFT)))
    return false;
  SDL_Point mouse = {e->button.x, e->button.y};
  for (int i = 0; i < COUNT; ++i)
    if (panel.controls[i].w > 0 &&
        SDL_PointInRect(&mouse, &panel.controls[i])) {
      panel.revision = SceneEditorDocumentRevision();
      if (i == START || i == END) {
        panel.editing = i;
        panel.draft[0] = 0;
        SDL_StartTextInput();
        return true;
      }
      MotionPaths paths;
      if (!SceneEditorMotionPathsRead(&paths))
        return true;
      MotionPathBinding *b = binding(&paths);
      if (!b)
        return true;
      if (i == END_ENABLE) {
        b->end_roll_enabled = !b->end_roll_enabled;
        if (b->end_roll_enabled)
          b->end_roll = b->start_roll;
      }
      if (i == RESET) {
        memset(b->start_up, 0, sizeof(b->start_up));
        b->start_roll = b->end_roll = 0;
        b->end_roll_enabled = false;
      }
      if (i == CAMERA_MODE)
        b->camera_orientation = (b->camera_orientation + 1) % 3;
      SceneEditorMotionPathsSet(&paths, panel.revision, message, size);
      return true;
    }
  if (panel.visible && SDL_PointInRect(&mouse, &panel.viewport))
    for (int k = 0; k < 2; ++k)
      if (panel.projected[k]) {
        double a;
        if (!angle(k, mouse.x, mouse.y, &a))
          continue;
        double dx = mouse.x - panel.cx[k], dy = mouse.y - panel.cy[k],
               det = panel.ux[k] * panel.ry[k] - panel.rx[k] * panel.uy[k];
        double u = (panel.ry[k] * dx - panel.rx[k] * dy) / det,
               v = (panel.ux[k] * dy - panel.uy[k] * dx) / det;
        if (fabs(hypot(u, v) - 1) < .22) {
          panel.dragging = true;
          panel.drag = k;
          panel.last = a;
          panel.delta = 0;
          panel.revision = SceneEditorDocumentRevision();
          SDL_CaptureMouse(SDL_TRUE);
          return true;
        }
      }
  return false;
}
void MotionOrientationHandlesDraw(SDL_Renderer *r,
                                  const SceneEditorDigestOverlayProjector *p,
                                  SDL_Rect viewport) {
  memset(panel.projected, 0, sizeof(panel.projected));
  panel.viewport = viewport;
  if (!panel.visible)
    return;
  for (int k = 0; k < 2; ++k) {
    MotionFrame f;
    TimelineVec3 pos;
    if (!MotionPathsRuntimeFrame(panel.target, k, &f) ||
        !MotionPathsRuntimeTargetPosition(panel.target, k, &pos))
      continue;
    if (!strcmp(panel.target, "camera/main") &&
        panel.binding.camera_orientation) {
      PreviewCameraSample camera = {0};
      if (EvaluatedCameraOrientation(&panel.binding, k, &camera) &&
          camera.has_orientation_frame)
        f = camera.orientation_frame;
    }
    int x, y, ux, uy, rx, ry;
    double length = fmax(p->span_max * .055, 1e-4);
    if (!SceneEditorDigestOverlayProjectPoint(p, pos.x, pos.y, pos.z, &x, &y) ||
        !SceneEditorDigestOverlayProjectPoint(
            p, pos.x + f.up[0] * length, pos.y + f.up[1] * length,
            pos.z + f.up[2] * length, &ux, &uy) ||
        !SceneEditorDigestOverlayProjectPoint(
            p, pos.x + f.right[0] * length, pos.y + f.right[1] * length,
            pos.z + f.right[2] * length, &rx, &ry))
      continue;
    panel.cx[k] = x;
    panel.cy[k] = y;
    panel.ux[k] = ux - x;
    panel.uy[k] = uy - y;
    panel.rx[k] = rx - x;
    panel.ry[k] = ry - y;
    panel.projected[k] = true;
    SDL_SetRenderDrawColor(r, k ? 230 : 110, k ? 160 : 240, k ? 255 : 170, 255);
    int lastx = 0, lasty = 0;
    for (int j = 0; j <= 48; ++j) {
      double a = j * 6.283185307179586 / 48;
      int xx = x + cos(a) * (ux - x) - sin(a) * (rx - x),
          yy = y + cos(a) * (uy - y) - sin(a) * (ry - y);
      if (j)
        SDL_RenderDrawLine(r, lastx, lasty, xx, yy);
      lastx = xx;
      lasty = yy;
    }
    double a = panel.dragging && panel.drag == k ? panel.delta : 0;
    int ex = x + cos(a) * (ux - x) - sin(a) * (rx - x),
        ey = y + cos(a) * (uy - y) - sin(a) * (ry - y);
    SDL_RenderDrawLine(r, x, y, ex, ey);
    SDL_Rect handle = {ex - 4, ey - 4, 8, 8};
    SDL_RenderDrawRect(r, &handle);
    ray_tracing_text_draw_utf8_at(
        r, ray_tracing_font_runtime_get_ui_regular(r, 11, 8),
        k ? "End roll" : "Start up", ex + 7, ey,
        (SDL_Color){160, 240, 200, 255});
  }
}
bool MotionOrientationPanelControl(const char *name, SDL_Rect *out) {
  if (panel.visible && (!strcmp(name, "path_start_up_handle") ||
                        !strcmp(name, "path_end_roll_handle"))) {
    int k = !strcmp(name, "path_end_roll_handle");
    if (!panel.projected[k])
      return false;
    *out = (SDL_Rect){panel.cx[k] + panel.ux[k] - 4,
                      panel.cy[k] + panel.uy[k] - 4, 8, 8};
    return true;
  }
  const char *names[] = {"path_start_roll", "path_end_roll_enabled",
                         "path_end_roll", "path_reset_roll",
                         "path_camera_orientation"};
  for (int i = 0; i < COUNT; ++i)
    if (!strcmp(name, names[i])) {
      *out = panel.controls[i];
      return out->w > 0;
    }
  return false;
}
