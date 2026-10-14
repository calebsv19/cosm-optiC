#ifndef SCENE_EDITOR_MOTION_PATH_PRESERVE_H
#define SCENE_EDITOR_MOTION_PATH_PRESERVE_H
#include <json-c/json.h>
/* Overlay semantic changes onto retained data, matching array rows by identity. */
json_object *SceneEditorMotionRetain(json_object *raw,json_object *before,json_object *after);
#endif
