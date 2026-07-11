#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_RECEIVER_POLICY_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_RECEIVER_POLICY_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_photon_trace_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"

typedef struct {
    int dielectricSceneObjectIndex;
    double receiverProbeDistance;
    uint32_t maxSelfHitSkips;
    bool rejectTransparentReceivers;
    bool rejectEmissiveReceivers;
    bool rejectSpecularDominantReceivers;
} RuntimeCausticPhotonReceiverPolicy3D;

typedef struct {
    uint64_t lookupAttemptCount;
    uint64_t directHitCount;
    uint64_t crossingProbeAttemptCount;
    uint64_t crossingProbeHitCount;
    uint64_t candidateCount;
    uint64_t acceptedHitCount;
    uint64_t missRejectCount;
    uint64_t selfHitRejectCount;
    uint64_t invalidRejectCount;
    uint64_t materialFilterRejectCount;
    uint64_t objectFilterRejectCount;
    uint64_t competingRejectCount;
    uint64_t selectedHitCount;
    uint64_t selectedBucketCount;
} RuntimeCausticPhotonReceiverPolicyReadback3D;

typedef struct {
    bool valid;
    uint64_t firstIndex;
    uint64_t hitCount;
    uint64_t bucketCount;
    uint64_t competingHitCount;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
} RuntimeCausticPhotonReceiverBucket3D;

void RuntimeCausticPhotonReceiverPolicy3D_Default(
    RuntimeCausticPhotonReceiverPolicy3D* policy);
void RuntimeCausticPhotonReceiverPolicy3D_ResetReadback(
    RuntimeCausticPhotonReceiverPolicyReadback3D* readback);
bool RuntimeCausticPhotonReceiverPolicy3D_SelectHit(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonTrace3D* trace,
    const RuntimeCausticPhotonReceiverPolicy3D* policy,
    HitInfo3D* out_hit,
    RuntimeCausticPhotonReceiverPolicyReadback3D* io_readback);
bool RuntimeCausticPhotonReceiverPolicy3D_SelectPrimaryBucket(
    const RuntimeCausticPhotonSurfaceHit3D* hits,
    uint64_t hit_count,
    RuntimeCausticPhotonReceiverBucket3D* out_bucket);
bool RuntimeCausticPhotonReceiverPolicy3D_HitMatchesBucket(
    const RuntimeCausticPhotonSurfaceHit3D* hit,
    const RuntimeCausticPhotonReceiverBucket3D* bucket);

#endif
