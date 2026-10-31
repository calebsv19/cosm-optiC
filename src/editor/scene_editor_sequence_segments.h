#include <json-c/json.h>
#include <stdbool.h>
bool SceneEditorSequenceSegments(json_object *scene, json_object *author,
                                 const char *action, json_object *params,
                                 unsigned long long revision);
