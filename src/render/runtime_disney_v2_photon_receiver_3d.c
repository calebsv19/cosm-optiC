#include "render/runtime_disney_v2_photon_receiver_3d.h"
#include "render/runtime_caustic_photon_direct_consumer_3d.h"

bool RuntimeDisneyV2PhotonReceiver3D_Apply(
    const HitInfo3D* hit, const RuntimeMaterialPayload3D* payload,
    RuntimeDisneyV2_3DResult* receiver) {
    if (!receiver || receiver->photonReceiverEvaluated ||
        !RuntimeCausticPhotonDirectConsumer3D_Active()) return false;
    receiver->photonReceiverEvaluated = true;
    Vec3 radiance;
    if (!RuntimeCausticPhotonDirectConsumer3D_SampleSurfaceWithPayload(
            hit, payload, &radiance, NULL)) return false;
    receiver->photonReceiverRadianceR = radiance.x;
    receiver->photonReceiverRadianceG = radiance.y;
    receiver->photonReceiverRadianceB = radiance.z;
    receiver->diffuseRadianceR += radiance.x;
    receiver->diffuseRadianceG += radiance.y;
    receiver->diffuseRadianceB += radiance.z;
    receiver->visible = true;
    return true;
}
