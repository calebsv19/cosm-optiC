#include "render/runtime_native_3d_render_unit.h"

#include <stdlib.h>
#include <string.h>

static RuntimeNative3DRenderUnit* s_runtime_native_3d_render_unit_scratch_cache = NULL;
static size_t s_runtime_native_3d_render_unit_scratch_cache_count = 0u;
static size_t s_runtime_native_3d_render_unit_scratch_cache_capacity = 0u;

static bool runtime_native_3d_render_unit_scratch_reuse_disabled(void) {
    const char* value = getenv("RAY_TRACING_NATIVE3D_DISABLE_RENDER_UNIT_SCRATCH_REUSE");
    if (!value || !*value) return false;
    return strcmp(value, "0") != 0 && strcmp(value, "false") != 0 &&
           strcmp(value, "FALSE") != 0 && strcmp(value, "off") != 0 &&
           strcmp(value, "OFF") != 0 && strcmp(value, "no") != 0 &&
           strcmp(value, "NO") != 0;
}

static void runtime_native_3d_render_unit_reset_scratch_counters(
    RuntimeNative3DRenderUnit* unit) {
    if (!unit) return;
    unit->scratchSetupCalls = 0u;
    unit->scratchCacheAcquireHits = 0u;
    unit->scratchCacheAcquireMisses = 0u;
    unit->radianceScratchResizeCalls = 0u;
    unit->radianceScratchReuseCalls = 0u;
    unit->radianceScratchClearBytes = 0u;
    unit->radianceScratchRequestedBytesMax = 0u;
    unit->radianceScratchCapacityBytesMax = 0u;
}

static uint64_t runtime_native_3d_render_unit_u64_product(uint64_t a, uint64_t b) {
    if (a != 0u && b > UINT64_MAX / a) {
        return UINT64_MAX;
    }
    return a * b;
}

static uint64_t runtime_native_3d_render_unit_pixel_count_bytes(int width, int height) {
    if (width <= 0 || height <= 0) return 0u;
    return runtime_native_3d_render_unit_u64_product((uint64_t)width, (uint64_t)height);
}

static uint64_t runtime_native_3d_render_unit_radiance_capacity_bytes(
    const RuntimeNative3DRenderUnit* unit) {
    if (!unit) return 0u;
    return runtime_native_3d_render_unit_u64_product(
        runtime_native_3d_render_unit_u64_product((uint64_t)unit->radianceCapacity,
                                                  (uint64_t)sizeof(float)),
        2u);
}

static uint64_t runtime_native_3d_render_unit_temporal_capacity_bytes(
    const RuntimeNative3DRenderUnit* unit) {
    uint64_t pixel_count = 0u;
    if (!unit || !unit->accumulation.accumulationBuffer || unit->accumulation.width <= 0 ||
        unit->accumulation.height <= 0) {
        return 0u;
    }
    pixel_count = runtime_native_3d_render_unit_pixel_count_bytes(unit->accumulation.width,
                                                                 unit->accumulation.height);
    return runtime_native_3d_render_unit_u64_product(
               pixel_count,
               (uint64_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS * (uint64_t)sizeof(float)) +
           runtime_native_3d_render_unit_u64_product(pixel_count, (uint64_t)sizeof(float)) +
           runtime_native_3d_render_unit_u64_product(pixel_count, (uint64_t)sizeof(uint16_t));
}

static uint64_t runtime_native_3d_render_unit_adaptive_mask_capacity_bytes(
    const RuntimeNative3DRenderUnit* unit) {
    uint64_t pixel_count = 0u;
    uint64_t tile_count = 0u;
    if (!unit || !unit->adaptiveMask.activeSampleMask || unit->adaptiveMask.width <= 0 ||
        unit->adaptiveMask.height <= 0) {
        return 0u;
    }
    pixel_count = runtime_native_3d_render_unit_pixel_count_bytes(unit->adaptiveMask.width,
                                                                 unit->adaptiveMask.height);
    if (unit->adaptiveMask.activeTileMask && unit->adaptiveMask.tilesX > 0 &&
        unit->adaptiveMask.tilesY > 0) {
        tile_count = runtime_native_3d_render_unit_pixel_count_bytes(unit->adaptiveMask.tilesX,
                                                                    unit->adaptiveMask.tilesY);
    }
    return runtime_native_3d_render_unit_u64_product(pixel_count, 3u) + tile_count;
}

static uint64_t runtime_native_3d_render_unit_adaptive_state_capacity_bytes(
    const RuntimeNative3DRenderUnit* unit) {
    if (!unit || !unit->adaptivePixelState.pixels || unit->adaptivePixelState.width <= 0 ||
        unit->adaptivePixelState.height <= 0) {
        return 0u;
    }
    return runtime_native_3d_render_unit_u64_product(
        runtime_native_3d_render_unit_pixel_count_bytes(unit->adaptivePixelState.width,
                                                       unit->adaptivePixelState.height),
        (uint64_t)sizeof(RuntimeNative3DAdaptivePixelState));
}

static uint64_t runtime_native_3d_render_unit_feature_capacity_bytes(
    const RuntimeNative3DRenderUnit* unit) {
    uint64_t pixel_count = 0u;
    if (!unit || !unit->featureBuffer.normalBuffer || unit->featureBuffer.width <= 0 ||
        unit->featureBuffer.height <= 0) {
        return 0u;
    }
    pixel_count = runtime_native_3d_render_unit_pixel_count_bytes(unit->featureBuffer.width,
                                                                 unit->featureBuffer.height);
    return runtime_native_3d_render_unit_u64_product(pixel_count, 3u * (uint64_t)sizeof(float)) +
           runtime_native_3d_render_unit_u64_product(pixel_count, 4u * (uint64_t)sizeof(float)) +
           runtime_native_3d_render_unit_u64_product(pixel_count, 2u * (uint64_t)sizeof(unsigned char)) +
           runtime_native_3d_render_unit_u64_product(pixel_count, 2u * (uint64_t)sizeof(int));
}

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

void RuntimeNative3DRenderUnit_TakeReusable(RuntimeNative3DRenderUnit* unit) {
    if (!unit) return;
    if (!runtime_native_3d_render_unit_scratch_reuse_disabled() &&
        s_runtime_native_3d_render_unit_scratch_cache_count > 0u) {
        const size_t slot = s_runtime_native_3d_render_unit_scratch_cache_count - 1u;
        *unit = s_runtime_native_3d_render_unit_scratch_cache[slot];
        memset(&s_runtime_native_3d_render_unit_scratch_cache[slot],
               0,
               sizeof(s_runtime_native_3d_render_unit_scratch_cache[slot]));
        s_runtime_native_3d_render_unit_scratch_cache_count -= 1u;
        runtime_native_3d_render_unit_reset_scratch_counters(unit);
        unit->scratchCacheAcquireHits = 1u;
        return;
    }

    RuntimeNative3DRenderUnit_Init(unit);
    runtime_native_3d_render_unit_reset_scratch_counters(unit);
    unit->scratchCacheAcquireMisses = 1u;
}

void RuntimeNative3DRenderUnit_ReturnReusable(RuntimeNative3DRenderUnit* unit) {
    RuntimeNative3DRenderUnit* resized_cache = NULL;
    if (!unit) return;
    unit->frame = NULL;
    unit->featuresPrepared = false;
    unit->committedSubpasses = 0;

    if (runtime_native_3d_render_unit_scratch_reuse_disabled()) {
        RuntimeNative3DRenderUnit_Free(unit);
        return;
    }
    if (s_runtime_native_3d_render_unit_scratch_cache_count >=
        s_runtime_native_3d_render_unit_scratch_cache_capacity) {
        size_t next_capacity =
            s_runtime_native_3d_render_unit_scratch_cache_capacity == 0u
                ? 8u
                : s_runtime_native_3d_render_unit_scratch_cache_capacity * 2u;
        if (next_capacity < s_runtime_native_3d_render_unit_scratch_cache_count + 1u) {
            next_capacity = s_runtime_native_3d_render_unit_scratch_cache_count + 1u;
        }
        resized_cache = (RuntimeNative3DRenderUnit*)realloc(
            s_runtime_native_3d_render_unit_scratch_cache,
            next_capacity * sizeof(*s_runtime_native_3d_render_unit_scratch_cache));
        if (!resized_cache) {
            RuntimeNative3DRenderUnit_Free(unit);
            return;
        }
        memset(resized_cache + s_runtime_native_3d_render_unit_scratch_cache_capacity,
               0,
               (next_capacity - s_runtime_native_3d_render_unit_scratch_cache_capacity) *
                   sizeof(*resized_cache));
        s_runtime_native_3d_render_unit_scratch_cache = resized_cache;
        s_runtime_native_3d_render_unit_scratch_cache_capacity = next_capacity;
    }
    s_runtime_native_3d_render_unit_scratch_cache
        [s_runtime_native_3d_render_unit_scratch_cache_count++] = *unit;
    memset(unit, 0, sizeof(*unit));
}

void RuntimeNative3DRenderUnit_ReleaseReusableScratchCache(void) {
    if (s_runtime_native_3d_render_unit_scratch_cache) {
        for (size_t i = 0; i < s_runtime_native_3d_render_unit_scratch_cache_count; ++i) {
            RuntimeNative3DRenderUnit_Free(&s_runtime_native_3d_render_unit_scratch_cache[i]);
        }
    }
    free(s_runtime_native_3d_render_unit_scratch_cache);
    s_runtime_native_3d_render_unit_scratch_cache = NULL;
    s_runtime_native_3d_render_unit_scratch_cache_count = 0u;
    s_runtime_native_3d_render_unit_scratch_cache_capacity = 0u;
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
    uint64_t requested_radiance_bytes = 0u;

    if (!unit || !frame || !frame->valid || start_x >= end_x || start_y >= end_y) {
        return false;
    }

    unit->scratchSetupCalls += 1u;
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
    requested_radiance_bytes = runtime_native_3d_render_unit_u64_product(
        runtime_native_3d_render_unit_u64_product((uint64_t)pixel_count,
                                                  (uint64_t)sizeof(*unit->subpassRadiance)),
        2u);
    if (requested_radiance_bytes > unit->radianceScratchRequestedBytesMax) {
        unit->radianceScratchRequestedBytesMax = requested_radiance_bytes;
    }
    if (unit->radianceCapacity < pixel_count || !unit->subpassRadiance ||
        !unit->resolvedRadiance) {
        free(unit->subpassRadiance);
        free(unit->resolvedRadiance);
        unit->subpassRadiance = (float*)calloc(pixel_count, sizeof(*unit->subpassRadiance));
        unit->resolvedRadiance = (float*)calloc(pixel_count, sizeof(*unit->resolvedRadiance));
        if (unit->subpassRadiance && unit->resolvedRadiance) {
            unit->radianceCapacity = pixel_count;
            unit->radianceScratchResizeCalls += 1u;
        } else {
            free(unit->subpassRadiance);
            free(unit->resolvedRadiance);
            unit->subpassRadiance = NULL;
            unit->resolvedRadiance = NULL;
            unit->radianceCapacity = 0u;
        }
    } else {
        unit->radianceScratchReuseCalls += 1u;
    }
    if (!unit->subpassRadiance || !unit->resolvedRadiance) {
        return false;
    }
    {
        const uint64_t capacity_bytes =
            runtime_native_3d_render_unit_radiance_capacity_bytes(unit);
        if (capacity_bytes > unit->radianceScratchCapacityBytesMax) {
            unit->radianceScratchCapacityBytesMax = capacity_bytes;
        }
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
    unit->radianceScratchClearBytes +=
        runtime_native_3d_render_unit_u64_product((uint64_t)pixel_count,
                                                  (uint64_t)sizeof(*unit->subpassRadiance));

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
        if (RuntimeNative3DAdaptiveSampling_RiskEarlyStopEnabled()) {
            stats.temporalAdaptiveEarlyStopBaseActivePixels =
                unit->adaptiveMask.conservativeEarlyStopBaseActivePixelCount;
            stats.temporalAdaptiveEarlyStopPaddingHoldPixels =
                unit->adaptiveMask.conservativeEarlyStopPaddingHoldPixelCount;
            stats.temporalAdaptiveEarlyStopPaddingHoldHighSeedPixels =
                unit->adaptiveMask.conservativeEarlyStopPaddingHoldHighSeedPixelCount;
            stats.temporalAdaptiveEarlyStopPaddingHoldMediumSeedPixels =
                unit->adaptiveMask.conservativeEarlyStopPaddingHoldMediumSeedPixelCount;
            stats.temporalAdaptiveEarlyStopActiveAfterPaddingPixels =
                unit->adaptiveMask.activePixelCount;
            for (int i = 0; i < RUNTIME_NATIVE_3D_ADAPTIVE_REGION_COUNT; ++i) {
                stats.temporalAdaptiveEarlyStopPaddingHoldRegionCounts[i] =
                    unit->adaptiveMask.conservativeEarlyStopPaddingHoldRegionCounts[i];
            }
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

static void runtime_native_3d_render_unit_budget_heatmap_color(
    const RuntimeNative3DAdaptivePixelState* pixel,
    int min_sample_floor,
    uint8_t* out_r,
    uint8_t* out_g,
    uint8_t* out_b) {
    const uint16_t flags = pixel ? pixel->flags : 0u;
    const uint16_t sample_count = pixel ? pixel->sampleCount : 0u;
    const bool safe_for_early_stop =
        RuntimeNative3DAdaptiveSampling_FlagsSafeForEarlyStop(flags);
    const int bucket =
        RuntimeNative3DAdaptiveSampling_BudgetBucket(sample_count, min_sample_floor);

    if (!pixel || sample_count == 0u) {
        *out_r = 34u;
        *out_g = 34u;
        *out_b = 34u;
        return;
    }

    if (!safe_for_early_stop) {
        if (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_TRANSPARENT_RISK) {
            *out_r = 196u;
            *out_g = 72u;
            *out_b = 255u;
            return;
        }
        if (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_GEOMETRY_EDGE_RISK) {
            *out_r = 245u;
            *out_g = 245u;
            *out_b = 245u;
            return;
        }
        if (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_DIRECT_LIGHT_RISK) {
            *out_r = 255u;
            *out_g = 220u;
            *out_b = 48u;
            return;
        }
        if (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVITY_RISK) {
            *out_r = 238u;
            *out_g = 94u;
            *out_b = 42u;
            return;
        }
        if (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_MATERIAL_RISK) {
            *out_r = 158u;
            *out_g = 86u;
            *out_b = 224u;
            return;
        }
        if (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_HIGH_RISK) {
            *out_r = 220u;
            *out_g = 44u;
            *out_b = 78u;
            return;
        }
    }

    switch (bucket) {
        case 0:
            *out_r = safe_for_early_stop ? 28u : 84u;
            *out_g = safe_for_early_stop ? 102u : 74u;
            *out_b = safe_for_early_stop ? 230u : 154u;
            break;
        case 1:
            *out_r = 44u;
            *out_g = 186u;
            *out_b = 124u;
            break;
        case 2:
            *out_r = 240u;
            *out_g = 170u;
            *out_b = 48u;
            break;
        default:
            *out_r = 220u;
            *out_g = 44u;
            *out_b = 78u;
            break;
    }
}

bool RuntimeNative3DRenderUnit_ResolveTemporalBudgetHeatmapToPixels(
    const RuntimeNative3DRenderUnit* unit,
    uint8_t* pixel_buffer,
    int pixel_width) {
    const RuntimeNative3DAdaptivePixelStateBuffer* state = NULL;
    int min_sample_floor = RUNTIME_NATIVE_3D_ADAPTIVE_MIN_SUBPASSES;

    if (!unit || !pixel_buffer || pixel_width <= 0 || unit->width <= 0 || unit->height <= 0) {
        return false;
    }
    state = &unit->adaptivePixelState;
    if (!state->pixels || state->width != unit->width || state->height != unit->height) {
        return false;
    }
    if (state->summary.minSampleFloor > 0) {
        min_sample_floor = state->summary.minSampleFloor;
    }

    for (int y = 0; y < unit->height; ++y) {
        for (int x = 0; x < unit->width; ++x) {
            const size_t local_index = (size_t)y * (size_t)unit->width + (size_t)x;
            const size_t pixel_index =
                ((size_t)(unit->startY + y) * (size_t)pixel_width +
                 (size_t)(unit->startX + x)) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            uint8_t r = 0u;
            uint8_t g = 0u;
            uint8_t b = 0u;
            runtime_native_3d_render_unit_budget_heatmap_color(&state->pixels[local_index],
                                                               min_sample_floor,
                                                               &r,
                                                               &g,
                                                               &b);
            pixel_buffer[pixel_index] = r;
            pixel_buffer[pixel_index + 1u] = g;
            pixel_buffer[pixel_index + 2u] = b;
            pixel_buffer[pixel_index + 3u] = 255u;
        }
    }
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

void RuntimeNative3DRenderUnit_RecordScratchStats(const RuntimeNative3DRenderUnit* unit,
                                                  RuntimeNative3DRenderStats* out_stats) {
    uint64_t radiance_bytes = 0u;
    uint64_t temporal_bytes = 0u;
    uint64_t adaptive_mask_bytes = 0u;
    uint64_t adaptive_state_bytes = 0u;
    uint64_t feature_bytes = 0u;
    uint64_t total_bytes = 0u;

    if (!unit || !out_stats) return;

    radiance_bytes = runtime_native_3d_render_unit_radiance_capacity_bytes(unit);
    temporal_bytes = runtime_native_3d_render_unit_temporal_capacity_bytes(unit);
    adaptive_mask_bytes = runtime_native_3d_render_unit_adaptive_mask_capacity_bytes(unit);
    adaptive_state_bytes = runtime_native_3d_render_unit_adaptive_state_capacity_bytes(unit);
    feature_bytes = runtime_native_3d_render_unit_feature_capacity_bytes(unit);
    total_bytes = radiance_bytes + temporal_bytes + adaptive_mask_bytes +
                  adaptive_state_bytes + feature_bytes;

    out_stats->renderUnitScratchOwnerCount += 1u;
    out_stats->renderUnitScratchSetupCalls += unit->scratchSetupCalls;
    out_stats->renderUnitScratchCacheAcquireHits += unit->scratchCacheAcquireHits;
    out_stats->renderUnitScratchCacheAcquireMisses += unit->scratchCacheAcquireMisses;
    out_stats->renderUnitRadianceScratchResizeCalls += unit->radianceScratchResizeCalls;
    out_stats->renderUnitRadianceScratchReuseCalls += unit->radianceScratchReuseCalls;
    out_stats->renderUnitRadianceScratchClearBytes += unit->radianceScratchClearBytes;
    if (unit->radianceScratchRequestedBytesMax >
        out_stats->renderUnitRadianceScratchRequestedBytesMax) {
        out_stats->renderUnitRadianceScratchRequestedBytesMax =
            unit->radianceScratchRequestedBytesMax;
    }
    if (unit->radianceScratchCapacityBytesMax >
        out_stats->renderUnitRadianceScratchCapacityBytesMax) {
        out_stats->renderUnitRadianceScratchCapacityBytesMax =
            unit->radianceScratchCapacityBytesMax;
    }
    if (temporal_bytes > out_stats->renderUnitTemporalScratchCapacityBytesMax) {
        out_stats->renderUnitTemporalScratchCapacityBytesMax = temporal_bytes;
    }
    if (adaptive_mask_bytes > out_stats->renderUnitAdaptiveMaskScratchCapacityBytesMax) {
        out_stats->renderUnitAdaptiveMaskScratchCapacityBytesMax = adaptive_mask_bytes;
    }
    if (adaptive_state_bytes > out_stats->renderUnitAdaptiveStateScratchCapacityBytesMax) {
        out_stats->renderUnitAdaptiveStateScratchCapacityBytesMax = adaptive_state_bytes;
    }
    if (feature_bytes > out_stats->renderUnitFeatureScratchCapacityBytesMax) {
        out_stats->renderUnitFeatureScratchCapacityBytesMax = feature_bytes;
    }
    out_stats->renderUnitScratchOwnedBytes += total_bytes;
    if (total_bytes > out_stats->renderUnitScratchMaxOwnerBytes) {
        out_stats->renderUnitScratchMaxOwnerBytes = total_bytes;
    }
    if (out_stats->renderUnitScratchOwnedBytes >
        out_stats->renderUnitScratchMaxFrameOwnedBytes) {
        out_stats->renderUnitScratchMaxFrameOwnedBytes =
            out_stats->renderUnitScratchOwnedBytes;
    }
}
