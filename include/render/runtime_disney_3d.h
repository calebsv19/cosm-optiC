#ifndef RENDER_RUNTIME_DISNEY_3D_H
#define RENDER_RUNTIME_DISNEY_3D_H

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
    double directRadianceR;
    double directRadianceG;
    double directRadianceB;
    double bounceRadiance;
    double bounceRadianceR;
    double bounceRadianceG;
    double bounceRadianceB;
    double emissionRadiance;
    double emissionRadianceR;
    double emissionRadianceG;
    double emissionRadianceB;
    double transmissionRadiance;
    double transmissionRadianceR;
    double transmissionRadianceG;
    double transmissionRadianceB;
    double baseRadiance;
    double baseRadianceR;
    double baseRadianceG;
    double baseRadianceB;
    double diffuseRadiance;
    double diffuseRadianceR;
    double diffuseRadianceG;
    double diffuseRadianceB;
    double specularRadiance;
    double specularRadianceR;
    double specularRadianceG;
    double specularRadianceB;
    double fresnelWeight;
    double roughnessWeight;
    double radiance;
    double radianceR;
    double radianceG;
    double radianceB;
    int secondaryRayCount;
    int secondaryHitCount;
    int secondaryContributingHitCount;
} RuntimeDisney3DResult;

bool RuntimeDisney3D_ShadeHit(const RuntimeScene3D* scene,
                              const HitInfo3D* hit,
                              const RuntimeNative3DSamplingContext* sampling,
                              RuntimeDisney3DResult* out_result);

bool RuntimeDisney3D_ShadePixel(const RuntimeScene3D* scene,
                                const RuntimeCameraProjector3D* projector,
                                double pixel_x,
                                double pixel_y,
                                const RuntimeNative3DSamplingContext* sampling,
                                RuntimeDisney3DResult* out_result);

#endif
