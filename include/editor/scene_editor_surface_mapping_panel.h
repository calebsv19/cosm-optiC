#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
int SceneEditorSurfaceMappingPanelRender(SDL_Renderer* renderer,SDL_Rect bounds,int y,int index,bool editable);
bool SceneEditorSurfaceMappingPanelEvent(const SDL_Event* event,int index);
void SceneEditorSurfaceMappingPanelRelease(const SDL_Event* event);
void SceneEditorSurfaceMappingPanelReset(void);
bool SceneEditorSurfaceMappingPanelActive(void);
/* Stable control identities for automation/native acceptance. */
bool SceneEditorSurfaceMappingPanelControl(const char* name,SDL_Rect* out);
