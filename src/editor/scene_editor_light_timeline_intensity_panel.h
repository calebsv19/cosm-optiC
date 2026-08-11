#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_INTENSITY_PANEL_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_INTENSITY_PANEL_H

#include "scene_editor_light_timeline_panel.h"

void scene_editor_light_timeline_intensity_panel_render(
    SDL_Renderer* renderer,
    const SDL_Rect* panel,
    const RuntimeSceneLightTimelineDocument* document,
    const SceneEditorLightTimelinePanelState* state);

#endif
