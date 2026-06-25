#ifndef RENDER_RUNTIME_SCENE_ACCEL_3D_H
#define RENDER_RUNTIME_SCENE_ACCEL_3D_H

#include <stdbool.h>
#include <stdint.h>

typedef enum RuntimeSceneAcceleration3DReuseStatus {
    RUNTIME_SCENE_ACCEL_3D_REUSE_DISABLED = 0,
    RUNTIME_SCENE_ACCEL_3D_REUSE_REBUILT,
    RUNTIME_SCENE_ACCEL_3D_REUSE_REUSED,
    RUNTIME_SCENE_ACCEL_3D_REUSE_REFIT
} RuntimeSceneAcceleration3DReuseStatus;

typedef struct RuntimeSceneAcceleration3DDiagnostics {
    bool enabled;
    RuntimeSceneAcceleration3DReuseStatus reuseStatus;
    uint64_t blasCacheHits;
    uint64_t blasCacheMisses;
    uint64_t blasCacheInvalidations;
    uint64_t blasFullRebuilds;
    uint64_t blasCachedAssetCount;
    uint64_t tlasNodeCount;
    uint64_t tlasInstanceCount;
    uint64_t tlasRebuilds;
    uint64_t tlasRefits;
} RuntimeSceneAcceleration3DDiagnostics;

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

#endif
