#ifndef SCENE_EDITOR_AGENT_MOTION_H
#define SCENE_EDITOR_AGENT_MOTION_H
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
bool SceneEditorAgentMotionReadOnly(const char *name);
bool SceneEditorAgentMotionDispatch(const char *name,json_object *params,json_object *response,char *message,size_t size);
#endif
