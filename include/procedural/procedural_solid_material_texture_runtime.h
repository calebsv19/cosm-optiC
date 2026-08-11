#ifndef PROCEDURAL_SOLID_MATERIAL_TEXTURE_RUNTIME_H
#define PROCEDURAL_SOLID_MATERIAL_TEXTURE_RUNTIME_H

#include "procedural/procedural_solid_material_weighted_texture.h"
#include "render/runtime_material_texture_stack_3d.h"

#include <stdbool.h>
#include <stddef.h>

bool ProceduralSolidMaterialWeightedTextures_BuildStack(
    const ProceduralSolidMaterialWeightedTextureV1 *textures,
    size_t texture_count,
    RuntimeMaterialTextureStack *out_stack);

bool ProceduralSolidMaterialWeightedTextures_EvaluatePlacedUV(
    const ProceduralSolidMaterialWeightedTextureV1 *textures,
    size_t texture_count,
    const SceneObject *object,
    double u,
    double v,
    int seed_key,
    const RuntimeMaterialSurfaceEval *base_eval,
    RuntimeMaterialSurfaceEval *out_eval);

bool ProceduralSolidMaterialWeightedTextures_EvaluateMicrodetailPlacedUV(
    const ProceduralSolidMaterialWeightedTextureV1 *textures,
    size_t texture_count,
    const SceneObject *object,
    double u,
    double v,
    int seed_key,
    const RuntimeMaterialSurfaceEval *base_eval,
    RuntimeMaterialSurfaceEval *out_eval);

#endif
