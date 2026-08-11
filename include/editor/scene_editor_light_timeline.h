#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "animation/evaluated_scene_snapshot.h"
#include "editor/scene_editor_digest_overlay.h"
#include "editor/scene_editor_pane_host.h"

typedef enum SceneEditorLightTimelineLane {
    SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION = 0,
    SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY
} SceneEditorLightTimelineLane;

void SceneEditorLightTimelineReset(void);
void SceneEditorLightTimelineSyncRuntime(void);
bool SceneEditorLightTimelineHasSelectedLight(void);
TimelineStatus SceneEditorLightTimelineSelectTargetId(const char* target_id);
const char* SceneEditorLightTimelineSelectedTargetId(void);
TimelineStatus SceneEditorLightTimelineInsertKey(int64_t frame, double progress);
TimelineStatus SceneEditorLightTimelineInsertIntensityKey(
    int64_t frame, double intensity);
TimelineStatus SceneEditorLightTimelineSelectLane(
    SceneEditorLightTimelineLane lane);
SceneEditorLightTimelineLane SceneEditorLightTimelineSelectedLane(void);
TimelineStatus SceneEditorLightTimelineDeleteSelectedKey(void);
TimelineStatus SceneEditorLightTimelineSetSelectedInterpolation(
    TimelineInterpolation interpolation);
bool SceneEditorLightTimelineUndo(void);
bool SceneEditorLightTimelineRedo(void);
bool SceneEditorLightTimelineInteractionActive(void);
bool SceneEditorLightTimelinePlaying(void);
bool SceneEditorLightTimelineTogglePlayback(void);
bool SceneEditorLightTimelineAdvancePlayback(void);
bool SceneEditorLightTimelineCopyEvaluatedScene(
    RayEvaluatedSceneSnapshot* out_snapshot);
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
