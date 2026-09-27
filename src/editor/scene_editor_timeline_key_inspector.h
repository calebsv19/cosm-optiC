#ifndef SCENE_EDITOR_TIMELINE_KEY_INSPECTOR_H
#define SCENE_EDITOR_TIMELINE_KEY_INSPECTOR_H
#include <SDL2/SDL.h>
#include <stdbool.h>
void SceneEditorTimelineKeyInspectorDraw(SDL_Renderer* renderer,SDL_Rect rect);
bool SceneEditorTimelineKeyInspectorEvent(SDL_Event* event);
void SceneEditorTimelineKeyInspectorReset(void);
bool SceneEditorTimelineKeyInspectorControl(const char* name,SDL_Rect* out);
#endif
