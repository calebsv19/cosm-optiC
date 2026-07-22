#include "render/runtime_caustic_photon_sample_support_3d.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_caustic_photon_surface_kernel_3d.h"

static int photon_sample_support_compare_double(const void* lhs_ptr,
                                                const void* rhs_ptr) {
    const double lhs = *(const double*)lhs_ptr;
    const double rhs = *(const double*)rhs_ptr;
    return lhs < rhs ? -1 : lhs > rhs ? 1 : 0;
}

static bool photon_sample_support_same_receiver(
    const RuntimeCausticPhotonMapRecord3D* lhs,
    const RuntimeCausticPhotonMapRecord3D* rhs,
    double min_normal_dot) {
    if (!lhs || !rhs || lhs->sceneObjectIndex != rhs->sceneObjectIndex) {
        return false;
    }
    if (lhs->materialId >= 0 && rhs->materialId >= 0 &&
        lhs->materialId != rhs->materialId) {
        return false;
    }
    return vec3_dot(lhs->normal, rhs->normal) >= min_normal_dot;
}

bool RuntimeCausticPhotonSampleSupport3D_Prepare(
    RuntimeCausticPhotonMapRecord3D* records,
    uint64_t record_count,
    uint64_t neighbor_limit,
    double min_normal_dot,
    RuntimeCausticPhotonSampleSupportReadback3D* out_readback) {
    RuntimeCausticPhotonSampleSupportReadback3D readback;
    double* distances;

    memset(&readback, 0, sizeof(readback));
    readback.recordCount = record_count;
    readback.neighborLimit = neighbor_limit;
    readback.minimumSupportRadius = DBL_MAX;
    if (out_readback) *out_readback = readback;
    if (!records || record_count == 0u || neighbor_limit == 0u ||
        record_count > (uint64_t)(SIZE_MAX / sizeof(double))) {
        return false;
    }
    distances = (double*)malloc((size_t)record_count * sizeof(double));
    if (!distances) return false;
    for (uint64_t i = 0u; i < record_count; ++i) {
        RuntimeCausticPhotonMapRecord3D* record = &records[i];
        uint64_t distance_count = 0u;
        double support_radius = record->queryRadius;
        bool adaptive = false;
        for (uint64_t j = 0u; j < record_count; ++j) {
            Vec3 delta;
            double distance;
            if (i == j || !photon_sample_support_same_receiver(
                              record, &records[j], min_normal_dot)) {
                continue;
            }
            delta = vec3_sub(record->position, records[j].position);
            distance = vec3_length(delta);
            if (!(distance >= 0.0) || distance >= record->queryRadius) continue;
            distances[distance_count++] = distance;
        }
        if (distance_count >= neighbor_limit) {
            qsort(distances,
                  (size_t)distance_count,
                  sizeof(double),
                  photon_sample_support_compare_double);
            support_radius =
                RuntimeCausticPhotonSurfaceKernel3D_BoundedAdaptiveRadius(
                    distances[neighbor_limit - 1u],
                    record->queryRadius,
                    &adaptive);
        }
        record->sampleCenteredSupportRadius = support_radius;
        record->sampleCenteredSupportNeighborCount = distance_count;
        record->sampleCenteredSupportAdaptive = adaptive;
        record->sampleCenteredSupportPrepared = true;
        if (adaptive) readback.adaptiveRecordCount++;
        else readback.maximumRadiusRecordCount++;
        if (support_radius < readback.minimumSupportRadius) {
            readback.minimumSupportRadius = support_radius;
        }
        if (support_radius > readback.maximumSupportRadius) {
            readback.maximumSupportRadius = support_radius;
        }
        readback.meanSupportRadius += support_radius;
    }
    free(distances);
    readback.meanSupportRadius /= (double)record_count;
    readback.valid = true;
    if (out_readback) *out_readback = readback;
    return true;
}
