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
    double bounceRadiance;
    double radiance;
    int secondaryRayCount;
    int secondaryHitCount;
    int secondaryContributingHitCount;
} RuntimeMaterialResponse3DResult;

bool RuntimeMaterialResponse3D_ShadeHit(const RuntimeScene3D* scene,
                                        const HitInfo3D* hit,
                                        RuntimeMaterialResponse3DResult* out_result);

bool RuntimeMaterialResponse3D_ShadePixel(const RuntimeScene3D* scene,
                                          const RuntimeCameraProjector3D* projector,
                                          double pixel_x,
                                          double pixel_y,
                                          RuntimeMaterialResponse3DResult* out_result);

#endif
