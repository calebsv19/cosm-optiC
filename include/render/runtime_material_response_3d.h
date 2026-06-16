#ifndef RENDER_RUNTIME_MATERIAL_RESPONSE_3D_H
#define RENDER_RUNTIME_MATERIAL_RESPONSE_3D_H

#include <stdbool.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_material_payload_3d.h"

typedef struct {
    bool hit;
    bool visible;
    bool materialResolved;
    Ray3D primaryRay;
    HitInfo3D hitInfo;
    RuntimeMaterialPayload3D payload;
    double directRadiance;
    double directRadianceR;
    double directRadianceG;
    double directRadianceB;
    double bounceRadiance;
    double bounceRadianceR;
    double bounceRadianceG;
    double bounceRadianceB;
    double specularRadiance;
    double specularRadianceR;
    double specularRadianceG;
    double specularRadianceB;
    double mirrorDominance;
    double mirrorBaseAttenuation;
    double mirrorBaseRadianceBeforeAttenuation;
    double mirrorBaseRadianceAfterAttenuation;
    double radiance;
    double radianceR;
    double radianceG;
    double radianceB;
    int secondaryRayCount;
    int secondaryHitCount;
    int secondaryContributingHitCount;
    int specularRayCount;
    int specularHitCount;
    int specularContributingHitCount;
} RuntimeMaterialResponse3DResult;

bool RuntimeMaterialResponse3D_ShadeHit(const RuntimeScene3D* scene,
                                        const HitInfo3D* hit,
                                        const RuntimeNative3DSamplingContext* sampling,
                                        RuntimeMaterialResponse3DResult* out_result);

bool RuntimeMaterialResponse3D_ShadeHitWithPayload(const RuntimeScene3D* scene,
                                                   const HitInfo3D* hit,
                                                   const RuntimeMaterialPayload3D* payload,
                                                   const RuntimeNative3DSamplingContext* sampling,
                                                   RuntimeMaterialResponse3DResult* out_result);

bool RuntimeMaterialResponse3D_ShadePrimaryHit(const RuntimeScene3D* scene,
                                               const RuntimePrimaryHit3DResult* primary_hit,
                                               const RuntimeNative3DSamplingContext* sampling,
                                               RuntimeMaterialResponse3DResult* out_result);

bool RuntimeMaterialResponse3D_ShadePrimaryHitWithPayload(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeMaterialResponse3DResult* out_result);

bool RuntimeMaterialResponse3D_ShadePixel(const RuntimeScene3D* scene,
                                          const RuntimeCameraProjector3D* projector,
                                          double pixel_x,
                                          double pixel_y,
                                          const RuntimeNative3DSamplingContext* sampling,
                                          RuntimeMaterialResponse3DResult* out_result);

#endif
