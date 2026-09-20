#ifndef SCENE_EDITOR_RENAME_H
#define SCENE_EDITOR_RENAME_H
#include <SDL2/SDL.h>
#include <stdbool.h>
bool SceneEditorRenameBegin(void);
bool SceneEditorRenameHandleEvent(SDL_Event* event);
void SceneEditorRenameRender(SDL_Renderer* renderer);
void SceneEditorRenameCancel(void);
bool SceneEditorRenameActive(void);
#endif
