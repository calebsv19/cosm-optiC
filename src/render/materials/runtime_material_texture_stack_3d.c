#include "render/runtime_material_texture_stack_3d.h"

#include <math.h>
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

static double runtime_material_texture_stack_lerp(double a, double b, double t) {
    return a + ((b - a) * runtime_material_texture_stack_clamp01(t));
}

static double runtime_material_texture_stack_fract(double value) {
    return value - floor(value);
}

static double runtime_material_texture_stack_smooth(double value) {
    value = runtime_material_texture_stack_clamp01(value);
    return value * value * (3.0 - (2.0 * value));
}

static double runtime_material_texture_stack_hash(int ix, int iy, int seed) {
    unsigned int x = (unsigned int)ix;
    unsigned int y = (unsigned int)iy;
    unsigned int h = 2166136261u;
    h = (h ^ x) * 16777619u;
    h = (h ^ (y + 0x9e3779b9u)) * 16777619u;
    h = (h ^ (unsigned int)seed) * 16777619u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return (double)(h & 0x00FFFFFFu) / (double)0x01000000u;
}

static double runtime_material_texture_stack_value_noise(double u, double v, int seed) {
    int x0 = (int)floor(u);
    int y0 = (int)floor(v);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    double tx = runtime_material_texture_stack_smooth(u - (double)x0);
    double ty = runtime_material_texture_stack_smooth(v - (double)y0);
    double n00 = runtime_material_texture_stack_hash(x0, y0, seed);
    double n10 = runtime_material_texture_stack_hash(x1, y0, seed);
    double n01 = runtime_material_texture_stack_hash(x0, y1, seed);
    double n11 = runtime_material_texture_stack_hash(x1, y1, seed);
    double nx0 = runtime_material_texture_stack_lerp(n00, n10, tx);
    double nx1 = runtime_material_texture_stack_lerp(n01, n11, tx);
    return runtime_material_texture_stack_lerp(nx0, nx1, ty);
}

static double runtime_material_texture_stack_fbm(double u, double v, int seed) {
    double sum = 0.0;
    double amp = 0.5;
    double norm = 0.0;
    double freq = 1.0;
    for (int octave = 0; octave < 4; ++octave) {
        sum += runtime_material_texture_stack_value_noise(u * freq,
                                                          v * freq,
                                                          seed + octave * 97) * amp;
        norm += amp;
        amp *= 0.5;
        freq *= 2.0;
    }
    if (norm <= 1e-9) return 0.0;
    return runtime_material_texture_stack_clamp01(sum / norm);
}

static double runtime_material_texture_stack_threshold_mask(double value,
                                                            double coverage,
                                                            double edge_softness,
                                                            double contrast,
                                                            double strength) {
    double threshold = runtime_material_texture_stack_lerp(0.88, 0.18, coverage);
    double fade = runtime_material_texture_stack_lerp(0.035, 0.42, edge_softness);
    double mask = runtime_material_texture_stack_clamp01((value - threshold) / fade);
    double contrast_power = runtime_material_texture_stack_lerp(2.3, 0.55, contrast);
    return runtime_material_texture_stack_clamp01(pow(mask, contrast_power) * strength);
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

RuntimeMaterialSurfaceEval RuntimeMaterialSurfaceEvalMakeBase(double color_r,
                                                              double color_g,
                                                              double color_b,
                                                              double roughness,
                                                              double reflectivity,
                                                              double spec_weight,
                                                              double diffuse_weight,
                                                              double transparency) {
    RuntimeMaterialSurfaceEval eval;
    memset(&eval, 0, sizeof(eval));
    eval.colorR = runtime_material_texture_stack_clamp01(color_r);
    eval.colorG = runtime_material_texture_stack_clamp01(color_g);
    eval.colorB = runtime_material_texture_stack_clamp01(color_b);
    eval.roughness = runtime_material_texture_stack_clamp01(roughness);
    eval.reflectivity = runtime_material_texture_stack_clamp01(reflectivity);
    eval.specWeight = runtime_material_texture_stack_clamp01(spec_weight);
    eval.diffuseWeight = runtime_material_texture_stack_clamp01(diffuse_weight);
    eval.transparency = runtime_material_texture_stack_clamp01(transparency);
    return eval;
}

static void runtime_material_surface_eval_refresh(RuntimeMaterialSurfaceEval* eval) {
    double weight_sum = 0.0;
    if (!eval) return;
    eval->colorR = runtime_material_texture_stack_clamp01(eval->colorR);
    eval->colorG = runtime_material_texture_stack_clamp01(eval->colorG);
    eval->colorB = runtime_material_texture_stack_clamp01(eval->colorB);
    eval->roughness = runtime_material_texture_stack_clamp01(eval->roughness);
    if (eval->roughness < 0.02) {
        eval->roughness = 0.02;
    }
    eval->reflectivity = runtime_material_texture_stack_clamp01(eval->reflectivity);
    eval->specWeight = runtime_material_texture_stack_clamp01(eval->specWeight);
    eval->diffuseWeight = runtime_material_texture_stack_clamp01(eval->diffuseWeight);
    eval->transparency = runtime_material_texture_stack_clamp01(eval->transparency);
    weight_sum = eval->diffuseWeight + eval->specWeight;
    if (weight_sum > 1.0) {
        eval->diffuseWeight /= weight_sum;
        eval->specWeight /= weight_sum;
    }
}

typedef struct {
    double u;
    double v;
} RuntimeMaterialTextureStackUV;

typedef struct {
    bool active;
    double mask;
    double colorR;
    double colorG;
    double colorB;
    double roughness;
    double reflectivity;
    double specWeight;
    double diffuseWeight;
} RuntimeMaterialTextureStackBaseSample;

static RuntimeMaterialTextureStackUV runtime_material_texture_stack_transform_uv(
    double u,
    double v,
    const RuntimeMaterialTexture3DPlacement* placement) {
    RuntimeMaterialTextureStackUV uv;
    double scale = 1.0;
    double rotation = 0.0;
    double centered_u = u - 0.5;
    double centered_v = v - 0.5;
    double cos_r = 1.0;
    double sin_r = 0.0;
    double rotated_u = u;
    double rotated_v = v;

    if (placement) {
        scale = placement->scale;
        rotation = placement->rotation;
    }
    if (!(scale > 1e-6)) {
        scale = 1.0;
    }

    cos_r = cos(rotation);
    sin_r = sin(rotation);
    rotated_u = (centered_u * cos_r) - (centered_v * sin_r) + 0.5;
    rotated_v = (centered_u * sin_r) + (centered_v * cos_r) + 0.5;

    uv.u = runtime_material_texture_stack_fract((rotated_u * scale) +
                                                (placement ? placement->offsetU : 0.0));
    uv.v = runtime_material_texture_stack_fract((rotated_v * scale) +
                                                (placement ? placement->offsetV : 0.0));
    return uv;
}

static void runtime_material_texture_stack_base_sample_init(
    RuntimeMaterialTextureStackBaseSample* sample) {
    if (!sample) return;
    memset(sample, 0, sizeof(*sample));
    sample->roughness = 0.5;
    sample->reflectivity = 0.0;
    sample->specWeight = 0.0;
    sample->diffuseWeight = 1.0;
}

static RuntimeMaterialTextureStackBaseSample runtime_material_texture_stack_sample_brushed_metal(
    RuntimeMaterialTextureStackUV uv,
    const RuntimeMaterialTexture3DParams* params,
    int seed) {
    RuntimeMaterialTextureStackBaseSample sample;
    double grain = params ? params->grain : 0.5;
    double color_depth = params ? params->colorDepth : 0.5;
    double flow = params ? params->flow : 0.0;
    double freq = runtime_material_texture_stack_lerp(18.0, 82.0, grain);
    double warp = runtime_material_texture_stack_fbm(uv.u * 2.5, uv.v * 2.5, seed + 11);
    double streak = runtime_material_texture_stack_fbm((uv.u + ((warp - 0.5) * 0.08)) * 2.0,
                                                       (uv.v * freq) + (flow * 5.0),
                                                       seed + 23);
    double tone = runtime_material_texture_stack_lerp(0.36, 0.76, streak);
    runtime_material_texture_stack_base_sample_init(&sample);
    tone = runtime_material_texture_stack_lerp(0.48, tone, color_depth);
    sample.active = true;
    sample.mask = 1.0;
    sample.colorR = tone * 0.92;
    sample.colorG = tone * 0.96;
    sample.colorB = tone;
    sample.roughness = runtime_material_texture_stack_lerp(0.16, 0.36, streak);
    sample.reflectivity = runtime_material_texture_stack_lerp(0.62, 0.82, 1.0 - streak);
    sample.specWeight = runtime_material_texture_stack_lerp(0.62, 0.86, 1.0 - streak);
    sample.diffuseWeight = 0.22;
    return sample;
}

static RuntimeMaterialTextureStackBaseSample runtime_material_texture_stack_sample_wood(
    RuntimeMaterialTextureStackUV uv,
    const RuntimeMaterialTexture3DParams* params,
    int seed) {
    RuntimeMaterialTextureStackBaseSample sample;
    double grain = params ? params->grain : 0.5;
    double color_depth = params ? params->colorDepth : 0.5;
    double flow = params ? params->flow : 0.0;
    double freq = runtime_material_texture_stack_lerp(4.0, 18.0, grain);
    double warp = runtime_material_texture_stack_fbm(uv.u * 3.0,
                                                     (uv.v * 1.8) + (flow * 2.0),
                                                     seed + 101);
    double rings = sin(((uv.u * freq) + (warp * 1.9) + (uv.v * flow * 2.5)) *
                       6.283185307179586);
    double line = runtime_material_texture_stack_clamp01((rings * 0.5) + 0.5);
    double pore = runtime_material_texture_stack_fbm(uv.u * freq * 2.0,
                                                     uv.v * freq * 0.6,
                                                     seed + 127);
    double tone = runtime_material_texture_stack_clamp01((line * 0.72) + (pore * 0.28));
    double depth = runtime_material_texture_stack_lerp(0.45, 1.0, color_depth);
    runtime_material_texture_stack_base_sample_init(&sample);
    sample.active = true;
    sample.mask = 1.0;
    sample.colorR = runtime_material_texture_stack_lerp(0.30, 0.72, tone) * depth;
    sample.colorG = runtime_material_texture_stack_lerp(0.13, 0.39, tone) * depth;
    sample.colorB = runtime_material_texture_stack_lerp(0.05, 0.14, tone) * depth;
    sample.roughness = runtime_material_texture_stack_lerp(0.42, 0.72, pore);
    sample.reflectivity = 0.10;
    sample.specWeight = runtime_material_texture_stack_lerp(0.14, 0.28, 1.0 - pore);
    sample.diffuseWeight = 0.88;
    return sample;
}

static RuntimeMaterialTextureStackBaseSample runtime_material_texture_stack_sample_brick(
    RuntimeMaterialTextureStackUV uv,
    const RuntimeMaterialTexture3DParams* params,
    int seed) {
    RuntimeMaterialTextureStackBaseSample sample;
    double grain = params ? params->grain : 0.5;
    double color_depth = params ? params->colorDepth : 0.5;
    double tiles_u = runtime_material_texture_stack_lerp(3.0, 8.0, grain);
    double tiles_v = tiles_u * 0.48;
    double row_value = floor(uv.v * tiles_v);
    int row = (int)row_value;
    double cell_u = (uv.u * tiles_u) + ((row & 1) ? 0.5 : 0.0);
    double cell_v = uv.v * tiles_v;
    int ix = (int)floor(cell_u);
    int iy = (int)floor(cell_v);
    double local_u = runtime_material_texture_stack_fract(cell_u);
    double local_v = runtime_material_texture_stack_fract(cell_v);
    double mortar = 0.045;
    double edge_u = local_u < 0.5 ? local_u : 1.0 - local_u;
    double edge_v = local_v < 0.5 ? local_v : 1.0 - local_v;
    double brick_jitter = runtime_material_texture_stack_hash(ix, iy, seed + 211);
    double grit = runtime_material_texture_stack_fbm(cell_u * 3.0, cell_v * 3.0, seed + 223);
    double tone = runtime_material_texture_stack_clamp01((brick_jitter * 0.35) + (grit * 0.65));
    double depth = runtime_material_texture_stack_lerp(0.55, 1.0, color_depth);
    runtime_material_texture_stack_base_sample_init(&sample);
    sample.active = true;
    sample.mask = 1.0;
    if (edge_u < mortar || edge_v < mortar) {
        double m = runtime_material_texture_stack_lerp(0.38, 0.56, grit);
        sample.colorR = m;
        sample.colorG = m * 0.94;
        sample.colorB = m * 0.86;
        sample.roughness = 0.92;
        sample.reflectivity = 0.02;
        sample.specWeight = 0.06;
        sample.diffuseWeight = 0.94;
    } else {
        sample.colorR = runtime_material_texture_stack_lerp(0.44, 0.72, tone) * depth;
        sample.colorG = runtime_material_texture_stack_lerp(0.12, 0.28, tone) * depth;
        sample.colorB = runtime_material_texture_stack_lerp(0.06, 0.10, tone) * depth;
        sample.roughness = runtime_material_texture_stack_lerp(0.68, 0.88, grit);
        sample.reflectivity = 0.03;
        sample.specWeight = 0.08;
        sample.diffuseWeight = 0.92;
    }
    return sample;
}

static RuntimeMaterialTextureStackBaseSample runtime_material_texture_stack_sample_concrete(
    RuntimeMaterialTextureStackUV uv,
    const RuntimeMaterialTexture3DParams* params,
    int seed) {
    RuntimeMaterialTextureStackBaseSample sample;
    double grain = params ? params->grain : 0.5;
    double color_depth = params ? params->colorDepth : 0.5;
    double freq = runtime_material_texture_stack_lerp(5.0, 20.0, grain);
    double cloud = runtime_material_texture_stack_fbm(uv.u * freq, uv.v * freq, seed + 307);
    double speckle = runtime_material_texture_stack_value_noise(uv.u * freq * 5.0,
                                                                uv.v * freq * 5.0,
                                                                seed + 331);
    double tone = runtime_material_texture_stack_clamp01((cloud * 0.82) + (speckle * 0.18));
    double gray = runtime_material_texture_stack_lerp(0.36, 0.72, tone);
    runtime_material_texture_stack_base_sample_init(&sample);
    gray = runtime_material_texture_stack_lerp(0.50, gray, color_depth);
    sample.active = true;
    sample.mask = 1.0;
    sample.colorR = gray;
    sample.colorG = gray * 0.98;
    sample.colorB = gray * 0.93;
    sample.roughness = runtime_material_texture_stack_lerp(0.78, 0.96, speckle);
    sample.reflectivity = 0.025;
    sample.specWeight = 0.055;
    sample.diffuseWeight = 0.95;
    return sample;
}

static RuntimeMaterialTextureStackBaseSample runtime_material_texture_stack_sample_stone(
    RuntimeMaterialTextureStackUV uv,
    const RuntimeMaterialTexture3DParams* params,
    int seed) {
    RuntimeMaterialTextureStackBaseSample sample;
    double grain = params ? params->grain : 0.5;
    double color_depth = params ? params->colorDepth : 0.5;
    double freq = runtime_material_texture_stack_lerp(3.0, 12.0, grain);
    double cloud = runtime_material_texture_stack_fbm(uv.u * freq, uv.v * freq, seed + 401);
    double vein = fabs(sin(((uv.u * 1.6) + (uv.v * 2.4) + cloud) * 9.42477796076938));
    double vein_mask = pow(runtime_material_texture_stack_clamp01(vein), 5.0);
    double tone = runtime_material_texture_stack_clamp01((cloud * 0.75) + (vein_mask * 0.25));
    double depth = runtime_material_texture_stack_lerp(0.62, 1.0, color_depth);
    runtime_material_texture_stack_base_sample_init(&sample);
    sample.active = true;
    sample.mask = 1.0;
    sample.colorR = runtime_material_texture_stack_lerp(0.32, 0.66, tone) * depth;
    sample.colorG = runtime_material_texture_stack_lerp(0.33, 0.64, tone) * depth;
    sample.colorB = runtime_material_texture_stack_lerp(0.34, 0.60, tone) * depth;
    sample.roughness = runtime_material_texture_stack_lerp(0.52, 0.82, cloud);
    sample.reflectivity = runtime_material_texture_stack_lerp(0.06, 0.14, vein_mask);
    sample.specWeight = runtime_material_texture_stack_lerp(0.12, 0.26, vein_mask);
    sample.diffuseWeight = 0.84;
    return sample;
}

static bool runtime_material_texture_stack_sample_base_layer(
    const RuntimeMaterialTextureLayer* layer,
    double u,
    double v,
    int seed,
    RuntimeMaterialTextureStackBaseSample* out_sample) {
    RuntimeMaterialTextureStackUV uv;
    RuntimeMaterialTexture3DParams params;
    if (!layer || !out_sample) return false;
    runtime_material_texture_stack_base_sample_init(out_sample);
    if (!layer->enabled ||
        layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID ||
        layer->opacity <= 1e-9 ||
        layer->placement.strength <= 1e-9) {
        return false;
    }

    uv = runtime_material_texture_stack_transform_uv(u, v, &layer->placement);
    params = RuntimeMaterialTexture3DNormalizeParams(layer->params);
    if (params.seed != 0) {
        seed ^= params.seed * 83492791;
    }

    if (layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL) {
        *out_sample = runtime_material_texture_stack_sample_brushed_metal(uv, &params, seed);
    } else if (layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD) {
        *out_sample = runtime_material_texture_stack_sample_wood(uv, &params, seed);
    } else if (layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRICK) {
        *out_sample = runtime_material_texture_stack_sample_brick(uv, &params, seed);
    } else if (layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_CONCRETE) {
        *out_sample = runtime_material_texture_stack_sample_concrete(uv, &params, seed);
    } else if (layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_STONE) {
        *out_sample = runtime_material_texture_stack_sample_stone(uv, &params, seed);
    } else {
        return false;
    }

    return out_sample->active;
}

static void runtime_material_surface_eval_apply_base_sample(
    RuntimeMaterialSurfaceEval* eval,
    const RuntimeMaterialTextureStackBaseSample* sample,
    double amount,
    int layer_index) {
    amount = runtime_material_texture_stack_clamp01(amount);
    if (!eval || !sample || !sample->active || amount <= 1e-9) return;

    eval->active = true;
    if (layer_index >= 0 && layer_index < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
        eval->layerMasks[layer_index] =
            runtime_material_texture_stack_clamp01(sample->mask * amount);
    }
    if ((sample->mask * amount) > eval->textureMask) {
        eval->textureMask = runtime_material_texture_stack_clamp01(sample->mask * amount);
    }

    eval->colorR = runtime_material_texture_stack_lerp(eval->colorR, sample->colorR, amount);
    eval->colorG = runtime_material_texture_stack_lerp(eval->colorG, sample->colorG, amount);
    eval->colorB = runtime_material_texture_stack_lerp(eval->colorB, sample->colorB, amount);
    eval->roughness =
        runtime_material_texture_stack_lerp(eval->roughness, sample->roughness, amount);
    eval->reflectivity =
        runtime_material_texture_stack_lerp(eval->reflectivity, sample->reflectivity, amount);
    eval->specWeight =
        runtime_material_texture_stack_lerp(eval->specWeight, sample->specWeight, amount);
    eval->diffuseWeight =
        runtime_material_texture_stack_lerp(eval->diffuseWeight, sample->diffuseWeight, amount);
}

static void runtime_material_surface_eval_apply_sample(
    RuntimeMaterialSurfaceEval* eval,
    const RuntimeMaterialTexture3DSample* sample,
    double opacity) {
    double mask = 0.0;
    if (!eval || !sample || !sample->active) return;
    mask = runtime_material_texture_stack_clamp01(sample->mask * opacity);
    if (mask <= 1e-9) return;

    eval->active = true;
    if (mask > eval->textureMask) {
        eval->textureMask = mask;
        eval->textureU = sample->u;
        eval->textureV = sample->v;
    }

    if (sample->kind == RUNTIME_MATERIAL_TEXTURE_3D_RUST) {
        double color_t = runtime_material_texture_stack_clamp01(
            mask * runtime_material_texture_stack_lerp(0.35, 1.35, sample->colorDepth));
        double damage_t = runtime_material_texture_stack_clamp01(
            mask * runtime_material_texture_stack_lerp(0.20, 1.25, sample->surfaceDamage));
        eval->colorR = runtime_material_texture_stack_lerp(eval->colorR, 0.74, color_t);
        eval->colorG = runtime_material_texture_stack_lerp(eval->colorG, 0.26, color_t);
        eval->colorB = runtime_material_texture_stack_lerp(eval->colorB, 0.08, color_t);
        eval->reflectivity *= 1.0 - (0.90 * damage_t);
        eval->roughness = runtime_material_texture_stack_lerp(eval->roughness, 0.96, damage_t);
        eval->specWeight *= 1.0 - (0.80 * damage_t);
        eval->diffuseWeight = runtime_material_texture_stack_lerp(eval->diffuseWeight, 0.90, damage_t);
        eval->transparency *= 1.0 - damage_t;
    } else if (sample->kind == RUNTIME_MATERIAL_TEXTURE_3D_FOG) {
        double color_t = runtime_material_texture_stack_clamp01(
            mask * runtime_material_texture_stack_lerp(0.20, 0.85, sample->colorDepth));
        double damage_t = runtime_material_texture_stack_clamp01(
            mask * runtime_material_texture_stack_lerp(0.15, 1.0, sample->surfaceDamage));
        eval->colorR = runtime_material_texture_stack_lerp(eval->colorR, 0.82, color_t);
        eval->colorG = runtime_material_texture_stack_lerp(eval->colorG, 0.86, color_t);
        eval->colorB = runtime_material_texture_stack_lerp(eval->colorB, 0.88, color_t);
        eval->reflectivity *= 1.0 - (0.35 * damage_t);
        eval->roughness = runtime_material_texture_stack_lerp(eval->roughness, 1.0, damage_t);
        eval->specWeight *= 1.0 - (0.35 * damage_t);
        eval->diffuseWeight = runtime_material_texture_stack_lerp(eval->diffuseWeight, 0.70, damage_t);
        eval->transparency *= 1.0 - (0.25 * damage_t);
    }
}

static bool runtime_material_texture_stack_sample_grime_or_oil(
    const RuntimeMaterialTextureLayer* layer,
    double u,
    double v,
    int seed,
    RuntimeMaterialTexture3DSample* out_sample) {
    RuntimeMaterialTextureStackUV uv;
    RuntimeMaterialTexture3DParams params;
    double freq = 1.0;
    double warp = 0.0;
    double value = 0.0;
    if (!layer || !out_sample) return false;
    memset(out_sample, 0, sizeof(*out_sample));
    if (!layer->enabled ||
        (layer->kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME &&
         layer->kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL) ||
        layer->placement.strength <= 1e-9) {
        return false;
    }

    uv = runtime_material_texture_stack_transform_uv(u, v, &layer->placement);
    params = RuntimeMaterialTexture3DNormalizeParams(layer->params);
    if (params.seed != 0) {
        seed ^= params.seed * 83492791;
    }
    freq = layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME
               ? runtime_material_texture_stack_lerp(3.0, 18.0, params.grain)
               : runtime_material_texture_stack_lerp(2.0, 12.0, params.grain);
    warp = runtime_material_texture_stack_fbm(uv.u * freq * 0.35,
                                             uv.v * freq * 0.35,
                                             seed + 503);
    value = runtime_material_texture_stack_fbm((uv.u * freq) +
                                                   ((warp - 0.5) * params.flow * 3.0),
                                               (uv.v * freq * (1.0 + params.flow)) +
                                                   (params.flow * 4.0),
                                               seed + 607);
    if (layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL) {
        double smear = fabs(sin(((uv.u * 0.85) + (uv.v * 2.4) + warp) *
                                6.283185307179586));
        value = runtime_material_texture_stack_clamp01((value * 0.52) + (smear * 0.48));
    }

    out_sample->u = uv.u;
    out_sample->v = uv.v;
    out_sample->colorDepth = params.colorDepth;
    out_sample->surfaceDamage = params.surfaceDamage;
    out_sample->mask = runtime_material_texture_stack_threshold_mask(value,
                                                                     params.coverage,
                                                                     params.edgeSoftness,
                                                                     params.contrast,
                                                                     layer->placement.strength);
    out_sample->active = out_sample->mask > 1e-9;
    return out_sample->active;
}

static void runtime_material_surface_eval_apply_stack_overlay(
    RuntimeMaterialSurfaceEval* eval,
    const RuntimeMaterialTextureLayer* layer,
    const RuntimeMaterialTexture3DSample* sample) {
    double mask = 0.0;
    double color_t = 0.0;
    double damage_t = 0.0;
    if (!eval || !layer || !sample || !sample->active) return;
    mask = runtime_material_texture_stack_clamp01(sample->mask * layer->opacity);
    if (mask <= 1e-9) return;

    eval->active = true;
    if (mask > eval->textureMask) {
        eval->textureMask = mask;
        eval->textureU = sample->u;
        eval->textureV = sample->v;
    }

    color_t = runtime_material_texture_stack_clamp01(
        mask * runtime_material_texture_stack_lerp(0.25, 1.0, sample->colorDepth));
    damage_t = runtime_material_texture_stack_clamp01(
        mask * runtime_material_texture_stack_lerp(0.20, 1.0, sample->surfaceDamage));

    if (layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME) {
        eval->colorR *= runtime_material_texture_stack_lerp(1.0, 0.42, color_t);
        eval->colorG *= runtime_material_texture_stack_lerp(1.0, 0.36, color_t);
        eval->colorB *= runtime_material_texture_stack_lerp(1.0, 0.30, color_t);
        eval->roughness = runtime_material_texture_stack_lerp(eval->roughness, 0.92, damage_t);
        eval->reflectivity *= 1.0 - (0.45 * damage_t);
        eval->specWeight *= 1.0 - (0.35 * damage_t);
        eval->diffuseWeight = runtime_material_texture_stack_lerp(eval->diffuseWeight, 0.88, damage_t);
    } else if (layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL) {
        eval->colorR = runtime_material_texture_stack_lerp(eval->colorR, eval->colorR * 0.82, color_t);
        eval->colorG = runtime_material_texture_stack_lerp(eval->colorG, eval->colorG * 0.76, color_t);
        eval->colorB = runtime_material_texture_stack_lerp(eval->colorB, eval->colorB * 0.62, color_t);
        eval->roughness = runtime_material_texture_stack_lerp(eval->roughness, 0.08, damage_t);
        eval->reflectivity = runtime_material_texture_stack_lerp(eval->reflectivity, 0.72, damage_t);
        eval->specWeight = runtime_material_texture_stack_lerp(eval->specWeight, 0.86, damage_t);
        eval->diffuseWeight *= 1.0 - (0.28 * damage_t);
        /* Oil should only modulate an already-transmissive base; it should not
         * turn an opaque solid into a light-passing blocker. */
        eval->transparency *= runtime_material_texture_stack_lerp(1.0, 0.94, color_t * 0.35);
    }
}

static bool runtime_material_texture_stack_evaluate_placed_uv_internal(
    const RuntimeMaterialTextureStack* stack,
    const SceneObject* object,
    double u,
    double v,
    int seed_key,
    const RuntimeMaterialSurfaceEval* base_eval,
    RuntimeMaterialSurfaceEval* out_eval,
    bool include_base_layers) {
    RuntimeMaterialTextureStack normalized;
    RuntimeMaterialSurfaceEval eval;
    int limit = 0;

    if (!stack || !object || !base_eval || !out_eval) return false;

    normalized = RuntimeMaterialTextureStackNormalize(*stack);
    eval = *base_eval;
    limit = normalized.layerCount;
    if (limit > RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
        limit = RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS;
    }

    for (int i = 0; i < limit; ++i) {
        const RuntimeMaterialTextureLayer* layer = &normalized.layers[i];
        RuntimeMaterialTexture3DSample sample;
        RuntimeMaterialTextureStackBaseSample base_sample;
        int layer_seed = i <= 1 ? seed_key : seed_key ^ ((i + 1) * 1000003);
        double transparency_before = eval.transparency;
        memset(&sample, 0, sizeof(sample));
        if (!layer->enabled || layer->opacity <= 1e-9) {
            continue;
        }
        if (layer->role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE) {
            if (!include_base_layers) {
                continue;
            }
            double amount = runtime_material_texture_stack_clamp01(layer->opacity *
                                                                   layer->placement.strength);
            memset(&base_sample, 0, sizeof(base_sample));
            if (runtime_material_texture_stack_sample_base_layer(layer,
                                                                 u,
                                                                 v,
                                                                 layer_seed,
                                                                 &base_sample)) {
                runtime_material_surface_eval_apply_base_sample(&eval,
                                                                &base_sample,
                                                                amount,
                                                                i);
            }
            continue;
        }
        if (layer->role != RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_OVERLAY) continue;
        if (layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME ||
            layer->kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL) {
            if (runtime_material_texture_stack_sample_grime_or_oil(layer,
                                                                    u,
                                                                    v,
                                                                    layer_seed,
                                                                    &sample)) {
                eval.layerMasks[i] =
                    runtime_material_texture_stack_clamp01(sample.mask * layer->opacity);
                runtime_material_surface_eval_apply_stack_overlay(&eval, layer, &sample);
                if (eval.transparency > transparency_before) {
                    eval.transparency = transparency_before;
                }
            }
            continue;
        }
        if (RuntimeMaterialTexture3D_SamplePlacedUV(object,
                                                    u,
                                                    v,
                                                    layer_seed,
                                                    &layer->placement,
                                                    &sample)) {
            eval.layerMasks[i] = runtime_material_texture_stack_clamp01(sample.mask * layer->opacity);
            runtime_material_surface_eval_apply_sample(&eval, &sample, layer->opacity);
            if (eval.transparency > transparency_before) {
                eval.transparency = transparency_before;
            }
        }
    }

    if (eval.transparency > base_eval->transparency) {
        eval.transparency = base_eval->transparency;
    }
    runtime_material_surface_eval_refresh(&eval);
    *out_eval = eval;
    return eval.active;
}

bool RuntimeMaterialTextureStackEvaluatePlacedUV(
    const RuntimeMaterialTextureStack* stack,
    const SceneObject* object,
    double u,
    double v,
    int seed_key,
    const RuntimeMaterialSurfaceEval* base_eval,
    RuntimeMaterialSurfaceEval* out_eval) {
    return runtime_material_texture_stack_evaluate_placed_uv_internal(stack,
                                                                      object,
                                                                      u,
                                                                      v,
                                                                      seed_key,
                                                                      base_eval,
                                                                      out_eval,
                                                                      true);
}

bool RuntimeMaterialTextureStackEvaluateOverlayPlacedUV(
    const RuntimeMaterialTextureStack* stack,
    const SceneObject* object,
    double u,
    double v,
    int seed_key,
    const RuntimeMaterialSurfaceEval* base_eval,
    RuntimeMaterialSurfaceEval* out_eval) {
    return runtime_material_texture_stack_evaluate_placed_uv_internal(stack,
                                                                      object,
                                                                      u,
                                                                      v,
                                                                      seed_key,
                                                                      base_eval,
                                                                      out_eval,
                                                                      false);
}
