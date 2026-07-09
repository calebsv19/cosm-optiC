#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_REQUEST_SNAPSHOT_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_REQUEST_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include "render/runtime_ray_3d.h"

#define RUNTIME_NATIVE_3D_RENDER_REQUEST_SNAPSHOT_PATH_MAX 512

typedef struct RuntimeNative3DRenderRequestSnapshot {
    bool valid;
    bool generationBound;
    uint64_t generation;
    int outputWidth;
    int outputHeight;
    int renderWidth;
    int renderHeight;
    int hostWidth;
    int hostHeight;
    int frameIndex;
    int frameCount;
    int temporalFrames;
    int tileSize;
    RayTracing3DIntegratorId integratorId;
    bool samplingBound;
    RuntimeNative3DSamplingContext sampling;
    bool resourceBudgetBound;
    RuntimeNative3DResourceBudget resourceBudget;
    bool preparedFrameBound;
    bool preparedFrameValid;
    int preparedFrameWidth;
    int preparedFrameHeight;
    uint64_t preparedPrimitiveCount;
    uint64_t preparedTriangleCount;
    bool materialSnapshotBound;
    uint64_t materialCount;
    uint64_t materialObjectBindingCount;
    bool lightSnapshotBound;
    uint64_t enabledLightCount;
    uint64_t materialEmitterLightCount;
    bool sceneAccelerationBound;
    RuntimeRay3DTraceRoute traceRoute;
    uint64_t tlasInstanceCount;
    uint64_t tlasNodeCount;
    bool traceContextCallbackBound;
    bool volumeEnabled;
    bool volumeAttached;
    bool volumeFrameSelectionDynamic;
    bool waterSurfaceSourceFound;
    bool waterSurfaceLoaded;
    bool waterSurfaceMeshAttached;
    bool waterSurfaceFrameSelectionDynamic;
    uint64_t waterSurfaceSampleCount;
    int waterSurfaceTriangleCount;
    bool frameDataflowLedgerEnabled;
    bool outputRootBound;
    bool summaryDestinationBound;
    bool progressDestinationBound;
    char outputRoot[RUNTIME_NATIVE_3D_RENDER_REQUEST_SNAPSHOT_PATH_MAX];
    char summaryPath[RUNTIME_NATIVE_3D_RENDER_REQUEST_SNAPSHOT_PATH_MAX];
    char progressPath[RUNTIME_NATIVE_3D_RENDER_REQUEST_SNAPSHOT_PATH_MAX];
    bool cancelTokenBound;
    RuntimeNative3DTileSchedulerCancelToken cancelToken;
    uint64_t cancelGeneration;
} RuntimeNative3DRenderRequestSnapshot;

typedef struct RuntimeNative3DRenderRequestSnapshotDesc {
    bool generationBound;
    uint64_t generation;
    int outputWidth;
    int outputHeight;
    int renderWidth;
    int renderHeight;
    int hostWidth;
    int hostHeight;
    int frameIndex;
    int frameCount;
    int temporalFrames;
    int tileSize;
    RayTracing3DIntegratorId integratorId;
    const RuntimeNative3DSamplingContext* sampling;
    const RuntimeNative3DResourceBudget* resourceBudget;
    const RuntimeNative3DPreparedFrame* preparedFrame;
    bool preparedFrameBound;
    bool preparedFrameValid;
    int preparedFrameWidth;
    int preparedFrameHeight;
    uint64_t preparedPrimitiveCount;
    uint64_t preparedTriangleCount;
    bool materialSnapshotBound;
    uint64_t materialCount;
    uint64_t materialObjectBindingCount;
    bool lightSnapshotBound;
    uint64_t enabledLightCount;
    uint64_t materialEmitterLightCount;
    bool sceneAccelerationBound;
    RuntimeRay3DTraceRoute traceRoute;
    uint64_t tlasInstanceCount;
    uint64_t tlasNodeCount;
    bool traceContextCallbackBound;
    bool volumeEnabled;
    bool volumeAttached;
    bool volumeFrameSelectionDynamic;
    bool waterSurfaceSourceFound;
    bool waterSurfaceLoaded;
    bool waterSurfaceMeshAttached;
    bool waterSurfaceFrameSelectionDynamic;
    uint64_t waterSurfaceSampleCount;
    int waterSurfaceTriangleCount;
    bool frameDataflowLedgerEnabled;
    const char* outputRoot;
    const char* summaryPath;
    const char* progressPath;
    const RuntimeNative3DTileSchedulerCancelToken* cancelToken;
} RuntimeNative3DRenderRequestSnapshotDesc;

void RuntimeNative3DRenderRequestSnapshot_Init(
    RuntimeNative3DRenderRequestSnapshot* snapshot);
bool RuntimeNative3DRenderRequestSnapshot_Build(
    RuntimeNative3DRenderRequestSnapshot* out_snapshot,
    const RuntimeNative3DRenderRequestSnapshotDesc* desc);

#endif
