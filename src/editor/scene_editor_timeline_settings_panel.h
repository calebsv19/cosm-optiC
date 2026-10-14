#ifndef SCENE_EDITOR_TIMELINE_SETTINGS_PANEL_H
#define SCENE_EDITOR_TIMELINE_SETTINGS_PANEL_H
#include <SDL2/SDL.h>
#include <stdbool.h>
int SceneEditorTimelineSettingsDraw(SDL_Renderer* renderer,SDL_Rect pane,int y);
bool SceneEditorTimelineSettingsEvent(SDL_Event* event);
bool SceneEditorTimelineSettingsActive(void);
void SceneEditorTimelineSettingsReset(void);
bool SceneEditorTimelineSettingsControl(const char* name,SDL_Rect* out);
#endif
