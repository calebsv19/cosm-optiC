#ifndef SCENE_EDITOR_AGENT_LIGHTING_H
#define SCENE_EDITOR_AGENT_LIGHTING_H
#include <stdbool.h>
#include <stddef.h>
#include <json-c/json.h>
bool SceneEditorAgentLightingReadOnly(const char* name);
bool SceneEditorAgentLightingDispatch(const char* name,json_object* params,json_object* response,char* message,size_t size);
#endif
