#ifndef RENDER_RUNTIME_DIRECT_LIGHT_3D_H
#define RENDER_RUNTIME_DIRECT_LIGHT_3D_H

#include <stdbool.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_light_set_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_visibility_3d.h"

typedef struct {
    bool hit;
    Ray3D primaryRay;
    RuntimeVisibility3DTransmittance primaryTransmittance;
    HitInfo3D hitInfo;
} RuntimePrimaryHit3DResult;

typedef struct {
    bool hit;
    bool visible;
    Ray3D primaryRay;
    HitInfo3D hitInfo;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double lightDistance;
    double ndotl;
    double attenuation;
    double radiance;
    double radianceR;
    double radianceG;
    double radianceB;
    int evaluatedLightCount;
    int contributingLightCount;
    int visibilityRayCount;
    double peakLightContribution;
} RuntimeDirectLight3DResult;

bool RuntimeDirectLight3D_TracePrimaryHit(const RuntimeScene3D* scene,
                                          const RuntimeCameraProjector3D* projector,
                                          double pixel_x,
                                          double pixel_y,
                                          RuntimePrimaryHit3DResult* out_result);

bool RuntimeDirectLight3D_ShadeHit(const RuntimeScene3D* scene,
                                   const HitInfo3D* hit,
                                   const RuntimeNative3DSamplingContext* sampling,
                                   RuntimeDirectLight3DResult* out_result);

bool RuntimeDirectLight3D_ShadeHitWithPayload(const RuntimeScene3D* scene,
                                              const HitInfo3D* hit,
                                              const RuntimeMaterialPayload3D* payload,
                                              const RuntimeNative3DSamplingContext* sampling,
                                              RuntimeDirectLight3DResult* out_result);

bool RuntimeDirectLight3D_ShadeHitWithLightSet(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeLightSet3D* light_set,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDirectLight3DResult* out_result);

bool RuntimeDirectLight3D_ShadeHitWithLightSetAndPayload(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeLightSet3D* light_set,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDirectLight3DResult* out_result);

bool RuntimeDirectLight3D_ShadePrimaryHit(const RuntimeScene3D* scene,
                                          const RuntimePrimaryHit3DResult* primary_hit,
                                          const RuntimeNative3DSamplingContext* sampling,
                                          RuntimeDirectLight3DResult* out_result);

bool RuntimeDirectLight3D_ShadePrimaryHitWithPayload(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDirectLight3DResult* out_result);

bool RuntimeDirectLight3D_ShadePixel(const RuntimeScene3D* scene,
                                     const RuntimeCameraProjector3D* projector,
                                     double pixel_x,
                                     double pixel_y,
                                     const RuntimeNative3DSamplingContext* sampling,
                                     RuntimeDirectLight3DResult* out_result);

#endif
