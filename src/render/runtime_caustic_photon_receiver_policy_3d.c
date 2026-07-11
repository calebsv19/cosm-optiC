#include "render/runtime_caustic_photon_receiver_policy_3d.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "render/runtime_material_payload_3d.h"

static bool receiver_policy_direction_valid(Vec3 direction) {
    return vec3_length(direction) > 1.0e-9;
}

static bool receiver_policy_hit_identity_matches(
    const RuntimeCausticPhotonSurfaceHit3D* hit,
    int scene_object_index,
    int primitive_index,
    int triangle_index) {
    return hit && hit->sceneObjectIndex == scene_object_index &&
           hit->primitiveIndex == primitive_index && hit->triangleIndex == triangle_index;
}

static bool receiver_policy_hit_is_specular_dominant(
    const RuntimeMaterialPayload3D* payload) {
    if (!payload || !payload->valid) return false;
    if (payload->bsdf.reflectivity >= 0.85) return true;
    if (payload->bsdf.specWeight >= 0.85 && payload->bsdf.diffuseWeight <= 0.15) {
        return true;
    }
    return false;
}

static bool receiver_policy_accepts_hit(
    const RuntimeCausticPhotonReceiverPolicy3D* policy,
    const HitInfo3D* hit,
    RuntimeCausticPhotonReceiverPolicyReadback3D* io_readback) {
    RuntimeMaterialPayload3D payload = {0};

    if (!policy || !hit) return false;
    if (hit->sceneObjectIndex == policy->dielectricSceneObjectIndex) {
        if (io_readback) io_readback->objectFilterRejectCount += 1u;
        return false;
    }
    if (!RuntimeMaterialPayload3D_ResolveFromHit(hit, &payload) || !payload.valid) {
        if (io_readback) io_readback->materialFilterRejectCount += 1u;
        return false;
    }
    if (policy->rejectTransparentReceivers && payload.transparency > 1.0e-4) {
        if (io_readback) io_readback->materialFilterRejectCount += 1u;
        return false;
    }
    if (policy->rejectEmissiveReceivers &&
        (payload.emissive > 1.0e-6 || payload.bsdf.emissive > 1.0e-6)) {
        if (io_readback) io_readback->materialFilterRejectCount += 1u;
        return false;
    }
    if (policy->rejectSpecularDominantReceivers &&
        receiver_policy_hit_is_specular_dominant(&payload)) {
        if (io_readback) io_readback->materialFilterRejectCount += 1u;
        return false;
    }
    return true;
}

static bool receiver_policy_trace_direction(
    const RuntimeScene3D* scene,
    Vec3 origin,
    Vec3 direction,
    const RuntimeCausticPhotonReceiverPolicy3D* policy,
    double t_max,
    HitInfo3D* out_hit,
    RuntimeCausticPhotonReceiverPolicyReadback3D* io_readback) {
    uint32_t max_self_hit_skips = policy && policy->maxSelfHitSkips > 0u
                                      ? policy->maxSelfHitSkips
                                      : 6u;

    if (!scene || !policy || !out_hit || !receiver_policy_direction_valid(direction)) {
        return false;
    }
    direction = vec3_normalize(direction);
    origin = vec3_add(origin, vec3_scale(direction, 1.0e-4));

    for (uint32_t attempt = 0u; attempt < max_self_hit_skips; ++attempt) {
        Ray3D ray = RuntimeRay3D_Make(origin, direction);
        HitInfo3D hit;
        HitInfo3D_Reset(&hit);
        if (!RuntimeRay3D_TraceSceneFirstHit(scene, &ray, 1.0e-4, t_max, &hit)) {
            return false;
        }
        if (io_readback) io_readback->candidateCount += 1u;
        if (receiver_policy_accepts_hit(policy, &hit, io_readback)) {
            *out_hit = hit;
            if (io_readback) io_readback->acceptedHitCount += 1u;
            return true;
        }
        if (hit.sceneObjectIndex == policy->dielectricSceneObjectIndex && io_readback) {
            io_readback->selfHitRejectCount += 1u;
        }
        origin = vec3_add(hit.position, vec3_scale(direction, 1.0e-4));
    }
    return false;
}

void RuntimeCausticPhotonReceiverPolicy3D_Default(
    RuntimeCausticPhotonReceiverPolicy3D* policy) {
    if (!policy) return;
    memset(policy, 0, sizeof(*policy));
    policy->dielectricSceneObjectIndex = -1;
    policy->receiverProbeDistance = 1.0;
    policy->maxSelfHitSkips = 6u;
    policy->rejectTransparentReceivers = true;
    policy->rejectEmissiveReceivers = true;
    policy->rejectSpecularDominantReceivers = true;
}

void RuntimeCausticPhotonReceiverPolicy3D_ResetReadback(
    RuntimeCausticPhotonReceiverPolicyReadback3D* readback) {
    if (!readback) return;
    memset(readback, 0, sizeof(*readback));
}

bool RuntimeCausticPhotonReceiverPolicy3D_SelectHit(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonTrace3D* trace,
    const RuntimeCausticPhotonReceiverPolicy3D* policy,
    HitInfo3D* out_hit,
    RuntimeCausticPhotonReceiverPolicyReadback3D* io_readback) {
    static const Vec3 probe_directions[] = {
        {0.0, 0.0, 1.0},
        {0.0, 0.0, -1.0},
        {0.0, 1.0, 0.0},
        {0.0, -1.0, 0.0},
        {1.0, 0.0, 0.0},
        {-1.0, 0.0, 0.0},
    };
    double probe_distance = 1.0;

    if (!scene || !trace || !trace->valid || !policy || !out_hit) {
        if (io_readback) io_readback->invalidRejectCount += 1u;
        return false;
    }
    if (io_readback) io_readback->lookupAttemptCount += 1u;
    if (receiver_policy_trace_direction(scene,
                                        trace->postExitOrigin,
                                        trace->postExitDirection,
                                        policy,
                                        1.0e6,
                                        out_hit,
                                        io_readback)) {
        if (io_readback) io_readback->directHitCount += 1u;
        return true;
    }

    probe_distance = policy->receiverProbeDistance > 0.0
                         ? policy->receiverProbeDistance
                         : 1.0;
    for (size_t i = 0u; i < sizeof(probe_directions) / sizeof(probe_directions[0]);
         ++i) {
        if (io_readback) io_readback->crossingProbeAttemptCount += 1u;
        if (receiver_policy_trace_direction(scene,
                                            trace->receiverCrossing,
                                            probe_directions[i],
                                            policy,
                                            probe_distance,
                                            out_hit,
                                            io_readback)) {
            if (io_readback) io_readback->crossingProbeHitCount += 1u;
            return true;
        }
    }
    if (io_readback) io_readback->missRejectCount += 1u;
    return false;
}

bool RuntimeCausticPhotonReceiverPolicy3D_SelectPrimaryBucket(
    const RuntimeCausticPhotonSurfaceHit3D* hits,
    uint64_t hit_count,
    RuntimeCausticPhotonReceiverBucket3D* out_bucket) {
    RuntimeCausticPhotonReceiverBucket3D best;
    uint64_t bucket_count = 0u;

    if (out_bucket) memset(out_bucket, 0, sizeof(*out_bucket));
    if (!hits || hit_count == 0u || !out_bucket) return false;

    memset(&best, 0, sizeof(best));
    for (uint64_t i = 0u; i < hit_count; ++i) {
        const RuntimeCausticPhotonSurfaceHit3D* candidate = &hits[i];
        bool first_in_bucket = true;
        uint64_t candidate_count = 0u;

        for (uint64_t previous = 0u; previous < i; ++previous) {
            if (receiver_policy_hit_identity_matches(&hits[previous],
                                                     candidate->sceneObjectIndex,
                                                     candidate->primitiveIndex,
                                                     candidate->triangleIndex)) {
                first_in_bucket = false;
                break;
            }
        }
        if (!first_in_bucket) continue;
        bucket_count += 1u;
        for (uint64_t j = i; j < hit_count; ++j) {
            if (receiver_policy_hit_identity_matches(&hits[j],
                                                     candidate->sceneObjectIndex,
                                                     candidate->primitiveIndex,
                                                     candidate->triangleIndex)) {
                candidate_count += 1u;
            }
        }
        if (!best.valid || candidate_count > best.hitCount) {
            best.valid = true;
            best.firstIndex = i;
            best.hitCount = candidate_count;
            best.sceneObjectIndex = candidate->sceneObjectIndex;
            best.primitiveIndex = candidate->primitiveIndex;
            best.triangleIndex = candidate->triangleIndex;
        }
    }
    if (!best.valid) return false;
    best.bucketCount = bucket_count;
    best.competingHitCount = hit_count > best.hitCount ? hit_count - best.hitCount : 0u;
    *out_bucket = best;
    return true;
}

bool RuntimeCausticPhotonReceiverPolicy3D_HitMatchesBucket(
    const RuntimeCausticPhotonSurfaceHit3D* hit,
    const RuntimeCausticPhotonReceiverBucket3D* bucket) {
    return hit && bucket && bucket->valid &&
           receiver_policy_hit_identity_matches(hit,
                                                bucket->sceneObjectIndex,
                                                bucket->primitiveIndex,
                                                bucket->triangleIndex);
}
