#ifndef SCENE_EDITOR_EVENT_CAMERA_H
#define SCENE_EDITOR_EVENT_CAMERA_H
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
/* Both routes use the existing retained owner and revision/interaction guards.
 */
bool SceneEditorEventCameraCommand(const char *, json_object *,
                                   unsigned long long, char *, size_t);
bool SceneEditorEventCameraCandidates(json_object *, json_object *, char *,
                                      size_t);
#endif
