#pragma once
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
bool SceneEditorAgentMaterialReadOnly(const char *method);
bool SceneEditorAgentMaterialDispatch(const char *method, json_object *params,
    json_object *response, char *message, size_t size);
