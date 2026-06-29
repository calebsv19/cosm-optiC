#include "render/runtime_material_texture_stack_3d.h"

#include <stdio.h>
#include <string.h>

static double runtime_material_texture_stack_clamp(double value,
                                                   double min_value,
                                                   double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_material_texture_stack_clamp01(double value) {
    return runtime_material_texture_stack_clamp(value, 0.0, 1.0);
}

static bool runtime_material_texture_stack_valid_kind(RuntimeMaterialTextureLayerKind kind) {
    return kind >= RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID &&
           kind <= RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR;
}

static void runtime_material_texture_stack_copy_text(char* dst,
                                                     size_t dst_size,
                                                     const char* src) {
    if (!dst || dst_size == 0u) return;
    if (!src) src = "";
    snprintf(dst, dst_size, "%s", src);
}

static RuntimeMaterialTexture3DKind runtime_material_texture_stack_legacy_texture_kind(
    RuntimeMaterialTextureLayerKind kind) {
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST) {
        return RUNTIME_MATERIAL_TEXTURE_3D_RUST;
    }
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG) {
        return RUNTIME_MATERIAL_TEXTURE_3D_FOG;
    }
    return RUNTIME_MATERIAL_TEXTURE_3D_NONE;
}

static RuntimeMaterialTextureLayerKind runtime_material_texture_stack_layer_kind_from_legacy(
    int texture_id) {
    if (texture_id == RUNTIME_MATERIAL_TEXTURE_3D_RUST) {
        return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
    }
    if (texture_id == RUNTIME_MATERIAL_TEXTURE_3D_FOG) {
        return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG;
    }
    return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
}

const char* RuntimeMaterialTextureLayerKindStableId(RuntimeMaterialTextureLayerKind kind) {
    switch (kind) {
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID:
            return "solid";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST:
            return "rust";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG:
            return "fog";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME:
            return "grime";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL:
            return "oil";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL:
            return "brushed_metal";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD:
            return "wood";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRICK:
            return "brick";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_CONCRETE:
            return "concrete";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_STONE:
            return "stone";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES:
            return "scratches";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR:
            return "edge_wear";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE:
        default:
            return "none";
    }
}

const char* RuntimeMaterialTextureLayerKindDisplayName(RuntimeMaterialTextureLayerKind kind) {
    switch (kind) {
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID:
            return "Solid";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST:
            return "Rust";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG:
            return "Fog";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME:
            return "Grime";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL:
            return "Oil";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL:
            return "Brushed Metal";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD:
            return "Wood";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRICK:
            return "Brick";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_CONCRETE:
            return "Concrete";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_STONE:
            return "Stone";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES:
            return "Scratches";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR:
            return "Edge Wear";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE:
        default:
            return "None";
    }
}

RuntimeMaterialTextureLayerKind RuntimeMaterialTextureLayerKindFromStableId(
    const char* stable_id) {
    if (!stable_id) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    if (strcmp(stable_id, "solid") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID;
    if (strcmp(stable_id, "rust") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
    if (strcmp(stable_id, "fog") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG;
    if (strcmp(stable_id, "grime") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME;
    if (strcmp(stable_id, "oil") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL;
    if (strcmp(stable_id, "metal") == 0) {
        return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL;
    }
    if (strcmp(stable_id, "brushed_metal") == 0) {
        return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL;
    }
    if (strcmp(stable_id, "wood") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD;
    if (strcmp(stable_id, "brick") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRICK;
    if (strcmp(stable_id, "concrete") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_CONCRETE;
    if (strcmp(stable_id, "stone") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_STONE;
    if (strcmp(stable_id, "scratches") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES;
    if (strcmp(stable_id, "edge_wear") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR;
    return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
}

const char* RuntimeMaterialTextureLayerBlendModeStableId(
    RuntimeMaterialTextureLayerBlendMode blend_mode) {
    switch (blend_mode) {
        case RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_REPLACE:
            return "replace";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_TINT:
            return "tint";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_MULTIPLY:
            return "multiply";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_OVERLAY_DAMAGE:
            return "overlay_damage";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_TRANSPARENT_SPECULAR:
            return "transparent_specular";
        case RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_MATERIAL_ONLY:
            return "material_only";
        default:
            return "tint";
    }
}

RuntimeMaterialTextureLayerBlendMode RuntimeMaterialTextureLayerBlendModeFromStableId(
    const char* stable_id) {
    if (!stable_id) return RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_TINT;
    if (strcmp(stable_id, "replace") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_REPLACE;
    if (strcmp(stable_id, "tint") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_TINT;
    if (strcmp(stable_id, "multiply") == 0) return RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_MULTIPLY;
    if (strcmp(stable_id, "overlay_damage") == 0) {
        return RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_OVERLAY_DAMAGE;
    }
    if (strcmp(stable_id, "transparent_specular") == 0) {
        return RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_TRANSPARENT_SPECULAR;
    }
    if (strcmp(stable_id, "material_only") == 0) {
        return RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_MATERIAL_ONLY;
    }
    return RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_TINT;
}

bool RuntimeMaterialTextureLayerKindIsBase(RuntimeMaterialTextureLayerKind kind) {
    return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID ||
           kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL ||
           kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD ||
           kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRICK ||
           kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_CONCRETE ||
           kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_STONE;
}

bool RuntimeMaterialTextureLayerKindIsOverlay(RuntimeMaterialTextureLayerKind kind) {
    return kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST ||
           kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG ||
           kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME ||
           kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL ||
           kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES ||
           kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR;
}

static RuntimeMaterialTextureLayer runtime_material_texture_layer_make(
    RuntimeMaterialTextureLayerKind kind,
    RuntimeMaterialTextureLayerRole role) {
    RuntimeMaterialTextureLayer layer;
    RuntimeMaterialTexture3DParams params;
    memset(&layer, 0, sizeof(layer));
    layer.enabled = runtime_material_texture_stack_valid_kind(kind);
    layer.kind = kind;
    layer.role = role;
    layer.opacity = 1.0;
    layer.placement.scale = 1.0;
    layer.placement.strength = 1.0;
    layer.placement.params = RuntimeMaterialTexture3DDefaultParams();
    layer.params = RuntimeMaterialTexture3DDefaultParams();
    layer.blendMode = role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE
                          ? RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_REPLACE
                          : RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_TINT;
    runtime_material_texture_stack_copy_text(layer.layerId,
                                             sizeof(layer.layerId),
                                             RuntimeMaterialTextureLayerKindStableId(kind));
    runtime_material_texture_stack_copy_text(layer.displayName,
                                             sizeof(layer.displayName),
                                             RuntimeMaterialTextureLayerKindDisplayName(kind));
    layer.placement.textureId = runtime_material_texture_stack_legacy_texture_kind(kind);

    params = layer.params;
    if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST) {
        params.patternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH;
        params.coverage = 0.42;
        params.grain = 0.72;
        params.edgeSoftness = 0.34;
        params.contrast = 0.70;
        params.flow = 0.08;
        params.colorDepth = 0.72;
        params.surfaceDamage = 0.80;
        layer.blendMode = RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_OVERLAY_DAMAGE;
        layer.roughnessInfluence = 0.94;
        layer.reflectivityInfluence = -0.90;
        layer.specularInfluence = -0.80;
        layer.diffuseInfluence = 0.90;
        layer.transparencyInfluence = -1.0;
    } else if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG) {
        params.patternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW;
        params.coverage = 0.56;
        params.grain = 0.24;
        params.edgeSoftness = 0.86;
        params.contrast = 0.24;
        params.flow = 0.62;
        params.colorDepth = 0.28;
        params.surfaceDamage = 0.12;
        layer.blendMode = RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_OVERLAY_DAMAGE;
        layer.roughnessInfluence = 1.0;
        layer.reflectivityInfluence = -0.35;
        layer.specularInfluence = -0.35;
        layer.diffuseInfluence = 0.70;
        layer.transparencyInfluence = -0.25;
    } else if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME) {
        params.patternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW;
        params.coverage = 0.58;
        params.grain = 0.48;
        params.edgeSoftness = 0.64;
        params.contrast = 0.74;
        params.flow = 0.42;
        params.colorDepth = 0.38;
        params.surfaceDamage = 0.58;
        layer.blendMode = RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_MULTIPLY;
        layer.roughnessInfluence = 0.65;
        layer.reflectivityInfluence = -0.45;
        layer.specularInfluence = -0.35;
        layer.diffuseInfluence = 0.45;
    } else if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL) {
        params.patternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW;
        params.coverage = 0.34;
        params.grain = 0.16;
        params.edgeSoftness = 0.74;
        params.contrast = 0.32;
        params.flow = 0.78;
        params.colorDepth = 0.24;
        params.surfaceDamage = 0.10;
        layer.blendMode = RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_TRANSPARENT_SPECULAR;
        layer.roughnessInfluence = -0.35;
        layer.reflectivityInfluence = 0.35;
        layer.specularInfluence = 0.55;
        layer.transparencyInfluence = 0.15;
    } else if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES) {
        layer.blendMode = RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_OVERLAY_DAMAGE;
        layer.roughnessInfluence = 0.50;
        layer.reflectivityInfluence = -0.25;
        layer.specularInfluence = -0.20;
    } else if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR) {
        layer.blendMode = RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_OVERLAY_DAMAGE;
        layer.roughnessInfluence = 0.35;
        layer.reflectivityInfluence = -0.20;
        layer.specularInfluence = -0.15;
    }
    layer.params = RuntimeMaterialTexture3DNormalizeParams(params);
    layer.placement.params = layer.params;

    return RuntimeMaterialTextureLayerNormalize(layer);
}

RuntimeMaterialTextureLayer RuntimeMaterialTextureLayerMakeBase(
    RuntimeMaterialTextureLayerKind kind) {
    if (!RuntimeMaterialTextureLayerKindIsBase(kind)) {
        kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID;
    }
    return runtime_material_texture_layer_make(kind, RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE);
}

RuntimeMaterialTextureLayer RuntimeMaterialTextureLayerMakeOverlay(
    RuntimeMaterialTextureLayerKind kind) {
    if (!RuntimeMaterialTextureLayerKindIsOverlay(kind)) {
        kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
    }
    return runtime_material_texture_layer_make(kind, RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_OVERLAY);
}

RuntimeMaterialTextureLayer RuntimeMaterialTextureLayerNormalize(
    RuntimeMaterialTextureLayer layer) {
    if (!runtime_material_texture_stack_valid_kind(layer.kind)) {
        RuntimeMaterialTextureLayer empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }

    if (RuntimeMaterialTextureLayerKindIsBase(layer.kind)) {
        layer.role = RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE;
        if (layer.blendMode != RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_REPLACE) {
            layer.blendMode = RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_REPLACE;
        }
    } else {
        layer.role = RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_OVERLAY;
    }

    layer.enabled = layer.enabled ? true : false;
    layer.opacity = runtime_material_texture_stack_clamp01(layer.opacity);
    if (!(layer.placement.scale > 1e-6)) {
        layer.placement.scale = 1.0;
    }
    layer.placement.strength = runtime_material_texture_stack_clamp01(layer.placement.strength);
    layer.placement.textureId = runtime_material_texture_stack_legacy_texture_kind(layer.kind);
    layer.placement.params = RuntimeMaterialTexture3DNormalizeParams(layer.placement.params);
    layer.params = RuntimeMaterialTexture3DNormalizeParams(layer.params);
    if (layer.layerId[0] == '\0') {
        runtime_material_texture_stack_copy_text(layer.layerId,
                                                 sizeof(layer.layerId),
                                                 RuntimeMaterialTextureLayerKindStableId(layer.kind));
    }
    if (layer.displayName[0] == '\0') {
        runtime_material_texture_stack_copy_text(layer.displayName,
                                                 sizeof(layer.displayName),
                                                 RuntimeMaterialTextureLayerKindDisplayName(layer.kind));
    }
    layer.roughnessInfluence =
        runtime_material_texture_stack_clamp(layer.roughnessInfluence, -1.0, 1.0);
    layer.reflectivityInfluence =
        runtime_material_texture_stack_clamp(layer.reflectivityInfluence, -1.0, 1.0);
    layer.specularInfluence =
        runtime_material_texture_stack_clamp(layer.specularInfluence, -1.0, 1.0);
    layer.diffuseInfluence =
        runtime_material_texture_stack_clamp(layer.diffuseInfluence, -1.0, 1.0);
    layer.transparencyInfluence =
        runtime_material_texture_stack_clamp(layer.transparencyInfluence, -1.0, 1.0);
    return layer;
}

RuntimeMaterialTextureStack RuntimeMaterialTextureStackEmpty(void) {
    RuntimeMaterialTextureStack stack;
    memset(&stack, 0, sizeof(stack));
    return stack;
}

static bool runtime_material_texture_stack_layer_id_seen(const RuntimeMaterialTextureStack* stack,
                                                         int before_index,
                                                         const char* layer_id) {
    if (!stack || !layer_id || !layer_id[0]) return false;
    for (int i = 0; i < before_index; ++i) {
        if (stack->layers[i].layerId[0] &&
            strcmp(stack->layers[i].layerId, layer_id) == 0) {
            return true;
        }
    }
    return false;
}

static void runtime_material_texture_stack_make_indexed_layer_id(
    RuntimeMaterialTextureLayer* layer,
    int layer_index) {
    char base[RUNTIME_MATERIAL_TEXTURE_LAYER_ID_SIZE];
    int suffix_len = 0;
    int base_limit = 0;
    if (!layer) return;
    runtime_material_texture_stack_copy_text(base, sizeof(base), layer->layerId);
    suffix_len = snprintf(NULL, 0, "_%d", layer_index);
    if (suffix_len < 0) suffix_len = 0;
    base_limit = (int)sizeof(layer->layerId) - suffix_len - 1;
    if (base_limit < 1) base_limit = 1;
    snprintf(layer->layerId,
             sizeof(layer->layerId),
             "%.*s_%d",
             base_limit,
             base,
             layer_index);
}

RuntimeMaterialTextureStack RuntimeMaterialTextureStackNormalize(
    RuntimeMaterialTextureStack stack) {
    if (stack.layerCount < 0) {
        stack.layerCount = 0;
    }
    if (stack.layerCount > RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
        stack.layerCount = RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS;
    }
    for (int i = 0; i < stack.layerCount; ++i) {
        stack.layers[i] = RuntimeMaterialTextureLayerNormalize(stack.layers[i]);
        if (runtime_material_texture_stack_layer_id_seen(&stack, i, stack.layers[i].layerId)) {
            runtime_material_texture_stack_make_indexed_layer_id(&stack.layers[i], i);
            while (runtime_material_texture_stack_layer_id_seen(&stack, i, stack.layers[i].layerId)) {
                runtime_material_texture_stack_make_indexed_layer_id(&stack.layers[i], i);
            }
        }
    }
    for (int i = stack.layerCount; i < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS; ++i) {
        memset(&stack.layers[i], 0, sizeof(stack.layers[i]));
    }
    return stack;
}

bool RuntimeMaterialTextureStackBuildLegacyFromObject(
    const SceneObject* object,
    RuntimeMaterialTextureStack* out_stack) {
    RuntimeMaterialTextureStack stack;
    RuntimeMaterialTextureLayerKind overlay_kind;
    RuntimeMaterialTextureLayer overlay;

    if (!object || !out_stack) return false;

    stack = RuntimeMaterialTextureStackEmpty();
    stack.layers[stack.layerCount++] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);

    overlay_kind = runtime_material_texture_stack_layer_kind_from_legacy(object->textureId);
    if (overlay_kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE &&
        stack.layerCount < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
        overlay = RuntimeMaterialTextureLayerMakeOverlay(overlay_kind);
        overlay.placement.offsetU = object->textureOffsetU;
        overlay.placement.offsetV = object->textureOffsetV;
        overlay.placement.scale = object->textureScale;
        overlay.placement.strength = object->textureStrength;
        overlay.placement.params = RuntimeMaterialTexture3DParamsFromObject(object);
        overlay.params = overlay.placement.params;
        stack.layers[stack.layerCount++] = RuntimeMaterialTextureLayerNormalize(overlay);
    }

    *out_stack = RuntimeMaterialTextureStackNormalize(stack);
    return true;
}

bool RuntimeMaterialTextureStackBuildLegacyFromPlacement(
    const RuntimeMaterialTexture3DPlacement* placement,
    RuntimeMaterialTextureStack* out_stack) {
    RuntimeMaterialTextureStack stack;
    RuntimeMaterialTextureLayerKind overlay_kind;
    RuntimeMaterialTextureLayer overlay;

    if (!out_stack) return false;

    stack = RuntimeMaterialTextureStackEmpty();
    stack.layers[stack.layerCount++] =
        RuntimeMaterialTextureLayerMakeBase(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID);

    overlay_kind =
        runtime_material_texture_stack_layer_kind_from_legacy(placement ? placement->textureId : 0);
    if (placement &&
        overlay_kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE &&
        stack.layerCount < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
        overlay = RuntimeMaterialTextureLayerMakeOverlay(overlay_kind);
        overlay.placement = *placement;
        overlay.placement.params = RuntimeMaterialTexture3DNormalizeParams(placement->params);
        overlay.params = overlay.placement.params;
        stack.layers[stack.layerCount++] = RuntimeMaterialTextureLayerNormalize(overlay);
    }

    *out_stack = RuntimeMaterialTextureStackNormalize(stack);
    return true;
}

int RuntimeMaterialTextureStackActiveLayerCount(const RuntimeMaterialTextureStack* stack) {
    int count = 0;
    int limit = 0;
    if (!stack) return 0;
    limit = stack->layerCount;
    if (limit < 0) limit = 0;
    if (limit > RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
        limit = RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS;
    }
    for (int i = 0; i < limit; ++i) {
        if (stack->layers[i].enabled &&
            stack->layers[i].kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) {
            count += 1;
        }
    }
    return count;
}
