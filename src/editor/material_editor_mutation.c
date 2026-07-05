#include "editor/material_editor_internal.h"

#include "editor/material_editor_face_preview.h"
#include "editor/material_editor_layer_model.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "material/material.h"

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

static bool material_editor_focused_material_is_glass(const SceneObject* obj) {
    return obj && obj->material_id == MATERIAL_PRESET_TRANSPARENT;
}

static bool material_editor_focused_material_is_mirror(const SceneObject* obj) {
    return obj && obj->material_id == MATERIAL_PRESET_MIRROR;
}

static bool material_editor_focused_material_is_metal(const SceneObject* obj) {
    return obj && obj->material_id == MATERIAL_PRESET_ROUGH_METAL;
}

static double material_editor_clamp_ior(double value) {
    if (value < 1.0) return 1.0;
    if (value > 2.0) return 2.0;
    return value;
}

static double material_editor_clamp_absorption_distance(double value) {
    if (value < 0.5) return 0.5;
    if (value > 5.0) return 5.0;
    return value;
}

static double material_editor_clamp_signed_influence(double value) {
    if (value < -1.0) return -1.0;
    if (value > 1.0) return 1.0;
    return value;
}

static int material_editor_response_tint_step_color(const SceneObject* obj, double delta) {
    int step = (int)(delta * 255.0);
    int r = (int)SceneObjectColorR(obj) + step;
    int g = (int)SceneObjectColorG(obj) + step;
    int b = (int)SceneObjectColorB(obj) + step;
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return SceneObjectPackRGBBytes((Uint8)r, (Uint8)g, (Uint8)b);
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

const char* MaterialEditorMutationDestinationLabel(
    MaterialEditorMutationDestination destination) {
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_OBJECT_ASSIGNMENT) {
        return "object_assignment";
    }
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK) {
        return "material_stack";
    }
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE) {
        return "face_override";
    }
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_LEGACY_OBJECT_TEXTURE_FALLBACK) {
        return "legacy_object_texture_fallback";
    }
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_AUTHORED_TEXTURE_BINDING) {
        return "authored_texture_binding";
    }
    return "none";
}

const char* MaterialEditorPanelGroupLabel(MaterialEditorPanelGroup group) {
    if (group == MATERIAL_EDITOR_PANEL_GROUP_BASE_LAYER) {
        return "Base Layer";
    }
    if (group == MATERIAL_EDITOR_PANEL_GROUP_PHYSICAL_RESPONSE) {
        return "Physical Response";
    }
    if (group == MATERIAL_EDITOR_PANEL_GROUP_TEXTURE_BINDING) {
        return "Texture Binding";
    }
    if (group == MATERIAL_EDITOR_PANEL_GROUP_FACE_OVERRIDE) {
        return "Face Override";
    }
    if (group == MATERIAL_EDITOR_PANEL_GROUP_PREVIEW_READBACK) {
        return "Preview & Readback";
    }
    return "None";
}

MaterialEditorPanelGroup MaterialEditorPanelGroupForMutationDestination(
    MaterialEditorMutationDestination destination) {
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK) {
        return MATERIAL_EDITOR_PANEL_GROUP_BASE_LAYER;
    }
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE) {
        return MATERIAL_EDITOR_PANEL_GROUP_FACE_OVERRIDE;
    }
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_LEGACY_OBJECT_TEXTURE_FALLBACK ||
        destination == MATERIAL_EDITOR_MUTATION_DESTINATION_AUTHORED_TEXTURE_BINDING) {
        return MATERIAL_EDITOR_PANEL_GROUP_TEXTURE_BINDING;
    }
    return MATERIAL_EDITOR_PANEL_GROUP_NONE;
}

MaterialEditorMutationDestination
MaterialEditorMutationDestinationForFocusedTextureControls(void) {
    SceneObject* obj = material_editor_focused_object();
    if (!obj) return MATERIAL_EDITOR_MUTATION_DESTINATION_NONE;
    if (s_material_editor_active_face_group_index >= 0) {
        return MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE;
    }
    if (material_editor_use_object_layer_controls(obj)) {
        return MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK;
    }
    return MATERIAL_EDITOR_MUTATION_DESTINATION_LEGACY_OBJECT_TEXTURE_FALLBACK;
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
    MaterialEditorMutationDestination destination =
        MaterialEditorMutationDestinationForFocusedTextureControls();
    if (!obj) return false;
    if (texture_id < 0 || texture_id > 2) return false;
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE) {
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
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK) {
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
    MaterialEditorMutationDestination destination =
        MaterialEditorMutationDestinationForFocusedTextureControls();
    SceneEditorMaterialFacePlacementField field =
        SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH;
    if (!obj) return false;
    value = material_editor_clamp01(value);
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE) {
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
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK) {
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
    MaterialEditorMutationDestination destination =
        MaterialEditorMutationDestinationForFocusedTextureControls();
    RuntimeMaterialTexture3DParams params;
    if (!obj) return false;
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE) {
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
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK) {
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
    MaterialEditorMutationDestination destination =
        MaterialEditorMutationDestinationForFocusedTextureControls();
    RuntimeMaterialTexture3DParams params;
    if (!obj) return false;
    if (material_editor_texture_param_slot(kind) < 0) return false;
    value = material_editor_clamp01(value);
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_FACE_OVERRIDE) {
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
    if (destination == MATERIAL_EDITOR_MUTATION_DESTINATION_MATERIAL_STACK) {
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

bool MaterialEditorApplyLayerInfluenceValueToFocused(MaterialEditorLayerInfluenceKind kind,
                                                     double value) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_object_index < 0) return false;
    if (kind <= MATERIAL_EDITOR_LAYER_INFLUENCE_NONE ||
        kind > MATERIAL_EDITOR_LAYER_INFLUENCE_TRANSPARENCY) {
        return false;
    }
    if (!MaterialEditorLayerModelApplyInfluenceValue(obj,
                                                     focused_object_index,
                                                     kind,
                                                     material_editor_clamp_signed_influence(value))) {
        return false;
    }
    material_editor_apply_object_scope_to_all_faces(focused_object_index);
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorApplyLayerInfluenceStepToFocused(MaterialEditorLayerInfluenceKind kind,
                                                    double delta) {
    SceneObject* obj = material_editor_focused_object();
    RuntimeMaterialTextureLayer layer = {0};
    double current = 0.0;
    if (!obj) return false;
    if (!material_editor_get_active_layer(obj, NULL, &layer, NULL)) return false;
    if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_ROUGHNESS) {
        current = layer.roughnessInfluence;
    } else if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_REFLECTIVITY) {
        current = layer.reflectivityInfluence;
    } else if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_SPECULAR) {
        current = layer.specularInfluence;
    } else if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_DIFFUSE) {
        current = layer.diffuseInfluence;
    } else if (kind == MATERIAL_EDITOR_LAYER_INFLUENCE_TRANSPARENCY) {
        current = layer.transparencyInfluence;
    } else {
        return false;
    }
    return MaterialEditorApplyLayerInfluenceValueToFocused(
        kind,
        material_editor_clamp_signed_influence(current + delta));
}

bool MaterialEditorApplyResponseValueToFocused(MaterialEditorResponseField field, double value) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_object_index < 0) return false;
    if (material_editor_focused_material_is_mirror(obj) &&
        (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS ||
         field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY ||
         field == MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR)) {
        SceneObjectSeedMirrorResponseOverrideFromMaterial(obj);
        value = material_editor_clamp01(value);
        if (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS) {
            obj->mirrorRoughness = value;
            obj->roughness = value;
        } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY) {
            obj->mirrorReflectivity = value;
            obj->reflectivity = value;
        } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR) {
            obj->mirrorSpecular = value;
        }
        material_editor_apply_object_scope_to_all_faces(focused_object_index);
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (material_editor_focused_material_is_metal(obj) &&
        (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS ||
         field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY ||
         field == MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR)) {
        value = material_editor_clamp01(value);
        if (!MaterialEditorLayerModelApplyResponseValue(obj,
                                                        focused_object_index,
                                                        field,
                                                        value)) {
            return false;
        }
        material_editor_apply_object_scope_to_all_faces(focused_object_index);
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (!material_editor_focused_material_is_glass(obj)) return false;
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS ||
        field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY ||
        field == MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR) {
        value = material_editor_clamp01(value);
        if (!MaterialEditorLayerModelApplyResponseValue(obj,
                                                        focused_object_index,
                                                        field,
                                                        value)) {
            return false;
        }
        material_editor_apply_object_scope_to_all_faces(focused_object_index);
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION ||
        field == MATERIAL_EDITOR_RESPONSE_FIELD_IOR ||
        field == MATERIAL_EDITOR_RESPONSE_FIELD_ABSORPTION ||
        field == MATERIAL_EDITOR_RESPONSE_FIELD_THIN_WALLED) {
        SceneObjectSeedGlassTransportOverrideFromMaterial(obj);
        if (field == MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION) {
            obj->glassTransmission = material_editor_clamp01(value);
        } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_IOR) {
            obj->glassIor = material_editor_clamp_ior(value);
        } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_ABSORPTION) {
            obj->glassAbsorptionDistance =
                material_editor_clamp_absorption_distance(value);
        } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_THIN_WALLED) {
            obj->glassThinWalled = value >= 0.5;
        }
        material_editor_apply_object_scope_to_all_faces(focused_object_index);
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    return false;
}

bool MaterialEditorApplyResponseTintToFocused(int packed_color) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    if (!obj || focused_object_index < 0) return false;
    if (material_editor_focused_material_is_mirror(obj)) {
        SceneObjectSeedMirrorResponseOverrideFromMaterial(obj);
        obj->mirrorTint = packed_color & 0xFFFFFF;
        obj->color = obj->mirrorTint;
        material_editor_apply_object_scope_to_all_faces(focused_object_index);
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (material_editor_focused_material_is_metal(obj)) {
        obj->color = packed_color & 0xFFFFFF;
        material_editor_apply_object_scope_to_all_faces(focused_object_index);
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (!material_editor_focused_material_is_glass(obj)) return false;
    obj->color = packed_color & 0xFFFFFF;
    material_editor_apply_object_scope_to_all_faces(focused_object_index);
    MarkObjectDirty(obj);
    MaterialEditorFacePreviewInvalidate();
    return true;
}

bool MaterialEditorApplyResponseStepToFocused(MaterialEditorResponseField field, double delta) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    RuntimeMaterialTextureLayer layer = {0};
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    double current = 0.0;
    if (!obj || focused_object_index < 0) return false;
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_TINT) {
        return MaterialEditorApplyResponseTintToFocused(
            material_editor_response_tint_step_color(obj, delta));
    }
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION ||
        field == MATERIAL_EDITOR_RESPONSE_FIELD_IOR ||
        field == MATERIAL_EDITOR_RESPONSE_FIELD_ABSORPTION ||
        field == MATERIAL_EDITOR_RESPONSE_FIELD_THIN_WALLED) {
        double transmission = 0.0;
        double ior = 1.0;
        double absorption_distance = 1.0;
        bool thin_walled = false;
        if (!material_editor_focused_material_is_glass(obj)) return false;
        SceneObjectResolveGlassTransport(obj,
                                         &transmission,
                                         &ior,
                                         &absorption_distance,
                                         &thin_walled);
        if (field == MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION) {
            current = transmission + delta;
        } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_IOR) {
            current = ior + delta;
        } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_ABSORPTION) {
            current = absorption_distance + delta;
        } else {
            current = thin_walled ? 0.0 : 1.0;
        }
        return MaterialEditorApplyResponseValueToFocused(field, current);
    }
    if (material_editor_focused_material_is_mirror(obj) &&
        (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS ||
         field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY ||
         field == MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR)) {
        double reflectivity = 0.0;
        double roughness = 0.0;
        double specular = 0.0;
        SceneObjectResolveMirrorResponse(obj, &reflectivity, &roughness, &specular, NULL);
        if (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS) {
            current = roughness;
        } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY) {
            current = reflectivity;
        } else {
            current = specular;
        }
        return MaterialEditorApplyResponseValueToFocused(field,
                                                         material_editor_clamp01(current + delta));
    }
    if (material_editor_focused_material_is_metal(obj) &&
        (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS ||
         field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY ||
         field == MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR)) {
        if (!material_editor_get_active_layer(obj, NULL, &layer, NULL)) {
            if (!MaterialEditorLayerModelEnsureEditableStack(obj, focused_object_index, &stack)) {
                return false;
            }
            layer = stack.layers[0];
        }
        if (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS) {
            current = layer.roughnessInfluence;
        } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY) {
            current = layer.reflectivityInfluence;
        } else {
            current = layer.specularInfluence;
        }
        return MaterialEditorApplyResponseValueToFocused(field,
                                                         material_editor_clamp01(current + delta));
    }
    if (!material_editor_get_active_layer(obj, NULL, &layer, NULL)) return false;
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS) {
        current = layer.roughnessInfluence;
    } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY) {
        current = layer.reflectivityInfluence;
    } else if (field == MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR) {
        current = layer.specularInfluence;
    } else {
        return false;
    }
    return MaterialEditorApplyResponseValueToFocused(field,
                                                     material_editor_clamp01(current + delta));
}

bool MaterialEditorApplyGlassOverlayForFocused(RuntimeMaterialTextureLayerKind kind) {
    SceneObject* obj = material_editor_focused_object();
    int focused_object_index = MaterialEditorResolveFocusedObjectIndex();
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    if (!obj || focused_object_index < 0) return false;
    if (!material_editor_focused_material_is_glass(obj)) return false;
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE ||
        kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID) {
        if (!MaterialEditorLayerModelEnsureEditableStack(obj, focused_object_index, &stack)) {
            return false;
        }
        if (!MaterialEditorLayerModelSetActiveIndex(obj, focused_object_index, 0)) {
            return false;
        }
        MarkObjectDirty(obj);
        MaterialEditorFacePreviewInvalidate();
        return true;
    }
    if (kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG &&
        kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES &&
        kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL &&
        kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME) {
        return false;
    }
    if (!MaterialEditorLayerModelEnsureEditableStack(obj, focused_object_index, &stack)) {
        return false;
    }
    for (int i = 1; i < stack.layerCount; ++i) {
        if (stack.layers[i].kind == kind) {
            if (!MaterialEditorLayerModelSetActiveIndex(obj, focused_object_index, i)) {
                return false;
            }
            MarkObjectDirty(obj);
            MaterialEditorFacePreviewInvalidate();
            return true;
        }
    }
    if (stack.layerCount >= RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) return false;
    stack.layers[stack.layerCount] = RuntimeMaterialTextureLayerMakeOverlay(kind);
    stack.layerCount += 1;
    if (!MaterialEditorLayerModelSetStack(focused_object_index, &stack)) return false;
    if (!MaterialEditorLayerModelSetActiveIndex(obj,
                                                focused_object_index,
                                                stack.layerCount - 1)) {
        return false;
    }
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
