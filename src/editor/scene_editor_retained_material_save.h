#ifndef SCENE_EDITOR_RETAINED_MATERIAL_SAVE_H
#define SCENE_EDITOR_RETAINED_MATERIAL_SAVE_H
#include <json-c/json.h>
void SceneEditorRetainedMaterialBaseline(json_object *encoded);
json_object *SceneEditorRetainedMaterialsForSave(json_object *encoded);
#endif
