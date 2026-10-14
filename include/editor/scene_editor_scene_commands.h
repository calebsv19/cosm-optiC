#ifndef SCENE_EDITOR_SCENE_COMMANDS_H
#define SCENE_EDITOR_SCENE_COMMANDS_H
#include <stdbool.h>
#include <stddef.h>
#include "editor/scene_editor.h"
bool SceneEditorSceneCommand(const char* operation,const char* path,unsigned long long revision,char* message,size_t size);
void SceneEditorSceneDialogOpen(const char* operation);
bool SceneEditorSceneDialogEvent(SDL_Event* event);
void SceneEditorSceneDialogDraw(SDL_Renderer* renderer);
bool SceneEditorSceneDialogActive(void);
#endif
