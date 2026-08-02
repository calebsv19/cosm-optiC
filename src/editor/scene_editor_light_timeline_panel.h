#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_PANEL_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_PANEL_H

#include <stdbool.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include "animation/evaluated_scene_snapshot.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "scene_editor_light_timeline_edit.h"
#include "scene_editor_light_timeline_view.h"

typedef struct SceneEditorLightTimelinePanelState {
    int64_t current_frame;
    int selected_key_index;
    int selected_path_point_index;
    int mouse_x;
    int mouse_y;
    bool playing;
    bool pointer_over_panel;
    SceneEditorLightTimelineTraversalMode traversal_mode;
    const SceneEditorLightTimelineView* view;
    const RayEvaluatedSceneSnapshot* evaluated_scene;
} SceneEditorLightTimelinePanelState;

void scene_editor_light_timeline_panel_render(
    SDL_Renderer* renderer,
    const SDL_Rect* panel,
    const RuntimeSceneLightTimelineDocument* document,
    const SceneEditorLightTimelinePanelState* state);

#endif
