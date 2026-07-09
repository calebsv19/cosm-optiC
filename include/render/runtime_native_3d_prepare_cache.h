#ifndef RENDER_RUNTIME_NATIVE_3D_PREPARE_CACHE_H
#define RENDER_RUNTIME_NATIVE_3D_PREPARE_CACHE_H

#include <stdbool.h>
#include <stdint.h>

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
} RuntimeNative3DPreparedSceneCacheStats;

void RuntimeNative3DPreparedSceneMarkDirty(const char* reason);
void RuntimeNative3DPreparedSceneCacheStatsSnapshot(
    RuntimeNative3DPreparedSceneCacheStats* out_stats);
void RuntimeNative3DPreparedSceneCacheResetForTests(void);

#endif
