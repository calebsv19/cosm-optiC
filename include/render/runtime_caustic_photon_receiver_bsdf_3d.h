#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_RECEIVER_BSDF_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_RECEIVER_BSDF_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_material_payload_3d.h"

typedef struct {
    bool attempted;
    bool applied;
    Vec3 baseColor;
    double diffuseWeight;
    double roughness;
    double meanIncidentCosine;
    double lambertianNormalization;
    Vec3 inputPhysicalFlux;
    Vec3 response;
    Vec3 outputRadiance;
} RuntimeCausticPhotonReceiverBsdfReadback3D;

bool RuntimeCausticPhotonReceiverBsdf3D_EvaluateDiffuse(
    const RuntimeMaterialPayload3D* material,
    const RuntimeCausticPhotonMapQueryResult3D* query,
    Vec3* out_radiance,
    RuntimeCausticPhotonReceiverBsdfReadback3D* out_readback);

#endif
