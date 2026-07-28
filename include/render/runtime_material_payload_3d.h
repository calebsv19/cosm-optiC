#ifndef RENDER_RUNTIME_MATERIAL_PAYLOAD_3D_H
#define RENDER_RUNTIME_MATERIAL_PAYLOAD_3D_H

#include <stdbool.h>

#include "render/material_bsdf.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_material_texture_stack_3d.h"

typedef struct {
    bool valid;
    int sceneObjectIndex;
    int materialId;
    MaterialBSDF bsdf;
    double baseColorR;
    double baseColorG;
    double baseColorB;
    double emissive;
    double transparency;
    double opticalIor;
    double absorptionDistance;
    double glassInterfaceTintR;
    double glassInterfaceTintG;
    double glassInterfaceTintB;
    double glassAbsorptionColorR;
    double glassAbsorptionColorG;
    double glassAbsorptionColorB;
    bool hasGlassInterfaceTintOverride;
    bool hasGlassAbsorptionColorOverride;
    bool thinWalled;
    bool hasGlassTransportOverride;
    double textureMask;
    double textureU;
    double textureV;
    bool hasMicrodetailNormal;
    double microdetailHeight;
    double microdetailSlopeU;
    double microdetailSlopeV;
    Vec3 microdetailShadingNormal;
} RuntimeMaterialPayload3D;

void RuntimeMaterialPayload3D_Reset(RuntimeMaterialPayload3D* payload);

bool RuntimeMaterialPayload3D_ApplySurfaceEval(
    const RuntimeMaterialSurfaceEval* surface_eval,
    RuntimeMaterialPayload3D* payload);

bool RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(int scene_object_index,
                                                          RuntimeMaterialPayload3D* out_payload);

bool RuntimeMaterialPayload3D_ResolveFromHit(const HitInfo3D* hit,
                                             RuntimeMaterialPayload3D* out_payload);

bool RuntimeMaterialPayload3D_ResolveShadingNormal(
    const HitInfo3D* hit,
    RuntimeMaterialPayload3D* io_payload);

bool RuntimeMaterialPayload3D_ApplyShadingNormal(
    const RuntimeMaterialPayload3D* payload,
    HitInfo3D* io_hit);

#endif
