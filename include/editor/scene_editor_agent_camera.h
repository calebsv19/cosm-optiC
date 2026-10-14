#ifndef SCENE_EDITOR_AGENT_CAMERA_H
#define SCENE_EDITOR_AGENT_CAMERA_H
#include <stdbool.h>
#include <stddef.h>
#include <json-c/json.h>
bool SceneEditorAgentCameraReadOnly(const char* name);
bool SceneEditorAgentCameraDispatch(const char* name,json_object* params,json_object* response,char* message,size_t size);
#endif
