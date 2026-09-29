#include "scene_editor_camera_path_panel.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_timeline.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <stdio.h>
#include <string.h>
static SDL_Rect controls[3];
static bool enabled[3];
static char relationship[180];
void SceneEditorCameraPathPanelReset(void) { memset(controls, 0, sizeof(controls)); }
int SceneEditorCameraPathPanelDraw(SDL_Renderer *r, const MotionPaths *paths,
    const MotionPath *path, SDL_Rect clip, int x, int y, int width) {
  const MotionPathBinding *binding = NULL;
  for (size_t i = 0; i < paths->binding_count; ++i)
    if (paths->bindings[i].enabled && !strcmp(paths->bindings[i].target_id, "camera/main")) binding = &paths->bindings[i];
  bool bound = binding && !strcmp(binding->path_id, path->id);
  snprintf(relationship, sizeof(relationship), "Camera: %s", binding ? binding->path_id : "legacy position source");
  ray_tracing_text_draw_utf8_at(r, ray_tracing_font_runtime_get_ui_regular(r, 12, 9),
      relationship, x, y, SceneEditorChromeShellResolvePalette().text_primary);
  y += 26;
  const char *labels[] = {bound ? "Camera attached to this route" : "Attach camera on this route",
                         "Detach camera: restore source", "Edit camera route timing >"};
  enabled[0] = !bound; enabled[1] = enabled[2] = bound;
  for (int i = 0; i < 3; ++i) {
    controls[i] = (SDL_Rect){x, y, width, 28};
    SceneEditorRenderButton(r, controls[i], labels[i], false, enabled[i]);
    if (y < clip.y || y + 28 > clip.y + clip.h) controls[i] = (SDL_Rect){0};
    y += 34;
  }
  ray_tracing_text_draw_utf8_at(r, ray_tracing_font_runtime_get_ui_regular(r, 12, 9),
      "Orientation / FOV stay independent.", x, y, SceneEditorChromeShellResolvePalette().text_primary);
  return y + 30;
}
bool SceneEditorCameraPathPanelEvent(SDL_Event *e, const MotionPath *path,
    char *message, size_t size) {
  if (!path || e->type != SDL_MOUSEBUTTONDOWN || e->button.button != SDL_BUTTON_LEFT) return false;
  for (int i = 0; i < 3; ++i) {
    if (controls[i].w <= 0 || !SDL_PointInRect(&(SDL_Point){e->button.x, e->button.y}, &controls[i])) continue;
    if (!enabled[i]) return true;
    if (i < 2) {
      SceneEditorMotionPathBindCamera(path->id, i == 0, SceneEditorDocumentRevision(), message, size);
    } else {
      static TimelineDocument doc;
      if (SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK)
        for (size_t j = 0; j < doc.track_count; ++j)
          if (doc.tracks[j].enabled && !strcmp(doc.tracks[j].property_id, MOTION_CAMERA_PROGRESS_PROPERTY)) {
            SceneEditorTimelineSelectTrack(j);
            SceneEditorRenderAuthoringSetTiming(true);
            break;
          }
    }
    return true;
  }
  return false;
}
bool SceneEditorCameraPathPanelControl(const char *name, SDL_Rect *out) {
  const char *names[] = {"path_camera_attach", "path_camera_detach", "path_camera_timing"};
  for (int i = 0; i < 3; ++i)
    if (!strcmp(name, names[i])) { *out = controls[i]; return out->w > 0; }
  return false;
}
