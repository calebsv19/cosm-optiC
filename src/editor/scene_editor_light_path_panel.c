#include "scene_editor_light_path_panel.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_timeline.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "import/runtime_scene_light_timeline_io.h"
#include <stdio.h>
#include <string.h>
static SDL_Rect controls[3];
static bool enabled[3];
static char relationship[180];
static char target[TIMELINE_ID_CAPACITY];
void SceneEditorLightPathPanelReset(void) { memset(controls, 0, sizeof(controls)); }
int SceneEditorLightPathPanelDraw(SDL_Renderer *r, const MotionPaths *paths,
    const MotionPath *path, SDL_Rect clip, int x, int y, int width) {
  static RuntimeSceneLightTimelineDocument spatial;
  target[0] = 0;
  if (RuntimeSceneLightTimelineGetLast(&spatial) && spatial.progress_track_index < spatial.timeline.track_count)
    snprintf(target, sizeof(target), "%s", spatial.timeline.tracks[spatial.progress_track_index].target_id);
  const MotionPathBinding *binding = NULL;
  for (size_t i = 0; i < paths->binding_count; ++i)
    if (target[0] && paths->bindings[i].enabled && !strcmp(paths->bindings[i].target_id, target)) binding = &paths->bindings[i];
  bool bound = binding && !strcmp(binding->path_id, path->id);
  snprintf(relationship, sizeof(relationship), "%s: %s", target[0] ? target : "Light (activate timeline first)", binding ? binding->path_id : "legacy position source");
  ray_tracing_text_draw_utf8_at(r, ray_tracing_font_runtime_get_ui_regular(r, 12, 9),
      relationship, x, y, SceneEditorChromeShellResolvePalette().text_primary);
  y += 26;
  const char *labels[] = {bound ? "Light attached to this route" : "Attach light on this route",
                         "Detach light: restore source", "Edit light route timing >"};
  enabled[0] = target[0] && !bound; enabled[1] = enabled[2] = bound;
  for (int i = 0; i < 3; ++i) {
    controls[i] = (SDL_Rect){x, y, width, 28};
    SceneEditorRenderButton(r, controls[i], labels[i], false, enabled[i]);
    if (y < clip.y || y + 28 > clip.y + clip.h) controls[i] = (SDL_Rect){0};
    y += 34;
  }
  ray_tracing_text_draw_utf8_at(r, ray_tracing_font_runtime_get_ui_regular(r, 12, 9),
      "Intensity stays independent.", x, y, SceneEditorChromeShellResolvePalette().text_primary);
  return y + 30;
}
bool SceneEditorLightPathPanelEvent(SDL_Event *e, const MotionPath *path,
    char *message, size_t size) {
  if (!path || e->type != SDL_MOUSEBUTTONDOWN || e->button.button != SDL_BUTTON_LEFT) return false;
  for (int i = 0; i < 3; ++i) {
    if (controls[i].w <= 0 || !SDL_PointInRect(&(SDL_Point){e->button.x, e->button.y}, &controls[i])) continue;
    if (!enabled[i]) return true;
    if (i < 2) {
      SceneEditorMotionPathBindLight(path->id, i == 0, SceneEditorDocumentRevision(), message, size);
    } else {
      static TimelineDocument doc;
      if (SceneEditorDocumentGetTimeline(&doc) == TIMELINE_STATUS_OK)
        for (size_t j = 0; j < doc.track_count; ++j)
          if (doc.tracks[j].enabled && !strcmp(doc.tracks[j].property_id, MOTION_LIGHT_PROGRESS_PROPERTY)) {
            SceneEditorTimelineSelectTrack(j);
            SceneEditorRenderAuthoringSetTiming(true);
            break;
          }
    }
    return true;
  }
  return false;
}
bool SceneEditorLightPathPanelControl(const char *name, SDL_Rect *out) {
  const char *names[] = {"path_light_attach", "path_light_detach", "path_light_timing"};
  for (int i = 0; i < 3; ++i)
    if (!strcmp(name, names[i])) { *out = controls[i]; return out->w > 0; }
  return false;
}
