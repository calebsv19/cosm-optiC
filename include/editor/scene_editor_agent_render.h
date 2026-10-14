#ifndef SCENE_EDITOR_AGENT_RENDER_H
#define SCENE_EDITOR_AGENT_RENDER_H
#include <stdbool.h>
#include <stddef.h>
#include <json-c/json.h>
bool SceneEditorAgentRenderReadOnly(const char* name);
bool SceneEditorAgentRenderDispatch(const char* name,json_object* params,json_object* response,char* message,size_t size);
#endif
