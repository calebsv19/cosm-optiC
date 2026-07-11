#include "render/runtime_caustic_photon_integration_3d.h"

#include <math.h>
#include <string.h>

static bool receiver_contribution_record_identity_matches(
    const RuntimeCausticPhotonReceiverContributionRecord3D* bucket,
    const RuntimeCausticPhotonMapRecord3D* record) {
    return bucket && record &&
           bucket->sceneObjectIndex == record->sceneObjectIndex &&
           bucket->primitiveIndex == record->primitiveIndex &&
           bucket->triangleIndex == record->triangleIndex;
}

static double receiver_contribution_positive_or_default(double value,
                                                        double fallback) {
    return value > 0.0 ? value : fallback;
}

static void receiver_contribution_finalize_bucket(
    RuntimeCausticPhotonReceiverContributionRecord3D* bucket) {
    if (!bucket || bucket->storedRecordCount == 0u) return;
    bucket->position = vec3_scale(bucket->position,
                                  1.0 / (double)bucket->storedRecordCount);
    if (vec3_length(bucket->normal) > 1.0e-9) {
        bucket->normal = vec3_normalize(bucket->normal);
    } else {
        bucket->normal = vec3(0.0, 1.0, 0.0);
    }
    bucket->radius = receiver_contribution_positive_or_default(bucket->radius,
                                                               0.10);
}

static RuntimeCausticPhotonReceiverContributionRecord3D*
receiver_contribution_find_or_add_bucket(
    RuntimeCausticPhotonReceiverContributionReadback3D* readback,
    const RuntimeCausticPhotonMapRecord3D* record) {
    RuntimeCausticPhotonReceiverContributionRecord3D* bucket = NULL;

    if (!readback || !record) return NULL;
    for (uint64_t i = 0u; i < readback->recordCount; ++i) {
        if (receiver_contribution_record_identity_matches(&readback->records[i],
                                                          record)) {
            return &readback->records[i];
        }
    }
    if (readback->recordCount >=
        RUNTIME_CAUSTIC_PHOTON_RECEIVER_CONTRIBUTION_MAX_RECORDS) {
        return NULL;
    }
    bucket = &readback->records[readback->recordCount++];
    memset(bucket, 0, sizeof(*bucket));
    bucket->valid = true;
    bucket->sceneObjectIndex = record->sceneObjectIndex;
    bucket->primitiveIndex = record->primitiveIndex;
    bucket->triangleIndex = record->triangleIndex;
    return bucket;
}

static void receiver_contribution_collect_buckets(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticPhotonReceiverContributionReadback3D* readback) {
    if (!surface_map || !readback || !RuntimeCausticPhotonMap3D_IsAllocated(surface_map)) {
        return;
    }
    for (uint64_t i = 0u; i < surface_map->recordCount; ++i) {
        const RuntimeCausticPhotonMapRecord3D* record = &surface_map->records[i];
        RuntimeCausticPhotonReceiverContributionRecord3D* bucket =
            receiver_contribution_find_or_add_bucket(readback, record);
        if (!bucket) continue;
        bucket->storedRecordCount += 1u;
        bucket->position = vec3_add(bucket->position, record->position);
        bucket->normal = vec3_add(bucket->normal, record->normal);
        bucket->radius = fmax(bucket->radius, record->queryRadius);
    }
    readback->receiverBucketCount = readback->recordCount;
    for (uint64_t i = 0u; i < readback->recordCount; ++i) {
        receiver_contribution_finalize_bucket(&readback->records[i]);
    }
}

bool RuntimeCausticPhotonIntegration3D_DepositSurfaceContributionsForReceiverBuckets(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticSurfaceCache3D* surface_cache,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    RuntimeCausticPhotonReceiverContributionReadback3D* out_readback) {
    RuntimeCausticPhotonIntegrationSettings3D default_settings;
    const RuntimeCausticPhotonIntegrationSettings3D* active_settings = settings;
    RuntimeCausticPhotonReceiverContributionReadback3D readback;

    memset(&readback, 0, sizeof(readback));
    if (out_readback) *out_readback = readback;
    if (!out_readback) return false;
    if (!active_settings) {
        RuntimeCausticPhotonIntegration3D_DefaultSettings(&default_settings);
        active_settings = &default_settings;
    }

    readback.attempted = true;
    readback.suppressed = !active_settings->renderContributionEnabled;
    if (RuntimeCausticPhotonIntegration3D_RouteForSettings(active_settings) !=
            RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY ||
        !active_settings->surfaceQueryEnabled ||
        !active_settings->renderContributionEnabled ||
        !RuntimeCausticPhotonMap3D_IsAllocated(surface_map) ||
        !RuntimeCausticSurfaceCache3D_IsAllocated(surface_cache)) {
        *out_readback = readback;
        return false;
    }

    receiver_contribution_collect_buckets(surface_map, &readback);
    for (uint64_t i = 0u; i < readback.recordCount; ++i) {
        RuntimeCausticPhotonReceiverContributionRecord3D* record =
            &readback.records[i];
        RuntimeCausticPhotonIntegrationQuery3D query;
        RuntimeCausticPhotonIntegrationResult3D query_result;
        RuntimeCausticPhotonContribution3D contribution;
        RuntimeCausticPhotonContributionDepositResult3D deposit_result;

        RuntimeCausticPhotonIntegration3D_DefaultQuery(&query);
        query.querySurface = true;
        query.queryVolume = false;
        query.surface.position = record->position;
        query.surface.normal = record->normal;
        query.surface.radius = fmax(
            record->radius,
            receiver_contribution_positive_or_default(active_settings->surfaceQueryRadius,
                                                      0.10));
        query.surface.sceneObjectIndex = record->sceneObjectIndex;
        query.surface.primitiveIndex = record->primitiveIndex;
        query.surface.triangleIndex = record->triangleIndex;

        readback.receiverQueryAttemptCount += 1u;
        if (!RuntimeCausticPhotonIntegration3D_Query(surface_map,
                                                     NULL,
                                                     active_settings,
                                                     &query,
                                                     &query_result)) {
            record->candidateCount = query_result.surfaceCandidateCount;
            record->contributingCount = query_result.surfaceContributingCount;
            continue;
        }
        record->queryHit = true;
        record->candidateCount = query_result.surfaceCandidateCount;
        record->contributingCount = query_result.surfaceContributingCount;
        readback.receiverQueryHitCount += 1u;
        readback.receiverSurfaceCandidateCount += query_result.surfaceCandidateCount;
        readback.receiverSurfaceContributingCount +=
            query_result.surfaceContributingCount;

        if (!RuntimeCausticPhotonIntegration3D_BuildContribution(active_settings,
                                                                 &query,
                                                                 &query_result,
                                                                 &contribution)) {
            continue;
        }
        record->contributionEligible = true;
        record->radiance = contribution.surfaceRadiance;
        readback.receiverContributionEligibleCount += 1u;
        readback.receiverSurfaceRadiance =
            vec3_add(readback.receiverSurfaceRadiance, contribution.surfaceRadiance);
        readback.receiverSurfaceDepositAttemptCount += 1u;
        if (RuntimeCausticPhotonIntegration3D_DepositContributionToCaches(
                surface_cache,
                NULL,
                &contribution,
                &deposit_result) &&
            deposit_result.surfaceDeposited) {
            record->surfaceDeposited = true;
            readback.receiverSurfaceDepositAcceptedCount += 1u;
        }
    }

    readback.eligible = readback.receiverSurfaceDepositAcceptedCount > 0u;
    *out_readback = readback;
    return readback.eligible;
}
