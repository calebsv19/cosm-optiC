#include "app/ray_tracing_desktop_async_bridge.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "engine/Render/render_pipeline.h"
#include "render/integrators/integrator_common.h"
#include "render/pipeline/ray_tracing2_preview_present.h"
#include "render/ray_tracing2.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_native_3d_async_render_bridge.h"
#include "render/runtime_native_3d_async_render_job.h"
#include "render/runtime_native_3d_preview_reconstruction.h"
#include "render/runtime_native_3d_progress_hud.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_resolution.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/timer_hud_api.h"

typedef struct RayTracingDesktopAsyncBridgeState {
    RuntimeNative3DAsyncRenderJob* job;
    RuntimeNative3DAsyncRenderProgressBuffer* progress;
    RuntimeNative3DPreparedFrame preparedFrame;
    TileGrid tileGrid;
    uint8_t* renderPixels;
    uint8_t* hostPixels;
    uint8_t* displayPixels;
    uint8_t* progressScratch;
    size_t renderCapacity;
    size_t hostCapacity;
    size_t displayCapacity;
    size_t progressScratchCapacity;
    uint64_t generation;
    bool jobActive;
    bool preparedFrameOwned;
    bool displayValid;
    bool loggedEnabled;
    int outputWidth;
    int outputHeight;
    int renderWidth;
    int renderHeight;
    int hostWidth;
    int hostHeight;
    int tileSize;
    int temporalFrames;
    RayTracing3DIntegratorId integratorId;
    Runtime3DUpscaleMode upscaleMode;
    RuntimeNative3DSamplingContext sampling;
} RayTracingDesktopAsyncBridgeState;

static RayTracingDesktopAsyncBridgeState s_desktop_async_bridge;
static uint32_t s_desktop_async_sample_sequence = 1u;

static bool desktop_async_env_enabled(void) {
    const char* value = getenv("RAY_TRACING_NATIVE3D_ASYNC_DESKTOP_BRIDGE");
    if (!value || !value[0]) return false;
    return strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 || strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

static bool desktop_async_trace_enabled(void) {
    const char* value = getenv("RAY_TRACING_NATIVE3D_ASYNC_DESKTOP_TRACE");
    if (!value || !value[0]) return false;
    return strcmp(value, "0") != 0 && strcmp(value, "false") != 0 &&
           strcmp(value, "FALSE") != 0 && strcmp(value, "off") != 0;
}

static RuntimeNative3DSamplingContext desktop_async_next_sampling(void) {
    RuntimeNative3DSamplingContext sampling = {0};
    sampling.sampleSequence = s_desktop_async_sample_sequence++;
    if (sampling.sampleSequence == 0u) {
        sampling.sampleSequence = s_desktop_async_sample_sequence++;
    }
    return sampling;
}

static int desktop_async_temporal_frames(RayTracing3DIntegratorId integrator_id) {
    int frames = animSettings.temporalFrames3D;
    if (integrator_id == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT) {
        return 1;
    }
    if (frames < RUNTIME_3D_TEMPORAL_FRAMES_MIN) {
        frames = RUNTIME_3D_TEMPORAL_FRAMES_MIN;
    }
    if (frames > RUNTIME_3D_TEMPORAL_FRAMES_MAX) {
        frames = RUNTIME_3D_TEMPORAL_FRAMES_MAX;
    }
    return frames;
}

static bool desktop_async_ensure_bytes(uint8_t** buffer,
                                       size_t* capacity,
                                       size_t required) {
    uint8_t* resized = NULL;
    if (!buffer || !capacity || required == 0u) return false;
    if (*capacity >= required && *buffer) return true;
    resized = (uint8_t*)realloc(*buffer, required);
    if (!resized) return false;
    *buffer = resized;
    *capacity = required;
    return true;
}

static void desktop_async_free_prepared_frame(RayTracingDesktopAsyncBridgeState* state) {
    if (!state || !state->preparedFrameOwned) return;
    RuntimeNative3DPreparedFrame_Free(&state->preparedFrame);
    memset(&state->preparedFrame, 0, sizeof(state->preparedFrame));
    state->preparedFrameOwned = false;
}

static void desktop_async_reset_job(RayTracingDesktopAsyncBridgeState* state) {
    if (!state) return;
    if (state->job) {
        RuntimeNative3DAsyncRenderJobStatus status =
            RuntimeNative3DAsyncRenderJob_GetStatus(state->job);
        if (desktop_async_trace_enabled()) {
            fprintf(stderr,
                    "[native3d_async_desktop] shutdown_cancel_first generation=%llu active=%d status_before=%d\n",
                    (unsigned long long)state->generation,
                    state->jobActive ? 1 : 0,
                    (int)status);
        }
        RuntimeNative3DAsyncRenderJob_Destroy(state->job);
        state->job = NULL;
        if (desktop_async_trace_enabled()) {
            fprintf(stderr,
                    "[native3d_async_desktop] shutdown_complete generation=%llu\n",
                    (unsigned long long)state->generation);
        }
    }
    desktop_async_free_prepared_frame(state);
    state->jobActive = false;
}

static bool desktop_async_resolve_dirty_host_union(
    const RuntimeNative3DTileSchedulerProgress* progress,
    int render_width,
    int render_height,
    int host_width,
    int host_height,
    SDL_Rect* out_rect) {
    SDL_Rect union_rect = {0};
    bool seeded = false;
    if (!progress || !progress->dirtyTiles || !out_rect) return false;
    for (size_t i = 0; i < progress->dirtyTileCount; ++i) {
        SDL_Rect rect = {0};
        const IntegratorTile* tile = &progress->dirtyTiles[i];
        if (!RuntimeNative3DPreviewResolveDirtyHostRect(tile->originX,
                                                        tile->originY,
                                                        tile->width,
                                                        tile->height,
                                                        render_width,
                                                        render_height,
                                                        host_width,
                                                        host_height,
                                                        &rect)) {
            continue;
        }
        if (!seeded) {
            union_rect = rect;
            seeded = true;
        } else {
            const int min_x = (rect.x < union_rect.x) ? rect.x : union_rect.x;
            const int min_y = (rect.y < union_rect.y) ? rect.y : union_rect.y;
            const int max_x = ((rect.x + rect.w) > (union_rect.x + union_rect.w))
                                  ? (rect.x + rect.w)
                                  : (union_rect.x + union_rect.w);
            const int max_y = ((rect.y + rect.h) > (union_rect.y + union_rect.h))
                                  ? (rect.y + rect.h)
                                  : (union_rect.y + union_rect.h);
            union_rect.x = min_x;
            union_rect.y = min_y;
            union_rect.w = max_x - min_x;
            union_rect.h = max_y - min_y;
        }
    }
    if (!seeded) return false;
    *out_rect = union_rect;
    return true;
}

static bool desktop_async_publish_tile_progress(
    const RuntimeNative3DTileSchedulerProgress* progress,
    void* user_data) {
    RayTracingDesktopAsyncBridgeState* state =
        (RayTracingDesktopAsyncBridgeState*)user_data;
    SDL_Rect dirty = {0};
    RuntimeNative3DAsyncRenderProgressRect bridge_rect = {0};
    if (!state || !progress || !state->progress) return false;
    if (!desktop_async_resolve_dirty_host_union(progress,
                                                state->renderWidth,
                                                state->renderHeight,
                                                state->hostWidth,
                                                state->hostHeight,
                                                &dirty)) {
        return true;
    }
    if (!RuntimeNative3DPreviewReconstructABGRRectWithMode(state->renderPixels,
                                                           state->renderWidth,
                                                           state->renderHeight,
                                                           state->hostPixels,
                                                           state->hostWidth,
                                                           state->hostHeight,
                                                           &dirty,
                                                           state->upscaleMode)) {
        return false;
    }
    bridge_rect.x = dirty.x;
    bridge_rect.y = dirty.y;
    bridge_rect.width = dirty.w;
    bridge_rect.height = dirty.h;
    return RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(
        state->progress,
        state->generation,
        state->hostPixels,
        state->hostWidth,
        state->hostHeight,
        bridge_rect);
}

static void desktop_async_ignore_temporal_progress(int started_subpasses,
                                                   int completed_subpasses,
                                                   int total_subpasses,
                                                   void* user_data) {
    (void)started_subpasses;
    (void)completed_subpasses;
    (void)total_subpasses;
    (void)user_data;
}

static bool desktop_async_publish_full_frame(RayTracingDesktopAsyncBridgeState* state) {
    RuntimeNative3DAsyncRenderProgressRect rect = {0};
    if (!state || !state->progress || !state->hostPixels) return false;
    rect.x = 0;
    rect.y = 0;
    rect.width = state->hostWidth;
    rect.height = state->hostHeight;
    return RuntimeNative3DAsyncRenderProgressBuffer_PublishDirtyRectABGR(
        state->progress,
        state->generation,
        state->hostPixels,
        state->hostWidth,
        state->hostHeight,
        rect);
}

static bool desktop_async_worker_run(
    const RuntimeNative3DRenderRequestSnapshot* snapshot,
    const RuntimeNative3DTileSchedulerCancelToken* cancel_token,
    void* user_data,
    RuntimeNative3DAsyncRenderJobResult* out_result) {
    RayTracingDesktopAsyncBridgeState* state =
        (RayTracingDesktopAsyncBridgeState*)user_data;
    RuntimeNative3DRenderStats stats = {0};
    RuntimeNative3DTileSchedulerControl control = {
        .cancelToken = cancel_token,
    };
    bool ok = false;
    bool cancel_requested = false;
    if (!state || !snapshot || !state->preparedFrameOwned || !state->renderPixels) {
        return false;
    }
    RuntimeNative3DFillPixelBufferBackground(state->renderPixels,
                                             state->renderWidth,
                                             state->renderHeight,
                                             &state->preparedFrame.scene,
                                             &state->preparedFrame.projector);
    ok = RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressBudgetAndControl(
        state->renderPixels,
        state->integratorId,
        &state->preparedFrame,
        state->temporalFrames,
        desktop_async_ignore_temporal_progress,
        NULL,
        desktop_async_publish_tile_progress,
        state,
        NULL,
        &control,
        &stats);
    cancel_requested = cancel_token && cancel_token->cancelRequested &&
                       *cancel_token->cancelRequested;
    if (ok && !cancel_requested) {
        ok = RuntimeNative3DPreviewReconstructABGRWithMode(state->renderPixels,
                                                           state->renderWidth,
                                                           state->renderHeight,
                                                           state->hostPixels,
                                                           state->hostWidth,
                                                           state->hostHeight,
                                                           state->upscaleMode);
        if (ok) {
            ok = desktop_async_publish_full_frame(state);
        }
    }
    if (out_result) {
        out_result->valid = true;
        out_result->succeeded = ok && !cancel_requested;
        out_result->canceled = cancel_requested;
        out_result->cancelRequested = cancel_requested;
        out_result->generation = state->generation;
        out_result->completionValue = stats.temporalCommittedSubpasses;
        out_result->snapshot = *snapshot;
    }
    return ok && !cancel_requested;
}

static bool desktop_async_apply_progress(RayTracingDesktopAsyncBridgeState* state) {
    RuntimeNative3DAsyncRenderProgressSnapshot snapshot = {0};
    size_t required = 0u;
    size_t row_bytes = 0u;
    if (!state || !state->progress || !state->displayPixels) return false;
    if (!RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(state->progress,
                                                             state->generation,
                                                             &snapshot,
                                                             state->progressScratch,
                                                             state->progressScratchCapacity,
                                                             &required)) {
        if (snapshot.valid && !snapshot.staleGeneration && required > 0u &&
            desktop_async_ensure_bytes(&state->progressScratch,
                                       &state->progressScratchCapacity,
                                       required)) {
            if (!RuntimeNative3DAsyncRenderProgressBuffer_CopyLatest(
                    state->progress,
                    state->generation,
                    &snapshot,
                    state->progressScratch,
                    state->progressScratchCapacity,
                    &required)) {
                return false;
            }
        } else {
            return false;
        }
    }
    if (!snapshot.valid || snapshot.staleGeneration ||
        snapshot.rect.width <= 0 || snapshot.rect.height <= 0) {
        return false;
    }
    row_bytes = (size_t)snapshot.rect.width * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    for (int y = 0; y < snapshot.rect.height; ++y) {
        const size_t dst_offset =
            (((size_t)(snapshot.rect.y + y) * (size_t)state->hostWidth) +
             (size_t)snapshot.rect.x) *
            (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        const size_t src_offset = (size_t)y * row_bytes;
        memcpy(state->displayPixels + dst_offset,
               state->progressScratch + src_offset,
               row_bytes);
    }
    state->displayValid = true;
    if (desktop_async_trace_enabled()) {
        fprintf(stderr,
                "[native3d_async_desktop] progress generation=%llu sequence=%llu rect=%d,%d %dx%d bytes=%zu\n",
                (unsigned long long)state->generation,
                (unsigned long long)snapshot.sequence,
                snapshot.rect.x,
                snapshot.rect.y,
                snapshot.rect.width,
                snapshot.rect.height,
                snapshot.byteCount);
    }
    return true;
}

static void desktop_async_poll_job(RayTracingDesktopAsyncBridgeState* state) {
    RuntimeNative3DAsyncRenderJobResult result = {0};
    RuntimeNative3DAsyncRenderPublishStatus publish_status;
    RuntimeNative3DAsyncRenderJobStatus status;
    if (!state || !state->jobActive || !state->job) return;
    (void)desktop_async_apply_progress(state);
    status = RuntimeNative3DAsyncRenderJob_GetStatus(state->job);
    if (status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_RUNNING ||
        status == RUNTIME_NATIVE_3D_ASYNC_RENDER_JOB_IDLE) {
        return;
    }
    publish_status = RuntimeNative3DAsyncRenderJob_TryPublish(state->job,
                                                              state->generation,
                                                              &result);
    (void)RuntimeNative3DAsyncRenderJob_Join(state->job);
    if (desktop_async_trace_enabled()) {
        fprintf(stderr,
                "[native3d_async_desktop] publish generation=%llu status=%d result_valid=%d succeeded=%d canceled=%d stale=%d\n",
                (unsigned long long)state->generation,
                (int)publish_status,
                result.valid ? 1 : 0,
                result.succeeded ? 1 : 0,
                result.canceled ? 1 : 0,
                result.staleGeneration ? 1 : 0);
    }
    if (publish_status == RUNTIME_NATIVE_3D_ASYNC_RENDER_PUBLISH_PUBLISHED) {
        (void)desktop_async_apply_progress(state);
    }
    desktop_async_reset_job(state);
}

static bool desktop_async_start_job(RayTracingDesktopAsyncBridgeState* state,
                                    const RayTracingRuntimeRoute* route,
                                    int output_width,
                                    int output_height,
                                    int host_width,
                                    int host_height,
                                    int render_width,
                                    int render_height,
                                    double light_x,
                                    double light_y) {
    RuntimeNative3DRenderRequestSnapshot snapshot = {0};
    RuntimeNative3DRenderRequestSnapshotDesc snapshot_desc = {0};
    RuntimeNative3DAsyncRenderAssessment assessment;
    RuntimeNative3DAsyncRenderJobStartDesc start_desc = {0};
    RuntimeSceneAcceleration3DDiagnostics accel_diag =
        RuntimeSceneAcceleration3DDiagnostics_Disabled();
    volatile bool cancel_probe = false;
    RuntimeNative3DTileSchedulerCancelToken cancel_token = {0};
    bool display_dimensions_changed = false;
    size_t render_bytes = 0u;
    size_t host_bytes = 0u;
    if (!state || !route) return false;

    state->generation += 1u;
    if (state->generation == 0u) {
        state->generation = 1u;
    }
    display_dimensions_changed = state->hostWidth != host_width ||
                                 state->hostHeight != host_height;
    state->outputWidth = output_width;
    state->outputHeight = output_height;
    state->hostWidth = host_width;
    state->hostHeight = host_height;
    state->renderWidth = render_width;
    state->renderHeight = render_height;
    state->tileSize =
        RuntimeNative3DTileSchedulerResolveTileSizeForScale(animSettings.tileSize,
                                                            animSettings.renderScale3D);
    state->temporalFrames = desktop_async_temporal_frames(route->integratorMode3D);
    state->integratorId = route->integratorMode3D;
    state->upscaleMode = (Runtime3DUpscaleMode)animSettings.upscaleMode3D;
    state->sampling = desktop_async_next_sampling();

    render_bytes = (size_t)render_width * (size_t)render_height *
                   (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    host_bytes = (size_t)host_width * (size_t)host_height *
                 (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    if (!desktop_async_ensure_bytes(&state->renderPixels, &state->renderCapacity, render_bytes) ||
        !desktop_async_ensure_bytes(&state->hostPixels, &state->hostCapacity, host_bytes) ||
        !desktop_async_ensure_bytes(&state->displayPixels, &state->displayCapacity, host_bytes)) {
        return false;
    }
    if (!state->displayValid ||
        state->displayCapacity < host_bytes ||
        display_dimensions_changed) {
        RuntimeNative3DFillPixelBufferEnvironment(state->displayPixels,
                                                  (size_t)host_width * (size_t)host_height);
        state->displayValid = true;
    }
    RuntimeNative3DFillPixelBufferEnvironment(state->renderPixels,
                                              (size_t)render_width * (size_t)render_height);
    RuntimeNative3DFillPixelBufferEnvironment(state->hostPixels,
                                              (size_t)host_width * (size_t)host_height);
    TileGridEnsure(&state->tileGrid, render_width, render_height, state->tileSize);
    TileGridClear(&state->tileGrid);

    desktop_async_free_prepared_frame(state);
    if (!RuntimeNative3DPrepareFrameWithSampling(&state->preparedFrame,
                                                 render_width,
                                                 render_height,
                                                 AnimationCurrentNormalizedT(),
                                                 light_x,
                                                 light_y,
                                                 &state->sampling)) {
        return false;
    }
    state->preparedFrameOwned = true;
    (void)RuntimeNative3DPrepareFrameTileOccupancy(&state->preparedFrame,
                                                   state->tileSize);
    RuntimeSceneAcceleration3D_AppendTLASDiagnostics(&accel_diag);

    cancel_token.cancelRequested = &cancel_probe;
    cancel_token.generation = state->generation;
    snapshot_desc.generationBound = true;
    snapshot_desc.generation = state->generation;
    snapshot_desc.outputWidth = output_width;
    snapshot_desc.outputHeight = output_height;
    snapshot_desc.renderWidth = render_width;
    snapshot_desc.renderHeight = render_height;
    snapshot_desc.hostWidth = host_width;
    snapshot_desc.hostHeight = host_height;
    snapshot_desc.frameIndex = AnimationCurrentAbsoluteFrameIndex();
    snapshot_desc.frameCount = AnimationConfiguredPathFrameCount();
    snapshot_desc.temporalFrames = state->temporalFrames;
    snapshot_desc.tileSize = state->tileSize;
    snapshot_desc.integratorId = state->integratorId;
    snapshot_desc.sampling = &state->sampling;
    snapshot_desc.preparedFrame = &state->preparedFrame;
    snapshot_desc.sceneAccelerationBound = accel_diag.tlasNodeCount > 0u;
    snapshot_desc.traceRoute = RuntimeRay3D_CurrentTraceRoute();
    snapshot_desc.tlasInstanceCount = accel_diag.tlasInstanceCount;
    snapshot_desc.tlasNodeCount = accel_diag.tlasNodeCount;
    snapshot_desc.traceContextCallbackBound = true;
    snapshot_desc.volumeEnabled = animSettings.volumeInteractionEnabled;
    snapshot_desc.volumeAttached = animSettings.volumeInteractionEnabled &&
                                   animSettings.volumeSourceKind != VOLUME_SOURCE_NONE &&
                                   animSettings.volumeSourcePath[0] != '\0';
    snapshot_desc.volumeFrameSelectionDynamic = snapshot_desc.volumeAttached;
    snapshot_desc.waterSurfaceFrameSelectionDynamic = snapshot_desc.volumeAttached;
    snapshot_desc.cancelToken = &cancel_token;
    if (!RuntimeNative3DRenderRequestSnapshot_Build(&snapshot, &snapshot_desc)) {
        desktop_async_free_prepared_frame(state);
        return false;
    }
    assessment = RuntimeNative3DAsyncRender_AssessSnapshot(&snapshot);
    if (!assessment.ready || !assessment.requiresExclusiveRenderContext) {
        if (desktop_async_trace_enabled()) {
            fprintf(stderr,
                    "[native3d_async_desktop] fallback: %s (%s)\n",
                    RuntimeNative3DAsyncRenderReadiness_Name(assessment.readiness),
                    assessment.reason ? assessment.reason : "n/a");
        }
        desktop_async_free_prepared_frame(state);
        return false;
    }
    if (!state->progress) {
        state->progress = RuntimeNative3DAsyncRenderProgressBuffer_Create();
        if (!state->progress) {
            desktop_async_free_prepared_frame(state);
            return false;
        }
    }
    RuntimeNative3DAsyncRenderProgressBuffer_Reset(state->progress);
    if (!state->job) {
        state->job = RuntimeNative3DAsyncRenderJob_Create();
        if (!state->job) {
            desktop_async_free_prepared_frame(state);
            return false;
        }
    }
    start_desc.snapshot = snapshot;
    start_desc.generation = state->generation;
    start_desc.run_fn = desktop_async_worker_run;
    start_desc.user_data = state;
    if (!RuntimeNative3DAsyncRenderJob_Start(state->job, &start_desc)) {
        desktop_async_reset_job(state);
        return false;
    }
    state->jobActive = true;
    if (desktop_async_trace_enabled()) {
        fprintf(stderr,
                "[native3d_async_desktop] started generation=%llu output=%dx%d host=%dx%d render=%dx%d tile=%d temporal=%d integrator=%d\n",
                (unsigned long long)state->generation,
                output_width,
                output_height,
                host_width,
                host_height,
                render_width,
                render_height,
                state->tileSize,
                state->temporalFrames,
                (int)state->integratorId);
    }
    return true;
}

static bool desktop_async_maybe_start(RayTracingDesktopAsyncBridgeState* state,
                                      int output_width,
                                      int output_height,
                                      double light_x,
                                      double light_y) {
    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    RenderContext* render_context = getRenderContext();
    int host_width = output_width;
    int host_height = output_height;
    int render_width = output_width;
    int render_height = output_height;
    int render_scale = RuntimeNative3DClampRenderScale(animSettings.renderScale3D);
    if (!state || state->jobActive) return true;
    if (!RayTracingModeBackend_IsNative3D(&route) || !route.useTiles) {
        return false;
    }
    if (animSettings.deepRenderMode) {
        return false;
    }
    if (!RuntimeNative3DResolveHostDimensions(output_width,
                                              output_height,
                                              render_context ? render_context->width : output_width,
                                              render_context ? render_context->height : output_height,
                                              render_scale,
                                              &host_width,
                                              &host_height)) {
        return false;
    }
    if (!RuntimeNative3DResolveScaledDimensions(host_width,
                                                host_height,
                                                render_scale,
                                                &render_width,
                                                &render_height)) {
        return false;
    }
    return desktop_async_start_job(state,
                                   &route,
                                   output_width,
                                   output_height,
                                   host_width,
                                   host_height,
                                   render_width,
                                   render_height,
                                   light_x,
                                   light_y);
}

bool RayTracingDesktopAsyncBridge_SubmitFrame(SDL_Window* window_arg,
                                              SDL_Renderer* renderer_arg,
                                              double light_x,
                                              double light_y,
                                              int* frame_counter,
                                              bool* running_arg) {
    RayTracingDesktopAsyncBridgeState* state = &s_desktop_async_bridge;
    TimerHUDSession* timer_hud = timer_hud_session();
    SDL_Rect dst_rect = {0, 0, sceneSettings.windowWidth, sceneSettings.windowHeight};
    bool frame_presented = false;
    (void)frame_counter;
    if (!desktop_async_env_enabled()) {
        return false;
    }
    if (!running_arg || !renderer_arg || !window_arg) {
        return false;
    }
    if (!state->loggedEnabled) {
        fprintf(stderr,
                "[native3d_async_desktop] enabled; unsupported routes fall back to sync render.\n");
        state->loggedEnabled = true;
    }

    setRenderContext(renderer_arg,
                     window_arg,
                     sceneSettings.windowWidth,
                     sceneSettings.windowHeight);
    SetLightPosition(light_x, light_y);
    desktop_async_poll_job(state);
    if (!desktop_async_maybe_start(state,
                                   sceneSettings.windowWidth,
                                   sceneSettings.windowHeight,
                                   light_x,
                                   light_y)) {
        desktop_async_reset_job(state);
        return false;
    }
    desktop_async_poll_job(state);

    if (timer_hud) {
        ts_session_frame_start(timer_hud);
        ts_session_start_timer(timer_hud, "Render Scene Frame");
    }
    render_set_clear_color(renderer_arg, 0, 0, 0, 255);
    if (!render_begin_frame()) {
        if (render_device_lost()) {
            *running_arg = false;
        }
        if (timer_hud) {
            ts_session_stop_timer(timer_hud, "Render Scene Frame");
            ts_session_frame_end(timer_hud);
        }
        return true;
    }

    if (state->displayValid && state->displayPixels) {
        RayTracing2PreviewPresent_DrawABGRBufferToRectFiltered(
            renderer_arg,
            state->displayPixels,
            state->hostWidth,
            state->hostHeight,
            dst_rect,
            animSettings.upscaleMode3D == RUNTIME_3D_UPSCALE_MODE_BILINEAR);
    }
    if (timer_hud) {
        ts_session_stop_timer(timer_hud, "Render Scene Frame");
        ts_session_render(timer_hud);
    }
    RuntimeNative3DProgressHUD_Reset();
    RuntimeNative3DProgressHUD_Draw(renderer_arg);
    frame_presented = render_end_frame();
    if (!frame_presented && render_device_lost()) {
        *running_arg = false;
    }
    if (timer_hud) {
        ts_session_frame_end(timer_hud);
    }
    return true;
}

void RayTracingDesktopAsyncBridge_Shutdown(void) {
    RayTracingDesktopAsyncBridgeState* state = &s_desktop_async_bridge;
    desktop_async_reset_job(state);
    if (state->progress) {
        RuntimeNative3DAsyncRenderProgressBuffer_Destroy(state->progress);
        state->progress = NULL;
    }
    TileGridFree(&state->tileGrid);
    free(state->renderPixels);
    free(state->hostPixels);
    free(state->displayPixels);
    free(state->progressScratch);
    memset(state, 0, sizeof(*state));
}
