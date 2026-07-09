#include "editor/scene_editor_material_stack.h"

#include <string.h>

static bool s_scene_editor_material_stack_has_object_stack[MAX_OBJECTS];
static RuntimeMaterialTextureStack s_scene_editor_material_object_stacks[MAX_OBJECTS];

static bool scene_editor_material_stack_valid_index(int scene_object_index) {
    return scene_object_index >= 0 && scene_object_index < MAX_OBJECTS;
}

void SceneEditorMaterialStackResetAll(void) {
    memset(s_scene_editor_material_stack_has_object_stack,
           0,
           sizeof(s_scene_editor_material_stack_has_object_stack));
    memset(s_scene_editor_material_object_stacks,
           0,
           sizeof(s_scene_editor_material_object_stacks));
}

bool SceneEditorMaterialStackSetObjectStack(int scene_object_index,
                                            const RuntimeMaterialTextureStack* stack) {
    if (!scene_editor_material_stack_valid_index(scene_object_index) || !stack) return false;
    s_scene_editor_material_object_stacks[scene_object_index] =
        RuntimeMaterialTextureStackNormalize(*stack);
    s_scene_editor_material_stack_has_object_stack[scene_object_index] = true;
    return true;
}

bool SceneEditorMaterialStackClearObjectStack(int scene_object_index) {
    if (!scene_editor_material_stack_valid_index(scene_object_index)) return false;
    s_scene_editor_material_stack_has_object_stack[scene_object_index] = false;
    memset(&s_scene_editor_material_object_stacks[scene_object_index],
           0,
           sizeof(s_scene_editor_material_object_stacks[scene_object_index]));
    return true;
}

bool SceneEditorMaterialStackHasObjectStack(int scene_object_index) {
    if (!scene_editor_material_stack_valid_index(scene_object_index)) return false;
    return s_scene_editor_material_stack_has_object_stack[scene_object_index];
}

bool SceneEditorMaterialStackGetObjectStack(int scene_object_index,
                                            RuntimeMaterialTextureStack* out_stack) {
    if (!scene_editor_material_stack_valid_index(scene_object_index) ||
        !out_stack ||
        !s_scene_editor_material_stack_has_object_stack[scene_object_index]) {
        return false;
    }
    *out_stack = RuntimeMaterialTextureStackNormalize(
        s_scene_editor_material_object_stacks[scene_object_index]);
    return true;
}

bool SceneEditorMaterialStackGetEffectiveObjectStack(const SceneObject* object,
                                                     int scene_object_index,
                                                     RuntimeMaterialTextureStack* out_stack) {
    if (!out_stack) return false;
    if (SceneEditorMaterialStackGetObjectStack(scene_object_index, out_stack)) {
        return true;
    }
    return RuntimeMaterialTextureStackBuildLegacyFromObject(object, out_stack);
}
