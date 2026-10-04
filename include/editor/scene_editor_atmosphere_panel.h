#ifndef SCENE_EDITOR_ATMOSPHERE_PANEL_H
#define SCENE_EDITOR_ATMOSPHERE_PANEL_H
#include <SDL2/SDL.h>
void SceneEditorAtmospherePanelBeginFrame(void);
int SceneEditorAtmospherePanelDraw(SDL_Renderer* renderer,SDL_Rect bounds);
bool SceneEditorAtmospherePanelEvent(SDL_Event* event);
bool SceneEditorAtmospherePanelControl(const char* name,SDL_Rect* out);
bool SceneEditorAtmospherePanelInteractionActive(void);
#endif
