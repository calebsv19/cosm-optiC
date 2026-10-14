#ifndef SCENE_EDITOR_AGENT_ANIMATION_H
#define SCENE_EDITOR_AGENT_ANIMATION_H
#include <stdbool.h>
#include <stddef.h>
#include <json-c/json.h>
bool SceneEditorAgentAnimationReadOnly(const char* name);
bool SceneEditorAgentAnimationDispatch(const char* name,json_object* params,json_object* response,char* message,size_t size);
#endif
