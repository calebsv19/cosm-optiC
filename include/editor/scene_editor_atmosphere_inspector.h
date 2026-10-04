#ifndef SCENE_EDITOR_ATMOSPHERE_INSPECTOR_H
#define SCENE_EDITOR_ATMOSPHERE_INSPECTOR_H
#include <SDL2/SDL.h>
#include <stdbool.h>
int SceneEditorAtmosphereInspectorDraw(SDL_Renderer* renderer,SDL_Rect bounds);
bool SceneEditorAtmosphereInspectorEvent(SDL_Event* event);
bool SceneEditorAtmosphereInspectorActive(void);
void SceneEditorAtmosphereInspectorReset(void);
void SceneEditorAtmosphereInspectorBeginFrame(void);
bool SceneEditorAtmosphereInspectorControl(const char* name,SDL_Rect* out);
#endif
