#ifndef RENDER_RUNTIME_CURVE_BLAS_3D_H
#define RENDER_RUNTIME_CURVE_BLAS_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_curve_primitive_3d.h"

typedef enum RuntimeCurveBLAS3DTraceResult {
    RUNTIME_CURVE_BLAS_3D_TRACE_MISS = 0,
    RUNTIME_CURVE_BLAS_3D_TRACE_HIT = 1,
    RUNTIME_CURVE_BLAS_3D_TRACE_OVERFLOW = 2
} RuntimeCurveBLAS3DTraceResult;

typedef struct RuntimeCurveBLAS3DBuildStats {
    bool ready;
    size_t primitiveCount;
    size_t nodeCount;
    size_t leafCount;
    size_t maxDepth;
    size_t totalBytes;
} RuntimeCurveBLAS3DBuildStats;

typedef struct RuntimeCurveBLAS3DTraceStats {
    uint64_t traceCalls;
    uint64_t traceHits;
    uint64_t traceMisses;
    uint64_t traceOverflows;
    uint64_t nodeVisits;
    uint64_t aabbTests;
    uint64_t primitiveTests;
    uint64_t primitiveHits;
    uint64_t maxStackDepth;
} RuntimeCurveBLAS3DTraceStats;

void RuntimeCurveAsset3D_ClearBLAS(RuntimeCurveAsset3D *asset);
bool RuntimeCurveAsset3D_BuildBLAS(RuntimeCurveAsset3D *asset);
bool RuntimeCurveAsset3D_HasReadyBLAS(const RuntimeCurveAsset3D *asset);
bool RuntimeCurveAsset3D_BLASBuildStats(
    const RuntimeCurveAsset3D *asset,
    RuntimeCurveBLAS3DBuildStats *out_stats);

RuntimeCurveBLAS3DTraceResult RuntimeCurveBLAS3D_TraceFirstHitStatus(
    const RuntimeCurveAsset3D *asset,
    const Ray3D *ray,
    double t_min,
    double t_max,
    HitInfo3D *out_hit);
bool RuntimeCurveBLAS3D_TraceFirstHit(
    const RuntimeCurveAsset3D *asset,
    const Ray3D *ray,
    double t_min,
    double t_max,
    HitInfo3D *out_hit);

void RuntimeCurveBLAS3D_ResetTraceStats(void);
void RuntimeCurveBLAS3D_SnapshotTraceStats(
    RuntimeCurveBLAS3DTraceStats *out_stats);

#endif
