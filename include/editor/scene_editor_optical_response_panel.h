#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
int SceneEditorOpticalResponsePanelRender(SDL_Renderer *r,SDL_Rect b,int index);
bool SceneEditorOpticalResponsePanelEvent(const SDL_Event *event,int index);
bool SceneEditorOpticalResponsePanelControl(const char *name,SDL_Rect *out);
void SceneEditorOpticalResponsePanelInvalidate(void);
