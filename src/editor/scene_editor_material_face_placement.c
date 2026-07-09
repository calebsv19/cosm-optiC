#include "editor/scene_editor_material_face_placement.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_MAX_OVERRIDES 128

static SceneEditorMaterialFacePlacement
    s_face_placements[SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_MAX_OVERRIDES];

static double scene_editor_material_face_placement_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static int scene_editor_material_face_placement_clamp_texture_id(int texture_id) {
    if (texture_id < RUNTIME_MATERIAL_TEXTURE_3D_NONE) {
        return RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    }
    if (texture_id > RUNTIME_MATERIAL_TEXTURE_3D_FOG) {
        return RUNTIME_MATERIAL_TEXTURE_3D_FOG;
    }
    return texture_id;
}

static bool scene_editor_material_face_placement_layer_matches(
    const SceneEditorMaterialFacePlacement* placement,
    const char* layer_id) {
    const bool wants_legacy = !layer_id || !layer_id[0];
    if (!placement) return false;
    if (wants_legacy) return placement->layerId[0] == '\0';
    return strcmp(placement->layerId, layer_id) == 0;
}

static int scene_editor_material_face_placement_find_layer(int scene_object_index,
                                                           int face_group_index,
                                                           const char* layer_id) {
    if (scene_object_index < 0 || face_group_index < 0) return -1;
    for (int i = 0; i < SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_MAX_OVERRIDES; ++i) {
        if (s_face_placements[i].hasOverride &&
            s_face_placements[i].sceneObjectIndex == scene_object_index &&
            s_face_placements[i].faceGroupIndex == face_group_index &&
            scene_editor_material_face_placement_layer_matches(&s_face_placements[i],
                                                               layer_id)) {
            return i;
        }
    }
    return -1;
}

static int scene_editor_material_face_placement_find(int scene_object_index,
                                                     int face_group_index) {
    if (scene_object_index < 0 || face_group_index < 0) return -1;
    for (int i = 0; i < SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_MAX_OVERRIDES; ++i) {
        if (s_face_placements[i].hasOverride &&
            s_face_placements[i].sceneObjectIndex == scene_object_index &&
            s_face_placements[i].faceGroupIndex == face_group_index) {
            return i;
        }
    }
    return -1;
}

static int scene_editor_material_face_placement_find_or_alloc_layer(
    int scene_object_index,
    int face_group_index,
    const char* layer_id) {
    int existing =
        scene_editor_material_face_placement_find_layer(scene_object_index,
                                                        face_group_index,
                                                        layer_id);
    if (existing >= 0) return existing;
    if (scene_object_index < 0 || face_group_index < 0) return -1;
    for (int i = 0; i < SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_MAX_OVERRIDES; ++i) {
        if (!s_face_placements[i].hasOverride) {
            s_face_placements[i].hasOverride = true;
            s_face_placements[i].sceneObjectIndex = scene_object_index;
            s_face_placements[i].faceGroupIndex = face_group_index;
            if (layer_id && layer_id[0]) {
                snprintf(s_face_placements[i].layerId,
                         sizeof(s_face_placements[i].layerId),
                         "%s",
                         layer_id);
            }
            return i;
        }
    }
    return -1;
}

static SceneEditorMaterialFacePlacement scene_editor_material_face_placement_defaults(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index) {
    SceneEditorMaterialFacePlacement placement;
    memset(&placement, 0, sizeof(placement));
    placement.sceneObjectIndex = scene_object_index;
    placement.faceGroupIndex = face_group_index;
    placement.layerIndex = -1;
    placement.scale = 1.0;
    if (object) {
        placement.textureId = object->textureId;
        placement.offsetU = object->textureOffsetU;
        placement.offsetV = object->textureOffsetV;
        placement.scale = object->textureScale;
        placement.strength = object->textureStrength;
        placement.params = RuntimeMaterialTexture3DParamsFromObject(object);
    } else {
        placement.params = RuntimeMaterialTexture3DDefaultParams();
    }
    if (!(placement.scale > 1e-6)) placement.scale = 1.0;
    return placement;
}

void SceneEditorMaterialFacePlacementResetAll(void) {
    memset(s_face_placements, 0, sizeof(s_face_placements));
}

void SceneEditorMaterialFacePlacementResetObject(int scene_object_index) {
    if (scene_object_index < 0) return;
    for (int i = 0; i < SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_MAX_OVERRIDES; ++i) {
        if (s_face_placements[i].hasOverride &&
            s_face_placements[i].sceneObjectIndex == scene_object_index) {
            memset(&s_face_placements[i], 0, sizeof(s_face_placements[i]));
        }
    }
}

bool SceneEditorMaterialFacePlacementResetFace(int scene_object_index, int face_group_index) {
    int index = scene_editor_material_face_placement_find(scene_object_index, face_group_index);
    if (index < 0) return false;
    memset(&s_face_placements[index], 0, sizeof(s_face_placements[index]));
    return true;
}

bool SceneEditorMaterialFacePlacementResetFaceLayer(int scene_object_index,
                                                    int face_group_index,
                                                    const char* layer_id) {
    int index =
        scene_editor_material_face_placement_find_layer(scene_object_index,
                                                        face_group_index,
                                                        layer_id);
    if (index < 0) return false;
    memset(&s_face_placements[index], 0, sizeof(s_face_placements[index]));
    return true;
}

bool SceneEditorMaterialFacePlacementHasOverride(int scene_object_index, int face_group_index) {
    return scene_editor_material_face_placement_find(scene_object_index, face_group_index) >= 0;
}

bool SceneEditorMaterialFacePlacementHasOverrideForLayer(int scene_object_index,
                                                         int face_group_index,
                                                         const char* layer_id) {
    return scene_editor_material_face_placement_find_layer(scene_object_index,
                                                           face_group_index,
                                                           layer_id) >= 0;
}

bool SceneEditorMaterialFacePlacementObjectHasActiveTextureOverride(int scene_object_index) {
    if (scene_object_index < 0) return false;
    for (int i = 0; i < SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_MAX_OVERRIDES; ++i) {
        if (!s_face_placements[i].hasOverride ||
            s_face_placements[i].sceneObjectIndex != scene_object_index) {
            continue;
        }
        if (s_face_placements[i].textureId > RUNTIME_MATERIAL_TEXTURE_3D_NONE &&
            s_face_placements[i].strength > 1e-9) {
            return true;
        }
    }
    return false;
}

bool SceneEditorMaterialFacePlacementSetOverride(
    const SceneEditorMaterialFacePlacement* placement) {
    int index = -1;
    SceneEditorMaterialFacePlacement stored;
    if (!placement || placement->sceneObjectIndex < 0 || placement->faceGroupIndex < 0) {
        return false;
    }
    index = scene_editor_material_face_placement_find_or_alloc_layer(placement->sceneObjectIndex,
                                                                     placement->faceGroupIndex,
                                                                     placement->layerId);
    if (index < 0) return false;
    stored = *placement;
    stored.hasOverride = true;
    if (stored.layerIndex < -1) stored.layerIndex = -1;
    stored.textureId = scene_editor_material_face_placement_clamp_texture_id(stored.textureId);
    if (!(stored.scale > 1e-6)) stored.scale = 1.0;
    stored.strength = scene_editor_material_face_placement_clamp01(stored.strength);
    stored.params = RuntimeMaterialTexture3DNormalizeParams(stored.params);
    s_face_placements[index] = stored;
    return true;
}

int SceneEditorMaterialFacePlacementOverrideCountForObject(int scene_object_index) {
    int count = 0;
    if (scene_object_index < 0) return 0;
    for (int i = 0; i < SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_MAX_OVERRIDES; ++i) {
        if (s_face_placements[i].hasOverride &&
            s_face_placements[i].sceneObjectIndex == scene_object_index) {
            count += 1;
        }
    }
    return count;
}

bool SceneEditorMaterialFacePlacementGetOverrideForObject(
    int scene_object_index,
    int ordinal,
    SceneEditorMaterialFacePlacement* out_placement) {
    int count = 0;
    if (!out_placement || scene_object_index < 0 || ordinal < 0) return false;
    for (int i = 0; i < SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_MAX_OVERRIDES; ++i) {
        if (s_face_placements[i].hasOverride &&
            s_face_placements[i].sceneObjectIndex == scene_object_index) {
            if (count == ordinal) {
                *out_placement = s_face_placements[i];
                return true;
            }
            count += 1;
        }
    }
    return false;
}

SceneEditorMaterialFacePlacement SceneEditorMaterialFacePlacementGetEffective(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index) {
    int index = scene_editor_material_face_placement_find(scene_object_index, face_group_index);
    if (index >= 0) return s_face_placements[index];
    return scene_editor_material_face_placement_defaults(object, scene_object_index, face_group_index);
}

SceneEditorMaterialFacePlacement SceneEditorMaterialFacePlacementGetEffectiveForLayer(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index,
    const char* layer_id) {
    int index =
        scene_editor_material_face_placement_find_layer(scene_object_index,
                                                        face_group_index,
                                                        layer_id);
    if (index >= 0) return s_face_placements[index];
    return scene_editor_material_face_placement_defaults(object, scene_object_index, face_group_index);
}

static bool scene_editor_material_face_placement_apply_to_layer(
    const SceneEditorMaterialFacePlacement* placement,
    RuntimeMaterialTextureLayer* layer) {
    if (!placement || !layer) return false;
    layer->placement.textureId = placement->textureId;
    layer->placement.offsetU = placement->offsetU;
    layer->placement.offsetV = placement->offsetV;
    layer->placement.scale = placement->scale;
    layer->placement.strength = placement->strength;
    layer->placement.rotation = placement->rotation;
    layer->params = placement->params;
    layer->placement.params = placement->params;
    *layer = RuntimeMaterialTextureLayerNormalize(*layer);
    return true;
}

bool SceneEditorMaterialFacePlacementApplyOverridesToStack(const SceneObject* object,
                                                           int scene_object_index,
                                                           int face_group_index,
                                                           RuntimeMaterialTextureStack* stack) {
    bool applied = false;
    if (!object || !stack || scene_object_index < 0 || face_group_index < 0) {
        return false;
    }
    for (int i = 0; i < SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_MAX_OVERRIDES; ++i) {
        SceneEditorMaterialFacePlacement* placement = &s_face_placements[i];
        if (!placement->hasOverride ||
            placement->sceneObjectIndex != scene_object_index ||
            placement->faceGroupIndex != face_group_index) {
            continue;
        }
        if (!placement->layerId[0]) {
            continue;
        }
        for (int layer_i = 0; layer_i < stack->layerCount; ++layer_i) {
            if (strcmp(stack->layers[layer_i].layerId, placement->layerId) == 0) {
                scene_editor_material_face_placement_apply_to_layer(placement,
                                                                    &stack->layers[layer_i]);
                applied = true;
                break;
            }
        }
    }
    if (applied) {
        *stack = RuntimeMaterialTextureStackNormalize(*stack);
    }
    return applied;
}

bool SceneEditorMaterialFacePlacementApplyNormalizedValue(const SceneObject* object,
                                                         int scene_object_index,
                                                         int face_group_index,
                                                         SceneEditorMaterialFacePlacementField field,
                                                         double normalized_value) {
    int index = -1;
    SceneEditorMaterialFacePlacement effective;
    if (!object || scene_object_index < 0 || face_group_index < 0) return false;
    effective = SceneEditorMaterialFacePlacementGetEffective(object, scene_object_index, face_group_index);
    index = scene_editor_material_face_placement_find_or_alloc_layer(scene_object_index,
                                                                     face_group_index,
                                                                     effective.layerId);
    if (index < 0) return false;
    effective.hasOverride = true;
    effective.sceneObjectIndex = scene_object_index;
    effective.faceGroupIndex = face_group_index;
    normalized_value = scene_editor_material_face_placement_clamp01(normalized_value);
    if (field == SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH) {
        effective.strength = normalized_value;
    } else if (field == SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_SCALE) {
        effective.scale = 0.25 + normalized_value * 7.75;
    } else if (field == SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_U) {
        effective.offsetU = normalized_value;
    } else if (field == SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_V) {
        effective.offsetV = normalized_value;
    } else if (field == SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_ROTATION) {
        effective.rotation = (normalized_value - 0.5) * (M_PI * 2.0);
    } else {
        return false;
    }
    s_face_placements[index] = effective;
    return true;
}

bool SceneEditorMaterialFacePlacementApplyTextureKind(const SceneObject* object,
                                                      int scene_object_index,
                                                      int face_group_index,
                                                      int texture_id) {
    int index = -1;
    SceneEditorMaterialFacePlacement effective;
    if (!object || scene_object_index < 0 || face_group_index < 0) return false;
    effective = SceneEditorMaterialFacePlacementGetEffective(object,
                                                             scene_object_index,
                                                             face_group_index);
    index = scene_editor_material_face_placement_find_or_alloc_layer(scene_object_index,
                                                                     face_group_index,
                                                                     effective.layerId);
    if (index < 0) return false;
    effective.hasOverride = true;
    effective.sceneObjectIndex = scene_object_index;
    effective.faceGroupIndex = face_group_index;
    effective.textureId = scene_editor_material_face_placement_clamp_texture_id(texture_id);
    s_face_placements[index] = effective;
    return true;
}

bool SceneEditorMaterialFacePlacementApplyTextureParamNormalizedValue(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index,
    SceneEditorMaterialTextureParamField field,
    double normalized_value) {
    int index = -1;
    SceneEditorMaterialFacePlacement effective;
    if (!object || scene_object_index < 0 || face_group_index < 0) return false;
    effective = SceneEditorMaterialFacePlacementGetEffective(object, scene_object_index, face_group_index);
    index = scene_editor_material_face_placement_find_or_alloc_layer(scene_object_index,
                                                                     face_group_index,
                                                                     effective.layerId);
    if (index < 0) return false;
    effective.hasOverride = true;
    effective.sceneObjectIndex = scene_object_index;
    effective.faceGroupIndex = face_group_index;
    normalized_value = scene_editor_material_face_placement_clamp01(normalized_value);
    if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_COVERAGE) {
        effective.params.coverage = normalized_value;
    } else if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_GRAIN) {
        effective.params.grain = normalized_value;
    } else if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_EDGE_SOFTNESS) {
        effective.params.edgeSoftness = normalized_value;
    } else if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_CONTRAST) {
        effective.params.contrast = normalized_value;
    } else if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_FLOW) {
        effective.params.flow = normalized_value;
    } else if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_COLOR_DEPTH) {
        effective.params.colorDepth = normalized_value;
    } else if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_SURFACE_DAMAGE) {
        effective.params.surfaceDamage = normalized_value;
    } else if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_SEED) {
        effective.params.seed = (int)lround(normalized_value * 9999.0);
    } else {
        return false;
    }
    effective.params = RuntimeMaterialTexture3DNormalizeParams(effective.params);
    s_face_placements[index] = effective;
    return true;
}

bool SceneEditorMaterialFacePlacementApplyTextureParamPatternMode(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index,
    int pattern_mode) {
    int index = -1;
    SceneEditorMaterialFacePlacement effective;
    if (!object || scene_object_index < 0 || face_group_index < 0) return false;
    effective = SceneEditorMaterialFacePlacementGetEffective(object, scene_object_index, face_group_index);
    index = scene_editor_material_face_placement_find_or_alloc_layer(scene_object_index,
                                                                     face_group_index,
                                                                     effective.layerId);
    if (index < 0) return false;
    effective.hasOverride = true;
    effective.sceneObjectIndex = scene_object_index;
    effective.faceGroupIndex = face_group_index;
    effective.params.patternMode = pattern_mode;
    effective.params = RuntimeMaterialTexture3DNormalizeParams(effective.params);
    s_face_placements[index] = effective;
    return true;
}

double SceneEditorMaterialFacePlacementGetNormalizedValue(const SceneObject* object,
                                                         int scene_object_index,
                                                         int face_group_index,
                                                         SceneEditorMaterialFacePlacementField field) {
    SceneEditorMaterialFacePlacement placement =
        SceneEditorMaterialFacePlacementGetEffective(object, scene_object_index, face_group_index);
    if (field == SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_STRENGTH) {
        return scene_editor_material_face_placement_clamp01(placement.strength);
    }
    if (field == SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_SCALE) {
        double value = placement.scale;
        if (value < 0.25) value = 0.25;
        if (value > 8.0) value = 8.0;
        return (value - 0.25) / 7.75;
    }
    if (field == SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_U) {
        return scene_editor_material_face_placement_clamp01(placement.offsetU);
    }
    if (field == SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_OFFSET_V) {
        return scene_editor_material_face_placement_clamp01(placement.offsetV);
    }
    if (field == SCENE_EDITOR_MATERIAL_FACE_PLACEMENT_ROTATION) {
        return scene_editor_material_face_placement_clamp01((placement.rotation / (M_PI * 2.0)) + 0.5);
    }
    return 0.0;
}

double SceneEditorMaterialFacePlacementGetTextureParamNormalizedValue(
    const SceneObject* object,
    int scene_object_index,
    int face_group_index,
    SceneEditorMaterialTextureParamField field) {
    SceneEditorMaterialFacePlacement placement =
        SceneEditorMaterialFacePlacementGetEffective(object, scene_object_index, face_group_index);
    RuntimeMaterialTexture3DParams params = RuntimeMaterialTexture3DNormalizeParams(placement.params);
    if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_PATTERN) {
        return scene_editor_material_face_placement_clamp01((double)params.patternMode / 3.0);
    }
    if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_COVERAGE) return params.coverage;
    if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_GRAIN) return params.grain;
    if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_EDGE_SOFTNESS) return params.edgeSoftness;
    if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_CONTRAST) return params.contrast;
    if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_FLOW) return params.flow;
    if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_COLOR_DEPTH) return params.colorDepth;
    if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_SURFACE_DAMAGE) return params.surfaceDamage;
    if (field == SCENE_EDITOR_MATERIAL_TEXTURE_PARAM_SEED) {
        return scene_editor_material_face_placement_clamp01((double)params.seed / 9999.0);
    }
    return 0.0;
}

void SceneEditorMaterialFacePlacementResolveIslandUV(int local_triangle_index,
                                                    double bary_u,
                                                    double bary_v,
                                                    double bary_w,
                                                    double* out_u,
                                                    double* out_v) {
    double u = bary_v;
    double v = bary_w;
    if ((local_triangle_index & 1) == 0) {
        u = bary_v + bary_w;
        v = bary_w;
    } else {
        u = bary_v;
        v = bary_v + bary_w;
    }
    if (out_u) *out_u = u;
    if (out_v) *out_v = v;
    (void)bary_u;
}

RuntimeMaterialTexture3DPlacement SceneEditorMaterialFacePlacementToRuntime(
    const SceneEditorMaterialFacePlacement* placement) {
    RuntimeMaterialTexture3DPlacement runtime;
    memset(&runtime, 0, sizeof(runtime));
    runtime.scale = 1.0;
    if (placement) {
        runtime.textureId = placement->textureId;
        runtime.offsetU = placement->offsetU;
        runtime.offsetV = placement->offsetV;
        runtime.scale = placement->scale;
        runtime.strength = placement->strength;
        runtime.rotation = placement->rotation;
        runtime.params = RuntimeMaterialTexture3DNormalizeParams(placement->params);
    } else {
        runtime.params = RuntimeMaterialTexture3DDefaultParams();
    }
    if (!(runtime.scale > 1e-6)) runtime.scale = 1.0;
    return runtime;
}
