#ifndef SCENE_EDITOR_RETAINED_LIGHT_SAVE_H
#define SCENE_EDITOR_RETAINED_LIGHT_SAVE_H
#include <json-c/json.h>
/* Return an owned payload; preserve retained bytes when runtime meaning matches. */
json_object* SceneEditorRetainedLightForSave(json_object* encoded,double world_scale);
#endif
