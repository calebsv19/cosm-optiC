#ifndef SCENE_EDITOR_SAVE_AS_ASSETS_H
#define SCENE_EDITOR_SAVE_AS_ASSETS_H
#include <stdbool.h>
#include <stddef.h>
bool SceneEditorSaveAsAssetsBegin(const char* target,char* stage,size_t capacity,char* message,size_t size);
bool SceneEditorSaveAsAssetsFinish(const char* stage,bool commit,char* message,size_t size);
#endif
