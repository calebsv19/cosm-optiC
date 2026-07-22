#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_pane_host.h"

void SceneEditorLightTimelineReset(void);
void SceneEditorLightTimelineSyncRuntime(void);
bool SceneEditorLightTimelineHasSelectedLight(void);
TimelineStatus SceneEditorLightTimelineSelectTargetId(const char* target_id);
const char* SceneEditorLightTimelineSelectedTargetId(void);
TimelineStatus SceneEditorLightTimelineInsertKey(int64_t frame, double progress);
TimelineStatus SceneEditorLightTimelineDeleteSelectedKey(void);
bool SceneEditorLightTimelineUndo(void);
bool SceneEditorLightTimelineRedo(void);
bool SceneEditorLightTimelineInteractionActive(void);
bool SceneEditorLightTimelineToggle(SceneEditorPaneHost* pane_host);
bool SceneEditorLightTimelineHandleEvent(
    SDL_Event* event,
    SceneEditorPaneHost* pane_host,
    const SceneEditorPaneLayout* layout,
    const SceneEditorDigestOverlayNavState* nav_state);
void SceneEditorLightTimelineRenderViewportProxies(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    int mouse_x,
    int mouse_y);
void SceneEditorLightTimelineRenderPanel(
    SDL_Renderer* renderer,
    const SceneEditorPaneLayout* layout);

#endif
