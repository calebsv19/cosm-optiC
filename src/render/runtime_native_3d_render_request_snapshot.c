#include "render/runtime_native_3d_render_request_snapshot.h"

#include <stdio.h>
#include <string.h>

static bool runtime_native_3d_snapshot_copy_path(char* dst,
                                                 size_t dst_size,
                                                 const char* src) {
    if (!dst || dst_size == 0u) return false;
    dst[0] = '\0';
    if (!src || !src[0]) return false;
    snprintf(dst, dst_size, "%s", src);
    return dst[0] != '\0';
}

void RuntimeNative3DRenderRequestSnapshot_Init(
    RuntimeNative3DRenderRequestSnapshot* snapshot) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->traceRoute = RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH;
}

bool RuntimeNative3DRenderRequestSnapshot_Build(
    RuntimeNative3DRenderRequestSnapshot* out_snapshot,
    const RuntimeNative3DRenderRequestSnapshotDesc* desc) {
    RuntimeNative3DRenderRequestSnapshot snapshot;

    if (!out_snapshot) return false;
    RuntimeNative3DRenderRequestSnapshot_Init(&snapshot);
    if (!desc) {
        *out_snapshot = snapshot;
        return false;
    }

    snapshot.generationBound = desc->generationBound;
    snapshot.generation = desc->generation;
    snapshot.outputWidth = desc->outputWidth;
    snapshot.outputHeight = desc->outputHeight;
    snapshot.renderWidth = desc->renderWidth;
    snapshot.renderHeight = desc->renderHeight;
    snapshot.hostWidth = desc->hostWidth;
    snapshot.hostHeight = desc->hostHeight;
    snapshot.frameIndex = desc->frameIndex;
    snapshot.frameCount = desc->frameCount;
    snapshot.temporalFrames = desc->temporalFrames;
    snapshot.tileSize = desc->tileSize;
    snapshot.integratorId = desc->integratorId;
    if (desc->sampling) {
        snapshot.samplingBound = true;
        snapshot.sampling = *desc->sampling;
    }
    if (desc->resourceBudget) {
        snapshot.resourceBudgetBound = true;
        snapshot.resourceBudget = *desc->resourceBudget;
    }
    snapshot.preparedFrameBound = desc->preparedFrameBound || desc->preparedFrame != NULL;
    if (desc->preparedFrame) {
        snapshot.preparedFrameValid = desc->preparedFrame->valid;
        snapshot.preparedFrameWidth = desc->preparedFrame->width;
        snapshot.preparedFrameHeight = desc->preparedFrame->height;
        snapshot.preparedPrimitiveCount =
            desc->preparedFrame->scene.primitiveCount > 0
                ? (uint64_t)desc->preparedFrame->scene.primitiveCount
                : 0u;
        snapshot.preparedTriangleCount =
            desc->preparedFrame->scene.triangleMesh.triangleCount > 0
                ? (uint64_t)desc->preparedFrame->scene.triangleMesh.triangleCount
                : 0u;
    } else {
        snapshot.preparedFrameValid = desc->preparedFrameValid;
        snapshot.preparedFrameWidth = desc->preparedFrameWidth;
        snapshot.preparedFrameHeight = desc->preparedFrameHeight;
        snapshot.preparedPrimitiveCount = desc->preparedPrimitiveCount;
        snapshot.preparedTriangleCount = desc->preparedTriangleCount;
    }
    snapshot.materialSnapshotBound = desc->materialSnapshotBound;
    snapshot.materialCount = desc->materialCount;
    snapshot.materialObjectBindingCount = desc->materialObjectBindingCount;
    snapshot.lightSnapshotBound = desc->lightSnapshotBound;
    snapshot.enabledLightCount = desc->enabledLightCount;
    snapshot.materialEmitterLightCount = desc->materialEmitterLightCount;
    snapshot.sceneAccelerationBound = desc->sceneAccelerationBound;
    snapshot.traceRoute = desc->traceRoute;
    snapshot.tlasInstanceCount = desc->tlasInstanceCount;
    snapshot.tlasNodeCount = desc->tlasNodeCount;
    snapshot.traceContextCallbackBound = desc->traceContextCallbackBound;
    snapshot.volumeEnabled = desc->volumeEnabled;
    snapshot.volumeAttached = desc->volumeAttached;
    snapshot.volumeFrameSelectionDynamic = desc->volumeFrameSelectionDynamic;
    snapshot.waterSurfaceSourceFound = desc->waterSurfaceSourceFound;
    snapshot.waterSurfaceLoaded = desc->waterSurfaceLoaded;
    snapshot.waterSurfaceMeshAttached = desc->waterSurfaceMeshAttached;
    snapshot.waterSurfaceFrameSelectionDynamic =
        desc->waterSurfaceFrameSelectionDynamic;
    snapshot.waterSurfaceSampleCount = desc->waterSurfaceSampleCount;
    snapshot.waterSurfaceTriangleCount = desc->waterSurfaceTriangleCount;
    snapshot.frameDataflowLedgerEnabled = desc->frameDataflowLedgerEnabled;
    snapshot.outputRootBound = runtime_native_3d_snapshot_copy_path(
        snapshot.outputRoot,
        sizeof(snapshot.outputRoot),
        desc->outputRoot);
    snapshot.summaryDestinationBound = runtime_native_3d_snapshot_copy_path(
        snapshot.summaryPath,
        sizeof(snapshot.summaryPath),
        desc->summaryPath);
    snapshot.progressDestinationBound = runtime_native_3d_snapshot_copy_path(
        snapshot.progressPath,
        sizeof(snapshot.progressPath),
        desc->progressPath);
    if (desc->cancelToken) {
        snapshot.cancelTokenBound = true;
        snapshot.cancelToken = *desc->cancelToken;
        snapshot.cancelGeneration = desc->cancelToken->generation;
    }

    snapshot.valid = snapshot.outputWidth > 0 && snapshot.outputHeight > 0 &&
                     snapshot.renderWidth > 0 && snapshot.renderHeight > 0 &&
                     snapshot.hostWidth > 0 && snapshot.hostHeight > 0 &&
                     snapshot.frameCount >= 0 && snapshot.temporalFrames >= 0;
    *out_snapshot = snapshot;
    return snapshot.valid;
}
