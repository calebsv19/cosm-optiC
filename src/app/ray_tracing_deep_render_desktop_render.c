#include "app/ray_tracing_deep_render_desktop_render_internal.h"

#include <SDL2/SDL.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "engine/Render/render_pipeline.h"
#include "render/integrators/integrator_common.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_native_3d_preview_reconstruction.h"
#include "render/runtime_native_3d_resolution.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_accel_3d.h"

static uint32_t s_deep_render_desktop_sample_sequence = 1u;

static void deep_render_desktop_set_reason(const char** out_reason,
                                           const char* reason) {
    if (out_reason) *out_reason = reason;
}

static RuntimeNative3DSamplingContext deep_render_desktop_next_sampling(void) {
    RuntimeNative3DSamplingContext sampling = {0};
    sampling.sampleSequence = s_deep_render_desktop_sample_sequence++;
    if (sampling.sampleSequence == 0u) {
        sampling.sampleSequence = s_deep_render_desktop_sample_sequence++;
    }
    return sampling;
}

static int deep_render_desktop_temporal_frames(
    RayTracing3DIntegratorId integrator_id) {
    int frames = animSettings.temporalFrames3D;
    if (integrator_id == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT) return 1;
    if (frames < RUNTIME_3D_TEMPORAL_FRAMES_MIN) {
        frames = RUNTIME_3D_TEMPORAL_FRAMES_MIN;
    }
    if (frames > RUNTIME_3D_TEMPORAL_FRAMES_MAX) {
        frames = RUNTIME_3D_TEMPORAL_FRAMES_MAX;
    }
    return frames;
}

static bool deep_render_desktop_ensure_bytes(uint8_t** pixels,
                                             size_t* capacity,
                                             size_t required) {
    uint8_t* resized = NULL;
    if (!pixels || !capacity || required == 0u) return false;
    if (*pixels && *capacity >= required) return true;
    resized = (uint8_t*)realloc(*pixels, required);
    if (!resized) return false;
    *pixels = resized;
    *capacity = required;
    return true;
}

static bool deep_render_desktop_resolve_dirty_union(
    const RuntimeNative3DTileSchedulerProgress* progress,
    const RayTracingDeepRenderDesktopRenderUnit* unit,
    SDL_Rect* out_rect) {
    SDL_Rect result = {0};
    bool seeded = false;
    if (!progress || !progress->dirtyTiles || !unit || !out_rect) return false;
    for (size_t i = 0u; i < progress->dirtyTileCount; ++i) {
        const IntegratorTile* tile = &progress->dirtyTiles[i];
        SDL_Rect rect = {0};
        if (!RuntimeNative3DPreviewResolveDirtyHostRect(tile->originX,
                                                        tile->originY,
                                                        tile->width,
                                                        tile->height,
                                                        unit->renderWidth,
                                                        unit->renderHeight,
                                                        unit->hostWidth,
                                                        unit->hostHeight,
                                                        &rect)) {
            continue;
        }
        if (!seeded) {
            result = rect;
            seeded = true;
        } else {
            const int min_x = rect.x < result.x ? rect.x : result.x;
            const int min_y = rect.y < result.y ? rect.y : result.y;
            const int max_x = rect.x + rect.w > result.x + result.w
                                  ? rect.x + rect.w
                                  : result.x + result.w;
            const int max_y = rect.y + rect.h > result.y + result.h
                                  ? rect.y + rect.h
                                  : result.y + result.h;
            result = (SDL_Rect){min_x, min_y, max_x - min_x, max_y - min_y};
        }
    }
    if (!seeded) return false;
    *out_rect = result;
    return true;
}

static bool deep_render_desktop_publish_progress(
    const RuntimeNative3DTileSchedulerProgress* progress,
    void* user_data) {
    RayTracingDeepRenderDesktopRenderUnit* unit =
        (RayTracingDeepRenderDesktopRenderUnit*)user_data;
    SDL_Rect dirty = {0};
    RuntimeNative3DAsyncRenderProgressRect rect = {0};
    if (!unit || !unit->progress || !unit->hostPixels ||
        !deep_render_desktop_resolve_dirty_union(progress, unit, &dirty)) {
        return unit != NULL;
    }
    if (!RuntimeNative3DPreviewReconstructABGRRectWithMode(unit->renderPixels,
                                                           unit->renderWidth,
                                                           unit->renderHeight,
                                                           unit->hostPixels,
                                                           unit->hostWidth,
                                                           unit->hostHeight,
                                                           &dirty,
                                                           unit->upscaleMode)) {
        return false;
    }
    rect.x = dirty.x;
    rect.y = dirty.y;
    rect.width = dirty.w;
    rect.height = dirty.h;
    return RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(
        unit->progress,
        unit->generation,
        unit->hostPixels,
        unit->hostWidth,
        unit->hostHeight,
        rect);
}

static void deep_render_desktop_ignore_temporal_progress(int started_subpasses,
                                                         int completed_subpasses,
                                                         int total_subpasses,
                                                         void* user_data) {
    (void)started_subpasses;
    (void)completed_subpasses;
    (void)total_subpasses;
    (void)user_data;
}

static bool deep_render_desktop_publish_full_frame(
    RayTracingDeepRenderDesktopRenderUnit* unit) {
    RuntimeNative3DAsyncRenderProgressRect rect = {0};
    if (!unit || !unit->progress || !unit->hostPixels) return false;
    rect.width = unit->hostWidth;
    rect.height = unit->hostHeight;
    return RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(
        unit->progress,
        unit->generation,
        unit->hostPixels,
        unit->hostWidth,
        unit->hostHeight,
        rect);
}

static bool deep_render_desktop_worker(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    void* user_data,
    RuntimeNative3DAsyncRenderJobResult* out_result) {
    RayTracingDeepRenderDesktopRenderUnit* unit =
        (RayTracingDeepRenderDesktopRenderUnit*)user_data;
    RayTracingDeepRenderFrameRequest* request = NULL;
    RuntimeNative3DRenderStats stats = {0};
    RuntimeNative3DTileSchedulerControl control = {.cancelToken = cancel_token};
    bool ok = false;
    bool canceled = false;
    if (!unit || !unit->session || !snapshot || !cancel_token || !out_result) {
        return false;
    }
    if (!unit->session->frameRequestOwned) return false;
    request = &unit->session->frameRequest;
    if (!request->valid || request->generation != unit->generation) return false;

    RuntimeNative3DFillPixelBufferBackground(unit->renderPixels,
                                             unit->renderWidth,
                                             unit->renderHeight,
                                             &request->preparedFrame.scene,
                                             &request->preparedFrame.projector);
    ok = RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressBudgetAndControl(
        unit->renderPixels,
        unit->integratorId,
        &request->preparedFrame,
        unit->temporalFrames,
        deep_render_desktop_ignore_temporal_progress,
        NULL,
        deep_render_desktop_publish_progress,
        unit,
        NULL,
        &control,
        &stats);
    canceled = RuntimeNative3DTileSchedulerCancelToken_IsRequested(cancel_token);
    if (ok && !canceled) {
        ok = RuntimeNative3DPreviewReconstructABGRWithMode(unit->renderPixels,
                                                           unit->renderWidth,
                                                           unit->renderHeight,
                                                           unit->hostPixels,
                                                           unit->hostWidth,
                                                           unit->hostHeight,
                                                           unit->upscaleMode) &&
             deep_render_desktop_publish_full_frame(unit);
    }
    out_result->valid = true;
    out_result->succeeded = ok && !canceled;
    out_result->canceled = canceled;
    out_result->cancelRequested = canceled;
    out_result->generation = unit->generation;
    out_result->completionValue = stats.temporalCommittedSubpasses;
    out_result->snapshot = *snapshot;
    return ok && !canceled;
}

void RayTracingDeepRenderDesktopRenderUnit_Init(
    RayTracingDeepRenderDesktopRenderUnit* unit) {
    if (!unit) return;
    memset(unit, 0, sizeof(*unit));
}

void RayTracingDeepRenderDesktopRenderUnit_Destroy(
    RayTracingDeepRenderDesktopRenderUnit* unit) {
    if (!unit) return;
    RuntimeNative3DAsyncRenderProgressBuffer_Destroy(unit->progress);
    free(unit->renderPixels);
    free(unit->hostPixels);
    RayTracingDeepRenderDesktopRenderUnit_Init(unit);
}

RayTracingDeepRenderDesktopStartStatus
RayTracingDeepRenderDesktopRenderUnit_Start(
    RayTracingDeepRenderDesktopRenderUnit* unit,
    RayTracingDeepRenderSession* session,
    RuntimeNative3DAsyncRenderJob* job,
    const RayTracingDeepRenderDesktopStartDesc* desc,
    const char** out_reason) {
    RayTracingRuntimeRoute route;
    RenderContext* render_context = getRenderContext();
    RuntimeNative3DPreparedFrame prepared = {0};
    RuntimeNative3DRenderRequestSnapshot snapshot = {0};
    RuntimeNative3DRenderRequestSnapshot dispatch = {0};
    RuntimeNative3DRenderRequestSnapshotDesc snapshot_desc = {0};
    RuntimeNative3DSamplingContext sampling;
    RuntimeSceneAcceleration3DDiagnostics accel =
        RuntimeSceneAcceleration3DDiagnostics_Disabled();
    RayTracingDeepRenderFrameRequest request;
    RayTracingDeepRenderFrameRequestDesc request_desc = {0};
    RayTracingDeepRenderFrameRequestStatus request_status;
    RuntimeNative3DAsyncRenderAssessment assessment;
    RuntimeNative3DAsyncRenderJobStartDesc start = {0};
    RuntimeNative3DTileSchedulerCancelToken probe_token = {0};
    atomic_bool cancel_probe = ATOMIC_VAR_INIT(false);
    size_t render_bytes = 0u;
    size_t host_bytes = 0u;

    deep_render_desktop_set_reason(out_reason, "invalid async desktop request");
    if (!unit || !session || !job || !desc || desc->generation == 0u ||
        !desc->outputRoot || !desc->frameDirectory || !desc->finalFramePath) {
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED;
    }
    route = RayTracingModeBackend_ResolveRoute();
    if (!RayTracingModeBackend_IsNative3D(&route) || !route.useTiles) {
        deep_render_desktop_set_reason(out_reason, "route is not native 3D tiled");
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_UNSUPPORTED;
    }
    if (animSettings.volumeInteractionEnabled) {
        deep_render_desktop_set_reason(
            out_reason, "dynamic volume or water selection requires sync fallback");
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_UNSUPPORTED;
    }

    unit->hostWidth = desc->outputWidth;
    unit->hostHeight = desc->outputHeight;
    if (!RuntimeNative3DResolveHostDimensions(
            desc->outputWidth,
            desc->outputHeight,
            render_context ? render_context->width : desc->outputWidth,
            render_context ? render_context->height : desc->outputHeight,
            RuntimeNative3DClampRenderScale(animSettings.renderScale3D),
            &unit->hostWidth,
            &unit->hostHeight) ||
        !RuntimeNative3DResolveScaledDimensions(
            unit->hostWidth,
            unit->hostHeight,
            RuntimeNative3DClampRenderScale(animSettings.renderScale3D),
            &unit->renderWidth,
            &unit->renderHeight)) {
        deep_render_desktop_set_reason(out_reason, "invalid async render dimensions");
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED;
    }
    unit->generation = desc->generation;
    unit->integratorId = route.integratorMode3D;
    unit->temporalFrames = deep_render_desktop_temporal_frames(unit->integratorId);
    unit->tileSize = RuntimeNative3DTileSchedulerResolveTileSizeForScale(
        animSettings.tileSize, animSettings.renderScale3D);
    unit->upscaleMode = (Runtime3DUpscaleMode)animSettings.upscaleMode3D;
    render_bytes = (size_t)unit->renderWidth * (size_t)unit->renderHeight *
                   (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    host_bytes = (size_t)unit->hostWidth * (size_t)unit->hostHeight *
                 (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    if (!deep_render_desktop_ensure_bytes(
            &unit->renderPixels, &unit->renderCapacity, render_bytes) ||
        !deep_render_desktop_ensure_bytes(
            &unit->hostPixels, &unit->hostCapacity, host_bytes)) {
        deep_render_desktop_set_reason(out_reason, "async render buffer allocation failed");
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED;
    }
    if (!unit->progress) {
        unit->progress = RuntimeNative3DAsyncRenderProgressBuffer_Create();
        if (!unit->progress) {
            deep_render_desktop_set_reason(out_reason, "progress buffer allocation failed");
            return RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED;
        }
    }
    RuntimeNative3DAsyncRenderProgressBuffer_Reset(unit->progress);
    RuntimeNative3DFillPixelBufferEnvironment(
        unit->renderPixels,
        (size_t)unit->renderWidth * (size_t)unit->renderHeight);
    RuntimeNative3DFillPixelBufferEnvironment(
        unit->hostPixels,
        (size_t)unit->hostWidth * (size_t)unit->hostHeight);

    sampling = deep_render_desktop_next_sampling();
    if (!RuntimeNative3DPrepareFrameWithSamplingAtFrameIndex(
            &prepared,
            unit->renderWidth,
            unit->renderHeight,
            desc->normalizedT,
            desc->absoluteFrameIndex,
            desc->lightX,
            desc->lightY,
            &sampling)) {
        deep_render_desktop_set_reason(out_reason, "native 3D frame preparation failed");
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED;
    }
    (void)RuntimeNative3DPrepareFrameTileOccupancy(&prepared, unit->tileSize);
    RuntimeSceneAcceleration3D_AppendTLASDiagnostics(&accel);
    probe_token.cancelRequested = &cancel_probe;
    probe_token.generation = desc->generation;
    snapshot_desc.generationBound = true;
    snapshot_desc.generation = desc->generation;
    snapshot_desc.outputWidth = desc->outputWidth;
    snapshot_desc.outputHeight = desc->outputHeight;
    snapshot_desc.renderWidth = unit->renderWidth;
    snapshot_desc.renderHeight = unit->renderHeight;
    snapshot_desc.hostWidth = unit->hostWidth;
    snapshot_desc.hostHeight = unit->hostHeight;
    snapshot_desc.frameIndex = desc->absoluteFrameIndex;
    snapshot_desc.frameCount = desc->frameCount;
    snapshot_desc.temporalFrames = unit->temporalFrames;
    snapshot_desc.tileSize = unit->tileSize;
    snapshot_desc.integratorId = unit->integratorId;
    snapshot_desc.sampling = &sampling;
    snapshot_desc.preparedFrame = &prepared;
    snapshot_desc.sceneAccelerationBound = accel.tlasNodeCount > 0u;
    snapshot_desc.traceRoute = RuntimeRay3D_CurrentTraceRoute();
    snapshot_desc.tlasInstanceCount = accel.tlasInstanceCount;
    snapshot_desc.tlasNodeCount = accel.tlasNodeCount;
    snapshot_desc.traceContextCallbackBound = true;
    snapshot_desc.volumeEnabled = false;
    snapshot_desc.volumeAttached = false;
    snapshot_desc.volumeFrameSelectionDynamic = false;
    snapshot_desc.waterSurfaceFrameSelectionDynamic = false;
    snapshot_desc.outputRoot = desc->outputRoot;
    snapshot_desc.cancelToken = &probe_token;
    if (!RuntimeNative3DRenderRequestSnapshot_Build(&snapshot, &snapshot_desc)) {
        RuntimeNative3DPreparedFrame_Free(&prepared);
        deep_render_desktop_set_reason(out_reason, "render snapshot build failed");
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED;
    }

    RayTracingDeepRenderFrameRequest_Init(&request);
    request_desc.generation = desc->generation;
    request_desc.localFrameIndex = desc->localFrameIndex;
    request_desc.absoluteFrameIndex = desc->absoluteFrameIndex;
    request_desc.frameCount = desc->frameCount;
    request_desc.frameDurationSeconds = desc->frameDurationSeconds;
    request_desc.animationTimeSeconds = desc->animationTimeSeconds;
    request_desc.normalizedT = desc->normalizedT;
    request_desc.camera = &prepared.scene.camera;
    request_desc.light = &prepared.scene.light;
    request_desc.renderSnapshot = &snapshot;
    request_desc.outputRoot = desc->outputRoot;
    request_desc.frameDirectory = desc->frameDirectory;
    request_desc.finalFramePath = desc->finalFramePath;
    if (!RayTracingDeepRenderFrameRequest_Build(
            &request, &request_desc, &prepared, &request_status)) {
        RuntimeNative3DPreparedFrame_Free(&prepared);
        deep_render_desktop_set_reason(
            out_reason, RayTracingDeepRenderFrameRequestStatus_Name(request_status));
        return request_status ==
                       RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_DYNAMIC_VOLUME_UNOWNED ||
                   request_status ==
                       RAY_TRACING_DEEP_RENDER_FRAME_REQUEST_DYNAMIC_WATER_UNOWNED
                   ? RAY_TRACING_DEEP_RENDER_DESKTOP_START_UNSUPPORTED
                   : RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED;
    }
    if (!RayTracingDeepRenderFrameRequest_BuildDispatchSnapshot(
            &request, &probe_token, &dispatch, &request_status)) {
        RayTracingDeepRenderFrameRequest_Destroy(&request);
        deep_render_desktop_set_reason(
            out_reason, RayTracingDeepRenderFrameRequestStatus_Name(request_status));
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED;
    }
    assessment = RuntimeNative3DAsyncRender_AssessSnapshot(&dispatch);
    if (!assessment.ready || !assessment.requiresExclusiveRenderContext) {
        RayTracingDeepRenderFrameRequest_Destroy(&request);
        deep_render_desktop_set_reason(out_reason, assessment.reason);
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_UNSUPPORTED;
    }
    if (!RayTracingDeepRenderSession_AdoptFrameRequest(session, &request)) {
        RayTracingDeepRenderFrameRequest_Destroy(&request);
        deep_render_desktop_set_reason(out_reason, "session rejected frame request");
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED;
    }
    unit->session = session;
    start.snapshot = dispatch;
    start.generation = desc->generation;
    start.run_fn = deep_render_desktop_worker;
    start.user_data = unit;
    if (!RuntimeNative3DAsyncRenderJob_Start(job, &start)) {
        (void)RayTracingDeepRenderSession_MarkFailed(
            session, RAY_TRACING_DEEP_RENDER_SESSION_FAILURE_RENDER);
        deep_render_desktop_set_reason(out_reason, "async render job start failed");
        return RAY_TRACING_DEEP_RENDER_DESKTOP_START_FAILED;
    }
    deep_render_desktop_set_reason(out_reason, "ready");
    return RAY_TRACING_DEEP_RENDER_DESKTOP_START_READY;
}
