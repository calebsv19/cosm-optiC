#include "editor/material_editor_internal.h"

#include "editor/material_editor_face_preview.h"
#include "editor/material_editor_layer_model.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"

#include <stdio.h>

static void material_editor_apply_object_scope_to_all_faces(int scene_object_index) {
    if (scene_object_index < 0) return;
    SceneEditorMaterialFacePlacementResetObject(scene_object_index);
}

static bool material_editor_active_layer_id_for_object(const SceneObject* obj,
                                                       int scene_object_index,
                                                       char* out_layer_id,
                                                       size_t out_layer_id_size) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    int layer_index = -1;
    if (!out_layer_id || out_layer_id_size == 0) return false;
    out_layer_id[0] = '\0';
    if (!SceneEditorMaterialStackHasObjectStack(scene_object_index) ||
        !SceneEditorMaterialStackGetEffectiveObjectStack(obj, scene_object_index, &stack)) {
        return false;
    }
    layer_index = MaterialEditorLayerModelGetActiveIndex(obj, scene_object_index);
    if (layer_index < 0 || layer_index >= stack.layerCount || !stack.layers[layer_index].layerId[0]) {
        return false;
    }
    snprintf(out_layer_id, out_layer_id_size, "%s", stack.layers[layer_index].layerId);
    return true;
}

static bool material_editor_seed_face_override_from_active_layer(const SceneObject* obj,
                                                                 int scene_object_index,
                                                                 int face_group_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    SceneEditorMaterialFacePlacement placement = {0};
    RuntimeMaterialTextureLayer layer = {0};
    int layer_index = 0;
    if (!obj || scene_object_index < 0 || face_group_index < 0) return false;
    if (SceneEditorMaterialFacePlacementHasOverride(scene_object_index, face_group_index)) {
        return true;
    }
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(obj, scene_object_index, &stack)) {
        return true;
    }
    layer_index = MaterialEditorLayerModelGetActiveIndex(obj, scene_object_index);
    if (layer_index < 0 || layer_index >= stack.layerCount) return true;
    layer = stack.layers[layer_index];
    placement = SceneEditorMaterialFacePlacementGetEffective(obj,
                                                             scene_object_index,
                                                             face_group_index);
    placement.hasOverride = true;
    placement.sceneObjectIndex = scene_object_index;
    placement.faceGroupIndex = face_group_index;
    placement.layerIndex = layer_index;
    if (SceneEditorMaterialStackHasObjectStack(scene_object_index)) {
        snprintf(placement.layerId, sizeof(placement.layerId), "%s", layer.layerId);
    }
    placement.textureId = layer.placement.textureId;
    placement.offsetU = layer.placement.offsetU;
    placement.offsetV = layer.placement.offsetV;
    placement.scale = layer.placement.scale;
    placement.strength = layer.placement.strength;
    placement.rotation = layer.placement.rotation;
    placement.params = layer.params;
    return SceneEditorMaterialFacePlacementSetOverride(&placement);
}

static void material_editor_assign_object_params(SceneObject* obj,
                                                 RuntimeMaterialTexture3DParams params) {
    if (!obj) return;
    params = RuntimeMaterialTexture3DNormalizeParams(params);
    obj->texturePatternMode = params.patternMode;
    obj->textureCoverage = params.coverage;
    obj->textureGrain = params.grain;
    obj->textureEdgeSoftness = params.edgeSoftness;
    obj->textureContrast = params.contrast;
    obj->textureFlow = params.flow;
    obj->textureColorDepth = params.colorDepth;
    obj->textureSurfaceDamage = params.surfaceDamage;
    obj->textureSeed = params.seed;
}

bool MaterialEditorAddOverlayLayerToFocused(void) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_object_index < 0) return false;
    if (s_material_editor_active_face_group_index >= 0) {
        MaterialEditorClearTriangleSelection();
    }
    if (!MaterialEditorLayerModelAddOverlay(obj, focused_object_index)) return false;
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorDeleteActiveLayer(void) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_object_index < 0) return false;
    if (s_material_editor_active_face_group_index >= 0) {
        MaterialEditorClearTriangleSelection();
    }
    if (!MaterialEditorLayerModelDeleteActiveLayer(obj, focused_object_index)) return false;
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorMoveActiveLayer(int direction) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_object_index < 0) return false;
    if (s_material_editor_active_face_group_index >= 0) {
        MaterialEditorClearTriangleSelection();
    }
    if (!MaterialEditorLayerModelMoveActiveLayer(obj, focused_object_index, direction)) return false;
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorToggleActiveLayerEnabled(void) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_object_index < 0) return false;
    if (s_material_editor_active_face_group_index >= 0) {
        MaterialEditorClearTriangleSelection();
    }
    if (!MaterialEditorLayerModelToggleActiveLayerEnabled(obj, focused_object_index)) return false;
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorApplyLayerKindToFocused(RuntimeMaterialTextureLayerKind kind) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_object_index < 0) return false;
    if (s_material_editor_active_face_group_index >= 0) {
        MaterialEditorClearTriangleSelection();
    }
    if (!MaterialEditorLayerModelApplyLayerKind(obj, focused_object_index, kind)) return false;
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorApplyTextureKindToFocused(int texture_id) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj) return false;
    if (texture_id < 0 || texture_id > 2) return false;
    if (s_material_editor_active_face_group_index >= 0) {
        if (!material_editor_seed_face_override_from_active_layer(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index)) {
            return false;
        }
        if (!SceneEditorMaterialFacePlacementApplyTextureKind(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index,
                texture_id)) {
            return false;
        }
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (material_editor_use_object_layer_controls(obj)) {
        if (!MaterialEditorLayerModelApplyLegacyTextureKind(obj, focused_object_index, texture_id)) {
            return false;
        }
        obj->textureId = texture_id;
        material_editor_apply_object_scope_to_all_faces(focused_object_index);
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    obj->textureId = texture_id;
    material_editor_apply_object_scope_to_all_faces(focused_object_index);
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorApplySliderValueToFocused(MaterialEditorSliderKind kind, double value) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    SceneEditorMaterialFacePlacementField field =
        SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH;
    if (!obj) return false;
    value = material_editor_clamp01(value);
    if (s_material_editor_active_face_group_index >= 0) {
        if (!material_editor_seed_face_override_from_active_layer(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index)) {
            return false;
        }
        if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH;
        } else if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_SCALE;
        } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_U;
        } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) {
            field = SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_V;
        } else {
            return false;
        }
        if (!SceneEditorMaterialFacePlacementApplyNormalizedValue(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index,
                field,
                value)) {
            return false;
        }
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (material_editor_use_object_layer_controls(obj)) {
        if (!MaterialEditorLayerModelApplyPlacementValue(obj,
                                                         focused_object_index,
                                                         (int)kind,
                                                         value)) {
            return false;
        }
        material_editor_apply_object_scope_to_all_faces(focused_object_index);
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (kind == MATERIAL_EDITOR_SLIDER_STRENGTH) {
        obj->textureStrength = value;
    } else if (kind == MATERIAL_EDITOR_SLIDER_SCALE) {
        obj->textureScale = 0.25 + value * 7.75;
    } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_U) {
        obj->textureOffsetU = value;
    } else if (kind == MATERIAL_EDITOR_SLIDER_OFFSET_V) {
        obj->textureOffsetV = value;
    } else {
        return false;
    }
    MarkObjectDirty(obj);
    material_editor_apply_object_scope_to_all_faces(focused_object_index);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorApplyTexturePatternToFocused(int pattern_mode) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    RuntimeMaterialTexture3DParams params;
    if (!obj) return false;
    if (s_material_editor_active_face_group_index >= 0) {
        if (!material_editor_seed_face_override_from_active_layer(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index)) {
            return false;
        }
        if (!SceneEditorMaterialFacePlacementApplyTextureParamPatternMode(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index,
                pattern_mode)) {
            return false;
        }
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (material_editor_use_object_layer_controls(obj)) {
        if (!MaterialEditorLayerModelApplyPatternMode(obj, focused_object_index, pattern_mode)) {
            return false;
        }
        material_editor_apply_object_scope_to_all_faces(focused_object_index);
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    params = RuntimeMaterialTexture3DParamsFromObject(obj);
    params.patternMode = pattern_mode;
    material_editor_assign_object_params(obj, params);
    material_editor_apply_object_scope_to_all_faces(focused_object_index);
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorApplyTextureParamValueToFocused(MaterialEditorTextureParamKind kind, double value) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    RuntimeMaterialTexture3DParams params;
    if (!obj) return false;
    if (material_editor_texture_param_slot(kind) < 0) return false;
    value = material_editor_clamp01(value);
    if (s_material_editor_active_face_group_index >= 0) {
        if (!material_editor_seed_face_override_from_active_layer(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index)) {
            return false;
        }
        if (!SceneEditorMaterialFacePlacementApplyTextureParamNormalizedValue(
                obj,
                focused_object_index,
                s_material_editor_active_face_group_index,
                material_editor_texture_param_field(kind),
                value)) {
            return false;
        }
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (material_editor_use_object_layer_controls(obj)) {
        if (!MaterialEditorLayerModelApplyParamValue(obj,
                                                     focused_object_index,
                                                     (int)kind,
                                                     value)) {
            return false;
        }
        material_editor_apply_object_scope_to_all_faces(focused_object_index);
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    params = RuntimeMaterialTexture3DParamsFromObject(obj);
    if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COVERAGE) {
        params.coverage = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_GRAIN) {
        params.grain = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_EDGE_SOFTNESS) {
        params.edgeSoftness = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_CONTRAST) {
        params.contrast = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_FLOW) {
        params.flow = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_COLOR_DEPTH) {
        params.colorDepth = value;
    } else if (kind == MATERIAL_EDITOR_TEXTURE_PARAM_SURFACE_DAMAGE) {
        params.surfaceDamage = value;
    } else {
        return false;
    }
    material_editor_assign_object_params(obj, params);
    material_editor_apply_object_scope_to_all_faces(focused_object_index);
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorResetActiveFacePlacement(void) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    char layer_id[RUNTIME_MATERIAL_TEXTURE_LAYER_ID_SIZE];
    if (!obj || focused_object_index < 0 || s_material_editor_active_face_group_index < 0) {
        return false;
    }
    material_editor_active_layer_id_for_object(obj,
                                               focused_object_index,
                                               layer_id,
                                               sizeof(layer_id));
    if (!SceneEditorMaterialFacePlacementResetFaceLayer(
            focused_object_index,
            s_material_editor_active_face_group_index,
            layer_id)) {
        return false;
    }
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorCopyActiveFacePlacementToSelected(void) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    SceneEditorMaterialFacePlacement active;
    char layer_id[RUNTIME_MATERIAL_TEXTURE_LAYER_ID_SIZE];
    int copied = 0;
    int seen[MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES];
    int seen_count = 0;
    if (!obj || focused_object_index < 0 || s_material_editor_active_face_group_index < 0) {
        return false;
    }
    material_editor_active_layer_id_for_object(obj,
                                               focused_object_index,
                                               layer_id,
                                               sizeof(layer_id));
    active = SceneEditorMaterialFacePlacementGetEffectiveForLayer(
        obj,
        focused_object_index,
        s_material_editor_active_face_group_index,
        layer_id);
    for (int i = 0; i < MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES; ++i) {
        seen[i] = -1;
    }
    for (int i = 0; i < s_material_editor_selected_triangle_count; ++i) {
        int face_group = s_material_editor_selected_triangles[i].faceGroupIndex;
        bool already_seen = false;
        SceneEditorMaterialFacePlacement target = active;
        if (s_material_editor_selected_triangles[i].sceneObjectIndex != focused_object_index ||
            face_group < 0 ||
            face_group == s_material_editor_active_face_group_index) {
            continue;
        }
        for (int j = 0; j < seen_count; ++j) {
            if (seen[j] == face_group) {
                already_seen = true;
                break;
            }
        }
        if (already_seen) continue;
        if (seen_count < MATERIAL_EDITOR_MAX_SELECTED_TRIANGLES) {
            seen[seen_count] = face_group;
            seen_count += 1;
        }
        target.sceneObjectIndex = focused_object_index;
        target.faceGroupIndex = face_group;
        target.hasOverride = true;
        if (SceneEditorMaterialFacePlacementSetOverride(&target)) {
            copied += 1;
        }
    }
    if (copied <= 0) return false;
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}
