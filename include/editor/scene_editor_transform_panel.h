#ifndef SCENE_EDITOR_TRANSFORM_PANEL_H
#define SCENE_EDITOR_TRANSFORM_PANEL_H

#include <SDL2/SDL.h>
#include <stdbool.h>

int SceneEditorTransformPanelRender(SDL_Renderer* renderer,
                                    SDL_Rect bounds,
                                    int top_y,
                                    int bottom_y);
bool SceneEditorTransformPanelHandleEvent(const SDL_Event* event);
bool SceneEditorTransformPanelInteractionActive(void);
bool SceneEditorTransformPanelPoll(void);
void SceneEditorTransformPanelReset(void);

#endif
