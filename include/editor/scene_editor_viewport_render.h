#ifndef SCENE_EDITOR_VIEWPORT_RENDER_H
#define SCENE_EDITOR_VIEWPORT_RENDER_H

#include <stdbool.h>

#include <SDL2/SDL.h>

typedef void (*SceneEditorViewportDigestRenderFn)(SDL_Renderer* renderer);

static inline bool SceneEditorViewportRenderShouldUseDigestLaneForState(
    bool route_controlled_3d,
    bool digest_valid) {
    return route_controlled_3d && digest_valid;
}

void SceneEditorViewportRenderDraw(SDL_Renderer* renderer,
                                   int current_mode,
                                   SceneEditorViewportDigestRenderFn digest_render);

#endif
