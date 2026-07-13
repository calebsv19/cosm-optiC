#include "render/runtime_native_3d_render_internal_host.h"

#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_accel_3d.h"

static bool runtime_native_3d_render_prepared_frame_serial(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    const RuntimeNative3DPreparedFrame* frame,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderUnit render_unit = {0};
    const RuntimeScene3D* trace_scene = NULL;
    bool ok = false;

    trace_scene = frame && frame->traceScene ? frame->traceScene
                                             : (frame ? &frame->scene : NULL);
    if (frame && frame->traceScene &&
        RuntimeRay3D_CurrentTraceRoute() !=
            RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH &&
        !RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(trace_scene)) {
        return false;
    }

    RuntimeNative3DRenderUnit_TakeReusable(&render_unit);
    ok = RuntimeNative3DRenderUnit_Setup(&render_unit,
                                         integrator_id,
                                         frame,
                                         0,
                                         0,
                                         frame->width,
                                         frame->height,
                                         &frame->sampling,
                                         temporal_frames,
                                         animSettings.disneyDenoiseEnabled);
    if (!ok) {
        RuntimeNative3DRenderUnit_Free(&render_unit);
        return false;
    }

    for (int subpass = 0; subpass < temporal_frames; ++subpass) {
        RuntimeNative3DRenderStats subpass_stats = {0};
        const int started_subpasses = subpass + 1;
        if (!RuntimeNative3DRenderUnit_ShouldRenderSubpass(&render_unit, subpass)) {
            break;
        }
        if (progress_callback) {
            progress_callback(started_subpasses,
                              RuntimeNative3DRenderUnit_CommittedSubpasses(&render_unit),
                              temporal_frames,
                              progress_user_data);
        }
        ok = RuntimeNative3DRenderUnit_RenderSubpass(&render_unit, subpass, &subpass_stats);
        if (!ok) {
            break;
        }
        if (progress_callback) {
            progress_callback(started_subpasses,
                              RuntimeNative3DRenderUnit_CommittedSubpasses(&render_unit),
                              temporal_frames,
                              progress_user_data);
        }
        if (out_stats) {
            RuntimeNative3DRenderStats_Accumulate(out_stats, &subpass_stats);
        }
    }

    if (ok) {
        RuntimeNative3DRenderStats resolve_stats = {0};
        if (RuntimeNative3DAdaptiveSampling_TemporalBudgetHeatmapEnabled()) {
            ok = RuntimeNative3DRenderUnit_ResolveTemporalBudgetHeatmapToPixels(
                &render_unit,
                pixel_buffer,
                frame->width);
            resolve_stats.temporalAdaptiveBudgetHeatmapEnabled = ok ? 1 : 0;
        } else {
            ok = RuntimeNative3DRenderUnit_ResolveCurrentToPixelsWithStats(&render_unit,
                                                                           pixel_buffer,
                                                                           frame->width,
                                                                           &resolve_stats);
        }
        if (ok && out_stats) {
            RuntimeNative3DRenderStats_Accumulate(out_stats, &resolve_stats);
        }
    }
    if (ok && out_stats) {
        RuntimeNative3DAdaptivePixelStateSummary adaptive_summary = {0};
        out_stats->temporalCommittedSubpasses =
            RuntimeNative3DRenderUnit_CommittedSubpasses(&render_unit);
        RuntimeNative3DRenderUnit_GetActivityCounts(&render_unit,
                                                    &out_stats->temporalActivePixelCount,
                                                    &out_stats->temporalActiveTileCount,
                                                    &out_stats->temporalInactiveTileCount);
        RuntimeNative3DRenderUnit_GetAdaptiveStateSummary(&render_unit, &adaptive_summary);
        runtime_native_3d_render_stats_record_adaptive_state_summary(out_stats,
                                                                     &adaptive_summary);
        RuntimeNative3DRenderUnit_RecordScratchStats(&render_unit, out_stats);
    }

    RuntimeNative3DRenderUnit_ReturnReusable(&render_unit);
    return ok;
}

static bool runtime_native_3d_render_should_use_tile_scheduler(int width, int height) {
    const int tile_size = RuntimeNative3DTileSchedulerResolveTileSizeForScale(
        animSettings.tileSize,
        animSettings.renderScale3D);
    if (!animSettings.useTiledRenderer) {
        return false;
    }
    return width > tile_size || height > tile_size;
}


void RuntimeNative3DPreparedFrame_Free(RuntimeNative3DPreparedFrame* frame) {
    if (!frame) return;
    RuntimeNative3DTileOccupancy_Free(&frame->tileOccupancy);
    RuntimeCausticVolumeCache3D_Free(&frame->causticVolumeCache);
    RuntimeCausticSurfaceCache3D_Free(&frame->causticSurfaceCache);
    RuntimeScene3D_Free(&frame->scene);
    memset(frame, 0, sizeof(*frame));
}

bool RuntimeNative3DRenderPreparedRegion(uint8_t* pixel_buffer,
                                         RayTracing3DIntegratorId integrator_id,
                                         const RuntimeNative3DPreparedFrame* frame,
                                         int start_x,
                                         int start_y,
                                         int end_x,
                                         int end_y,
                                         RuntimeNative3DRenderStats* out_stats) {
    float* radiance_buffer = NULL;
    const int region_width = end_x - start_x;
    const int region_height = end_y - start_y;
    bool ok = false;

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!pixel_buffer || !frame || !frame->valid) return false;
    if (region_width <= 0 || region_height <= 0) return true;
    radiance_buffer = (float*)calloc((size_t)region_width * (size_t)region_height *
                                         RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                     sizeof(*radiance_buffer));
    if (!radiance_buffer) return false;
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance_buffer,
                                                        region_width,
                                                        integrator_id,
                                                        frame,
                                                        start_x,
                                                        start_y,
                                                        end_x,
                                                        end_y,
                                                        out_stats);
    if (ok) {
        RuntimeNative3DResolveRadianceRegionToPixels(pixel_buffer,
                                                     frame->width,
                                                     radiance_buffer,
                                                     region_width,
                                                     start_x,
                                                     start_y,
                                                     end_x,
                                                     end_y);
    }
    free(radiance_buffer);
    return ok;
}

bool RuntimeNative3DRenderPreparedRegionRadianceRGB(float* radiance_buffer,
                                                    int radiance_stride,
                                                    RayTracing3DIntegratorId integrator_id,
                                                    const RuntimeNative3DPreparedFrame* frame,
                                                    int start_x,
                                                    int start_y,
                                                    int end_x,
                                                    int end_y,
                                                    RuntimeNative3DRenderStats* out_stats) {
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!radiance_buffer || radiance_stride <= 0 || !frame || !frame->valid) return false;
    return runtime_native_3d_render_dispatch_integrator(radiance_buffer,
                                                        radiance_stride,
                                                        integrator_id,
                                                        frame,
                                                        start_x,
                                                        start_y,
                                                        end_x,
                                                        end_y,
                                                        out_stats);
}

bool RuntimeNative3DRenderPreparedRegionLuminance(float* luminance_buffer,
                                                  int luminance_stride,
                                                  RayTracing3DIntegratorId integrator_id,
                                                  const RuntimeNative3DPreparedFrame* frame,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y,
                                                  RuntimeNative3DRenderStats* out_stats) {
    float* radiance_buffer = NULL;
    const int region_width = end_x - start_x;
    const int region_height = end_y - start_y;
    bool ok = false;
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!luminance_buffer || luminance_stride <= 0 || !frame || !frame->valid) return false;
    if (region_width <= 0 || region_height <= 0) return true;
    radiance_buffer = (float*)calloc((size_t)region_width * (size_t)region_height *
                                         RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                     sizeof(*radiance_buffer));
    if (!radiance_buffer) return false;
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance_buffer,
                                                        region_width,
                                                        integrator_id,
                                                        frame,
                                                        start_x,
                                                        start_y,
                                                        end_x,
                                                        end_y,
                                                        out_stats);
    if (ok) {
        for (int y = 0; y < region_height; ++y) {
            for (int x = 0; x < region_width; ++x) {
                const size_t dst_index = (size_t)y * (size_t)luminance_stride + (size_t)x;
                const size_t src_base =
                    ((size_t)y * (size_t)region_width + (size_t)x) *
                    (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
                luminance_buffer[dst_index] = 0.2126f * radiance_buffer[src_base] +
                                              0.7152f * radiance_buffer[src_base + 1u] +
                                              0.0722f * radiance_buffer[src_base + 2u];
            }
        }
    }
    free(radiance_buffer);
    return ok;
}

bool RuntimeNative3DPrepareFrameTileOccupancy(RuntimeNative3DPreparedFrame* frame, int tile_size) {
    if (!frame || !frame->valid) return false;
    return RuntimeNative3DTileOccupancy_Build(&frame->tileOccupancy,
                                              &frame->scene,
                                              &frame->projector,
                                              tile_size);
}

bool RuntimeNative3DPreparedRegionMayContainGeometry(const RuntimeNative3DPreparedFrame* frame,
                                                     int start_x,
                                                     int start_y,
                                                     int end_x,
                                                     int end_y) {
    if (!frame || !frame->valid) return true;
    if (frame->tileOccupancyConservativeAllTiles) return true;
    if (RuntimeVolume3D_HasActiveExtinction(&frame->scene.volume)) {
        return true;
    }
    return RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&frame->tileOccupancy,
                                                                 start_x,
                                                                 start_y,
                                                                 end_x,
                                                                 end_y);
}

bool RuntimeNative3DRenderToPixelBuffer(uint8_t* pixel_buffer,
                                        RayTracing3DIntegratorId integrator_id,
                                        int width,
                                        int height,
                                        double normalized_t,
                                        double live_light_x,
                                        double live_light_y,
                                        RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(pixel_buffer,
                                                                  integrator_id,
                                                                  width,
                                                                  height,
                                                                  normalized_t,
                                                                  live_light_x,
                                                                  live_light_y,
                                                                  NULL,
                                                                  1,
                                                                  out_stats);
}

bool RuntimeNative3DRenderToPixelBufferWithSampling(uint8_t* pixel_buffer,
                                                    RayTracing3DIntegratorId integrator_id,
                                                    int width,
                                                    int height,
                                                    double normalized_t,
                                                    double live_light_x,
                                                    double live_light_y,
                                                    const RuntimeNative3DSamplingContext* sampling,
                                                    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(pixel_buffer,
                                                                  integrator_id,
                                                                  width,
                                                                  height,
                                                                  normalized_t,
                                                                  live_light_x,
                                                                  live_light_y,
                                                                  sampling,
                                                                  1,
                                                                  out_stats);
}

bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgressAtFrameIndex(
        pixel_buffer,
        integrator_id,
        width,
        height,
        normalized_t,
        0,
        live_light_x,
        live_light_y,
        sampling,
        temporal_frames,
        NULL,
        NULL,
        out_stats);
}

bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgress(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgressAtFrameIndex(
        pixel_buffer,
        integrator_id,
        width,
        height,
        normalized_t,
        0,
        live_light_x,
        live_light_y,
        sampling,
        temporal_frames,
        progress_callback,
        progress_user_data,
        out_stats);
}

bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgressAtFrameIndex(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    int frame_index,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporalDetailedProgressAtFrameIndex(
        pixel_buffer,
        integrator_id,
        width,
        height,
        normalized_t,
        frame_index,
        live_light_x,
        live_light_y,
        sampling,
        temporal_frames,
        progress_callback,
        progress_user_data,
        NULL,
        NULL,
        out_stats);
}

typedef struct RuntimeNative3DTileProgressAdapter {
    RuntimeNative3DTemporalTileProgressCallback callback;
    void* user_data;
} RuntimeNative3DTileProgressAdapter;

static bool runtime_native_3d_render_tile_progress_adapter(
    const RuntimeNative3DTileSchedulerProgress* progress,
    void* user_data) {
    RuntimeNative3DTileProgressAdapter* adapter =
        (RuntimeNative3DTileProgressAdapter*)user_data;
    if (!progress || !adapter || !adapter->callback) {
        return false;
    }
    adapter->callback(progress->startedSubpasses,
                      progress->completedSubpasses,
                      progress->totalSubpasses,
                      progress->completedTilesInSubpass,
                      progress->totalTilesInSubpass,
                      adapter->user_data);
    return true;
}

bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalDetailedProgressAtFrameIndex(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    int frame_index,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DTemporalTileProgressCallback tile_progress_callback,
    void* tile_progress_user_data,
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporalDetailedProgressBudgetedAtFrameIndex(
        pixel_buffer,
        integrator_id,
        width,
        height,
        normalized_t,
        frame_index,
        live_light_x,
        live_light_y,
        sampling,
        temporal_frames,
        progress_callback,
        progress_user_data,
        tile_progress_callback,
        tile_progress_user_data,
        NULL,
        out_stats);
}

bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalDetailedProgressBudgetedAtFrameIndex(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    int frame_index,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DTemporalTileProgressCallback tile_progress_callback,
    void* tile_progress_user_data,
    const RuntimeNative3DResourceBudget* resource_budget,
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DTileProgressAdapter tile_progress_adapter = {0};
    bool ok = false;
    const int effective_temporal_frames = (temporal_frames <= 1) ? 1 : temporal_frames;

    if (!pixel_buffer || width <= 0 || height <= 0) return false;
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }

    ok = RuntimeNative3DPrepareFrameWithSamplingAtFrameIndex(&frame,
                                                             width,
                                                             height,
                                                             normalized_t,
                                                             frame_index,
                                                             live_light_x,
                                                             live_light_y,
                                                             sampling);
    if (!ok) {
        RuntimeNative3DFillPixelBufferEnvironment(pixel_buffer, (size_t)width * (size_t)height);
        return false;
    }
    RuntimeNative3DFillPixelBufferBackground(pixel_buffer,
                                             width,
                                             height,
                                             &frame.scene,
                                             &frame.projector);
    if (runtime_native_3d_render_should_use_tile_scheduler(width, height)) {
        if (tile_progress_callback) {
            tile_progress_adapter.callback = tile_progress_callback;
            tile_progress_adapter.user_data = tile_progress_user_data;
            ok = RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressAndBudget(
                pixel_buffer,
                integrator_id,
                &frame,
                effective_temporal_frames,
                progress_callback,
                progress_user_data,
                runtime_native_3d_render_tile_progress_adapter,
                &tile_progress_adapter,
                resource_budget,
                out_stats);
        } else {
            ok = RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressAndBudget(
                pixel_buffer,
                integrator_id,
                &frame,
                effective_temporal_frames,
                progress_callback,
                progress_user_data,
                NULL,
                NULL,
                resource_budget,
                out_stats);
        }
    } else {
        ok = runtime_native_3d_render_prepared_frame_serial(pixel_buffer,
                                                            integrator_id,
                                                            &frame,
                                                            effective_temporal_frames,
                                                            progress_callback,
                                                            progress_user_data,
                                                            out_stats);
    }
    if (ok && out_stats) {
        runtime_native_3d_render_stats_normalize_temporal(
            out_stats,
            out_stats->temporalCommittedSubpasses);
    }
    RuntimeNative3DPreparedFrame_Free(&frame);
    return ok;
}
