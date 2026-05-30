#include "config/scene/config_scene_material_persistence.h"

#include <stdio.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "render/runtime_material_texture_stack_3d.h"

static bool ConfigJsonGetDouble(struct json_object* obj, const char* key, double* out_value) {
    struct json_object* value = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value)) return false;
    if (!json_object_is_type(value, json_type_double) &&
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out_value = json_object_get_double(value);
    return true;
}

static bool ConfigJsonGetInt(struct json_object* obj, const char* key, int* out_value) {
    struct json_object* value = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value)) return false;
    if (!json_object_is_type(value, json_type_int) &&
        !json_object_is_type(value, json_type_double)) {
        return false;
    }
    *out_value = json_object_get_int(value);
    return true;
}

static void ConfigSaveTextureParams(struct json_object* target,
                                    RuntimeMaterialTexture3DParams params) {
    if (!target) return;
    params = RuntimeMaterialTexture3DNormalizeParams(params);
    json_object_object_add(target, "patternMode", json_object_new_int(params.patternMode));
    json_object_object_add(target, "coverage", json_object_new_double(params.coverage));
    json_object_object_add(target, "grain", json_object_new_double(params.grain));
    json_object_object_add(target, "edgeSoftness", json_object_new_double(params.edgeSoftness));
    json_object_object_add(target, "contrast", json_object_new_double(params.contrast));
    json_object_object_add(target, "flow", json_object_new_double(params.flow));
    json_object_object_add(target, "colorDepth", json_object_new_double(params.colorDepth));
    json_object_object_add(target, "surfaceDamage", json_object_new_double(params.surfaceDamage));
    json_object_object_add(target, "seed", json_object_new_int(params.seed));
}

static struct json_object* ConfigSaveTexturePlacement(
    const RuntimeMaterialTexture3DPlacement* placement) {
    struct json_object* placement_obj = json_object_new_object();
    if (!placement_obj || !placement) return placement_obj;
    json_object_object_add(placement_obj, "offsetU", json_object_new_double(placement->offsetU));
    json_object_object_add(placement_obj, "offsetV", json_object_new_double(placement->offsetV));
    json_object_object_add(placement_obj, "scale", json_object_new_double(placement->scale));
    json_object_object_add(placement_obj, "strength", json_object_new_double(placement->strength));
    json_object_object_add(placement_obj, "rotation", json_object_new_double(placement->rotation));
    return placement_obj;
}

struct json_object* ConfigSaveMaterialTextureStackForObject(int scene_object_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    bool explicit_stack = SceneEditorMaterialStackGetObjectStack(scene_object_index, &stack);
    bool legacy_texture = false;
    struct json_object* stack_obj = NULL;
    struct json_object* layers = NULL;
    const SceneObject* obj = NULL;

    if (scene_object_index < 0 || scene_object_index >= sceneSettings.objectCount) return NULL;
    obj = &sceneSettings.sceneObjects[scene_object_index];
    legacy_texture =
        obj->textureId != 0 ||
        obj->textureOffsetU != 0.0 ||
        obj->textureOffsetV != 0.0 ||
        obj->textureScale != 1.0 ||
        obj->textureStrength != 0.0 ||
        obj->texturePatternMode != 0 ||
        obj->textureCoverage != 0.5 ||
        obj->textureGrain != 0.5 ||
        obj->textureEdgeSoftness != 0.5 ||
        obj->textureContrast != 0.5 ||
        obj->textureFlow != 0.0 ||
        obj->textureColorDepth != 0.5 ||
        obj->textureSurfaceDamage != 0.5 ||
        obj->textureSeed != 0;
    if (!explicit_stack && !legacy_texture) return NULL;
    if (!explicit_stack && !RuntimeMaterialTextureStackBuildLegacyFromObject(obj, &stack)) {
        return NULL;
    }
    stack = RuntimeMaterialTextureStackNormalize(stack);

    stack_obj = json_object_new_object();
    layers = json_object_new_array();
    if (!stack_obj || !layers) {
        if (stack_obj) json_object_put(stack_obj);
        if (layers) json_object_put(layers);
        return NULL;
    }
    json_object_object_add(stack_obj, "version", json_object_new_int(1));
    for (int i = 0; i < stack.layerCount && i < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS; ++i) {
        RuntimeMaterialTextureLayer layer = RuntimeMaterialTextureLayerNormalize(stack.layers[i]);
        struct json_object* layer_obj = NULL;
        struct json_object* params = NULL;
        if (!layer.enabled || layer.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) continue;
        layer_obj = json_object_new_object();
        params = json_object_new_object();
        if (!layer_obj || !params) {
            if (layer_obj) json_object_put(layer_obj);
            if (params) json_object_put(params);
            json_object_put(layers);
            json_object_put(stack_obj);
            return NULL;
        }
        json_object_object_add(layer_obj, "id", json_object_new_string(layer.layerId));
        json_object_object_add(layer_obj, "name", json_object_new_string(layer.displayName));
        json_object_object_add(layer_obj,
                               "role",
                               json_object_new_string(layer.role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE
                                                          ? "base"
                                                          : "overlay"));
        json_object_object_add(layer_obj,
                               "kind",
                               json_object_new_string(RuntimeMaterialTextureLayerKindStableId(layer.kind)));
        json_object_object_add(layer_obj,
                               "blend",
                               json_object_new_string(
                                   RuntimeMaterialTextureLayerBlendModeStableId(layer.blendMode)));
        json_object_object_add(layer_obj, "enabled", json_object_new_boolean(layer.enabled));
        json_object_object_add(layer_obj, "opacity", json_object_new_double(layer.opacity));
        json_object_object_add(layer_obj, "placement", ConfigSaveTexturePlacement(&layer.placement));
        ConfigSaveTextureParams(params, layer.params);
        json_object_object_add(layer_obj, "parameters", params);
        json_object_object_add(layer_obj,
                               "roughnessInfluence",
                               json_object_new_double(layer.roughnessInfluence));
        json_object_object_add(layer_obj,
                               "reflectivityInfluence",
                               json_object_new_double(layer.reflectivityInfluence));
        json_object_object_add(layer_obj,
                               "specularInfluence",
                               json_object_new_double(layer.specularInfluence));
        json_object_object_add(layer_obj,
                               "diffuseInfluence",
                               json_object_new_double(layer.diffuseInfluence));
        json_object_object_add(layer_obj,
                               "transparencyInfluence",
                               json_object_new_double(layer.transparencyInfluence));
        json_object_array_add(layers, layer_obj);
    }
    json_object_object_add(stack_obj, "layers", layers);
    return stack_obj;
}

struct json_object* SaveMaterialFacePlacementsForObject(int scene_object_index) {
    int count = SceneEditorMaterialFacePlacementOverrideCountForObject(scene_object_index);
    struct json_object* placements = NULL;
    if (count <= 0) return NULL;
    placements = json_object_new_array();
    if (!placements) return NULL;
    for (int i = 0; i < count; ++i) {
        SceneEditorMaterialFacePlacement placement;
        struct json_object* entry = NULL;
        if (!SceneEditorMaterialFacePlacementGetOverrideForObject(scene_object_index,
                                                                  i,
                                                                  &placement)) {
            continue;
        }
        entry = json_object_new_object();
        if (!entry) {
            json_object_put(placements);
            return NULL;
        }
        json_object_object_add(entry, "faceGroupIndex", json_object_new_int(placement.faceGroupIndex));
        json_object_object_add(entry, "textureId", json_object_new_int(placement.textureId));
        json_object_object_add(entry, "offsetU", json_object_new_double(placement.offsetU));
        json_object_object_add(entry, "offsetV", json_object_new_double(placement.offsetV));
        json_object_object_add(entry, "scale", json_object_new_double(placement.scale));
        json_object_object_add(entry, "strength", json_object_new_double(placement.strength));
        json_object_object_add(entry, "rotation", json_object_new_double(placement.rotation));
        {
            RuntimeMaterialTexture3DParams params =
                RuntimeMaterialTexture3DNormalizeParams(placement.params);
            struct json_object* parameters = json_object_new_object();
            if (!parameters) {
                json_object_put(placements);
                return NULL;
            }
            json_object_object_add(parameters, "patternMode", json_object_new_int(params.patternMode));
            json_object_object_add(parameters, "coverage", json_object_new_double(params.coverage));
            json_object_object_add(parameters, "grain", json_object_new_double(params.grain));
            json_object_object_add(parameters, "edgeSoftness", json_object_new_double(params.edgeSoftness));
            json_object_object_add(parameters, "contrast", json_object_new_double(params.contrast));
            json_object_object_add(parameters, "flow", json_object_new_double(params.flow));
            json_object_object_add(parameters, "colorDepth", json_object_new_double(params.colorDepth));
            json_object_object_add(parameters, "surfaceDamage", json_object_new_double(params.surfaceDamage));
            json_object_object_add(parameters, "seed", json_object_new_int(params.seed));
            json_object_object_add(entry, "parameters", parameters);
        }
        json_object_array_add(placements, entry);
    }
    if (json_object_array_length(placements) == 0u) {
        json_object_put(placements);
        return NULL;
    }
    return placements;
}

static void ConfigLoadTextureParams(struct json_object* obj,
                                    RuntimeMaterialTexture3DParams* params) {
    struct json_object* parameters = NULL;
    if (!obj || !params) return;
    if (json_object_object_get_ex(obj, "parameters", &parameters) &&
        json_object_is_type(parameters, json_type_object)) {
        ConfigJsonGetInt(parameters, "patternMode", &params->patternMode);
        ConfigJsonGetInt(parameters, "pattern_mode", &params->patternMode);
        ConfigJsonGetDouble(parameters, "coverage", &params->coverage);
        ConfigJsonGetDouble(parameters, "grain", &params->grain);
        ConfigJsonGetDouble(parameters, "edgeSoftness", &params->edgeSoftness);
        ConfigJsonGetDouble(parameters, "edge_softness", &params->edgeSoftness);
        ConfigJsonGetDouble(parameters, "contrast", &params->contrast);
        ConfigJsonGetDouble(parameters, "flow", &params->flow);
        ConfigJsonGetDouble(parameters, "colorDepth", &params->colorDepth);
        ConfigJsonGetDouble(parameters, "color_depth", &params->colorDepth);
        ConfigJsonGetDouble(parameters, "surfaceDamage", &params->surfaceDamage);
        ConfigJsonGetDouble(parameters, "surface_damage", &params->surfaceDamage);
        ConfigJsonGetInt(parameters, "seed", &params->seed);
    }
    ConfigJsonGetInt(obj, "texturePatternMode", &params->patternMode);
    ConfigJsonGetDouble(obj, "textureCoverage", &params->coverage);
    ConfigJsonGetDouble(obj, "textureGrain", &params->grain);
    ConfigJsonGetDouble(obj, "textureEdgeSoftness", &params->edgeSoftness);
    ConfigJsonGetDouble(obj, "textureContrast", &params->contrast);
    ConfigJsonGetDouble(obj, "textureFlow", &params->flow);
    ConfigJsonGetDouble(obj, "textureColorDepth", &params->colorDepth);
    ConfigJsonGetDouble(obj, "textureSurfaceDamage", &params->surfaceDamage);
    ConfigJsonGetInt(obj, "textureSeed", &params->seed);
    *params = RuntimeMaterialTexture3DNormalizeParams(*params);
}

static void ConfigLoadTexturePlacement(struct json_object* obj,
                                       RuntimeMaterialTexture3DPlacement* placement) {
    struct json_object* placement_obj = NULL;
    if (!obj || !placement) return;
    if (json_object_object_get_ex(obj, "placement", &placement_obj) &&
        json_object_is_type(placement_obj, json_type_object)) {
        ConfigJsonGetDouble(placement_obj, "offsetU", &placement->offsetU);
        ConfigJsonGetDouble(placement_obj, "offset_u", &placement->offsetU);
        ConfigJsonGetDouble(placement_obj, "offsetV", &placement->offsetV);
        ConfigJsonGetDouble(placement_obj, "offset_v", &placement->offsetV);
        ConfigJsonGetDouble(placement_obj, "scale", &placement->scale);
        ConfigJsonGetDouble(placement_obj, "strength", &placement->strength);
        ConfigJsonGetDouble(placement_obj, "rotation", &placement->rotation);
    }
    ConfigJsonGetDouble(obj, "offsetU", &placement->offsetU);
    ConfigJsonGetDouble(obj, "offset_u", &placement->offsetU);
    ConfigJsonGetDouble(obj, "offsetV", &placement->offsetV);
    ConfigJsonGetDouble(obj, "offset_v", &placement->offsetV);
    ConfigJsonGetDouble(obj, "scale", &placement->scale);
    ConfigJsonGetDouble(obj, "strength", &placement->strength);
    ConfigJsonGetDouble(obj, "rotation", &placement->rotation);
}

bool ConfigLoadMaterialTextureStack(struct json_object* obj, int scene_object_index) {
    struct json_object* stack_obj = NULL;
    struct json_object* layers = NULL;
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    size_t count = 0u;
    if (!obj || scene_object_index < 0) return false;
    if (!json_object_object_get_ex(obj, "materialTextureStack", &stack_obj)) {
        json_object_object_get_ex(obj, "material_texture_stack", &stack_obj);
    }
    if (!stack_obj ||
        !json_object_is_type(stack_obj, json_type_object) ||
        !json_object_object_get_ex(stack_obj, "layers", &layers) ||
        !json_object_is_type(layers, json_type_array)) {
        SceneEditorMaterialStackClearObjectStack(scene_object_index);
        return false;
    }

    count = json_object_array_length(layers);
    for (size_t i = 0u;
         i < count && stack.layerCount < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS;
         ++i) {
        struct json_object* layer_obj = json_object_array_get_idx(layers, i);
        struct json_object* value = NULL;
        const char* kind_id = NULL;
        RuntimeMaterialTextureLayerKind kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
        RuntimeMaterialTextureLayer layer;
        if (!layer_obj || !json_object_is_type(layer_obj, json_type_object)) continue;
        if (json_object_object_get_ex(layer_obj, "kind", &value)) {
            kind_id = json_object_get_string(value);
        }
        kind = RuntimeMaterialTextureLayerKindFromStableId(kind_id);
        if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) continue;
        layer = RuntimeMaterialTextureLayerKindIsBase(kind)
                    ? RuntimeMaterialTextureLayerMakeBase(kind)
                    : RuntimeMaterialTextureLayerMakeOverlay(kind);
        if (json_object_object_get_ex(layer_obj, "id", &value)) {
            snprintf(layer.layerId, sizeof(layer.layerId), "%s", json_object_get_string(value));
        }
        if (json_object_object_get_ex(layer_obj, "name", &value)) {
            snprintf(layer.displayName, sizeof(layer.displayName), "%s", json_object_get_string(value));
        }
        if (json_object_object_get_ex(layer_obj, "blend", &value)) {
            layer.blendMode =
                RuntimeMaterialTextureLayerBlendModeFromStableId(json_object_get_string(value));
        }
        if (json_object_object_get_ex(layer_obj, "enabled", &value)) {
            layer.enabled = json_object_get_boolean(value);
        }
        ConfigJsonGetDouble(layer_obj, "opacity", &layer.opacity);
        ConfigLoadTexturePlacement(layer_obj, &layer.placement);
        ConfigLoadTextureParams(layer_obj, &layer.params);
        layer.placement.params = layer.params;
        ConfigJsonGetDouble(layer_obj, "roughnessInfluence", &layer.roughnessInfluence);
        ConfigJsonGetDouble(layer_obj, "roughness_influence", &layer.roughnessInfluence);
        ConfigJsonGetDouble(layer_obj, "reflectivityInfluence", &layer.reflectivityInfluence);
        ConfigJsonGetDouble(layer_obj, "reflectivity_influence", &layer.reflectivityInfluence);
        ConfigJsonGetDouble(layer_obj, "specularInfluence", &layer.specularInfluence);
        ConfigJsonGetDouble(layer_obj, "specular_influence", &layer.specularInfluence);
        ConfigJsonGetDouble(layer_obj, "diffuseInfluence", &layer.diffuseInfluence);
        ConfigJsonGetDouble(layer_obj, "diffuse_influence", &layer.diffuseInfluence);
        ConfigJsonGetDouble(layer_obj, "transparencyInfluence", &layer.transparencyInfluence);
        ConfigJsonGetDouble(layer_obj, "transparency_influence", &layer.transparencyInfluence);
        stack.layers[stack.layerCount++] = RuntimeMaterialTextureLayerNormalize(layer);
    }
    if (stack.layerCount <= 0) {
        SceneEditorMaterialStackClearObjectStack(scene_object_index);
        return false;
    }
    return SceneEditorMaterialStackSetObjectStack(scene_object_index, &stack);
}

void LoadMaterialFacePlacements(struct json_object* obj, int scene_object_index) {
    struct json_object* placements = NULL;
    size_t count = 0u;
    if (!obj || scene_object_index < 0) return;
    if (!json_object_object_get_ex(obj, "materialFacePlacements", &placements) ||
        !json_object_is_type(placements, json_type_array)) {
        return;
    }
    count = json_object_array_length(placements);
    for (size_t i = 0u; i < count; ++i) {
        struct json_object* entry = json_object_array_get_idx(placements, i);
        struct json_object* face_group = NULL;
        SceneEditorMaterialFacePlacement placement;
        if (!entry || !json_object_is_type(entry, json_type_object)) continue;
        if (!json_object_object_get_ex(entry, "faceGroupIndex", &face_group) ||
            !json_object_is_type(face_group, json_type_int)) {
            continue;
        }
        memset(&placement, 0, sizeof(placement));
        placement.hasOverride = true;
        placement.sceneObjectIndex = scene_object_index;
        placement.faceGroupIndex = json_object_get_int(face_group);
        placement.textureId = sceneSettings.sceneObjects[scene_object_index].textureId;
        placement.offsetU = sceneSettings.sceneObjects[scene_object_index].textureOffsetU;
        placement.offsetV = sceneSettings.sceneObjects[scene_object_index].textureOffsetV;
        placement.scale = sceneSettings.sceneObjects[scene_object_index].textureScale;
        placement.strength = sceneSettings.sceneObjects[scene_object_index].textureStrength;
        placement.params =
            RuntimeMaterialTexture3DParamsFromObject(&sceneSettings.sceneObjects[scene_object_index]);
        ConfigJsonGetInt(entry, "textureId", &placement.textureId);
        ConfigJsonGetDouble(entry, "offsetU", &placement.offsetU);
        ConfigJsonGetDouble(entry, "offsetV", &placement.offsetV);
        ConfigJsonGetDouble(entry, "scale", &placement.scale);
        ConfigJsonGetDouble(entry, "strength", &placement.strength);
        ConfigJsonGetDouble(entry, "rotation", &placement.rotation);
        ConfigLoadTextureParams(entry, &placement.params);
        SceneEditorMaterialFacePlacementSetOverride(&placement);
    }
}
