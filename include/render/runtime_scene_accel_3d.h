#ifndef RENDER_RUNTIME_SCENE_ACCEL_3D_H
#define RENDER_RUNTIME_SCENE_ACCEL_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "import/runtime_mesh_asset_loader.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"

typedef enum RuntimeSceneAcceleration3DReuseStatus {
    RUNTIME_SCENE_ACCEL_3D_REUSE_DISABLED = 0,
    RUNTIME_SCENE_ACCEL_3D_REUSE_REBUILT,
    RUNTIME_SCENE_ACCEL_3D_REUSE_REUSED,
    RUNTIME_SCENE_ACCEL_3D_REUSE_REFIT
} RuntimeSceneAcceleration3DReuseStatus;

typedef struct RuntimeSceneAcceleration3DDiagnostics {
    bool enabled;
    RuntimeSceneAcceleration3DReuseStatus reuseStatus;
    uint64_t blasPrepareCalls;
    uint64_t blasCacheHits;
    uint64_t blasCacheMisses;
    uint64_t blasCacheInvalidations;
    uint64_t blasFullRebuilds;
    uint64_t blasCachedAssetCount;
    uint64_t tlasNodeCount;
    uint64_t tlasInstanceCount;
    uint64_t tlasRebuilds;
    uint64_t tlasRefits;
    double blasBuildMs;
    double tlasBuildMs;
    double tlasBindMs;
} RuntimeSceneAcceleration3DDiagnostics;

typedef enum RuntimeSceneAcceleration3DTraceStatus {
    RUNTIME_SCENE_ACCEL_3D_TRACE_UNREADY = 0,
    RUNTIME_SCENE_ACCEL_3D_TRACE_MISS = 1,
    RUNTIME_SCENE_ACCEL_3D_TRACE_HIT = 2,
    RUNTIME_SCENE_ACCEL_3D_TRACE_UNSUPPORTED = 3,
    RUNTIME_SCENE_ACCEL_3D_TRACE_ERROR = 4
} RuntimeSceneAcceleration3DTraceStatus;

typedef struct RuntimeSceneAcceleration3DTraceStats {
    uint64_t traceCalls;
    uint64_t traceHits;
    uint64_t traceMisses;
    uint64_t traceUnready;
    uint64_t traceUnsupported;
    uint64_t traceErrors;
    uint64_t tlasNodeTests;
    uint64_t tlasNodeHits;
    uint64_t tlasInstanceTests;
    uint64_t blasTraceCalls;
    uint64_t blasTraceHits;
    uint64_t identityRemapMapHits;
    uint64_t identityRemapFallbackScans;
    uint64_t identityRemapFailures;
} RuntimeSceneAcceleration3DTraceStats;

static inline const char* RuntimeSceneAcceleration3DReuseStatusLabel(
    RuntimeSceneAcceleration3DReuseStatus status) {
    switch (status) {
        case RUNTIME_SCENE_ACCEL_3D_REUSE_REBUILT:
            return "rebuilt";
        case RUNTIME_SCENE_ACCEL_3D_REUSE_REUSED:
            return "reused";
        case RUNTIME_SCENE_ACCEL_3D_REUSE_REFIT:
            return "refit";
        case RUNTIME_SCENE_ACCEL_3D_REUSE_DISABLED:
        default:
            return "disabled";
    }
}

static inline RuntimeSceneAcceleration3DDiagnostics
RuntimeSceneAcceleration3DDiagnostics_Disabled(void) {
    RuntimeSceneAcceleration3DDiagnostics diagnostics = {0};
    diagnostics.enabled = false;
    diagnostics.reuseStatus = RUNTIME_SCENE_ACCEL_3D_REUSE_DISABLED;
    return diagnostics;
}

bool RuntimeSceneAcceleration3D_RebuildTLASFromScene(const RuntimeScene3D* scene);
bool RuntimeSceneAcceleration3D_RebuildPreparedFromSceneAndMeshAssets(
    const RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets);
bool RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(const RuntimeScene3D* scene);
RuntimeSceneAcceleration3DTraceStatus RuntimeSceneAcceleration3D_TraceFirstHit(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
    HitInfo3D* out_hit);
void RuntimeSceneAcceleration3D_ResetTraceStats(void);
void RuntimeSceneAcceleration3D_SnapshotTraceStats(
    RuntimeSceneAcceleration3DTraceStats* out_stats);
void RuntimeSceneAcceleration3D_AppendTLASDiagnostics(
    RuntimeSceneAcceleration3DDiagnostics* diagnostics);
void RuntimeSceneAcceleration3D_ResetTLASForTests(void);
const char* RuntimeSceneAcceleration3D_LastDiagnostics(void);

#endif
