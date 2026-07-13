#ifndef RENDER_RUNTIME_DIELECTRIC_TRANSPORT_3D_H
#define RENDER_RUNTIME_DIELECTRIC_TRANSPORT_3D_H

#include <stdbool.h>

#include "render/runtime_material_payload_3d.h"

typedef struct {
    bool entering;
    bool hasRefraction;
    bool totalInternalReflection;
    double etaFrom;
    double etaTo;
    double fresnel;
    Vec3 orientedNormal;
    Vec3 incidentDir;
    Vec3 reflectionDir;
    Vec3 refractionDir;
} RuntimeDielectricTransport3D;

bool RuntimeDielectricTransport3D_Resolve(const RuntimeMaterialPayload3D* payload,
                                          Vec3 surface_normal,
                                          Vec3 incident_dir,
                                          RuntimeDielectricTransport3D* out_transport);
bool RuntimeDielectricTransport3D_ResolveInterface(
    const RuntimeMaterialPayload3D* payload,
    Vec3 surface_normal,
    Vec3 incident_dir,
    double eta_from,
    double eta_to,
    RuntimeDielectricTransport3D* out_transport);

#endif
