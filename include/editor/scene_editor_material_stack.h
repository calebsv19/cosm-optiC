#ifndef EDITOR_SCENE_EDITOR_MATERIAL_STACK_H
#define EDITOR_SCENE_EDITOR_MATERIAL_STACK_H

#include <stdbool.h>

#include "render/runtime_material_texture_stack_3d.h"
#include "scene/object_manager.h"

void SceneEditorMaterialStackResetAll(void);
bool SceneEditorMaterialStackSetObjectStack(int scene_object_index,
                                            const RuntimeMaterialTextureStack* stack);
bool SceneEditorMaterialStackClearObjectStack(int scene_object_index);
bool SceneEditorMaterialStackHasObjectStack(int scene_object_index);
bool SceneEditorMaterialStackGetObjectStack(int scene_object_index,
                                            RuntimeMaterialTextureStack* out_stack);
bool SceneEditorMaterialStackGetEffectiveObjectStack(const SceneObject* object,
                                                     int scene_object_index,
                                                     RuntimeMaterialTextureStack* out_stack);

#endif
