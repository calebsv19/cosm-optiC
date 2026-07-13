#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_BSDF_DIRECTION_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_BSDF_DIRECTION_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_photon_bsdf_policy_3d.h"
#include "render/runtime_dielectric_transport_3d.h"

typedef struct {
    double unitU;
    double unitV;
} RuntimeCausticPhotonBsdfDirectionSample3D;

typedef struct {
    bool attempted;
    bool valid;
    bool totalInternalReflection;
    RuntimeCausticPhotonBsdfLobe3D lobe;
    Vec3 outgoingDirection;
    double angularPdf;
    double cosine;
    RuntimeDielectricTransport3D dielectric;
} RuntimeCausticPhotonBsdfDirection3D;

bool RuntimeCausticPhotonBsdfDirection3D_Sample(
    RuntimeCausticPhotonBsdfLobe3D lobe,
    const RuntimeMaterialPayload3D* material,
    Vec3 incident_direction,
    Vec3 surface_normal,
    const RuntimeCausticPhotonBsdfDirectionSample3D* sample,
    RuntimeCausticPhotonBsdfDirection3D* out_direction);
bool RuntimeCausticPhotonBsdfDirection3D_SampleInterface(
    RuntimeCausticPhotonBsdfLobe3D lobe,
    const RuntimeMaterialPayload3D* material,
    Vec3 incident_direction,
    Vec3 surface_normal,
    const RuntimeCausticPhotonBsdfDirectionSample3D* sample,
    double eta_from,
    double eta_to,
    RuntimeCausticPhotonBsdfDirection3D* out_direction);

#endif
