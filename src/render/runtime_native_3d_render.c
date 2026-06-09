#include "render/runtime_native_3d_render.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_native_3d_render_internal.h"
#include "render/runtime_native_3d_render_unit.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_scene_3d_samples.h"

static double runtime_native_3d_render_clamp_unit(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

uint8_t RuntimeNative3DResolveEnvironmentByte(void) {
    double value = animSettings.environmentBrightness;
    if (animation_config_environment_light_mode_clamp(animSettings.environmentLightMode) !=
        ENVIRONMENT_LIGHT_MODE_AMBIENT) {
        return 0u;
    }
    if (value < 0.0) value = 0.0;
    if (value > 255.0) value = 255.0;
    return (uint8_t)lround(value);
}

void RuntimeNative3DFillPixelBufferEnvironment(uint8_t* pixel_buffer, size_t pixel_count) {
    const uint8_t environment = RuntimeNative3DResolveEnvironmentByte();
    if (!pixel_buffer) return;

    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t base = i * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        pixel_buffer[base] = environment;
        pixel_buffer[base + 1u] = environment;
        pixel_buffer[base + 2u] = environment;
        pixel_buffer[base + 3u] = 0xFFu;
    }
}

void RuntimeNative3DResolveRadianceRegionToPixels(
    uint8_t* pixel_buffer,
    int pixel_width,
    const float* radiance_buffer,
    int radiance_stride,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    if (!pixel_buffer || !radiance_buffer || pixel_width <= 0 || radiance_stride <= 0) return;
    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        for (int x = start_x; x < end_x; ++x) {
            const int local_x = x - start_x;
            const size_t pixel_base =
                ((size_t)y * (size_t)pixel_width + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            const size_t radiance_base =
                ((size_t)local_y * (size_t)radiance_stride + (size_t)local_x) *
                (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            const uint8_t environment = (uint8_t)lround(
                runtime_native_3d_render_clamp_unit(
                    radiance_buffer[radiance_base +
                                    RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL]) *
                255.0);
            pixel_buffer[pixel_base] = TonemapCurveToByteWithFloor(
                radiance_buffer[radiance_base], environment);
            pixel_buffer[pixel_base + 1u] = TonemapCurveToByteWithFloor(
                radiance_buffer[radiance_base + 1u], environment);
            pixel_buffer[pixel_base + 2u] = TonemapCurveToByteWithFloor(
                radiance_buffer[radiance_base + 2u], environment);
            pixel_buffer[pixel_base + 3u] = 0xFFu;
        }
    }
}

void RuntimeNative3DRenderStats_Accumulate(RuntimeNative3DRenderStats* dst,
                                           const RuntimeNative3DRenderStats* src) {
    if (!dst || !src) return;
    dst->hitPixelCount += src->hitPixelCount;
    dst->visiblePixelCount += src->visiblePixelCount;
    dst->bouncePixelCount += src->bouncePixelCount;
    dst->secondaryRayCount += src->secondaryRayCount;
    dst->secondaryHitCount += src->secondaryHitCount;
    dst->secondaryContributingHitCount += src->secondaryContributingHitCount;
    dst->temporalCommittedSubpasses += src->temporalCommittedSubpasses;
    dst->temporalPixelsRendered += src->temporalPixelsRendered;
    dst->temporalPixelsSkipped += src->temporalPixelsSkipped;
    dst->temporalActivePixelCount += src->temporalActivePixelCount;
    dst->temporalActiveTileCount += src->temporalActiveTileCount;
    dst->temporalInactiveTileCount += src->temporalInactiveTileCount;
    dst->temporalMeasuredTileJobs += src->temporalMeasuredTileJobs;
    dst->temporalAdaptiveSplitParentCount += src->temporalAdaptiveSplitParentCount;
    dst->temporalAdaptiveChildTileCount += src->temporalAdaptiveChildTileCount;
    if (src->maxRadiance > dst->maxRadiance) {
        dst->maxRadiance = src->maxRadiance;
    }
    if (src->maxBounceRadiance > dst->maxBounceRadiance) {
        dst->maxBounceRadiance = src->maxBounceRadiance;
    }
    dst->totalBounceRadiance += src->totalBounceRadiance;
    dst->temporalTotalTileMs += src->temporalTotalTileMs;
    if (src->temporalMaxTileMs > dst->temporalMaxTileMs) {
        dst->temporalMaxTileMs = src->temporalMaxTileMs;
        dst->temporalSlowTileOriginX = src->temporalSlowTileOriginX;
        dst->temporalSlowTileOriginY = src->temporalSlowTileOriginY;
        dst->temporalSlowTileWidth = src->temporalSlowTileWidth;
        dst->temporalSlowTileHeight = src->temporalSlowTileHeight;
    }
    if (src->temporalMaxTileSubpassMs > dst->temporalMaxTileSubpassMs) {
        dst->temporalMaxTileSubpassMs = src->temporalMaxTileSubpassMs;
    }
    if (dst->temporalMeasuredTileJobs > 0) {
        dst->temporalAverageTileMs =
            dst->temporalTotalTileMs / (double)dst->temporalMeasuredTileJobs;
    }
}

static int runtime_native_3d_render_stats_round_divide(int value, int divisor) {
    if (divisor <= 1) return value;
    return (int)lround((double)value / (double)divisor);
}

static void runtime_native_3d_render_stats_normalize_temporal(
    RuntimeNative3DRenderStats* stats,
    int committed_subpasses) {
    if (!stats || committed_subpasses <= 1) return;
    stats->hitPixelCount =
        runtime_native_3d_render_stats_round_divide(stats->hitPixelCount, committed_subpasses);
    stats->visiblePixelCount =
        runtime_native_3d_render_stats_round_divide(stats->visiblePixelCount, committed_subpasses);
    stats->bouncePixelCount =
        runtime_native_3d_render_stats_round_divide(stats->bouncePixelCount, committed_subpasses);
    stats->secondaryRayCount =
        runtime_native_3d_render_stats_round_divide(stats->secondaryRayCount, committed_subpasses);
    stats->secondaryHitCount =
        runtime_native_3d_render_stats_round_divide(stats->secondaryHitCount, committed_subpasses);
    stats->secondaryContributingHitCount = runtime_native_3d_render_stats_round_divide(
        stats->secondaryContributingHitCount, committed_subpasses);
    stats->totalBounceRadiance /= (double)committed_subpasses;
}

static bool runtime_native_3d_render_prepared_frame_serial(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    const RuntimeNative3DPreparedFrame* frame,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderUnit render_unit = {0};
    bool ok = false;

    RuntimeNative3DRenderUnit_Init(&render_unit);
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
        ok = RuntimeNative3DRenderUnit_ResolveCurrentToPixels(&render_unit, pixel_buffer, frame->width);
    }
    if (ok && out_stats) {
        out_stats->temporalCommittedSubpasses =
            RuntimeNative3DRenderUnit_CommittedSubpasses(&render_unit);
        RuntimeNative3DRenderUnit_GetActivityCounts(&render_unit,
                                                    &out_stats->temporalActivePixelCount,
                                                    &out_stats->temporalActiveTileCount,
                                                    &out_stats->temporalInactiveTileCount);
    }

    RuntimeNative3DRenderUnit_Free(&render_unit);
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
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DTileProgressAdapter tile_progress_adapter = {0};
    bool ok = false;
    const int effective_temporal_frames = (temporal_frames <= 1) ? 1 : temporal_frames;

    if (!pixel_buffer || width <= 0 || height <= 0) return false;
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }

    RuntimeNative3DFillPixelBufferEnvironment(pixel_buffer, (size_t)width * (size_t)height);
    ok = RuntimeNative3DPrepareFrameWithSamplingAtFrameIndex(&frame,
                                                             width,
                                                             height,
                                                             normalized_t,
                                                             frame_index,
                                                             live_light_x,
                                                             live_light_y,
                                                             sampling);
    if (!ok) {
        return false;
    }
    if (runtime_native_3d_render_should_use_tile_scheduler(width, height)) {
        if (tile_progress_callback) {
            tile_progress_adapter.callback = tile_progress_callback;
            tile_progress_adapter.user_data = tile_progress_user_data;
            ok = RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgress(
                pixel_buffer,
                integrator_id,
                &frame,
                effective_temporal_frames,
                progress_callback,
                progress_user_data,
                runtime_native_3d_render_tile_progress_adapter,
                &tile_progress_adapter,
                out_stats);
        } else {
            ok = RuntimeNative3DRenderPreparedFrameTemporalTiled(pixel_buffer,
                                                                 integrator_id,
                                                                 &frame,
                                                                 effective_temporal_frames,
                                                                 progress_callback,
                                                                 progress_user_data,
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
