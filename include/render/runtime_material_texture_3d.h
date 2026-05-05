#ifndef RENDER_RUNTIME_MATERIAL_TEXTURE_3D_H
#define RENDER_RUNTIME_MATERIAL_TEXTURE_3D_H

#include <stdbool.h>

#include "render/runtime_ray_3d.h"
#include "scene/object_manager.h"

typedef enum {
    RUNTIME_MATERIAL_TEXTURE_3D_NONE = 0,
    RUNTIME_MATERIAL_TEXTURE_3D_RUST = 1,
    RUNTIME_MATERIAL_TEXTURE_3D_FOG = 2
} RuntimeMaterialTexture3DKind;

typedef enum {
    RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_DEFAULT = 0,
    RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_SPECKLE = 1,
    RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH = 2,
    RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW = 3
} RuntimeMaterialTexture3DPatternMode;

typedef struct {
    int patternMode;
    double coverage;
    double grain;
    double edgeSoftness;
    double contrast;
    double flow;
    double colorDepth;
    double surfaceDamage;
    int seed;
} RuntimeMaterialTexture3DParams;

typedef struct {
    bool active;
    RuntimeMaterialTexture3DKind kind;
    double u;
    double v;
    double mask;
    double colorDepth;
    double surfaceDamage;
} RuntimeMaterialTexture3DSample;

typedef struct {
    int textureId;
    double offsetU;
    double offsetV;
    double scale;
    double strength;
    double rotation;
    RuntimeMaterialTexture3DParams params;
} RuntimeMaterialTexture3DPlacement;

RuntimeMaterialTexture3DParams RuntimeMaterialTexture3DDefaultParams(void);
RuntimeMaterialTexture3DParams RuntimeMaterialTexture3DNormalizeParams(
    RuntimeMaterialTexture3DParams params);
RuntimeMaterialTexture3DParams RuntimeMaterialTexture3DParamsFromObject(
    const SceneObject* object);
bool RuntimeMaterialTexture3D_Sample(const SceneObject* object,
                                     const HitInfo3D* hit,
                                     RuntimeMaterialTexture3DSample* out_sample);
bool RuntimeMaterialTexture3D_SampleUV(const SceneObject* object,
                                       int triangle_index,
                                       double bary_v,
                                       double bary_w,
                                       RuntimeMaterialTexture3DSample* out_sample);
bool RuntimeMaterialTexture3D_SamplePlacedUV(const SceneObject* object,
                                             double u,
                                             double v,
                                             int seed_key,
                                             const RuntimeMaterialTexture3DPlacement* placement,
                                             RuntimeMaterialTexture3DSample* out_sample);

#endif
