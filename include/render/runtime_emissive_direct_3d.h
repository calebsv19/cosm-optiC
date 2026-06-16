#ifndef RENDER_RUNTIME_EMISSIVE_DIRECT_3D_H
#define RENDER_RUNTIME_EMISSIVE_DIRECT_3D_H

#include <stdbool.h>

#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_sampling.h"

typedef struct {
    double directRadiance;
    double directRadianceR;
    double directRadianceG;
    double directRadianceB;
    int sampledTriangleCount;
    int contributingTriangleCount;
    int candidateCount;
    int selectedCandidateCount;
    int visibilityRayCount;
    int fullScanFallbackCount;
} RuntimeEmissiveDirect3DResult;

bool RuntimeEmissiveDirect3D_ShadeHit(const RuntimeScene3D* scene,
                                      const HitInfo3D* hit,
                                      const RuntimeNative3DSamplingContext* sampling,
                                      RuntimeEmissiveDirect3DResult* out_result);

#endif
