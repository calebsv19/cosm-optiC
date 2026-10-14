#ifndef SCENE_EDITOR_OPTICAL_RESPONSE_H
#define SCENE_EDITOR_OPTICAL_RESPONSE_H
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
/* Retained object-level dielectric response; graphs keep source ownership. */
json_object *SceneEditorOpticalResponseRead(int index,char *message,size_t size);
bool SceneEditorOpticalResponseSet(int index,json_object *settings,unsigned long long revision,char *message,size_t size);
#endif
