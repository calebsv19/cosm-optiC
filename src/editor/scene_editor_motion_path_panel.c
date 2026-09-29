/* Independent path library and geometry authoring. Document commands own
 * history; this module retains only UI selection, text drafts and an
 * uncommitted drag. */
#include "editor/scene_editor_camera_authoring.h"
#include "editor/scene_editor_camera_inspector.h"
#include "editor/scene_editor_chrome_actions.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_light_authoring.h"
#include "editor/scene_editor_motion_paths.h"
#include "editor/scene_editor_object_commands.h"
#include "editor/scene_editor_pointer_event.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_timeline_selection.h"
#include "editor/scene_editor_workspace_profile.h"
#include "kit_ui_sdl.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "scene_editor_motion_path_viewport.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum {
  NEW,
  NAME,
  PREV,
  NEXT,
  ADD,
  REMOVE,
  MODE,
  DELETE_PATH,
  OBJECT_PREV,
  OBJECT_NEXT,
  ATTACH,
  DETACH,
  TIMING,
  SAVE,
  FRAME,
  SELECT_TOOL,
  PLACE_TOOL,
  FRAME_PATH,
  DEPTH,
  ATTACH_SECTION,
  DELETE_TOOL,
  CONTROL_COUNT
};
static struct {
  bool active, dragging, placing, show_followers;
  double plane_z;
  char path_id[64], object_id[128], draft[128], message[256];
  char labels[128][200];
  char point_labels[MOTION_POINT_CAPACITY][16];
  int label_count, point, editing, offset, max_offset, right_offset, right_max,
      handle;
  unsigned long long revision, message_revision;
  MotionPath drag;
  SDL_Rect controls[CONTROL_COUNT], rows[MOTION_PATH_CAPACITY], fields[9],
      viewport;
  bool enabled[CONTROL_COUNT];
  SceneEditorDigestOverlayProjector projector;
  bool projected;
  int down_x, down_y;
} ui = {.editing = -1};
static void cancel_field_edit(void) {
  ui.editing = -1;
  ui.draft[0] = 0;
  SDL_StopTextInput();
}
void SceneEditorMotionPathPanelReset(void) {
  if (ui.editing >= 0)
    SDL_StopTextInput();
  memset(&ui, 0, sizeof(ui));
  ui.editing = -1;
}
void SceneEditorMotionPathPanelSelect(bool selected) {
  ui.active = selected;
  ui.dragging = false;
  ui.editing = -1;
  SDL_StopTextInput();
  SceneEditorTimelinePause();
  if (selected) {
    SceneEditorCameraGestureCancel();
    SceneEditorLightGestureCancel();
    SceneEditorCameraInspectorReset();
    CameraEditorClearSelection();
    BezierEditorSetSelectedPointIndex(-1);
    SceneEditorTimelineClearSelection();
    MotionPaths paths;
    if (!SceneEditorMotionPathsRead(&paths))
      return;
    SceneEditorObjectReadback object;
    SceneEditorObjectInspect(&object);
    if (object.has_selection)
      snprintf(ui.object_id, sizeof(ui.object_id), "%s", object.selection.id);
    for (size_t i = 0; i < paths.binding_count; ++i)
      if (paths.bindings[i].enabled &&
          !strcmp(paths.bindings[i].object_id, ui.object_id))
        snprintf(ui.path_id, sizeof(ui.path_id), "%s",
                 paths.bindings[i].path_id);
    if (!ui.path_id[0] && paths.count)
      snprintf(ui.path_id, sizeof(ui.path_id), "%s", paths.paths[0].id);
  }
}
bool SceneEditorMotionPathPanelActive(void) {
  return ui.active &&
         SceneEditorWorkspaceProfileGet() == SCENE_WORKSPACE_RENDER;
}
static bool hit(SDL_Rect r, int x, int y) {
  return r.w > 0 && r.h > 0 && SDL_PointInRect(&(SDL_Point){x, y}, &r);
}
static MotionPath *selected(MotionPaths *d) {
  for (size_t i = 0; i < d->count; ++i)
    if (!strcmp(d->paths[i].id, ui.path_id)) {
      if (ui.point < 0 || ui.point >= (int)d->paths[i].count)
        ui.point = 0;
      return &d->paths[i];
    }
  return NULL;
}
static void label(SDL_Renderer *r, const char *s, int x, int y) {
  char *b = ui.labels[ui.label_count++ % 128];
  snprintf(b, 200, "%s", s);
  ray_tracing_text_draw_utf8_at(
      r, ray_tracing_font_runtime_get_ui_regular(r, 12, 9), b, x, y,
      SceneEditorChromeShellResolvePalette().text_primary);
}
static void scrollbar(SDL_Renderer *r, SDL_Rect pane, int maximum, int offset) {
  if (maximum <= 0)
    return;
  KitUiSdlScrollbarLayout scroll;
  kit_ui_sdl_scrollbar_layout(&pane, pane.h + maximum, offset, &scroll);
  kit_ui_sdl_draw_scrollbar(r, &scroll, (KitRenderColor){40, 44, 50, 255},
                            (KitRenderColor){130, 140, 155, 255});
}
static void button(SDL_Renderer *r, int index, SDL_Rect rect, const char *s,
                   bool enabled) {
  ui.controls[index] = rect;
  ui.enabled[index] = enabled;
  char *b = ui.labels[ui.label_count++ % 128];
  snprintf(b, 200, "%s", s);
  bool selected = (index == SELECT_TOOL && !ui.placing) ||
                  (index == PLACE_TOOL && ui.placing);
  SceneEditorRenderButton(r, rect, b, selected, enabled);
}
static bool projector(const SceneEditorPaneLayout *l) {
  RuntimeSceneBridge3DDigestState d;
  return SceneEditorDigestOverlayResolve(&d) &&
         SceneEditorDigestOverlayBuildProjector(
             &d, &l->viewport_rect, SceneEditorGetViewportNavState(),
             &ui.projector);
}
static bool project(const double v[3], int *x, int *y) {
  double s = SceneEditorDocumentWorldScale();
  return SceneEditorDigestOverlayProjectPoint(&ui.projector, v[0] * s, v[1] * s,
                                              v[2] * s, x, y);
}
static void overlay(SDL_Renderer *r, const MotionPath *p) {
  SDL_Rect prior;
  SDL_bool clipped = SDL_RenderIsClipEnabled(r);
  SDL_RenderGetClipRect(r, &prior);
  SDL_RenderSetClipRect(r, &ui.viewport);
  SDL_SetRenderDrawColor(r, 72, 210, 235, 255);
  int px = 0, py = 0;
  bool previous = false;
  for (int i = 0; i <= 256; ++i) {
    double v[3];
    int x, y;
    MotionPathPointAt(p, (double)i * (p->count - 1) / 256, v);
    bool valid = project(v, &x, &y);
    if (valid && previous) {
      SDL_RenderDrawLine(r, px, py, x, y);
      SDL_RenderDrawLine(r, px + 1, py, x + 1, y);
      SDL_RenderDrawLine(r, px, py + 1, x, y + 1);
    }
    px = x;
    py = y;
    previous = valid;
  }
  for (size_t i = 0; i < p->count; ++i) {
    const MotionPathPoint *point = &p->points[i];
    int x, y;
    if (!project(point->position, &x, &y))
      continue;
    SDL_SetRenderDrawColor(r, i == (size_t)ui.point ? 255 : 72,
                           i == (size_t)ui.point ? 210 : 210,
                           i == (size_t)ui.point ? 80 : 235, 255);
    SDL_Rect box = {x - 5, y - 5, 10, 10};
    SDL_RenderFillRect(r, &box);
    snprintf(ui.point_labels[i], sizeof(ui.point_labels[i]), "%zu", i + 1);
    ray_tracing_text_draw_utf8_at(
        r, ray_tracing_font_runtime_get_ui_regular(r, 12, 9),
        ui.point_labels[i], x + 9, y - 14, (SDL_Color){230, 250, 255, 255});
    if (i != (size_t)ui.point)
      continue;
    for (int h = 0; h < 2; ++h) {
      double v[3];
      for (int k = 0; k < 3; ++k)
        v[k] =
            point->position[k] + (h ? point->outgoing[k] : point->incoming[k]);
      int hx, hy;
      if (project(v, &hx, &hy)) {
        SDL_SetRenderDrawColor(r, 235, 155, 90, 255);
        SDL_RenderDrawLine(r, x, y, hx, hy);
        SDL_Rect handle = {hx - 4, hy - 4, 8, 8};
        SDL_RenderFillRect(r, &handle);
      }
    }
  }
  SDL_RenderSetClipRect(r, clipped ? &prior : NULL);
}
void SceneEditorMotionPathOverlayDraw(SceneEditor *e,
                                      const SceneEditorPaneLayout *l) {
  if (ui.active)
    return;
  TimelineTrack track;
  TimelineRate rate;
  TimelineRange range;
  TimelineSample sample;
  if (!SceneEditorTimelineSelectedTrack(&track, &rate, &range, &sample) ||
      strcmp(track.property_id, MOTION_PROGRESS_PROPERTY))
    return;
  MotionPaths paths;
  if (!SceneEditorMotionPathsRead(&paths))
    return;
  const char *id = NULL;
  for (size_t i = 0; i < paths.binding_count; ++i)
    if (paths.bindings[i].enabled &&
        !strcmp(paths.bindings[i].object_id, track.target_id + 7))
      id = paths.bindings[i].path_id;
  for (size_t i = 0; id && i < paths.count; ++i)
    if (!strcmp(paths.paths[i].id, id)) {
      ui.viewport = l->viewport_rect;
      ui.projected = projector(l);
      if (ui.projected)
        overlay(e->renderer, &paths.paths[i]);
      return;
    }
}
void SceneEditorMotionPathPanelDraw(SceneEditor *e,
                                    const SceneEditorPaneLayout *l) {
  if (ui.message_revision != SceneEditorDocumentRevision())
    ui.message[0] = 0;
  if (!ui.active)
    return;
  MotionPaths d;
  if (!SceneEditorMotionPathsRead(&d))
    return;
  MotionPath *p = selected(&d);
  ui.label_count = 0;
  memset(ui.controls, 0, sizeof(ui.controls));
  memset(ui.fields, 0, sizeof(ui.fields));
  memset(ui.rows, 0, sizeof(ui.rows));
  ui.viewport = l->viewport_rect;
  ui.projected = projector(l);
  if (p && ui.projected)
    overlay(e->renderer, ui.dragging ? &ui.drag : p);
  if (p && ui.projected && (ui.placing || (SDL_GetModState() & KMOD_SHIFT)) &&
      !(SDL_GetModState() & (KMOD_ALT | KMOD_CTRL | KMOD_GUI))) {
    SDL_Rect prior_clip;
    SDL_bool was_clipped = SDL_RenderIsClipEnabled(e->renderer);
    SDL_RenderGetClipRect(e->renderer, &prior_clip);
    SDL_RenderSetClipRect(e->renderer, &l->viewport_rect);
    MotionPathViewportGuide(e->renderer, &ui.projector, p, ui.plane_z);
    SDL_RenderSetClipRect(e->renderer, was_clipped ? &prior_clip : NULL);
  }
  SDL_Renderer *r = e->renderer;
  RayTracingThemePalette palette = SceneEditorChromeShellResolvePalette();
  SDL_Rect pane = l->left_content_rect;
  pane.y += 48;
  pane.h -= 48;
  SDL_Rect prior;
  SDL_bool clipped = SDL_RenderIsClipEnabled(r);
  SDL_RenderGetClipRect(r, &prior);
  SDL_RenderSetClipRect(r, &pane);
  SDL_SetRenderDrawColor(r, palette.panel_fill.r, palette.panel_fill.g,
                         palette.panel_fill.b, 255);
  SDL_RenderFillRect(r, &pane);
  int x = pane.x + 10, y = pane.y + 8 - ui.offset, w = pane.w - 20;
  char text[200];
  label(r, "1. Create and shape a path", x, y);
  y += 26;
  button(r, NEW, (SDL_Rect){x, y, w, 30}, "+ New Path",
         d.count < MOTION_PATH_CAPACITY);
  y += 38;
  if (p) {
    int half = (w - 6) / 2;
    button(r, SELECT_TOOL, (SDL_Rect){x, y, half, 28}, "Move", true);
    button(r, PLACE_TOOL, (SDL_Rect){x + half + 6, y, half, 28},
           "Add: Shift-click", p->count < MOTION_POINT_CAPACITY);
    y += 34;
    button(r, DELETE_TOOL, (SDL_Rect){x, y, half, 28}, "Delete point", p->count > 2);
    button(r, FRAME_PATH, (SDL_Rect){x + half + 6, y, half, 28}, "Frame path", true);
    y += 36;
    label(r, "Shift-click: add | Drag: move point", x, y); y += 24;
    label(r, "Option-drag: orbit | Right-drag: pan", x, y); y += 30;
  }
  for (size_t i = 0; i < d.count; ++i) {
    ui.rows[i] = (SDL_Rect){x, y, w, 28};
    char *b = ui.labels[ui.label_count++ % 128];
    snprintf(b, 200, "%s", d.paths[i].name);
    SceneEditorRenderButton(r, ui.rows[i], b, p == &d.paths[i], true);
    y += 32;
  }
  if (!p) {
    label(r, "Create a path, then attach an object.", x, y);
    y += 30;
  } else {
    button(r, DELETE_PATH, (SDL_Rect){x, y, w, 28},
           "Delete path (detach followers first)", true);
    y += 36;
    button(r, ATTACH_SECTION, (SDL_Rect){x, y, w, 30},
           ui.show_followers ? "2. Attach object (hide)"
                             : "2. Attach an object...",
           true);
    y += 38;
    if (ui.show_followers) {
      label(r, "Follower object", x, y);
      y += 26;
      SceneEditorDocumentObjectInfo info;
      bool object = SceneEditorDocumentObjectById(ui.object_id, &info);
      if (!object) {
        SceneEditorObjectReadback read;
        SceneEditorObjectInspect(&read);
        if (read.has_selection) {
          snprintf(ui.object_id, sizeof(ui.object_id), "%s", read.selection.id);
          info = read.selection;
          object = true;
        }
      }
      snprintf(text, sizeof(text), "%s",
               object ? info.name : "Choose an object with arrows");
      label(r, text, x, y);
      y += 26;
      button(r, OBJECT_PREV, (SDL_Rect){x, y, (w - 6) / 2, 28}, "< Object",
             true);
      button(r, OBJECT_NEXT, (SDL_Rect){x + (w + 6) / 2, y, (w - 6) / 2, 28},
             "Object >", true);
      y += 36;
      bool bound = false, restore_known = true;
      int followers = 0;
      for (size_t i = 0; i < d.binding_count; ++i)
        if (d.bindings[i].enabled && !strcmp(d.bindings[i].path_id, p->id)) {
          ++followers;
          if (!strcmp(d.bindings[i].object_id, ui.object_id)) {
            bound = true;
            restore_known = d.bindings[i].restore_known;
          }
        }
      snprintf(text, sizeof(text), "%d follower%s | world-space route",
               followers, followers == 1 ? "" : "s");
      label(r, text, x, y);
      y += 26;
      button(r, ATTACH, (SDL_Rect){x, y, w, 30},
             bound ? "Attached: on this path" : "Attach on path (replaces XYZ)",
             object && !bound);
      y += 36;
      if (bound && !restore_known) {
        label(r, "Prior source unknown; XYZ stays off.", x, y);
        y += 26;
      }
      button(r, DETACH, (SDL_Rect){x, y, w, 28},
             restore_known ? "Detach: restore prior source" : "Detach to static (legacy)",
             bound);
      y += 34;
      button(r, TIMING, (SDL_Rect){x, y, w, 30}, "Edit follower timing >",
             bound);
      y += 38;
      label(r, "Progress 0 = start; 1 = end.", x, y);
      y += 23;
      label(r, "Equal keys pause; later keys resume.", x, y);
      y += 30;
    } else {
      label(r, "No object needed to shape the route.", x, y);
      y += 30;
    }
  }
  button(r, SAVE, (SDL_Rect){x, y, w, 30}, "Save scene + animation", true);
  y += 38;
  button(r, FRAME, (SDL_Rect){x, y, w, 28}, "Frame scene", true);
  y += 36;
  if (ui.message[0]) {
    label(r, ui.message, x, y);
    y += 30;
  }
  ui.max_offset = y + ui.offset - (pane.y + pane.h);
  if (ui.max_offset < 0)
    ui.max_offset = 0;
  for (int i = 0; i < CONTROL_COUNT; ++i)
    if (i != DEPTH && (i < NAME || i > MODE) && (ui.controls[i].y < pane.y ||
        ui.controls[i].y + ui.controls[i].h > pane.y + pane.h))
      ui.controls[i] = (SDL_Rect){0};
  for (size_t i = 0; i < d.count; ++i)
    if (ui.rows[i].y < pane.y || ui.rows[i].y + ui.rows[i].h > pane.y + pane.h)
      ui.rows[i] = (SDL_Rect){0};
  if (ui.controls[ATTACH_SECTION].y < pane.y ||
      ui.controls[ATTACH_SECTION].y + ui.controls[ATTACH_SECTION].h >
          pane.y + pane.h)
    ui.controls[ATTACH_SECTION] = (SDL_Rect){0};
  scrollbar(r, pane, ui.max_offset, ui.offset);
  pane = l->right_content_rect;
  SDL_RenderSetClipRect(r, &pane);
  SDL_SetRenderDrawColor(r, palette.panel_fill.r, palette.panel_fill.g,
                         palette.panel_fill.b, 255);
  SDL_RenderFillRect(r, &pane);
  x = pane.x + 10;
  y = pane.y + 10 - ui.right_offset;
  w = pane.w - 20;
  label(r, "Path shape", x, y);
  y += 28;
  if (p) {
    snprintf(text, sizeof(text), "Name: %s",
             ui.editing == 9 ? ui.draft : p->name);
    button(r, NAME, (SDL_Rect){x, y, w, 30}, text, true);
    y += 40;
    snprintf(text, sizeof(text), "Point %d / %zu", ui.point + 1, p->count);
    label(r, text, x, y);
    y += 26;
    button(r, PREV, (SDL_Rect){x, y, (w - 6) / 2, 28}, "< Point", true);
    button(r, NEXT, (SDL_Rect){x + (w + 6) / 2, y, (w - 6) / 2, 28}, "Point >",
           true);
    y += 36;
    button(r, ADD, (SDL_Rect){x, y, (w - 6) / 2, 28}, "Split / extend",
           p->count < MOTION_POINT_CAPACITY);
    button(r, REMOVE, (SDL_Rect){x + (w + 6) / 2, y, (w - 6) / 2, 28},
           "Delete point", p->count > 2);
    y += 36;
    button(r, MODE, (SDL_Rect){x, y, w, 30},
           p->points[ui.point].linear ? "Next segment: Straight"
                                      : "Next segment: Cubic Bezier",
           ui.point < (int)p->count - 1);
    y += 40;
    char plane[100];
    snprintf(plane, sizeof(plane),
             ui.editing == 10 ? "Draw plane Z: %s_" : "Draw plane Z: %s",
             ui.editing == 10 ? ui.draft : "");
    if (ui.editing != 10)
      snprintf(plane, sizeof(plane), "Draw plane Z: %.5g", ui.plane_z);
    button(r, DEPTH, (SDL_Rect){x, y, w, 28}, plane, true);
    y += 36;
    const char *groups[] = {"Point position", "Incoming handle offset",
                            "Outgoing handle offset"};
    for (int g = 0; g < 3; ++g) {
      label(r, groups[g], x, y);
      y += 25;
      for (int k = 0; k < 3; ++k) {
        int index = g * 3 + k;
        double *v = g == 0   ? p->points[ui.point].position
                    : g == 1 ? p->points[ui.point].incoming
                             : p->points[ui.point].outgoing;
        char *b = ui.labels[ui.label_count++ % 128];
        if (ui.editing == index)
          snprintf(b, 200, "%c: %s_", 'X' + k, ui.draft);
        else
          snprintf(b, 200, "%c: %.5g", 'X' + k, v[k]);
        ui.fields[index] = (SDL_Rect){x + k * (w + 6) / 3, y, (w - 12) / 3, 30};
        SceneEditorRenderButton(r, ui.fields[index], b, ui.editing == index,
                                true);
      }
      y += 40;
    }
    label(r, "Enter applies. Escape cancels.", x, y);
    y += 25;
    label(r, "Add points: Shift-click in viewport.", x, y);
    y += 25;
    label(r, "Drag in XY; edit Z numerically.", x, y);
    y += 25;
    label(r, "Shape edits keep timing keys unchanged.", x, y);
  } else
    label(r, "Choose New Path to begin.", x, y);
  ui.right_max = y + 32 + ui.right_offset - (pane.y + pane.h);
  if (ui.right_max < 0)
    ui.right_max = 0;
  scrollbar(r, pane, ui.right_max, ui.right_offset);
  for (int i = 0; i < 9; ++i)
    if (ui.fields[i].y < pane.y ||
        ui.fields[i].y + ui.fields[i].h > pane.y + pane.h)
      ui.fields[i] = (SDL_Rect){0};
  if (ui.controls[DEPTH].y < pane.y ||
      ui.controls[DEPTH].y + ui.controls[DEPTH].h > pane.y + pane.h)
    ui.controls[DEPTH] = (SDL_Rect){0};
  for (int i = NAME; i <= MODE; ++i)
    if (ui.controls[i].y < pane.y ||
        ui.controls[i].y + ui.controls[i].h > pane.y + pane.h)
      ui.controls[i] = (SDL_Rect){0};
  SDL_RenderSetClipRect(r, clipped ? &prior : NULL);
}
static void object_step(int step) {
  int count = SceneEditorDocumentObjectCount(), at = -1;
  SceneEditorDocumentObjectInfo info;
  for (int i = 0; i < count; ++i)
    if (SceneEditorDocumentObjectAt(i, &info) && !strcmp(info.id, ui.object_id))
      at = i;
  if (!count)
    return;
  at = (at + step + count) % count;
  if (SceneEditorDocumentObjectAt(at, &info)) {
    snprintf(ui.object_id, sizeof(ui.object_id), "%s", info.id);
    SceneEditorObjectReadback result;
    SceneEditorObjectExecute(SCENE_OBJECT_SELECT, info.id, NULL, false,
                             SceneEditorDocumentRevision(), &result, ui.message,
                             sizeof(ui.message));
  }
}
static void add_point(MotionPath *p) {
  int at = ui.point;
  MotionPathPoint next = {0};
  unsigned serial = 1;
  bool used;
  do {
    snprintf(next.id, sizeof(next.id), "point-%u", serial++);
    used = false;
    for (size_t i = 0; i < p->count; ++i)
      if (!strcmp(next.id, p->points[i].id))
        used = true;
  } while (used);
  MotionPathPoint *a = &p->points[at];
  next.linear = a->linear;
  if (at == (int)p->count - 1) {
    for (int k = 0; k < 3; ++k) {
      double delta = a->position[k] - p->points[at - 1].position[k];
      next.position[k] = a->position[k] + delta;
      next.incoming[k] = -delta / 3;
      next.outgoing[k] = delta / 3;
    }
  } else {
    MotionPathPoint *b = &p->points[at + 1];
    for (int k = 0; k < 3; ++k) {
      double x = a->position[k], y = x + a->outgoing[k],
             z = b->position[k] + b->incoming[k], w = b->position[k];
      if (a->linear) {
        y = x + (w - x) / 3;
        z = x + 2 * (w - x) / 3;
      }
      double xy = (x + y) / 2, yz = (y + z) / 2, zw = (z + w) / 2,
             left = (xy + yz) / 2, right = (yz + zw) / 2,
             mid = (left + right) / 2;
      next.position[k] = mid;
      next.incoming[k] = left - mid;
      next.outgoing[k] = right - mid;
      a->outgoing[k] = xy - x;
      b->incoming[k] = zw - w;
    }
  }
  memmove(&p->points[at + 2], &p->points[at + 1],
          (p->count - at - 1) * sizeof(next));
  p->points[at + 1] = next;
  ++p->count;
  ui.point = at + 1;
}
static bool motion_path_event(SceneEditor *e, SDL_Event *event,
                              const SceneEditorPaneLayout *l) {
  (void)e;
  if (!ui.active)
    return false;
  MotionPaths d;
  if (!SceneEditorMotionPathsRead(&d))
    return false;
  MotionPath *p = selected(&d);
  /* Navigation owns modified clicks, including clicks on existing handles. */
  if ((event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP ||
       event->type == SDL_MOUSEMOTION) &&
      ((SDL_GetModState() & (KMOD_ALT | KMOD_CTRL | KMOD_GUI)) ||
       (event->type == SDL_MOUSEMOTION && (event->motion.state & (SDL_BUTTON_RMASK | SDL_BUTTON_MMASK))) ||
       (event->type != SDL_MOUSEMOTION && event->button.button != SDL_BUTTON_LEFT))) {
    ui.dragging = false;
    return false;
  }
  if (ui.editing >= 0 &&
      (event->type == SDL_TEXTINPUT || event->type == SDL_KEYDOWN)) {
    if (event->type == SDL_TEXTINPUT) {
      if (strlen(ui.draft) + strlen(event->text.text) < sizeof(ui.draft))
        strcat(ui.draft, event->text.text);
      return true;
    }
    if (event->key.keysym.sym == SDLK_ESCAPE) {
      ui.editing = -1;
      SDL_StopTextInput();
      return true;
    }
    if (event->key.keysym.sym == SDLK_BACKSPACE) {
      size_t n = strlen(ui.draft);
      if (n)
        ui.draft[n - 1] = 0;
      return true;
    }
    if (event->key.keysym.sym == SDLK_RETURN && p) {
      if (ui.editing == 10) {
        char *end;
        double v = strtod(ui.draft, &end);
        if (end != ui.draft && !*end && isfinite(v) && fabs(v) <= 1e12) {
          ui.plane_z = v;
          ui.editing = -1;
          SDL_StopTextInput();
        } else
          snprintf(ui.message, sizeof(ui.message),
                   "Enter a finite drawing height.");
        return true;
      }

      bool valid = true;
      if (ui.editing == 9) {
        valid = *ui.draft;
        snprintf(p->name, sizeof(p->name), "%s", ui.draft);
      } else {
        char *end;
        double value = strtod(ui.draft, &end);
        valid =
            end != ui.draft && !*end && isfinite(value) && fabs(value) <= 1e12;
        double *v = ui.editing < 3   ? p->points[ui.point].position
                    : ui.editing < 6 ? p->points[ui.point].incoming
                                     : p->points[ui.point].outgoing;
        if (valid)
          v[ui.editing % 3] = value;
      }
      if (valid && SceneEditorMotionPathsSet(&d, ui.revision, ui.message,
                                             sizeof(ui.message))) {
        ui.editing = -1;
        SDL_StopTextInput();
        snprintf(ui.message, sizeof(ui.message),
                 "Path updated. Undo is available.");
      } else if (!valid)
        snprintf(ui.message, sizeof(ui.message),
                 "Enter a valid name or finite number.");
      return true;
    }
    return true;
  }
  if (ui.dragging && (ui.revision != SceneEditorDocumentRevision() ||
                      (event->type == SDL_WINDOWEVENT &&
                       (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                        event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED))))
    ui.dragging = false;
  if (ui.dragging) {
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
      ui.dragging = false;
      return true;
    }
    if (event->type == SDL_MOUSEMOTION && p) {
      double x, y, z, s = SceneEditorDocumentWorldScale();
      MotionPathPoint *point = &ui.drag.points[ui.point];
      double plane = point->position[2] + (ui.handle == 1   ? point->incoming[2]
                                           : ui.handle == 2 ? point->outgoing[2]
                                                            : 0);
      if (SceneEditorDigestOverlayScreenRayToPlanePoint(
              &ui.projector, event->motion.x, event->motion.y, plane * s, &x,
              &y, &z)) {
        double *v = ui.handle == 1   ? point->incoming
                    : ui.handle == 2 ? point->outgoing
                                     : point->position;
        v[0] = x / s - (ui.handle ? point->position[0] : 0);
        v[1] = y / s - (ui.handle ? point->position[1] : 0);
      }
      return true;
    }
    if (event->type == SDL_MOUSEBUTTONUP) {
      ui.dragging = false;
      if (p && (abs(event->button.x - ui.down_x) > 2 ||
                abs(event->button.y - ui.down_y) > 2)) {
        *p = ui.drag;
        SceneEditorMotionPathsSet(&d, ui.revision, ui.message,
                                  sizeof(ui.message));
      }
      return true;
    }
  }
  if (event->type == SDL_MOUSEWHEEL) {
    int x, y;
    SceneEditorWheelPosition(event, &x, &y);
    if (hit(l->right_content_rect, x, y)) {
      ui.right_offset -= event->wheel.y * 32;
      ui.right_offset = ui.right_offset < 0              ? 0
                        : ui.right_offset > ui.right_max ? ui.right_max
                                                         : ui.right_offset;
      return true;
    }
    if (hit(l->left_content_rect, x, y)) {
      ui.offset -= event->wheel.y * 32;
      ui.offset = ui.offset < 0               ? 0
                  : ui.offset > ui.max_offset ? ui.max_offset
                                              : ui.offset;
      return true;
    }
  }
  if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
    ui.placing = false;
    ui.dragging = false;
    snprintf(ui.message, sizeof(ui.message),
             "Placement finished. Select or drag a point.");
    return true;
  }
  if (event->type != SDL_MOUSEBUTTONDOWN ||
      event->button.button != SDL_BUTTON_LEFT)
    return false;
  int x = event->button.x, y = event->button.y;
  for (size_t i = 0; i < d.count; ++i)
    if (hit(ui.rows[i], x, y)) {
      cancel_field_edit();
      snprintf(ui.path_id, sizeof(ui.path_id), "%s", d.paths[i].id);
      ui.point = 0;
      return true;
    }
  for (int i = 0; i < 9; ++i)
    if (p && hit(ui.fields[i], x, y)) {
      ui.editing = i;
      ui.draft[0] = 0;
      ui.revision = SceneEditorDocumentRevision();
      SDL_StartTextInput();
      return true;
    }
  for (int i = 0; i < CONTROL_COUNT; ++i)
    if (hit(ui.controls[i], x, y)) {
      if (!ui.enabled[i])
        return true;
      if (i == SELECT_TOOL || i == PLACE_TOOL) {
        ui.editing = -1;
        SDL_StopTextInput();
        ui.placing = i == PLACE_TOOL;
        ui.message[0] = 0;
        return true;
      }
      if (i == FRAME_PATH && p) {
        MotionPathViewportFrame(p, &l->viewport_rect);
        return true;
      }
      if (i == ATTACH_SECTION) {
        ui.show_followers = !ui.show_followers;
        return true;
      }
      if (i == DEPTH) {
        ui.editing = 10;
        ui.draft[0] = 0;
        SDL_StartTextInput();
        return true;
      }
      unsigned long long rev = SceneEditorDocumentRevision();
      bool changed = false;
      if (i == NEW) {
        double origin[3] = {0};
        SceneEditorObjectReadback read;
        SceneEditorObjectInspect(&read);
        SceneEditorDocumentTransform t;
        if (read.has_selection &&
            SceneEditorDocumentGetTransformForSceneIndex(
                read.selection.runtime_index, &t, NULL, 0))
          memcpy(origin, t.position, sizeof(origin));
        double length = ui.projected ? fmax(.01, ui.projector.span_max * .2) /
                                           SceneEditorDocumentWorldScale()
                                     : 2 / SceneEditorDocumentWorldScale();
        if (!read.has_selection && ui.projected) {
          origin[0] = ui.projector.center_x / SceneEditorDocumentWorldScale();
          origin[1] = ui.projector.center_y / SceneEditorDocumentWorldScale();
          origin[2] = ui.projector.center_z / SceneEditorDocumentWorldScale();
        }
        if (SceneEditorMotionPathCreate("New Path", origin, length, rev,
                                        ui.path_id, sizeof(ui.path_id),
                                        ui.message, sizeof(ui.message))) {
          ui.point = 0;
          ui.placing = true;
          ui.show_followers = false;
          ui.plane_z = origin[2];
          MotionPaths fresh;
          if (SceneEditorMotionPathsRead(&fresh)) {
            MotionPath *created = selected(&fresh);
            if (created)
              MotionPathViewportFrame(created, &l->viewport_rect);
          }
          ui.editing = 9;
          ui.draft[0] = 0;
          ui.revision = SceneEditorDocumentRevision();
          SDL_StartTextInput();
        }
        return true;
      }
      if (i == OBJECT_PREV || i == OBJECT_NEXT) {
        object_step(i == OBJECT_PREV ? -1 : 1);
        return true;
      }
      if (i == SAVE) {
        SceneEditorChromeActionsSaveAuthoring();
        return true;
      }
      if (i == FRAME) {
        SceneEditorFrameViewport(false);
        return true;
      }
      if (!p)
        return true;
      if (i == NAME) {
        ui.editing = 9;
        ui.draft[0] = 0;
        ui.revision = rev;
        SDL_StartTextInput();
        return true;
      }
      if (i == PREV || i == NEXT) {
        cancel_field_edit();
        ui.point = (ui.point + (i == PREV ? -1 : 1) + p->count) % p->count;
        return true;
      }
      if (i == ADD) {
        cancel_field_edit();
        add_point(p);
        changed = true;
      }
      if (i == REMOVE || i == DELETE_TOOL) {
        cancel_field_edit();
        memmove(&p->points[ui.point], &p->points[ui.point + 1],
                (p->count - ui.point - 1) * sizeof(p->points[0]));
        --p->count;
        if (ui.point >= (int)p->count)
          --ui.point;
        changed = true;
      }
      if (i == MODE) {
        p->points[ui.point].linear = !p->points[ui.point].linear;
        changed = true;
      }
      if (i == DELETE_PATH) {
        cancel_field_edit();
        for (size_t j = 0; j < d.binding_count; ++j)
          if (!strcmp(d.bindings[j].path_id, p->id) && d.bindings[j].enabled) {
            snprintf(ui.message, sizeof(ui.message),
                     "Detach follower %s before deleting.",
                     d.bindings[j].object_id);
            return true;
          }
        /* Remove inactive references too; their disabled keys remain retained.
         */
        for (size_t j = 0; j < d.binding_count;)
          if (!strcmp(d.bindings[j].path_id, p->id)) {
            memmove(&d.bindings[j], &d.bindings[j + 1],
                    (--d.binding_count - j) * sizeof(d.bindings[0]));
          } else
            ++j;
        size_t index = p - d.paths;
        memmove(p, p + 1, (--d.count - index) * sizeof(*p));
        changed = true;
        ui.path_id[0] = 0;
      }
      if (i == ATTACH || i == DETACH) {
        if (!SceneEditorTimelineCurrentSample(&(TimelineSample){0}) &&
            !SceneEditorTimelineActivate()) {
          snprintf(ui.message, sizeof(ui.message), "%s",
                   SceneEditorTimelineStatus());
          return true;
        }
        SceneEditorMotionPathBind(ui.object_id, p->id, i == ATTACH,
                                  SceneEditorDocumentRevision(), ui.message,
                                  sizeof(ui.message));
        return true;
      }
      if (i == TIMING) {
        static TimelineDocument doc;
        if (SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK)
          for (size_t j = 0; j < doc.track_count; ++j)
            if (doc.tracks[j].enabled &&
                !strcmp(doc.tracks[j].property_id, MOTION_PROGRESS_PROPERTY) &&
                !strcmp(doc.tracks[j].target_id + 7, ui.object_id)) {
              SceneEditorTimelineSelectTrack(j);
              SceneEditorMotionPathPanelSelect(false);
              SceneEditorRenderAuthoringSetTiming(true);
              break;
            }
        return true;
      }
      if (changed)
        SceneEditorMotionPathsSet(&d, rev, ui.message, sizeof(ui.message));
      return true;
    }
  if (hit(l->viewport_rect, x, y) && p && ui.projected) {
    if (SDL_GetModState() & KMOD_SHIFT) {
      double position[3];
      ui.editing = -1;
      SDL_StopTextInput();
      if (MotionPathViewportPlacement(&ui.projector, x, y, ui.plane_z,
                                      position) &&
          MotionPathViewportAppend(p, position)) {
        if (SceneEditorMotionPathsSet(&d, SceneEditorDocumentRevision(),
                                      ui.message, sizeof(ui.message))) {
          ui.point = (int)p->count - 1;
          snprintf(ui.message, sizeof(ui.message),
                   "Added point %zu. Shift-click to add more.", p->count);
        }
      } else
        snprintf(ui.message, sizeof(ui.message),
                 "Cannot place here: change view/plane or remove a point.");
      return true;
    }

    double best = 144;
    int point = -1, handle = 0;
    for (size_t i = 0; i < p->count; ++i) {
      for (int h = 0; h < 3; ++h) {
        if (h && i != (size_t)ui.point)
          continue;
        double v[3];
        for (int k = 0; k < 3; ++k)
          v[k] = p->points[i].position[k] + (h == 1   ? p->points[i].incoming[k]
                                             : h == 2 ? p->points[i].outgoing[k]
                                                      : 0);
        int px, py;
        if (project(v, &px, &py)) {
          double dist = (px - x) * (px - x) + (py - y) * (py - y);
          if (dist < best) {
            best = dist;
            point = i;
            handle = h;
          }
        }
      }
    }
    if (point >= 0) {
      cancel_field_edit();
      ui.point = point;
      ui.handle = handle;
      ui.drag = *p;
      ui.dragging = true;
      ui.revision = SceneEditorDocumentRevision();
      ui.down_x = x;
      ui.down_y = y;
    }
    return true;
  }
  return hit(l->right_content_rect, x, y) ||
         (hit(l->left_content_rect, x, y) && y >= l->left_content_rect.y + 48);
}
bool SceneEditorMotionPathPanelEvent(SceneEditor *editor, SDL_Event *event,
                                     const SceneEditorPaneLayout *layout) {
  bool handled = motion_path_event(editor, event, layout);
  if (handled)
    ui.message_revision = SceneEditorDocumentRevision();
  return handled;
}
bool SceneEditorMotionPathPanelControl(const char *name, SDL_Rect *out) {
  if (!ui.active)
    return false;
  if (!strncmp(name, "path_row/", 9)) {
    MotionPaths paths;
    if (SceneEditorMotionPathsRead(&paths))
      for (size_t i = 0; i < paths.count; ++i)
        if (!strcmp(name + 9, paths.paths[i].id)) {
          *out = ui.rows[i];
          return out->w > 0;
        }
    return false;
  }
  const char *names[] = {
      "new_path",         "path_name",       "path_previous",
      "path_next",        "path_add",        "path_remove",
      "path_segment",     "path_delete",     "follower_previous",
      "follower_next",    "path_attach",     "path_detach",
      "path_timing",      "path_save",       "path_frame",
      "path_select_tool", "path_place_tool", "path_frame_selected",
      "path_plane_z",     "path_followers",  "path_delete_selected"};
  for (int i = 0; i < CONTROL_COUNT; ++i)
    if (!strcmp(name, names[i])) {
      *out = ui.controls[i];
      return out->w > 0;
    }
  const char *fields[] = {"path_x",     "path_y",     "path_z",
                          "path_in_x",  "path_in_y",  "path_in_z",
                          "path_out_x", "path_out_y", "path_out_z"};
  for (int i = 0; i < 9; ++i)
    if (!strcmp(name, fields[i])) {
      *out = ui.fields[i];
      return out->w > 0;
    }
  return false;
}
