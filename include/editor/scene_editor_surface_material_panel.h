#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
int SceneEditorSurfaceMaterialPanelRender(SDL_Renderer* renderer,SDL_Rect bounds,int index);
bool SceneEditorSurfaceMaterialPanelEvent(const SDL_Event* event,int index);
bool SceneEditorSurfaceMaterialPanelActive(void);
bool SceneEditorSurfaceMaterialPanelControl(const char* name,SDL_Rect* out);
