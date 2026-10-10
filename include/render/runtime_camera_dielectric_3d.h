#ifndef RUNTIME_CAMERA_DIELECTRIC_3D_H
#define RUNTIME_CAMERA_DIELECTRIC_3D_H
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_caustic_photon_medium_stack_3d.h"
/* Reuse validated local medium bookkeeping; camera weights stay radiance, photons stay flux. */
typedef RuntimeCausticPhotonMediumStack3D RuntimeCameraMedium3D;
void RuntimeCameraMedium3D_Init(RuntimeCameraMedium3D* medium);
bool RuntimeCameraDielectric3D_Resolve(RuntimeCameraMedium3D* medium,
    const RuntimeMaterialPayload3D* payload,const HitInfo3D* hit,Vec3 incoming,
    RuntimeDielectricTransport3D* out);
bool RuntimeCameraMedium3D_Transmit(RuntimeCameraMedium3D* medium,
    const RuntimeMaterialPayload3D* payload,const HitInfo3D* hit,
    const RuntimeDielectricTransport3D* interface);
double RuntimeCameraDielectric3D_RadianceScale(const RuntimeMaterialPayload3D* payload,
    const RuntimeDielectricTransport3D* interface);
#endif
