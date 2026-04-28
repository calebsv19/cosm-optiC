#ifndef RENDER_RUNTIME_DIFFUSE_BOUNCE_3D_H
#define RENDER_RUNTIME_DIFFUSE_BOUNCE_3D_H

#include <stdbool.h>

#include "render/runtime_direct_light_3d.h"
#include "render/runtime_native_3d_sampling.h"

typedef struct {
    bool hit;
    bool visible;
    Ray3D primaryRay;
    HitInfo3D hitInfo;
    double directRadiance;
    double directRadianceR;
    double directRadianceG;
    double directRadianceB;
    double bounceRadiance;
    double bounceRadianceR;
    double bounceRadianceG;
    double bounceRadianceB;
    double radiance;
    double radianceR;
    double radianceG;
    double radianceB;
    int secondaryRayCount;
    int secondaryHitCount;
    int secondaryContributingHitCount;
} RuntimeDiffuseBounce3DResult;

bool RuntimeDiffuseBounce3D_ShadeHit(const RuntimeScene3D* scene,
                                     const HitInfo3D* hit,
                                     const RuntimeNative3DSamplingContext* sampling,
                                     RuntimeDiffuseBounce3DResult* out_result);

bool RuntimeDiffuseBounce3D_ShadePixel(const RuntimeScene3D* scene,
                                       const RuntimeCameraProjector3D* projector,
                                       double pixel_x,
                                       double pixel_y,
                                       const RuntimeNative3DSamplingContext* sampling,
                                       RuntimeDiffuseBounce3DResult* out_result);

#endif
