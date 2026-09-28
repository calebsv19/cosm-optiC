#ifndef SCENE_EDITOR_OBJECT_TIMELINE_PANEL_H
#define SCENE_EDITOR_OBJECT_TIMELINE_PANEL_H
#include <SDL2/SDL.h>
#include <stdbool.h>
/* Object position drafts are UI state, committed together through the document. */
bool SceneEditorObjectTimelinePanelPending(void);
void SceneEditorObjectTimelinePanelReset(void);
int SceneEditorObjectTimelinePanelDraw(SDL_Renderer* renderer,SDL_Rect rect);
bool SceneEditorObjectTimelinePanelEvent(SDL_Event* event);
bool SceneEditorObjectTimelinePanelControl(const char* name,SDL_Rect* out);
#endif
