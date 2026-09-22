#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>
int SceneEditorSurfaceMaterialPanelRender(SDL_Renderer* renderer,SDL_Rect bounds,int index);
bool SceneEditorSurfaceMaterialPanelEvent(const SDL_Event* event,int index);
bool SceneEditorSurfaceMaterialPanelActive(void);
bool SceneEditorSurfaceMaterialPanelControl(const char* name,SDL_Rect* out);

/* Shared assignment/section shell for every material family. */
int SceneEditorSurfaceMaterialHeaderRender(SDL_Renderer* renderer, SDL_Rect bounds, int index);
bool SceneEditorSurfaceMaterialHeaderEvent(const SDL_Event* event, int index);
bool SceneEditorSurfaceMaterialHeaderModal(void);
int SceneEditorSurfaceMaterialSection(void); /* Appearance, Sources, Coordinates, Preview */

void SceneEditorSurfaceMaterialPanelInvalidateControls(void);
/* Poll native image chooser without blocking the editor; shutdown cancels owned work. */
bool SceneEditorSurfaceMaterialPanelPoll(void);
void SceneEditorSurfaceMaterialPanelShutdown(void);
