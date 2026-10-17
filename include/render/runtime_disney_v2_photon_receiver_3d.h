#ifndef RENDER_RUNTIME_DISNEY_V2_PHOTON_RECEIVER_3D_H
#define RENDER_RUNTIME_DISNEY_V2_PHOTON_RECEIVER_3D_H
#include "render/runtime_disney_v2_3d.h"
/* One local Lambertian photon response. No camera/path throughput, MIS or
 * cache reconstruction belongs here. This also guards repeated local calls. */
bool RuntimeDisneyV2PhotonReceiver3D_Apply(
    const HitInfo3D* hit, const RuntimeMaterialPayload3D* payload,
    RuntimeDisneyV2_3DResult* receiver);
#endif
