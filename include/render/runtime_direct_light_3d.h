#ifndef RENDER_RUNTIME_DIRECT_LIGHT_3D_H
#define RENDER_RUNTIME_DIRECT_LIGHT_3D_H

#include <stdbool.h>

#include "render/runtime_camera_3d_rays.h"
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
    double lightDistance;
    double ndotl;
    double attenuation;
    double radiance;
    double radianceR;
    double radianceG;
    double radianceB;
} RuntimeDirectLight3DResult;

bool RuntimeDirectLight3D_TracePrimaryHit(const RuntimeScene3D* scene,
                                          const RuntimeCameraProjector3D* projector,
                                          double pixel_x,
                                          double pixel_y,
                                          RuntimePrimaryHit3DResult* out_result);

bool RuntimeDirectLight3D_ShadeHit(const RuntimeScene3D* scene,
                                   const HitInfo3D* hit,
                                   RuntimeDirectLight3DResult* out_result);

bool RuntimeDirectLight3D_ShadePixel(const RuntimeScene3D* scene,
                                     const RuntimeCameraProjector3D* projector,
                                     double pixel_x,
                                     double pixel_y,
                                     RuntimeDirectLight3DResult* out_result);

#endif
