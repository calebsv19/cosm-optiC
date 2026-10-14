#ifndef SCENE_EDITOR_AGENT_MESH_H
#define SCENE_EDITOR_AGENT_MESH_H
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
bool SceneEditorAgentMeshReadOnly(const char *name);
bool SceneEditorAgentMeshDispatch(const char *name,json_object *params,json_object *response,char *message,size_t size);
#endif
