#ifndef RENDER_RUNTIME_EMISSION_TRANSPARENCY_3D_H
#define RENDER_RUNTIME_EMISSION_TRANSPARENCY_3D_H

#include <stdbool.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_sampling.h"

typedef struct {
    bool hit;
    bool visible;
    bool payloadResolved;
    Ray3D primaryRay;
    HitInfo3D hitInfo;
    RuntimeMaterialPayload3D payload;
    double directRadiance;
    double bounceRadiance;
    double radiance;
    int secondaryRayCount;
    int secondaryHitCount;
    int secondaryContributingHitCount;
} RuntimeEmissionTransparency3DResult;

bool RuntimeEmissionTransparency3D_ShadeHit(const RuntimeScene3D* scene,
                                            const HitInfo3D* hit,
                                            const RuntimeNative3DSamplingContext* sampling,
                                            RuntimeEmissionTransparency3DResult* out_result);

bool RuntimeEmissionTransparency3D_ShadePixel(const RuntimeScene3D* scene,
                                              const RuntimeCameraProjector3D* projector,
                                              double pixel_x,
                                              double pixel_y,
                                              const RuntimeNative3DSamplingContext* sampling,
                                              RuntimeEmissionTransparency3DResult* out_result);

#endif
