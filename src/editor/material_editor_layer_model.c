#include "editor/material_editor_layer_model.h"

#include <stdio.h>
#include <string.h>

#include "editor/scene_editor_material_stack.h"

static int s_material_editor_layer_model_active_index = 0;

static double material_editor_layer_model_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double material_editor_layer_model_clamp_signed(double value) {
    if (value < -1.0) return -1.0;
    if (value > 1.0) return 1.0;
    return value;
}

static int material_editor_layer_model_clamp_index(const RuntimeMaterialTextureStack* stack,
                                                   int layer_index) {
    if (!stack || stack->layerCount <= 0) return 0;
    if (layer_index < 0) return 0;
    if (layer_index >= stack->layerCount) return stack->layerCount - 1;
    return layer_index;
}

static RuntimeMaterialTextureLayerKind material_editor_layer_model_next_overlay_kind(
    const RuntimeMaterialTextureStack* stack) {
    bool has_rust = false;
    bool has_grime = false;
    bool has_oil = false;
    bool has_fog = false;
    if (stack) {
        for (int i = 0; i < stack->layerCount; ++i) {
            if (stack->layers[i].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST) has_rust = true;
            if (stack->layers[i].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME) has_grime = true;
            if (stack->layers[i].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL) has_oil = true;
            if (stack->layers[i].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG) has_fog = true;
        }
    }
    if (!has_rust) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
    if (!has_grime) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME;
    if (!has_oil) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL;
    if (!has_fog) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG;
    return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
}

static void material_editor_layer_model_make_unique_id(RuntimeMaterialTextureStack* stack,
                                                       int layer_index) {
    const char* stable_id = NULL;
    if (!stack || layer_index < 0 || layer_index >= stack->layerCount) return;
    stable_id = RuntimeMaterialTextureLayerKindStableId(stack->layers[layer_index].kind);
    snprintf(stack->layers[layer_index].layerId,
             sizeof(stack->layers[layer_index].layerId),
             "%s_%d",
             stable_id ? stable_id : "layer",
             layer_index);
}

static bool material_editor_layer_model_update_active_layer(
    const SceneObject* object,
    int scene_object_index,
    RuntimeMaterialTextureLayer* out_layer,
    RuntimeMaterialTextureStack* out_stack,
    int* out_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    int index = 0;
    if (!out_layer || !MaterialEditorLayerModelEnsureEditableStack(object,
                                                                   scene_object_index,
                                                                   &stack)) {
        return false;
    }
    index = material_editor_layer_model_clamp_index(&stack,
                                                    s_material_editor_layer_model_active_index);
    *out_layer = stack.layers[index];
    if (out_stack) *out_stack = stack;
    if (out_index) *out_index = index;
    return true;
}

void MaterialEditorLayerModelReset(void) {
    s_material_editor_layer_model_active_index = 0;
}

int MaterialEditorLayerModelGetActiveIndex(const SceneObject* object, int scene_object_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    if (MaterialEditorLayerModelGetEffectiveStack(object, scene_object_index, &stack)) {
        if (!SceneEditorMaterialStackHasObjectStack(scene_object_index) &&
            s_material_editor_layer_model_active_index == 0 &&
            stack.layerCount > 1) {
            s_material_editor_layer_model_active_index = 1;
        }
        s_material_editor_layer_model_active_index =
            material_editor_layer_model_clamp_index(&stack,
                                                    s_material_editor_layer_model_active_index);
    } else {
        s_material_editor_layer_model_active_index = 0;
    }
    return s_material_editor_layer_model_active_index;
}

bool MaterialEditorLayerModelSetActiveIndex(const SceneObject* object,
                                            int scene_object_index,
                                            int layer_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    if (!MaterialEditorLayerModelGetEffectiveStack(object, scene_object_index, &stack)) {
        return false;
    }
    if (layer_index < 0 || layer_index >= stack.layerCount) return false;
    s_material_editor_layer_model_active_index = layer_index;
    return true;
}

bool MaterialEditorLayerModelGetEffectiveStack(const SceneObject* object,
                                               int scene_object_index,
                                               RuntimeMaterialTextureStack* out_stack) {
    return SceneEditorMaterialStackGetEffectiveObjectStack(object, scene_object_index, out_stack);
}

bool MaterialEditorLayerModelHasEditableStack(int scene_object_index) {
    return SceneEditorMaterialStackHasObjectStack(scene_object_index);
}

bool MaterialEditorLayerModelEnsureEditableStack(const SceneObject* object,
                                                 int scene_object_index,
                                                 RuntimeMaterialTextureStack* out_stack) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(object, scene_object_index, &stack)) {
        return false;
    }
    if (!SceneEditorMaterialStackHasObjectStack(scene_object_index)) {
        if (!SceneEditorMaterialStackSetObjectStack(scene_object_index, &stack)) {
            return false;
        }
    }
    if (out_stack) {
        return SceneEditorMaterialStackGetObjectStack(scene_object_index, out_stack);
    }
    return true;
}

bool MaterialEditorLayerModelSetStack(int scene_object_index,
                                      const RuntimeMaterialTextureStack* stack) {
    RuntimeMaterialTextureStack normalized = RuntimeMaterialTextureStackEmpty();
    if (!stack) return false;
    normalized = RuntimeMaterialTextureStackNormalize(*stack);
    if (!SceneEditorMaterialStackSetObjectStack(scene_object_index, &normalized)) {
        return false;
    }
    s_material_editor_layer_model_active_index =
        material_editor_layer_model_clamp_index(&normalized,
                                                s_material_editor_layer_model_active_index);
    return true;
}

bool MaterialEditorLayerModelAddOverlay(const SceneObject* object, int scene_object_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayerKind kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
    if (!MaterialEditorLayerModelEnsureEditableStack(object, scene_object_index, &stack)) {
        return false;
    }
    if (stack.layerCount >= RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) return false;
    kind = material_editor_layer_model_next_overlay_kind(&stack);
    stack.layers[stack.layerCount] = RuntimeMaterialTextureLayerMakeOverlay(kind);
    stack.layerCount += 1;
    material_editor_layer_model_make_unique_id(&stack, stack.layerCount - 1);
    s_material_editor_layer_model_active_index = stack.layerCount - 1;
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}

bool MaterialEditorLayerModelDeleteActiveLayer(const SceneObject* object, int scene_object_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    int index = 0;
    if (!MaterialEditorLayerModelEnsureEditableStack(object, scene_object_index, &stack)) {
        return false;
    }
    if (stack.layerCount <= 1) return false;
    index = material_editor_layer_model_clamp_index(&stack,
                                                    s_material_editor_layer_model_active_index);
    if (index == 0 && RuntimeMaterialTextureLayerKindIsBase(stack.layers[index].kind)) {
        return false;
    }
    for (int i = index; i + 1 < stack.layerCount; ++i) {
        stack.layers[i] = stack.layers[i + 1];
    }
    stack.layerCount -= 1;
    if (s_material_editor_layer_model_active_index >= stack.layerCount) {
        s_material_editor_layer_model_active_index = stack.layerCount - 1;
    }
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}

bool MaterialEditorLayerModelMoveActiveLayer(const SceneObject* object,
                                             int scene_object_index,
                                             int direction) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer tmp;
    int index = 0;
    int target = 0;
    if (direction == 0) return false;
    if (!MaterialEditorLayerModelEnsureEditableStack(object, scene_object_index, &stack)) {
        return false;
    }
    index = material_editor_layer_model_clamp_index(&stack,
                                                    s_material_editor_layer_model_active_index);
    target = index + (direction < 0 ? -1 : 1);
    if (target < 1 || target >= stack.layerCount) return false;
    if (index < 1) return false;
    tmp = stack.layers[index];
    stack.layers[index] = stack.layers[target];
    stack.layers[target] = tmp;
    s_material_editor_layer_model_active_index = target;
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}

bool MaterialEditorLayerModelToggleActiveLayerEnabled(const SceneObject* object,
                                                      int scene_object_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    int index = 0;
    if (!MaterialEditorLayerModelEnsureEditableStack(object, scene_object_index, &stack)) {
        return false;
    }
    index = material_editor_layer_model_clamp_index(&stack,
                                                    s_material_editor_layer_model_active_index);
    stack.layers[index].enabled = !stack.layers[index].enabled;
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}

bool MaterialEditorLayerModelApplyLayerKind(const SceneObject* object,
                                            int scene_object_index,
                                            RuntimeMaterialTextureLayerKind kind) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer previous;
    RuntimeMaterialTextureLayer replacement;
    int index = 0;
    if (!material_editor_layer_model_update_active_layer(object,
                                                         scene_object_index,
                                                         &previous,
                                                         &stack,
                                                         &index)) {
        return false;
    }
    if (RuntimeMaterialTextureLayerKindIsBase(previous.kind)) {
        if (!RuntimeMaterialTextureLayerKindIsBase(kind)) return false;
        replacement = RuntimeMaterialTextureLayerMakeBase(kind);
    } else {
        if (!RuntimeMaterialTextureLayerKindIsOverlay(kind)) return false;
        replacement = RuntimeMaterialTextureLayerMakeOverlay(kind);
    }
    replacement.enabled = previous.enabled;
    replacement.opacity = previous.opacity;
    replacement.placement = previous.placement;
    replacement.placement.textureId =
        replacement.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST
            ? RUNTIME_MATERIAL_TEXTURE_3D_RUST
            : replacement.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG
                  ? RUNTIME_MATERIAL_TEXTURE_3D_FOG
                  : RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    if (previous.kind == kind) {
        replacement.params = previous.params;
        replacement.placement.params = previous.params;
    } else {
        replacement.placement.params = replacement.params;
    }
    stack.layers[index] = RuntimeMaterialTextureLayerNormalize(replacement);
    material_editor_layer_model_make_unique_id(&stack, index);
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}

bool MaterialEditorLayerModelApplyLegacyTextureKind(const SceneObject* object,
                                                    int scene_object_index,
                                                    int texture_id) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer layer;
    int index = 0;
    if (texture_id < 0 || texture_id > 2) return false;
    if (!material_editor_layer_model_update_active_layer(object,
                                                         scene_object_index,
                                                         &layer,
                                                         &stack,
                                                         &index)) {
        return false;
    }
    if (texture_id == 0) {
        RuntimeMaterialTextureLayer base =
            RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);
        if (stack.layerCount > 0 &&
            RuntimeMaterialTextureLayerKindIsBase(stack.layers[0].kind)) {
            base = stack.layers[0];
            base.kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID;
            snprintf(base.displayName, sizeof(base.displayName), "%s", "Solid");
            snprintf(base.layerId, sizeof(base.layerId), "%s", "base_solid");
        }
        stack.layers[0] = RuntimeMaterialTextureLayerNormalize(base);
        for (int i = 1; i < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS; ++i) {
            memset(&stack.layers[i], 0, sizeof(stack.layers[i]));
        }
        stack.layerCount = 1;
        s_material_editor_layer_model_active_index = 0;
        return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
    }
    if (RuntimeMaterialTextureLayerKindIsBase(layer.kind)) {
        if (stack.layerCount >= RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) return false;
        index = stack.layerCount;
        stack.layerCount += 1;
        s_material_editor_layer_model_active_index = index;
    }
    stack.layers[index] = RuntimeMaterialTextureLayerMakeOverlay(
        texture_id == 1 ? RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST
                        : RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG);
    stack.layers[index].enabled = true;
    material_editor_layer_model_make_unique_id(&stack, index);
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}

bool MaterialEditorLayerModelApplyPlacementValue(const SceneObject* object,
                                                 int scene_object_index,
                                                 int field_kind,
                                                 double normalized_value) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer layer;
    int index = 0;
    normalized_value = material_editor_layer_model_clamp01(normalized_value);
    if (!material_editor_layer_model_update_active_layer(object,
                                                         scene_object_index,
                                                         &layer,
                                                         &stack,
                                                         &index)) {
        return false;
    }
    if (field_kind == 1) {
        layer.placement.strength = normalized_value;
        layer.opacity = normalized_value;
    } else if (field_kind == 2) {
        layer.placement.scale = 0.25 + normalized_value * 7.75;
    } else if (field_kind == 3) {
        layer.placement.offsetU = normalized_value;
    } else if (field_kind == 4) {
        layer.placement.offsetV = normalized_value;
    } else {
        return false;
    }
    stack.layers[index] = RuntimeMaterialTextureLayerNormalize(layer);
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}

bool MaterialEditorLayerModelApplyPatternMode(const SceneObject* object,
                                              int scene_object_index,
                                              int pattern_mode) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer layer;
    int index = 0;
    if (!material_editor_layer_model_update_active_layer(object,
                                                         scene_object_index,
                                                         &layer,
                                                         &stack,
                                                         &index)) {
        return false;
    }
    layer.params.patternMode = pattern_mode;
    layer.placement.params.patternMode = pattern_mode;
    stack.layers[index] = RuntimeMaterialTextureLayerNormalize(layer);
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}

bool MaterialEditorLayerModelApplyParamValue(const SceneObject* object,
                                             int scene_object_index,
                                             int param_kind,
                                             double normalized_value) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer layer;
    int index = 0;
    RuntimeMaterialTexture3DParams params;
    normalized_value = material_editor_layer_model_clamp01(normalized_value);
    if (!material_editor_layer_model_update_active_layer(object,
                                                         scene_object_index,
                                                         &layer,
                                                         &stack,
                                                         &index)) {
        return false;
    }
    params = layer.params;
    if (param_kind == 1) {
        params.coverage = normalized_value;
    } else if (param_kind == 2) {
        params.grain = normalized_value;
    } else if (param_kind == 3) {
        params.edgeSoftness = normalized_value;
    } else if (param_kind == 4) {
        params.contrast = normalized_value;
    } else if (param_kind == 5) {
        params.flow = normalized_value;
    } else if (param_kind == 6) {
        params.colorDepth = normalized_value;
    } else if (param_kind == 7) {
        params.surfaceDamage = normalized_value;
    } else {
        return false;
    }
    layer.params = params;
    layer.placement.params = params;
    stack.layers[index] = RuntimeMaterialTextureLayerNormalize(layer);
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}

bool MaterialEditorLayerModelApplyInfluenceValue(const SceneObject* object,
                                                 int scene_object_index,
                                                 MaterialEditorLayerInfluenceKind kind,
                                                 double signed_value) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer layer;
    int index = 0;
    signed_value = material_editor_layer_model_clamp_signed(signed_value);
    if (!material_editor_layer_model_update_active_layer(object,
                                                         scene_object_index,
                                                         &layer,
                                                         &stack,
                                                         &index)) {
        return false;
    }
    if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_ROUGHNESS) {
        layer.roughnessInfluence = signed_value;
    } else if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_REFLECTIVITY) {
        layer.reflectivityInfluence = signed_value;
    } else if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_SPECULAR) {
        layer.specularInfluence = signed_value;
    } else if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_DIFFUSE) {
        layer.diffuseInfluence = signed_value;
    } else if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_TRANSPARENCY) {
        layer.transparencyInfluence = signed_value;
    } else {
        return false;
    }
    stack.layers[index] = RuntimeMaterialTextureLayerNormalize(layer);
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}

bool MaterialEditorLayerModelApplyResponseValue(const SceneObject* object,
                                                int scene_object_index,
                                                MaterialEditorResponseField field,
                                                double normalized_value) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    RuntimeMaterialTextureLayer layer;
    int index = 0;
    normalized_value = material_editor_layer_model_clamp01(normalized_value);
    if (!material_editor_layer_model_update_active_layer(object,
                                                         scene_object_index,
                                                         &layer,
                                                         &stack,
                                                         &index)) {
        return false;
    }
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS) {
        layer.roughnessInfluence = normalized_value;
    } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY) {
        layer.reflectivityInfluence = normalized_value;
    } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR) {
        layer.specularInfluence = normalized_value;
    } else {
        return false;
    }
    stack.layers[index] = RuntimeMaterialTextureLayerNormalize(layer);
    return MaterialEditorLayerModelSetStack(scene_object_index, &stack);
}
