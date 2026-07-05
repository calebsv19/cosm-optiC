#ifndef RENDER_RUNTIME_TRIANGLE_BVH_3D_H
#define RENDER_RUNTIME_TRIANGLE_BVH_3D_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "render/runtime_ray_3d.h"

typedef struct RuntimeTriangleBVH3D RuntimeTriangleBVH3D;

typedef enum RuntimeTriangleBVH3DTraceResult {
    RUNTIME_TRIANGLE_BVH_3D_TRACE_MISS = 0,
    RUNTIME_TRIANGLE_BVH_3D_TRACE_HIT = 1,
    RUNTIME_TRIANGLE_BVH_3D_TRACE_OVERFLOW = 2
} RuntimeTriangleBVH3DTraceResult;

typedef struct RuntimeTriangleBVH3DBuildStats {
    bool ready;
    int triangleCount;
    int nodeCount;
    int leafCount;
    int maxDepth;
    int leafSize;
    int maxLeafTriangleCount;
    double buildCpuMs;
    double allocationCpuMs;
    double centroidBuildCpuMs;
    double treeBuildCpuMs;
    double rangeBoundsCpuMs;
    double sortCpuMs;
    double nodeAppendCpuMs;
    double finalStatsCpuMs;
    double buildUnaccountedCpuMs;
    uint64_t rangeBoundsCalls;
    uint64_t sortCalls;
    uint64_t nodeAppendCalls;
    int maxRangeBoundsCount;
    int maxSortCount;
    uint64_t nodeBytes;
    uint64_t indexBytes;
    uint64_t centroidBytes;
    uint64_t triangleBoundsMinBytes;
    uint64_t triangleBoundsMaxBytes;
    uint64_t sortScratchBytes;
    uint64_t buildScratchBytes;
    uint64_t totalBytes;
} RuntimeTriangleBVH3DBuildStats;

typedef struct RuntimeTriangleBVH3DTraceStats {
    uint64_t traceCalls;
    uint64_t traceHits;
    uint64_t traceMisses;
    uint64_t traceOverflows;
    uint64_t flatFallbackCalls;
    uint64_t overflowFallbackCalls;
    uint64_t nodeVisits;
    uint64_t leafVisits;
    uint64_t aabbTests;
    uint64_t aabbHits;
    uint64_t triangleTests;
    uint64_t triangleHits;
    uint64_t maxStackDepth;
} RuntimeTriangleBVH3DTraceStats;

void RuntimeTriangleMesh3D_ClearBVH(RuntimeTriangleMesh3D* mesh);
bool RuntimeTriangleMesh3D_BuildBVH(RuntimeTriangleMesh3D* mesh);
const char* RuntimeTriangleMesh3D_BVHLastDiagnostics(void);
bool RuntimeTriangleMesh3D_CopyBVH(RuntimeTriangleMesh3D* dst,
                                   const RuntimeTriangleMesh3D* src);
bool RuntimeTriangleMesh3D_WriteBVHCachePayload(FILE* file,
                                                const RuntimeTriangleMesh3D* mesh);
bool RuntimeTriangleMesh3D_ReadBVHCachePayload(FILE* file,
                                               RuntimeTriangleMesh3D* mesh,
                                               int expected_triangle_count,
                                               char* out_diagnostics,
                                               size_t out_diagnostics_size);
bool RuntimeTriangleMesh3D_HasReadyBVH(const RuntimeTriangleMesh3D* mesh);
int RuntimeTriangleMesh3D_BVHNodeCount(const RuntimeTriangleMesh3D* mesh);
int RuntimeTriangleMesh3D_BVHLeafCount(const RuntimeTriangleMesh3D* mesh);
bool RuntimeTriangleMesh3D_BVHBuildStats(const RuntimeTriangleMesh3D* mesh,
                                         RuntimeTriangleBVH3DBuildStats* out_stats);

void RuntimeTriangleBVH3D_ResetTraceStats(void);
void RuntimeTriangleBVH3D_DisableTraceStats(void);
void RuntimeTriangleBVH3D_SnapshotTraceStats(RuntimeTriangleBVH3DTraceStats* out_stats);
void RuntimeTriangleBVH3D_RecordFlatFallback(bool due_to_overflow);
void RuntimeTriangleBVH3D_SetTraversalStackLimitForTests(int max_stack_depth);

bool RuntimeTriangleBVH3D_TraceFirstHit(const RuntimeTriangleMesh3D* mesh,
                                        const Ray3D* ray,
                                        [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
                                        [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
                                        HitInfo3D* out_hit);
RuntimeTriangleBVH3DTraceResult RuntimeTriangleBVH3D_TraceFirstHitStatus(
    const RuntimeTriangleMesh3D* mesh,
    const Ray3D* ray,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
    HitInfo3D* out_hit);

#endif
