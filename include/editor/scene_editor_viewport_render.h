#ifndef SCENE_EDITOR_VIEWPORT_RENDER_H
#define SCENE_EDITOR_VIEWPORT_RENDER_H

#include <SDL2/SDL.h>

typedef void (*SceneEditorViewportDigestRenderFn)(SDL_Renderer* renderer);

void SceneEditorViewportRenderDraw(SDL_Renderer* renderer,
                                   int current_mode,
                                   SceneEditorViewportDigestRenderFn digest_render);

#endif
