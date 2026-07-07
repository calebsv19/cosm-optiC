#include "render/runtime_native_3d_render_unit.h"

#include <stdlib.h>
#include <string.h>

static RuntimeNative3DSamplingContext runtime_native_3d_render_unit_resolve_subpass_sampling(
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t sequence_offset,
    int total_subpasses) {
    RuntimeNative3DSamplingContext resolved = {0};
    if (sampling) {
        resolved = *sampling;
    }
    resolved.sampleSequence += sequence_offset;
    if (resolved.sampleSequence == 0U) {
        resolved.sampleSequence = sequence_offset + 1U;
    }
    resolved.temporalSubpassIndex = (uint16_t)((sequence_offset > 65535u) ? 65535u : sequence_offset);
    resolved.temporalSubpassCount =
        (uint16_t)((total_subpasses <= 1) ? 1 : ((total_subpasses > 65535) ? 65535 : total_subpasses));
    return resolved;
}

static bool runtime_native_3d_render_unit_resolve_adaptive_sampling_enabled(
    RayTracing3DIntegratorId integrator_id,
    int temporal_frames) {
    return RuntimeNative3DAdaptiveSampling_RuntimeEnabled() &&
           RuntimeNative3DAdaptiveSampling_ShouldUse(integrator_id, temporal_frames);
}

void RuntimeNative3DRenderUnit_Init(RuntimeNative3DRenderUnit* unit) {
    if (!unit) return;
    memset(unit, 0, sizeof(*unit));
    RuntimeNative3DTemporalAccumulation_Init(&unit->accumulation);
    RuntimeNative3DAdaptiveSamplingMask_Init(&unit->adaptiveMask);
    RuntimeNative3DAdaptivePixelStateBuffer_Init(&unit->adaptivePixelState);
    RuntimeNative3DFeatureBuffer_Init(&unit->featureBuffer);
}

void RuntimeNative3DRenderUnit_Free(RuntimeNative3DRenderUnit* unit) {
    if (!unit) return;
    free(unit->subpassRadiance);
    free(unit->resolvedRadiance);
    RuntimeNative3DTemporalAccumulation_Free(&unit->accumulation);
    RuntimeNative3DAdaptiveSamplingMask_Free(&unit->adaptiveMask);
    RuntimeNative3DAdaptivePixelStateBuffer_Free(&unit->adaptivePixelState);
    RuntimeNative3DFeatureBuffer_Free(&unit->featureBuffer);
    memset(unit, 0, sizeof(*unit));
}

static bool runtime_native_3d_render_unit_ensure_features(RuntimeNative3DRenderUnit* unit) {
    if (!unit || !unit->frame) return false;
    if (unit->featuresPrepared) {
        return true;
    }
    if (!(unit->useDenoise || unit->useDisneyTemporalPruning || unit->measureAdaptiveState)) {
        unit->featuresPrepared = true;
        return true;
    }
    if (!RuntimeNative3DFeatureBuffer_Ensure(&unit->featureBuffer, unit->width, unit->height) ||
        !RuntimeNative3DFeatureBuffer_RenderRegion(&unit->featureBuffer,
                                                   &unit->frame->scene,
                                                   &unit->frame->projector,
                                                   unit->startX,
                                                   unit->startY,
                                                   unit->endX,
                                                   unit->endY)) {
        return false;
    }
    unit->featuresPrepared = true;
    return true;
}

static bool runtime_native_3d_render_unit_prepare_initial_adaptive_mask(
    RuntimeNative3DRenderUnit* unit) {
    if (!unit || !unit->frame) return false;
    if (!unit->useAdaptiveSampling) {
        return true;
    }
    return RuntimeNative3DAdaptiveSampling_BeginTemporalActivityMask(
        &unit->adaptiveMask,
        unit->width,
        unit->height,
        RUNTIME_NATIVE_3D_ADAPTIVE_TILE_SIZE,
        RUNTIME_NATIVE_3D_ADAPTIVE_MIN_SUBPASSES);
}

bool RuntimeNative3DRenderUnit_Setup(RuntimeNative3DRenderUnit* unit,
                                     RayTracing3DIntegratorId integrator_id,
                                     const RuntimeNative3DPreparedFrame* frame,
                                     int start_x,
                                     int start_y,
                                     int end_x,
                                     int end_y,
                                     const RuntimeNative3DSamplingContext* sampling,
                                     int temporal_frames,
                                     bool disney_denoise_enabled) {
    size_t pixel_count = 0;

    if (!unit || !frame || !frame->valid || start_x >= end_x || start_y >= end_y) {
        return false;
    }

    unit->frame = frame;
    unit->integratorId = integrator_id;
    unit->startX = start_x;
    unit->startY = start_y;
    unit->endX = end_x;
    unit->endY = end_y;
    unit->width = end_x - start_x;
    unit->height = end_y - start_y;
    unit->temporalFrames = (temporal_frames <= 1) ? 1 : temporal_frames;
    unit->committedSubpasses = 0;
    unit->baseSampling = sampling ? *sampling : (RuntimeNative3DSamplingContext){0};
    unit->useAdaptiveSampling =
        runtime_native_3d_render_unit_resolve_adaptive_sampling_enabled(integrator_id,
                                                                        unit->temporalFrames);
    unit->useDisneyTemporalPruning =
        unit->useAdaptiveSampling && integrator_id == RAY_TRACING_3D_INTEGRATOR_DISNEY;
    unit->useDenoise =
        RuntimeNative3DDenoise_ShouldApply(integrator_id,
                                           unit->temporalFrames,
                                           disney_denoise_enabled);
    unit->measureAdaptiveState = unit->temporalFrames > 1;
    unit->featuresPrepared = false;

    if (!RuntimeNative3DTemporalAccumulation_Ensure(&unit->accumulation, unit->width, unit->height)) {
        return false;
    }
    RuntimeNative3DTemporalAccumulation_Clear(&unit->accumulation);
    RuntimeNative3DAdaptiveSamplingMask_Clear(&unit->adaptiveMask);
    RuntimeNative3DAdaptivePixelStateBuffer_Clear(&unit->adaptivePixelState);

    pixel_count = (size_t)unit->width * (size_t)unit->height *
                  (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    if (pixel_count == 0u) {
        return false;
    }
    if (unit->radianceCapacity < pixel_count) {
        free(unit->subpassRadiance);
        free(unit->resolvedRadiance);
        unit->subpassRadiance = (float*)calloc(pixel_count, sizeof(*unit->subpassRadiance));
        unit->resolvedRadiance = (float*)calloc(pixel_count, sizeof(*unit->resolvedRadiance));
        if (unit->subpassRadiance && unit->resolvedRadiance) {
            unit->radianceCapacity = pixel_count;
        } else {
            free(unit->subpassRadiance);
            free(unit->resolvedRadiance);
            unit->subpassRadiance = NULL;
            unit->resolvedRadiance = NULL;
            unit->radianceCapacity = 0u;
        }
    }
    if (!unit->subpassRadiance || !unit->resolvedRadiance) {
        return false;
    }

    if (!runtime_native_3d_render_unit_prepare_initial_adaptive_mask(unit)) {
        return false;
    }

    return true;
}

bool RuntimeNative3DRenderUnit_ShouldRenderSubpass(const RuntimeNative3DRenderUnit* unit,
                                                   int subpass_index) {
    if (!unit || subpass_index < 0 || subpass_index >= unit->temporalFrames) {
        return false;
    }
    if (unit->committedSubpasses > subpass_index) {
        return false;
    }
    if (!unit->useAdaptiveSampling || subpass_index == 0) {
        return true;
    }
    return RuntimeNative3DAdaptiveSampling_HasActiveSamples(&unit->adaptiveMask);
}

bool RuntimeNative3DRenderUnit_RenderSubpass(RuntimeNative3DRenderUnit* unit,
                                             int subpass_index,
                                             RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DPreparedFrame subpass_frame = {0};
    RuntimeNative3DSamplingContext subpass_sampling = {0};
    RuntimeNative3DRenderStats stats = {0};
    const uint8_t* active_mask = NULL;
    int active_mask_stride = 0;
    int active_pixels = 0;
    size_t pixel_count = 0;

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!unit || !unit->frame || subpass_index < 0 || subpass_index >= unit->temporalFrames) {
        return false;
    }
    if (!RuntimeNative3DRenderUnit_ShouldRenderSubpass(unit, subpass_index)) {
        return true;
    }

    if (unit->useAdaptiveSampling && subpass_index > 0) {
        active_mask = unit->adaptiveMask.activeSampleMask;
        active_mask_stride = unit->adaptiveMask.width;
        active_pixels = unit->adaptiveMask.activePixelCount;
    } else {
        active_pixels = unit->width * unit->height;
    }

    pixel_count = (size_t)unit->width * (size_t)unit->height *
                  (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    memset(unit->subpassRadiance, 0, pixel_count * sizeof(*unit->subpassRadiance));

    if ((unit->measureAdaptiveState || unit->useDenoise) &&
        !runtime_native_3d_render_unit_ensure_features(unit)) {
        return false;
    }

    subpass_frame = *unit->frame;
    subpass_sampling = runtime_native_3d_render_unit_resolve_subpass_sampling(
        &unit->baseSampling,
        (uint32_t)subpass_index,
        unit->temporalFrames);
    subpass_frame.sampling = subpass_sampling;
    subpass_frame.traceScene = &unit->frame->scene;
    if (unit->measureAdaptiveState) {
        subpass_frame.featureAttributionBuffer = &unit->featureBuffer;
        subpass_frame.featureAttributionStartX = unit->startX;
        subpass_frame.featureAttributionStartY = unit->startY;
        subpass_frame.directLightVisibilityAttributionEnabled = true;
    }

    if (!RuntimeNative3DAdaptiveSampling_RenderPreparedRegionRadianceRGBMasked(
            unit->subpassRadiance,
            unit->width,
            unit->integratorId,
            &subpass_frame,
            unit->startX,
            unit->startY,
            unit->endX,
            unit->endY,
            active_mask,
            active_mask_stride,
            &stats) ||
        !RuntimeNative3DTemporalAccumulation_AddRegionSamples(&unit->accumulation,
                                                              unit->subpassRadiance,
                                                              unit->width,
                                                              0,
                                                              0,
                                                              unit->width,
                                                              unit->height,
                                                              active_mask,
                                                              active_mask_stride)) {
        return false;
    }

    stats.temporalPixelsRendered = active_pixels;
    stats.temporalPixelsSkipped = (unit->width * unit->height) - active_pixels;
    RuntimeNative3DTemporalAccumulation_CommitSubpass(&unit->accumulation);
    if (unit->measureAdaptiveState &&
        !RuntimeNative3DAdaptiveSampling_MeasurePixelState(&unit->adaptivePixelState,
                                                           &unit->accumulation,
                                                           &unit->featureBuffer,
                                                           RUNTIME_NATIVE_3D_ADAPTIVE_TILE_SIZE,
                                                           RUNTIME_NATIVE_3D_ADAPTIVE_MIN_SUBPASSES,
                                                           4)) {
        return false;
    }
    if (unit->useAdaptiveSampling) {
        const bool refreshed =
            RuntimeNative3DAdaptiveSampling_RiskEarlyStopEnabled()
                ? RuntimeNative3DAdaptiveSampling_RefreshConservativeEarlyStopMaskFromPixelState(
                      &unit->adaptiveMask,
                      &unit->adaptivePixelState,
                      RUNTIME_NATIVE_3D_ADAPTIVE_TILE_SIZE)
                : RuntimeNative3DAdaptiveSampling_RefreshActivityMaskFromPixelState(
                      &unit->adaptiveMask,
                      &unit->adaptivePixelState,
                      RUNTIME_NATIVE_3D_ADAPTIVE_TILE_SIZE);
        if (!refreshed) {
            return false;
        }
    }

    unit->committedSubpasses += 1;
    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}

static void runtime_native_3d_render_unit_record_denoise_diagnostics(
    RuntimeNative3DRenderStats* stats,
    const RuntimeNative3DDenoiseDiagnostics* diagnostics) {
    if (!stats || !diagnostics) return;
    stats->denoiseTemporalFrameCount = diagnostics->temporalFrameCount;
    stats->denoiseRawPixelCount = diagnostics->rawPixelCount;
    stats->denoiseReconstructedPixelCount = diagnostics->reconstructedPixelCount;
    stats->denoiseStableInteriorSampleCount = diagnostics->stableInteriorSampleCount;
    stats->denoiseRejectedEdgeSampleCount = diagnostics->rejectedEdgeSampleCount;
    stats->denoisePreservedTransparentPixelCount = diagnostics->preservedTransparentPixelCount;
    stats->denoisePreservedMirrorGlossyPixelCount =
        diagnostics->preservedMirrorGlossyPixelCount;
    stats->denoiseSkippedUnstableTemporalPixelCount =
        diagnostics->skippedUnstableTemporalPixelCount;
    stats->denoiseSkippedInvalidSurfacePixelCount =
        diagnostics->skippedInvalidSurfacePixelCount;
    stats->denoiseRawRadianceLumaTotal = diagnostics->rawRadianceLumaTotal;
    stats->denoiseReconstructedRadianceLumaTotal =
        diagnostics->reconstructedRadianceLumaTotal;
}

bool RuntimeNative3DRenderUnit_ResolveCurrentToPixelsWithStats(
    const RuntimeNative3DRenderUnit* unit,
    uint8_t* pixel_buffer,
    int pixel_width,
    RuntimeNative3DRenderStats* out_stats) {
    size_t pixel_count = 0;

    if (!unit || !pixel_buffer || pixel_width <= 0 || unit->width <= 0 || unit->height <= 0) {
        return false;
    }

    if (!unit->useDenoise) {
        RuntimeNative3DTemporalAccumulation_ResolveToPixelBufferAtOffset(&unit->accumulation,
                                                                         pixel_buffer,
                                                                         pixel_width,
                                                                         unit->startX,
                                                                         unit->startY);
        return true;
    }

    pixel_count = (size_t)unit->width * (size_t)unit->height *
                  (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    memset(unit->resolvedRadiance, 0, pixel_count * sizeof(*unit->resolvedRadiance));
    {
        RuntimeNative3DDenoiseDiagnostics diagnostics = {0};
        if (!RuntimeNative3DTemporalAccumulation_ResolveRegionToRadianceBuffer(
                &unit->accumulation,
                unit->resolvedRadiance,
                unit->width,
                0,
                0,
                unit->width,
                unit->height) ||
            !RuntimeNative3DDenoise_ApplyForIntegrator(unit->resolvedRadiance,
                                                       unit->width,
                                                       &unit->featureBuffer,
                                                       unit->integratorId,
                                                       unit->committedSubpasses,
                                                       unit->accumulation.activityBuffer,
                                                       unit->accumulation.width,
                                                       out_stats ? &diagnostics : NULL)) {
            return false;
        }
        if (out_stats) {
            runtime_native_3d_render_unit_record_denoise_diagnostics(out_stats, &diagnostics);
        }
    }

    RuntimeNative3DResolveRadianceRegionToPixels(pixel_buffer,
                                                 pixel_width,
                                                 unit->resolvedRadiance,
                                                 unit->width,
                                                 unit->startX,
                                                 unit->startY,
                                                 unit->endX,
                                                 unit->endY);
    return true;
}

bool RuntimeNative3DRenderUnit_ResolveCurrentRawToPixels(const RuntimeNative3DRenderUnit* unit,
                                                         uint8_t* pixel_buffer,
                                                         int pixel_width) {
    if (!unit || !pixel_buffer || pixel_width <= 0 || unit->width <= 0 || unit->height <= 0) {
        return false;
    }
    RuntimeNative3DTemporalAccumulation_ResolveToPixelBufferAtOffset(&unit->accumulation,
                                                                     pixel_buffer,
                                                                     pixel_width,
                                                                     unit->startX,
                                                                     unit->startY);
    return true;
}

bool RuntimeNative3DRenderUnit_ResolveCurrentToPixels(const RuntimeNative3DRenderUnit* unit,
                                                      uint8_t* pixel_buffer,
                                                      int pixel_width) {
    return RuntimeNative3DRenderUnit_ResolveCurrentToPixelsWithStats(unit,
                                                                     pixel_buffer,
                                                                     pixel_width,
                                                                     NULL);
}

int RuntimeNative3DRenderUnit_CommittedSubpasses(const RuntimeNative3DRenderUnit* unit) {
    if (!unit) return 0;
    return unit->committedSubpasses;
}

void RuntimeNative3DRenderUnit_GetActivityCounts(const RuntimeNative3DRenderUnit* unit,
                                                 int* out_active_pixels,
                                                 int* out_active_tiles,
                                                 int* out_inactive_tiles) {
    if (out_active_pixels) *out_active_pixels = 0;
    if (out_active_tiles) *out_active_tiles = 0;
    if (out_inactive_tiles) *out_inactive_tiles = 0;
    if (!unit) return;
    if (out_active_pixels) *out_active_pixels = unit->adaptiveMask.activePixelCount;
    if (out_active_tiles) *out_active_tiles = unit->adaptiveMask.activeTileCount;
    if (out_inactive_tiles) *out_inactive_tiles = unit->adaptiveMask.inactiveTileCount;
}

void RuntimeNative3DRenderUnit_GetAdaptiveStateSummary(
    const RuntimeNative3DRenderUnit* unit,
    RuntimeNative3DAdaptivePixelStateSummary* out_summary) {
    if (!out_summary) return;
    memset(out_summary, 0, sizeof(*out_summary));
    if (!unit || !unit->measureAdaptiveState) return;
    *out_summary = unit->adaptivePixelState.summary;
}
