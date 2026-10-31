#ifndef SCENE_EDITOR_SEQUENCE_H
#define SCENE_EDITOR_SEQUENCE_H
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
bool SceneEditorSequenceReadOnly(const char *name);
bool SceneEditorSequenceDispatch(const char *name, json_object *params,
                                 json_object *response, char *message,
                                 size_t size);
bool SceneEditorSequenceCommand(const char *action, json_object *params,
                                unsigned long long revision, char *message,
                                size_t size);
json_object *SceneEditorSequenceRead(void);
#endif
