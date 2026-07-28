#include "procedural/procedural_solid_material_texture_runtime.h"

#include <math.h>

static double clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double eval_height(const RuntimeMaterialSurfaceEval *eval) {
    if (!eval) return 0.0;
    return clamp01(
        (0.2126 * eval->colorR) +
        (0.7152 * eval->colorG) +
        (0.0722 * eval->colorB));
}

static double clamp_slope(double value) {
    if (value < -0.45) return -0.45;
    if (value > 0.45) return 0.45;
    return value;
}

bool ProceduralSolidMaterialWeightedTextures_BuildStack(
    const ProceduralSolidMaterialWeightedTextureV1 *textures,
    size_t texture_count,
    RuntimeMaterialTextureStack *out_stack) {
    RuntimeMaterialTextureStack stack;
    if (!out_stack ||
        texture_count > RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS ||
        (texture_count > 0u && !textures)) {
        return false;
    }
    stack = RuntimeMaterialTextureStackEmpty();
    for (size_t i = 0u; i < texture_count; ++i) {
        const ProceduralSolidMaterialWeightedTextureV1 *weighted =
            &textures[i];
        const ProceduralSolidAuthoredTextureV1 *texture =
            &weighted->texture;
        RuntimeMaterialTextureLayerKind kind;
        RuntimeMaterialTextureLayer layer;
        if (!isfinite(weighted->weight) ||
            weighted->graph_layer_index >=
                RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS) {
            return false;
        }
        kind = RuntimeMaterialTextureLayerKindFromStableId(texture->kind);
        if (kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) return false;
        layer = RuntimeMaterialTextureLayerKindIsOverlay(kind)
                    ? RuntimeMaterialTextureLayerMakeOverlay(kind)
                    : RuntimeMaterialTextureLayerMakeBase(kind);
        layer.enabled = texture->enabled;
        layer.opacity =
            clamp01(texture->strength) * clamp01(weighted->weight);
        layer.placement.scale =
            texture->scale_units > 1e-9
                ? 1.0 / texture->scale_units
                : 1.0;
        layer.placement.strength = clamp01(texture->strength);
        layer.placement.params.coverage = texture->coverage;
        layer.placement.params.grain = texture->grain;
        layer.placement.params.edgeSoftness = texture->edge_softness;
        layer.placement.params.contrast = texture->contrast;
        layer.placement.params.flow = texture->flow;
        layer.placement.params.colorDepth = texture->color_depth;
        layer.placement.params.surfaceDamage = texture->surface_damage;
        layer.placement.params.seed = texture->seed;
        layer.params = layer.placement.params;
        stack.layers[stack.layerCount++] =
            RuntimeMaterialTextureLayerNormalize(layer);
    }
    *out_stack = stack;
    return true;
}

bool ProceduralSolidMaterialWeightedTextures_EvaluatePlacedUV(
    const ProceduralSolidMaterialWeightedTextureV1 *textures,
    size_t texture_count,
    const SceneObject *object,
    double u,
    double v,
    int seed_key,
    const RuntimeMaterialSurfaceEval *base_eval,
    RuntimeMaterialSurfaceEval *out_eval) {
    RuntimeMaterialTextureStack stack;
    if (!base_eval || !out_eval ||
        !ProceduralSolidMaterialWeightedTextures_BuildStack(
            textures, texture_count, &stack)) {
        return false;
    }
    if (texture_count == 0u) {
        *out_eval = *base_eval;
        return true;
    }
    return RuntimeMaterialTextureStackEvaluatePlacedUV(
        &stack, object, u, v, seed_key, base_eval, out_eval);
}

bool ProceduralSolidMaterialWeightedTextures_EvaluateMicrodetailPlacedUV(
    const ProceduralSolidMaterialWeightedTextureV1 *textures,
    size_t texture_count,
    const SceneObject *object,
    double u,
    double v,
    int seed_key,
    const RuntimeMaterialSurfaceEval *base_eval,
    RuntimeMaterialSurfaceEval *out_eval) {
    const double derivative_step = 1.0 / 1024.0;
    const double height_scale = 0.02;
    RuntimeMaterialSurfaceEval center;
    RuntimeMaterialSurfaceEval left;
    RuntimeMaterialSurfaceEval right;
    RuntimeMaterialSurfaceEval down;
    RuntimeMaterialSurfaceEval up;
    double retained = 1.0;
    double normal_strength;
    double slope_u;
    double slope_v;

    if (!textures || texture_count == 0u || !object || !base_eval || !out_eval) {
        return false;
    }
    if (!ProceduralSolidMaterialWeightedTextures_EvaluatePlacedUV(
            textures, texture_count, object, u, v, seed_key, base_eval, &center)) {
        return false;
    }
    for (size_t i = 0u; i < texture_count; ++i) {
        double contribution;
        if (!textures[i].texture.enabled) continue;
        contribution =
            clamp01(textures[i].texture.microdetail_normal_strength) *
            clamp01(textures[i].weight);
        retained *= 1.0 - contribution;
    }
    normal_strength = clamp01(1.0 - retained);
    if (normal_strength <= 1e-9) {
        *out_eval = center;
        return true;
    }
    if (!ProceduralSolidMaterialWeightedTextures_EvaluatePlacedUV(
            textures, texture_count, object, u - derivative_step, v,
            seed_key, base_eval, &left) ||
        !ProceduralSolidMaterialWeightedTextures_EvaluatePlacedUV(
            textures, texture_count, object, u + derivative_step, v,
            seed_key, base_eval, &right) ||
        !ProceduralSolidMaterialWeightedTextures_EvaluatePlacedUV(
            textures, texture_count, object, u, v - derivative_step,
            seed_key, base_eval, &down) ||
        !ProceduralSolidMaterialWeightedTextures_EvaluatePlacedUV(
            textures, texture_count, object, u, v + derivative_step,
            seed_key, base_eval, &up)) {
        return false;
    }

    slope_u = ((eval_height(&right) - eval_height(&left)) /
               (2.0 * derivative_step)) *
              height_scale * normal_strength;
    slope_v = ((eval_height(&up) - eval_height(&down)) /
               (2.0 * derivative_step)) *
              height_scale * normal_strength;
    center.microdetailNormalActive = true;
    center.microdetailHeight = eval_height(&center) * normal_strength;
    center.microdetailSlopeU = clamp_slope(slope_u);
    center.microdetailSlopeV = clamp_slope(slope_v);
    *out_eval = center;
    return true;
}
