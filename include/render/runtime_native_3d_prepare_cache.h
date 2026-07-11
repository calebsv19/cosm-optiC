#ifndef RENDER_RUNTIME_NATIVE_3D_PREPARE_CACHE_H
#define RENDER_RUNTIME_NATIVE_3D_PREPARE_CACHE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum RuntimeNative3DFrameBVHSkipDecision {
    RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_NOT_REQUESTED = 0,
    RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_ROUTE_REQUIRES_FLATTENED_BVH = 1,
    RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_TLAS_BIND_NOT_READY = 2,
    RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_SKIPPED_TLAS_READY = 3,
    RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_DISABLED_BY_ENV = 4
} RuntimeNative3DFrameBVHSkipDecision;

typedef struct RuntimeNative3DPreparedSceneCacheStats {
    uint64_t generation;
    uint64_t cachedGeneration;
    bool valid;
    uint64_t hits;
    uint64_t misses;
    uint64_t stores;
    uint64_t invalidations;
    bool staticGeometryReuseEnabled;
    uint64_t timeIndependentHits;
    double cachedNormalizedT;
    double lastRequestedNormalizedT;
    int cachedPrimitiveCount;
    int cachedTriangleCount;
    int cachedBVHNodeCount;
    int cachedBVHLeafCount;
    bool dataflowStatsEnabled;
    bool lastPrepareValid;
    bool lastPrepareCacheHit;
    bool lastPrepareCacheMiss;
    bool lastPrepareCopiedScene;
    uint64_t prepareCalls;
    uint64_t cacheHitPrepareCalls;
    uint64_t cacheMissPrepareCalls;
    uint64_t copyCalls;
    uint64_t cacheHitCopyCalls;
    uint64_t cacheMissCopyCalls;
    uint64_t bindAfterCopyCalls;
    uint64_t finalFrameBindCalls;
    uint64_t frameBVHEnsureCalls;
    uint64_t frameBVHBuildCalls;
    uint64_t frameBVHAlreadyReadyCalls;
    uint64_t frameBVHSkipForTLASCalls;
    uint64_t frameBVHTLASReadinessChecks;
    double sceneBuildMsTotal;
    double cacheStoreMsTotal;
    double copyMsTotal;
    double cacheHitCopyMsTotal;
    double cacheMissCopyMsTotal;
    double bindAfterCopyMsTotal;
    double finalFrameBindMsTotal;
    double frameBVHEnsureMsTotal;
    double frameBVHTLASReadinessBindMsTotal;
    double lastSceneBuildMs;
    double lastCacheStoreMs;
    double lastCopyMs;
    double lastBindAfterCopyMs;
    double lastFinalFrameBindMs;
    double lastFrameBVHEnsureMs;
    double lastFrameBVHTLASReadinessBindMs;
    bool flattenedBVHSkipOnTLASEnabled;
    bool flattenedBVHSkipOnTLASDefaultEnabled;
    bool flattenedBVHSkipOnTLASForceEnabled;
    bool flattenedBVHSkipOnTLASDefaultDisabled;
    bool lastFrameBVHRequired;
    bool lastFrameBVHSkippedForTLAS;
    bool lastFrameBVHReady;
    bool lastTLASReadyForFrameBVHSkip;
    RuntimeNative3DFrameBVHSkipDecision lastFrameBVHSkipDecision;
    int lastCopiedPrimitiveCount;
    int lastCopiedTriangleCount;
    int lastCopiedBVHNodeCount;
    int lastCopiedLightCount;
    int lastCopiedEmissiveCandidateCount;
    int lastFrameBVHTriangleCount;
    int lastFrameBVHNodeCount;
    uint64_t lastFrameBVHTotalBytes;
    uint64_t lastCopiedPrimitiveBytes;
    uint64_t lastCopiedTriangleBytes;
    uint64_t lastCopiedBVHBytes;
    uint64_t lastCopiedLightBytes;
    uint64_t lastCopiedEmissiveCandidateBytes;
    uint64_t lastCopiedEstimatedBytes;
    uint64_t totalCopiedEstimatedBytes;
} RuntimeNative3DPreparedSceneCacheStats;

void RuntimeNative3DPreparedSceneMarkDirty(const char* reason);
void RuntimeNative3DPreparedSceneCacheStatsSnapshot(
    RuntimeNative3DPreparedSceneCacheStats* out_stats);
void RuntimeNative3DPreparedSceneCacheResetForTests(void);

#endif
