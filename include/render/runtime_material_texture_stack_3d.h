#ifndef RENDER_RUNTIME_MATERIAL_TEXTURE_STACK_3D_H
#define RENDER_RUNTIME_MATERIAL_TEXTURE_STACK_3D_H

#include <stdbool.h>

#include "render/runtime_material_texture_3d.h"
#include "scene/object_manager.h"

#define RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS 8
#define RUNTIME_MATERIAL_TEXTURE_LAYER_ID_SIZE 32
#define RUNTIME_MATERIAL_TEXTURE_LAYER_NAME_SIZE 48

typedef enum {
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE = 0,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID = 1,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST = 2,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG = 3,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME = 4,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL = 5,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRUSHED_METAL = 6,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD = 7,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_BRICK = 8,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_CONCRETE = 9,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_STONE = 10,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES = 11,
    RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR = 12
} RuntimeMaterialTextureLayerKind;

typedef enum {
    RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE = 0,
    RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_OVERLAY = 1
} RuntimeMaterialTextureLayerRole;

typedef enum {
    RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_REPLACE = 0,
    RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_TINT = 1,
    RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_MULTIPLY = 2,
    RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_OVERLAY_DAMAGE = 3,
    RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_TRANSPARENT_SPECULAR = 4,
    RUNTIME_MATERIAL_TEXTURE_LAYER_BLEND_MATERIAL_ONLY = 5
} RuntimeMaterialTextureLayerBlendMode;

typedef struct {
    bool enabled;
    RuntimeMaterialTextureLayerKind kind;
    RuntimeMaterialTextureLayerRole role;
    RuntimeMaterialTextureLayerBlendMode blendMode;
    char layerId[RUNTIME_MATERIAL_TEXTURE_LAYER_ID_SIZE];
    char displayName[RUNTIME_MATERIAL_TEXTURE_LAYER_NAME_SIZE];
    double opacity;
    RuntimeMaterialTexture3DPlacement placement;
    RuntimeMaterialTexture3DParams params;
    double roughnessInfluence;
    double reflectivityInfluence;
    double specularInfluence;
    double diffuseInfluence;
    double transparencyInfluence;
} RuntimeMaterialTextureLayer;

typedef struct {
    int layerCount;
    RuntimeMaterialTextureLayer layers[RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS];
} RuntimeMaterialTextureStack;

typedef struct {
    bool active;
    double colorR;
    double colorG;
    double colorB;
    double roughness;
    double reflectivity;
    double specWeight;
    double diffuseWeight;
    double transparency;
    double textureMask;
    double textureU;
    double textureV;
    double layerMasks[RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS];
} RuntimeMaterialSurfaceEval;

typedef struct {
    bool active;
    double layerMask;
    bool roughnessActive;
    double roughnessScalar;
    bool reflectivityActive;
    double reflectivityCompat;
    bool specularActive;
    double specularWeight;
    bool diffuseActive;
    double diffuseWeight;
    bool opacityCoverageActive;
    double opacityCoverage;
    bool transmissionWeightActive;
    double transmissionWeight;
} RuntimeMaterialProceduralPhysicalChannels;

const char* RuntimeMaterialTextureLayerKindStableId(RuntimeMaterialTextureLayerKind kind);
const char* RuntimeMaterialTextureLayerKindDisplayName(RuntimeMaterialTextureLayerKind kind);
RuntimeMaterialTextureLayerKind RuntimeMaterialTextureLayerKindFromStableId(
    const char* stable_id);
const char* RuntimeMaterialTextureLayerBlendModeStableId(
    RuntimeMaterialTextureLayerBlendMode blend_mode);
RuntimeMaterialTextureLayerBlendMode RuntimeMaterialTextureLayerBlendModeFromStableId(
    const char* stable_id);
bool RuntimeMaterialTextureLayerKindIsBase(RuntimeMaterialTextureLayerKind kind);
bool RuntimeMaterialTextureLayerKindIsOverlay(RuntimeMaterialTextureLayerKind kind);

RuntimeMaterialTextureLayer RuntimeMaterialTextureLayerMakeBase(
    RuntimeMaterialTextureLayerKind kind);
RuntimeMaterialTextureLayer RuntimeMaterialTextureLayerMakeOverlay(
    RuntimeMaterialTextureLayerKind kind);
RuntimeMaterialTextureLayer RuntimeMaterialTextureLayerNormalize(
    RuntimeMaterialTextureLayer layer);

RuntimeMaterialTextureStack RuntimeMaterialTextureStackEmpty(void);
RuntimeMaterialTextureStack RuntimeMaterialTextureStackNormalize(
    RuntimeMaterialTextureStack stack);
bool RuntimeMaterialTextureStackBuildLegacyFromObject(
    const SceneObject* object,
    RuntimeMaterialTextureStack* out_stack);
bool RuntimeMaterialTextureStackBuildLegacyFromPlacement(
    const RuntimeMaterialTexture3DPlacement* placement,
    RuntimeMaterialTextureStack* out_stack);
int RuntimeMaterialTextureStackActiveLayerCount(const RuntimeMaterialTextureStack* stack);

RuntimeMaterialSurfaceEval RuntimeMaterialSurfaceEvalMakeBase(double color_r,
                                                              double color_g,
                                                              double color_b,
                                                              double roughness,
                                                              double reflectivity,
                                                              double spec_weight,
                                                              double diffuse_weight,
                                                              double transparency);
bool RuntimeMaterialSurfaceEvalProceduralPhysicalChannels(
    const RuntimeMaterialSurfaceEval* base_eval,
    const RuntimeMaterialSurfaceEval* surface_eval,
    RuntimeMaterialProceduralPhysicalChannels* out_channels);
bool RuntimeMaterialTextureStackEvaluatePlacedUV(
    const RuntimeMaterialTextureStack* stack,
    const SceneObject* object,
    double u,
    double v,
    int seed_key,
    const RuntimeMaterialSurfaceEval* base_eval,
    RuntimeMaterialSurfaceEval* out_eval);

bool RuntimeMaterialTextureStackEvaluatePhysicalChannelsPlacedUV(
    const RuntimeMaterialTextureStack* stack,
    const SceneObject* object,
    double u,
    double v,
    int seed_key,
    const RuntimeMaterialSurfaceEval* base_eval,
    RuntimeMaterialSurfaceEval* out_eval,
    RuntimeMaterialProceduralPhysicalChannels* out_channels);

bool RuntimeMaterialTextureStackEvaluateOverlayPlacedUV(
    const RuntimeMaterialTextureStack* stack,
    const SceneObject* object,
    double u,
    double v,
    int seed_key,
    const RuntimeMaterialSurfaceEval* base_eval,
    RuntimeMaterialSurfaceEval* out_eval);

#endif
