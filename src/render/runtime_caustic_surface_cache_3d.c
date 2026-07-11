#include "render/runtime_caustic_surface_cache_3d.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    RUNTIME_CAUSTIC_SURFACE_CACHE_DEFAULT_CAPACITY = 4096,
    RUNTIME_CAUSTIC_SURFACE_CACHE_MAX_CAPACITY = 65536
};

static double runtime_caustic_surface_cache_luma(double r, double g, double b) {
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

static double runtime_caustic_surface_cache_clamp(double value,
                                                  double min_value,
                                                  double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_caustic_surface_cache_kernel_area_normalization(double radius) {
    /*
     * Treat deposited record radiance as bounded flux, not as a peak value.
     * Broad footprints should spread that flux across receiver samples instead
     * of preserving peak intensity while touching more pixels. Keep compact
     * proof-scale footprints unchanged to avoid inflating small diagnostic dots.
     */
    const double gaussian_area = 2.0 * M_PI * radius * radius;
    if (!(gaussian_area > 1.0)) return 1.0;
    return 1.0 / gaussian_area;
}

void RuntimeCausticSurfaceCache3D_Init(RuntimeCausticSurfaceCache3D* cache) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
}

bool RuntimeCausticSurfaceCache3D_IsAllocated(const RuntimeCausticSurfaceCache3D* cache) {
    return cache && cache->ownsRecords && cache->records && cache->recordCapacity > 0u;
}

bool RuntimeCausticSurfaceCache3D_Allocate(RuntimeCausticSurfaceCache3D* cache,
                                           uint64_t record_capacity) {
    RuntimeCausticSurfaceCache3D allocated;

    if (!cache) return false;
    if (record_capacity == 0u) {
        record_capacity = RUNTIME_CAUSTIC_SURFACE_CACHE_DEFAULT_CAPACITY;
    }
    if (record_capacity > RUNTIME_CAUSTIC_SURFACE_CACHE_MAX_CAPACITY) {
        record_capacity = RUNTIME_CAUSTIC_SURFACE_CACHE_MAX_CAPACITY;
    }
    if (record_capacity > (uint64_t)(SIZE_MAX / sizeof(RuntimeCausticSurfaceCacheRecord3D))) {
        return false;
    }

    RuntimeCausticSurfaceCache3D_Init(&allocated);
    allocated.records = (RuntimeCausticSurfaceCacheRecord3D*)calloc(
        (size_t)record_capacity,
        sizeof(RuntimeCausticSurfaceCacheRecord3D));
    if (!allocated.records) return false;
    allocated.recordCapacity = record_capacity;
    allocated.ownsRecords = true;

    RuntimeCausticSurfaceCache3D_Free(cache);
    *cache = allocated;
    return true;
}

void RuntimeCausticSurfaceCache3D_Clear(RuntimeCausticSurfaceCache3D* cache) {
    if (!RuntimeCausticSurfaceCache3D_IsAllocated(cache)) return;
    memset(cache->records,
           0,
           (size_t)cache->recordCapacity * sizeof(RuntimeCausticSurfaceCacheRecord3D));
    cache->recordCount = 0u;
    cache->depositAttemptCount = 0u;
    cache->depositAcceptedCount = 0u;
    cache->depositRejectedCount = 0u;
    cache->sampleLookupCount = 0u;
    cache->sampleContributingCount = 0u;
    cache->nearestSampleCandidateCount = 0u;
    cache->nearestSampleDistance = 0.0;
    cache->nearestSampleRadius = 0.0;
    cache->nearestSampleNormalDot = 0.0;
}

void RuntimeCausticSurfaceCache3D_Free(RuntimeCausticSurfaceCache3D* cache) {
    if (!cache) return;
    free(cache->records);
    RuntimeCausticSurfaceCache3D_Init(cache);
}

bool RuntimeCausticSurfaceCache3D_DepositAtHit(RuntimeCausticSurfaceCache3D* cache,
                                               const HitInfo3D* hit,
                                               double radius,
                                               double radiance_r,
                                               double radiance_g,
                                               double radiance_b) {
    RuntimeCausticSurfaceCacheRecord3D* record = NULL;

    if (!cache) return false;
    cache->depositAttemptCount += 1u;
    if (!RuntimeCausticSurfaceCache3D_IsAllocated(cache) || !hit ||
        cache->recordCount >= cache->recordCapacity) {
        cache->depositRejectedCount += 1u;
        return false;
    }
    if (radiance_r < 0.0) radiance_r = 0.0;
    if (radiance_g < 0.0) radiance_g = 0.0;
    if (radiance_b < 0.0) radiance_b = 0.0;
    if (!(runtime_caustic_surface_cache_luma(radiance_r, radiance_g, radiance_b) > 1.0e-12)) {
        cache->depositRejectedCount += 1u;
        return false;
    }
    radius = runtime_caustic_surface_cache_clamp(radius, 0.005, 2.0);

    record = &cache->records[cache->recordCount++];
    record->position = hit->position;
    record->normal = vec3_normalize(hit->normal);
    record->radius = (float)radius;
    record->radianceR = (float)radiance_r;
    record->radianceG = (float)radiance_g;
    record->radianceB = (float)radiance_b;
    record->sceneObjectIndex = hit->sceneObjectIndex;
    record->primitiveIndex = hit->primitiveIndex;
    record->triangleIndex = hit->triangleIndex;
    cache->depositAcceptedCount += 1u;
    return true;
}

bool RuntimeCausticSurfaceCache3D_SampleAtHit(RuntimeCausticSurfaceCache3D* cache,
                                              const HitInfo3D* hit,
                                              Vec3* out_radiance) {
    Vec3 result = vec3(0.0, 0.0, 0.0);

    if (out_radiance) *out_radiance = result;
    if (!cache || !out_radiance) return false;
    cache->sampleLookupCount += 1u;
    if (!RuntimeCausticSurfaceCache3D_IsAllocated(cache) || !hit) {
        return false;
    }

    for (uint64_t i = 0u; i < cache->recordCount; ++i) {
        const RuntimeCausticSurfaceCacheRecord3D* record = &cache->records[i];
        Vec3 delta = vec3_sub(hit->position, record->position);
        double radius = fmax((double)record->radius, 0.005);
        double d2 = vec3_dot(delta, delta);
        double normal_dot = fabs(vec3_dot(vec3_normalize(hit->normal), record->normal));
        double weight = 0.0;
        double distance = sqrt(fmax(d2, 0.0));
        if (cache->nearestSampleCandidateCount == 0u ||
            distance < cache->nearestSampleDistance) {
            cache->nearestSampleDistance = distance;
            cache->nearestSampleRadius = radius;
            cache->nearestSampleNormalDot = normal_dot;
        }
        cache->nearestSampleCandidateCount += 1u;
        if (d2 > radius * radius || normal_dot < 0.25) {
            continue;
        }
        if (record->sceneObjectIndex >= 0 &&
            hit->sceneObjectIndex >= 0 &&
            record->sceneObjectIndex != hit->sceneObjectIndex) {
            continue;
        }
        weight = exp(-d2 / (2.0 * radius * radius)) *
                 runtime_caustic_surface_cache_clamp(normal_dot, 0.0, 1.0) *
                 runtime_caustic_surface_cache_kernel_area_normalization(radius);
        result.x += (double)record->radianceR * weight;
        result.y += (double)record->radianceG * weight;
        result.z += (double)record->radianceB * weight;
    }

    *out_radiance = result;
    if (result.x > 0.0 || result.y > 0.0 || result.z > 0.0) {
        cache->sampleContributingCount += 1u;
        return true;
    }
    return false;
}

void RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(
    const RuntimeCausticSurfaceCache3D* cache,
    RuntimeCausticSurfaceCacheDiagnostics3D* out_diagnostics) {
    RuntimeCausticSurfaceCacheDiagnostics3D diagnostics = {0};

    if (!out_diagnostics) return;
    if (!RuntimeCausticSurfaceCache3D_IsAllocated(cache)) {
        diagnostics.state = RUNTIME_CAUSTIC_CACHE_STATE_NONE;
        *out_diagnostics = diagnostics;
        return;
    }

    diagnostics.allocated = true;
    diagnostics.recordCapacity = cache->recordCapacity;
    diagnostics.recordCount = cache->recordCount;
    diagnostics.depositAttemptCount = cache->depositAttemptCount;
    diagnostics.depositAcceptedCount = cache->depositAcceptedCount;
    diagnostics.depositRejectedCount = cache->depositRejectedCount;
    diagnostics.sampleLookupCount = cache->sampleLookupCount;
    diagnostics.sampleContributingCount = cache->sampleContributingCount;
    diagnostics.nearestSampleCandidateCount = cache->nearestSampleCandidateCount;
    diagnostics.nearestSampleDistance = cache->nearestSampleDistance;
    diagnostics.nearestSampleRadius = cache->nearestSampleRadius;
    diagnostics.nearestSampleNormalDot = cache->nearestSampleNormalDot;
    for (uint64_t i = 0u; i < cache->recordCount; ++i) {
        const RuntimeCausticSurfaceCacheRecord3D* record = &cache->records[i];
        double luma = runtime_caustic_surface_cache_luma(record->radianceR,
                                                        record->radianceG,
                                                        record->radianceB);
        diagnostics.totalRadianceR += record->radianceR;
        diagnostics.totalRadianceG += record->radianceG;
        diagnostics.totalRadianceB += record->radianceB;
        if (luma > diagnostics.maxRecordRadiance) {
            diagnostics.maxRecordRadiance = luma;
        }
    }
    if (diagnostics.sampleContributingCount > 0u) {
        diagnostics.state = RUNTIME_CAUSTIC_CACHE_STATE_SAMPLED;
    } else if (diagnostics.recordCount > 0u) {
        diagnostics.state = RUNTIME_CAUSTIC_CACHE_STATE_POPULATED;
    } else {
        diagnostics.state = RUNTIME_CAUSTIC_CACHE_STATE_ALLOCATED_EMPTY;
    }
    *out_diagnostics = diagnostics;
}
