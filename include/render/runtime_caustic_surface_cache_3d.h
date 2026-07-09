#ifndef RENDER_RUNTIME_CAUSTIC_SURFACE_CACHE_3D_H
#define RENDER_RUNTIME_CAUSTIC_SURFACE_CACHE_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "math/vec3.h"
#include "render/runtime_caustic_settings_3d.h"
#include "render/runtime_ray_3d.h"

typedef struct {
    Vec3 position;
    Vec3 normal;
    float radius;
    float radianceR;
    float radianceG;
    float radianceB;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
} RuntimeCausticSurfaceCacheRecord3D;

typedef struct {
    bool allocated;
    uint64_t recordCapacity;
    uint64_t recordCount;
    uint64_t depositAttemptCount;
    uint64_t depositAcceptedCount;
    uint64_t depositRejectedCount;
    uint64_t sampleLookupCount;
    uint64_t sampleContributingCount;
    double totalRadianceR;
    double totalRadianceG;
    double totalRadianceB;
    double maxRecordRadiance;
    double nearestSampleDistance;
    double nearestSampleRadius;
    double nearestSampleNormalDot;
    uint64_t nearestSampleCandidateCount;
    RuntimeCausticCacheState3D state;
} RuntimeCausticSurfaceCacheDiagnostics3D;

typedef struct {
    RuntimeCausticSurfaceCacheRecord3D* records;
    uint64_t recordCapacity;
    uint64_t recordCount;
    uint64_t depositAttemptCount;
    uint64_t depositAcceptedCount;
    uint64_t depositRejectedCount;
    uint64_t sampleLookupCount;
    uint64_t sampleContributingCount;
    uint64_t nearestSampleCandidateCount;
    double nearestSampleDistance;
    double nearestSampleRadius;
    double nearestSampleNormalDot;
    bool ownsRecords;
} RuntimeCausticSurfaceCache3D;

void RuntimeCausticSurfaceCache3D_Init(RuntimeCausticSurfaceCache3D* cache);
bool RuntimeCausticSurfaceCache3D_IsAllocated(const RuntimeCausticSurfaceCache3D* cache);
bool RuntimeCausticSurfaceCache3D_Allocate(RuntimeCausticSurfaceCache3D* cache,
                                           uint64_t record_capacity);
void RuntimeCausticSurfaceCache3D_Clear(RuntimeCausticSurfaceCache3D* cache);
void RuntimeCausticSurfaceCache3D_Free(RuntimeCausticSurfaceCache3D* cache);
bool RuntimeCausticSurfaceCache3D_DepositAtHit(RuntimeCausticSurfaceCache3D* cache,
                                               const HitInfo3D* hit,
                                               double radius,
                                               double radiance_r,
                                               double radiance_g,
                                               double radiance_b);
bool RuntimeCausticSurfaceCache3D_SampleAtHit(RuntimeCausticSurfaceCache3D* cache,
                                              const HitInfo3D* hit,
                                              Vec3* out_radiance);
void RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(
    const RuntimeCausticSurfaceCache3D* cache,
    RuntimeCausticSurfaceCacheDiagnostics3D* out_diagnostics);

#endif
