#include "render/runtime_native_3d_prepared_scene_cache_internal.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "render/runtime_frame_dataflow_ledger_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

static RuntimeScene3D gRuntimeNative3DPreparedSceneCache;
static bool gRuntimeNative3DPreparedSceneCacheInitialized = false;
static bool gRuntimeNative3DPreparedSceneCacheValid = false;
static bool gRuntimeNative3DPreparedSceneCacheStaticGeometry = true;
static double gRuntimeNative3DPreparedSceneCacheNormalizedT = 0.0;
static double gRuntimeNative3DPreparedSceneCacheLastRequestedT = 0.0;
static uint64_t gRuntimeNative3DPreparedSceneDirtyGeneration = 1u;
static uint64_t gRuntimeNative3DPreparedSceneCachedGeneration = 0u;
static uint64_t gRuntimeNative3DPreparedSceneCacheHits = 0u;
static uint64_t gRuntimeNative3DPreparedSceneCacheMisses = 0u;
static uint64_t gRuntimeNative3DPreparedSceneCacheStores = 0u;
static uint64_t gRuntimeNative3DPreparedSceneCacheInvalidations = 0u;
static uint64_t gRuntimeNative3DPreparedSceneCacheTimeIndependentHits = 0u;
static RuntimeNative3DPreparedSceneCacheStats gRuntimeNative3DPreparedSceneDataflowStats;

double runtime_native_3d_prepare_elapsed_ms_since(const struct timespec* start_time) {
    struct timespec now = {0};
    double elapsed = 0.0;
    if (!start_time) return 0.0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    elapsed = (double)(now.tv_sec - start_time->tv_sec) * 1000.0;
    elapsed += (double)(now.tv_nsec - start_time->tv_nsec) / 1000000.0;
    return elapsed < 0.0 ? 0.0 : elapsed;
}

RuntimeNative3DPreparedSceneCacheStats*
runtime_native_3d_prepared_scene_dataflow_stats(void) {
    return &gRuntimeNative3DPreparedSceneDataflowStats;
}

static uint64_t runtime_native_3d_prepare_u64_product(uint64_t a, uint64_t b) {
    if (a == 0u || b == 0u) return 0u;
    if (a > UINT64_MAX / b) return UINT64_MAX;
    return a * b;
}

static uint64_t runtime_native_3d_prepare_u64_add(uint64_t a, uint64_t b) {
    if (UINT64_MAX - a < b) return UINT64_MAX;
    return a + b;
}

static uint64_t runtime_native_3d_prepare_scene_array_bytes(
    int count,
    uint64_t element_size) {
    if (count <= 0) return 0u;
    return runtime_native_3d_prepare_u64_product((uint64_t)count, element_size);
}

static void runtime_native_3d_prepare_dataflow_reset_last(bool enabled) {
    RuntimeNative3DPreparedSceneCacheStats* stats =
        &gRuntimeNative3DPreparedSceneDataflowStats;
    stats->dataflowStatsEnabled = enabled;
    stats->lastPrepareValid = false;
    stats->lastPrepareCacheHit = false;
    stats->lastPrepareCacheMiss = false;
    stats->lastPrepareCopiedScene = false;
    stats->lastSceneBuildMs = 0.0;
    stats->lastCacheStoreMs = 0.0;
    stats->lastCopyMs = 0.0;
    stats->lastBindAfterCopyMs = 0.0;
    stats->lastFinalFrameBindMs = 0.0;
    stats->lastFrameBVHEnsureMs = 0.0;
    stats->lastFrameBVHTLASReadinessBindMs = 0.0;
    stats->lastFrameBVHRequired = true;
    stats->lastFrameBVHSkippedForTLAS = false;
    stats->lastFrameBVHReady = false;
    stats->lastTLASReadyForFrameBVHSkip = false;
    stats->lastFrameBVHSkipDecision =
        RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_NOT_REQUESTED;
    stats->lastCopiedPrimitiveCount = 0;
    stats->lastCopiedTriangleCount = 0;
    stats->lastCopiedBVHNodeCount = 0;
    stats->lastCopiedLightCount = 0;
    stats->lastCopiedEmissiveCandidateCount = 0;
    stats->lastFrameBVHTriangleCount = 0;
    stats->lastFrameBVHNodeCount = 0;
    stats->lastFrameBVHTotalBytes = 0u;
    stats->lastCopiedPrimitiveBytes = 0u;
    stats->lastCopiedTriangleBytes = 0u;
    stats->lastCopiedBVHBytes = 0u;
    stats->lastCopiedLightBytes = 0u;
    stats->lastCopiedEmissiveCandidateBytes = 0u;
    stats->lastCopiedEstimatedBytes = 0u;
}

static void runtime_native_3d_prepare_dataflow_begin(bool enabled) {
    if (!enabled) {
        memset(&gRuntimeNative3DPreparedSceneDataflowStats,
               0,
               sizeof(gRuntimeNative3DPreparedSceneDataflowStats));
        return;
    }
    runtime_native_3d_prepare_dataflow_reset_last(enabled);
    gRuntimeNative3DPreparedSceneDataflowStats.prepareCalls += 1u;
}

static uint64_t runtime_native_3d_prepare_scene_copy_bytes(
    const RuntimeScene3D* scene,
    RuntimeTriangleBVH3DBuildStats* out_bvh_stats) {
    uint64_t bytes = 0u;
    RuntimeTriangleBVH3DBuildStats bvh_stats = {0};
    if (out_bvh_stats) memset(out_bvh_stats, 0, sizeof(*out_bvh_stats));
    if (!scene) return 0u;
    bytes = runtime_native_3d_prepare_u64_add(
        bytes,
        runtime_native_3d_prepare_scene_array_bytes(
            scene->primitiveCount,
            (uint64_t)sizeof(RuntimePrimitive3D)));
    bytes = runtime_native_3d_prepare_u64_add(
        bytes,
        runtime_native_3d_prepare_scene_array_bytes(
            scene->triangleMesh.triangleCount,
            (uint64_t)sizeof(RuntimeTriangle3D)));
    bytes = runtime_native_3d_prepare_u64_add(
        bytes,
        runtime_native_3d_prepare_scene_array_bytes(
            scene->lightSet.lightCount,
            (uint64_t)sizeof(RuntimeLightSource3D)));
    bytes = runtime_native_3d_prepare_u64_add(
        bytes,
        runtime_native_3d_prepare_scene_array_bytes(
            scene->emissiveLightSet.candidateCount,
            (uint64_t)sizeof(RuntimeEmissiveLightCandidate3D)));
    if (RuntimeTriangleMesh3D_BVHBuildStats(&scene->triangleMesh, &bvh_stats) &&
        bvh_stats.ready) {
        bytes = runtime_native_3d_prepare_u64_add(bytes, bvh_stats.totalBytes);
        if (out_bvh_stats) *out_bvh_stats = bvh_stats;
    }
    return bytes;
}

static bool runtime_native_3d_prepare_copy_scene_for_frame(RuntimeScene3D* dst,
                                                          const RuntimeScene3D* src,
                                                          bool cache_hit) {
    RuntimeTriangleBVH3DBuildStats bvh_stats = {0};
    struct timespec copy_started_at = {0};
    const bool enabled =
        gRuntimeNative3DPreparedSceneDataflowStats.dataflowStatsEnabled;
    const uint64_t primitive_bytes =
        runtime_native_3d_prepare_scene_array_bytes(
            src ? src->primitiveCount : 0,
            (uint64_t)sizeof(RuntimePrimitive3D));
    const uint64_t triangle_bytes =
        runtime_native_3d_prepare_scene_array_bytes(
            src ? src->triangleMesh.triangleCount : 0,
            (uint64_t)sizeof(RuntimeTriangle3D));
    const uint64_t light_bytes =
        runtime_native_3d_prepare_scene_array_bytes(
            src ? src->lightSet.lightCount : 0,
            (uint64_t)sizeof(RuntimeLightSource3D));
    const uint64_t emissive_bytes =
        runtime_native_3d_prepare_scene_array_bytes(
            src ? src->emissiveLightSet.candidateCount : 0,
            (uint64_t)sizeof(RuntimeEmissiveLightCandidate3D));
    const uint64_t estimated_bytes =
        runtime_native_3d_prepare_scene_copy_bytes(src, &bvh_stats);
    bool ok = false;
    (void)clock_gettime(CLOCK_MONOTONIC, &copy_started_at);
    ok = RuntimeScene3D_CopyGeometryFrom(dst, src);
    if (enabled) {
        RuntimeNative3DPreparedSceneCacheStats* stats =
            &gRuntimeNative3DPreparedSceneDataflowStats;
        const double copy_ms =
            runtime_native_3d_prepare_elapsed_ms_since(&copy_started_at);
        stats->copyCalls += 1u;
        if (cache_hit) {
            stats->cacheHitCopyCalls += 1u;
            stats->cacheHitCopyMsTotal += copy_ms;
            stats->lastPrepareCacheHit = true;
        } else {
            stats->cacheMissCopyCalls += 1u;
            stats->cacheMissCopyMsTotal += copy_ms;
            stats->lastPrepareCacheMiss = true;
        }
        stats->copyMsTotal += copy_ms;
        stats->lastCopyMs = copy_ms;
        stats->lastPrepareCopiedScene = ok;
        stats->lastCopiedPrimitiveCount = src ? src->primitiveCount : 0;
        stats->lastCopiedTriangleCount = src ? src->triangleMesh.triangleCount : 0;
        stats->lastCopiedBVHNodeCount = bvh_stats.nodeCount;
        stats->lastCopiedLightCount = src ? src->lightSet.lightCount : 0;
        stats->lastCopiedEmissiveCandidateCount =
            src ? src->emissiveLightSet.candidateCount : 0;
        stats->lastCopiedPrimitiveBytes = primitive_bytes;
        stats->lastCopiedTriangleBytes = triangle_bytes;
        stats->lastCopiedBVHBytes = bvh_stats.totalBytes;
        stats->lastCopiedLightBytes = light_bytes;
        stats->lastCopiedEmissiveCandidateBytes = emissive_bytes;
        stats->lastCopiedEstimatedBytes = estimated_bytes;
        stats->totalCopiedEstimatedBytes =
            runtime_native_3d_prepare_u64_add(stats->totalCopiedEstimatedBytes,
                                              estimated_bytes);
    }
    return ok;
}

bool runtime_native_3d_prepare_bind_scene_for_frame(const RuntimeScene3D* scene,
                                                    bool final_frame_bind) {
    struct timespec bind_started_at = {0};
    const bool enabled =
        gRuntimeNative3DPreparedSceneDataflowStats.dataflowStatsEnabled;
    bool ok = false;
    (void)clock_gettime(CLOCK_MONOTONIC, &bind_started_at);
    ok = RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(scene);
    if (enabled) {
        RuntimeNative3DPreparedSceneCacheStats* stats =
            &gRuntimeNative3DPreparedSceneDataflowStats;
        const double bind_ms =
            runtime_native_3d_prepare_elapsed_ms_since(&bind_started_at);
        if (final_frame_bind) {
            stats->finalFrameBindCalls += 1u;
            stats->finalFrameBindMsTotal += bind_ms;
            stats->lastFinalFrameBindMs = bind_ms;
        } else {
            stats->bindAfterCopyCalls += 1u;
            stats->bindAfterCopyMsTotal += bind_ms;
            stats->lastBindAfterCopyMs = bind_ms;
        }
    }
    return ok;
}

static void runtime_native_3d_prepared_scene_cache_ensure_initialized(void) {
    if (gRuntimeNative3DPreparedSceneCacheInitialized) return;
    RuntimeScene3D_Init(&gRuntimeNative3DPreparedSceneCache);
    gRuntimeNative3DPreparedSceneCacheInitialized = true;
}

void RuntimeNative3DPreparedSceneMarkDirty(const char* reason) {
    (void)reason;
    runtime_native_3d_prepared_scene_cache_ensure_initialized();
    gRuntimeNative3DPreparedSceneDirtyGeneration += 1u;
    if (gRuntimeNative3DPreparedSceneCacheValid) {
        gRuntimeNative3DPreparedSceneCacheInvalidations += 1u;
    }
    gRuntimeNative3DPreparedSceneCacheValid = false;
    gRuntimeNative3DPreparedSceneCachedGeneration = 0u;
    RuntimeScene3D_Free(&gRuntimeNative3DPreparedSceneCache);
    RuntimeScene3D_Init(&gRuntimeNative3DPreparedSceneCache);
}

void RuntimeNative3DPreparedSceneCacheStatsSnapshot(
    RuntimeNative3DPreparedSceneCacheStats* out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(*out_stats));
    runtime_native_3d_prepared_scene_cache_ensure_initialized();
    out_stats->generation = gRuntimeNative3DPreparedSceneDirtyGeneration;
    out_stats->cachedGeneration = gRuntimeNative3DPreparedSceneCachedGeneration;
    out_stats->valid = gRuntimeNative3DPreparedSceneCacheValid;
    out_stats->hits = gRuntimeNative3DPreparedSceneCacheHits;
    out_stats->misses = gRuntimeNative3DPreparedSceneCacheMisses;
    out_stats->stores = gRuntimeNative3DPreparedSceneCacheStores;
    out_stats->invalidations = gRuntimeNative3DPreparedSceneCacheInvalidations;
    out_stats->staticGeometryReuseEnabled =
        gRuntimeNative3DPreparedSceneCacheStaticGeometry;
    out_stats->timeIndependentHits =
        gRuntimeNative3DPreparedSceneCacheTimeIndependentHits;
    out_stats->cachedNormalizedT = gRuntimeNative3DPreparedSceneCacheNormalizedT;
    out_stats->lastRequestedNormalizedT =
        gRuntimeNative3DPreparedSceneCacheLastRequestedT;
    if (gRuntimeNative3DPreparedSceneCacheValid) {
        out_stats->cachedPrimitiveCount =
            gRuntimeNative3DPreparedSceneCache.primitiveCount;
        out_stats->cachedTriangleCount =
            gRuntimeNative3DPreparedSceneCache.triangleMesh.triangleCount;
        out_stats->cachedBVHNodeCount =
            RuntimeTriangleMesh3D_BVHNodeCount(
                &gRuntimeNative3DPreparedSceneCache.triangleMesh);
        out_stats->cachedBVHLeafCount =
            RuntimeTriangleMesh3D_BVHLeafCount(
                &gRuntimeNative3DPreparedSceneCache.triangleMesh);
    }
    out_stats->dataflowStatsEnabled =
        gRuntimeNative3DPreparedSceneDataflowStats.dataflowStatsEnabled;
    out_stats->lastPrepareValid =
        gRuntimeNative3DPreparedSceneDataflowStats.lastPrepareValid;
    out_stats->lastPrepareCacheHit =
        gRuntimeNative3DPreparedSceneDataflowStats.lastPrepareCacheHit;
    out_stats->lastPrepareCacheMiss =
        gRuntimeNative3DPreparedSceneDataflowStats.lastPrepareCacheMiss;
    out_stats->lastPrepareCopiedScene =
        gRuntimeNative3DPreparedSceneDataflowStats.lastPrepareCopiedScene;
    out_stats->prepareCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.prepareCalls;
    out_stats->cacheHitPrepareCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.cacheHitPrepareCalls;
    out_stats->cacheMissPrepareCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.cacheMissPrepareCalls;
    out_stats->copyCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.copyCalls;
    out_stats->cacheHitCopyCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.cacheHitCopyCalls;
    out_stats->cacheMissCopyCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.cacheMissCopyCalls;
    out_stats->bindAfterCopyCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.bindAfterCopyCalls;
    out_stats->finalFrameBindCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.finalFrameBindCalls;
    out_stats->frameBVHEnsureCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.frameBVHEnsureCalls;
    out_stats->frameBVHBuildCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.frameBVHBuildCalls;
    out_stats->frameBVHAlreadyReadyCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.frameBVHAlreadyReadyCalls;
    out_stats->frameBVHSkipForTLASCalls =
        gRuntimeNative3DPreparedSceneDataflowStats.frameBVHSkipForTLASCalls;
    out_stats->frameBVHTLASReadinessChecks =
        gRuntimeNative3DPreparedSceneDataflowStats.frameBVHTLASReadinessChecks;
    out_stats->sceneBuildMsTotal =
        gRuntimeNative3DPreparedSceneDataflowStats.sceneBuildMsTotal;
    out_stats->cacheStoreMsTotal =
        gRuntimeNative3DPreparedSceneDataflowStats.cacheStoreMsTotal;
    out_stats->copyMsTotal =
        gRuntimeNative3DPreparedSceneDataflowStats.copyMsTotal;
    out_stats->cacheHitCopyMsTotal =
        gRuntimeNative3DPreparedSceneDataflowStats.cacheHitCopyMsTotal;
    out_stats->cacheMissCopyMsTotal =
        gRuntimeNative3DPreparedSceneDataflowStats.cacheMissCopyMsTotal;
    out_stats->bindAfterCopyMsTotal =
        gRuntimeNative3DPreparedSceneDataflowStats.bindAfterCopyMsTotal;
    out_stats->finalFrameBindMsTotal =
        gRuntimeNative3DPreparedSceneDataflowStats.finalFrameBindMsTotal;
    out_stats->frameBVHEnsureMsTotal =
        gRuntimeNative3DPreparedSceneDataflowStats.frameBVHEnsureMsTotal;
    out_stats->frameBVHTLASReadinessBindMsTotal =
        gRuntimeNative3DPreparedSceneDataflowStats.frameBVHTLASReadinessBindMsTotal;
    out_stats->lastSceneBuildMs =
        gRuntimeNative3DPreparedSceneDataflowStats.lastSceneBuildMs;
    out_stats->lastCacheStoreMs =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCacheStoreMs;
    out_stats->lastCopyMs =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopyMs;
    out_stats->lastBindAfterCopyMs =
        gRuntimeNative3DPreparedSceneDataflowStats.lastBindAfterCopyMs;
    out_stats->lastFinalFrameBindMs =
        gRuntimeNative3DPreparedSceneDataflowStats.lastFinalFrameBindMs;
    out_stats->lastFrameBVHEnsureMs =
        gRuntimeNative3DPreparedSceneDataflowStats.lastFrameBVHEnsureMs;
    out_stats->lastFrameBVHTLASReadinessBindMs =
        gRuntimeNative3DPreparedSceneDataflowStats.lastFrameBVHTLASReadinessBindMs;
    out_stats->flattenedBVHSkipOnTLASEnabled =
        gRuntimeNative3DPreparedSceneDataflowStats.flattenedBVHSkipOnTLASEnabled;
    out_stats->flattenedBVHSkipOnTLASDefaultEnabled =
        gRuntimeNative3DPreparedSceneDataflowStats.flattenedBVHSkipOnTLASDefaultEnabled;
    out_stats->flattenedBVHSkipOnTLASForceEnabled =
        gRuntimeNative3DPreparedSceneDataflowStats.flattenedBVHSkipOnTLASForceEnabled;
    out_stats->flattenedBVHSkipOnTLASDefaultDisabled =
        gRuntimeNative3DPreparedSceneDataflowStats.flattenedBVHSkipOnTLASDefaultDisabled;
    out_stats->lastFrameBVHRequired =
        gRuntimeNative3DPreparedSceneDataflowStats.lastFrameBVHRequired;
    out_stats->lastFrameBVHSkippedForTLAS =
        gRuntimeNative3DPreparedSceneDataflowStats.lastFrameBVHSkippedForTLAS;
    out_stats->lastFrameBVHReady =
        gRuntimeNative3DPreparedSceneDataflowStats.lastFrameBVHReady;
    out_stats->lastTLASReadyForFrameBVHSkip =
        gRuntimeNative3DPreparedSceneDataflowStats.lastTLASReadyForFrameBVHSkip;
    out_stats->lastFrameBVHSkipDecision =
        gRuntimeNative3DPreparedSceneDataflowStats.lastFrameBVHSkipDecision;
    out_stats->lastCopiedPrimitiveCount =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedPrimitiveCount;
    out_stats->lastCopiedTriangleCount =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedTriangleCount;
    out_stats->lastCopiedBVHNodeCount =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedBVHNodeCount;
    out_stats->lastCopiedLightCount =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedLightCount;
    out_stats->lastCopiedEmissiveCandidateCount =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedEmissiveCandidateCount;
    out_stats->lastFrameBVHTriangleCount =
        gRuntimeNative3DPreparedSceneDataflowStats.lastFrameBVHTriangleCount;
    out_stats->lastFrameBVHNodeCount =
        gRuntimeNative3DPreparedSceneDataflowStats.lastFrameBVHNodeCount;
    out_stats->lastFrameBVHTotalBytes =
        gRuntimeNative3DPreparedSceneDataflowStats.lastFrameBVHTotalBytes;
    out_stats->lastCopiedPrimitiveBytes =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedPrimitiveBytes;
    out_stats->lastCopiedTriangleBytes =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedTriangleBytes;
    out_stats->lastCopiedBVHBytes =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedBVHBytes;
    out_stats->lastCopiedLightBytes =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedLightBytes;
    out_stats->lastCopiedEmissiveCandidateBytes =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedEmissiveCandidateBytes;
    out_stats->lastCopiedEstimatedBytes =
        gRuntimeNative3DPreparedSceneDataflowStats.lastCopiedEstimatedBytes;
    out_stats->totalCopiedEstimatedBytes =
        gRuntimeNative3DPreparedSceneDataflowStats.totalCopiedEstimatedBytes;
}

void RuntimeNative3DPreparedSceneCacheResetForTests(void) {
    runtime_native_3d_prepared_scene_cache_ensure_initialized();
    RuntimeScene3D_Free(&gRuntimeNative3DPreparedSceneCache);
    RuntimeScene3D_Init(&gRuntimeNative3DPreparedSceneCache);
    gRuntimeNative3DPreparedSceneCacheValid = false;
    gRuntimeNative3DPreparedSceneCacheStaticGeometry = true;
    gRuntimeNative3DPreparedSceneCacheNormalizedT = 0.0;
    gRuntimeNative3DPreparedSceneCacheLastRequestedT = 0.0;
    gRuntimeNative3DPreparedSceneDirtyGeneration = 1u;
    gRuntimeNative3DPreparedSceneCachedGeneration = 0u;
    gRuntimeNative3DPreparedSceneCacheHits = 0u;
    gRuntimeNative3DPreparedSceneCacheMisses = 0u;
    gRuntimeNative3DPreparedSceneCacheStores = 0u;
    gRuntimeNative3DPreparedSceneCacheInvalidations = 0u;
    gRuntimeNative3DPreparedSceneCacheTimeIndependentHits = 0u;
    memset(&gRuntimeNative3DPreparedSceneDataflowStats,
           0,
           sizeof(gRuntimeNative3DPreparedSceneDataflowStats));
}

bool runtime_native_3d_prepared_scene_build_or_copy_for_frame(
    RuntimeScene3D* scene,
    double normalized_t) {
    const bool dataflow_enabled = RuntimeFrameDataflowLedger3D_IsEnabled();

    if (!scene) return false;
    runtime_native_3d_prepare_dataflow_begin(dataflow_enabled);
    runtime_native_3d_prepared_scene_cache_ensure_initialized();
    gRuntimeNative3DPreparedSceneCacheLastRequestedT = normalized_t;
    if (gRuntimeNative3DPreparedSceneCacheValid &&
        gRuntimeNative3DPreparedSceneCachedGeneration ==
            gRuntimeNative3DPreparedSceneDirtyGeneration &&
        (gRuntimeNative3DPreparedSceneCacheStaticGeometry ||
         fabs(gRuntimeNative3DPreparedSceneCacheNormalizedT - normalized_t) <= 1e-12)) {
        gRuntimeNative3DPreparedSceneCacheHits += 1u;
        if (dataflow_enabled) {
            gRuntimeNative3DPreparedSceneDataflowStats.cacheHitPrepareCalls += 1u;
            gRuntimeNative3DPreparedSceneDataflowStats.lastPrepareCacheHit = true;
        }
        if (fabs(gRuntimeNative3DPreparedSceneCacheNormalizedT - normalized_t) > 1e-12) {
            gRuntimeNative3DPreparedSceneCacheTimeIndependentHits += 1u;
        }
        if (!runtime_native_3d_prepare_copy_scene_for_frame(
                scene,
                &gRuntimeNative3DPreparedSceneCache,
                true)) {
            return false;
        }
        runtime_native_3d_prepare_bind_scene_for_frame(scene, false);
        RuntimeScene3D_RefreshMaterialFlags(scene);
        return true;
    }

    RuntimeScene3D built_scene = {0};
    struct timespec stage_started_at = {0};
    RuntimeScene3D_Init(&built_scene);
    gRuntimeNative3DPreparedSceneCacheMisses += 1u;
    if (dataflow_enabled) {
        gRuntimeNative3DPreparedSceneDataflowStats.cacheMissPrepareCalls += 1u;
        gRuntimeNative3DPreparedSceneDataflowStats.lastPrepareCacheMiss = true;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_started_at);
    if (!RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&built_scene, normalized_t)) {
        RuntimeScene3D_Free(&built_scene);
        return false;
    }
    if (dataflow_enabled) {
        const double build_ms =
            runtime_native_3d_prepare_elapsed_ms_since(&stage_started_at);
        gRuntimeNative3DPreparedSceneDataflowStats.sceneBuildMsTotal += build_ms;
        gRuntimeNative3DPreparedSceneDataflowStats.lastSceneBuildMs = build_ms;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_started_at);
    RuntimeScene3D_Free(&gRuntimeNative3DPreparedSceneCache);
    gRuntimeNative3DPreparedSceneCache = built_scene;
    memset(&built_scene, 0, sizeof(built_scene));
    gRuntimeNative3DPreparedSceneCacheValid = true;
    gRuntimeNative3DPreparedSceneCacheStaticGeometry = true;
    gRuntimeNative3DPreparedSceneCacheNormalizedT = normalized_t;
    gRuntimeNative3DPreparedSceneCachedGeneration =
        gRuntimeNative3DPreparedSceneDirtyGeneration;
    gRuntimeNative3DPreparedSceneCacheStores += 1u;
    if (dataflow_enabled) {
        const double store_ms =
            runtime_native_3d_prepare_elapsed_ms_since(&stage_started_at);
        gRuntimeNative3DPreparedSceneDataflowStats.cacheStoreMsTotal += store_ms;
        gRuntimeNative3DPreparedSceneDataflowStats.lastCacheStoreMs = store_ms;
    }
    if (!runtime_native_3d_prepare_copy_scene_for_frame(
            scene,
            &gRuntimeNative3DPreparedSceneCache,
            false)) {
        return false;
    }
    runtime_native_3d_prepare_bind_scene_for_frame(scene, false);
    RuntimeScene3D_RefreshMaterialFlags(scene);
    return true;
}
