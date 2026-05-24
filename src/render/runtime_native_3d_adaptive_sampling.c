#include "render/runtime_native_3d_adaptive_sampling.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_ray_3d.h"

static const float kRuntimeNative3DAdaptiveActivityThreshold = 0.05f;
static const float kRuntimeNative3DAdaptiveMinimumActivityThreshold = 0.008f;
static const float kRuntimeNative3DAdaptiveRiskHoldMedium = 0.35f;
static const float kRuntimeNative3DAdaptiveRiskHoldHigh = 0.70f;
static const float kRuntimeNative3DAdaptiveNormalEdgeDotThreshold = 0.92f;
static const float kRuntimeNative3DAdaptiveDepthEdgeRatioThreshold = 0.08f;
static const float kRuntimeNative3DAdaptiveTileRiskInfluence = 0.5f;
static const float kRuntimeNative3DAdaptiveHighActivityScale = 1.75f;
static const int kRuntimeNative3DAdaptiveNeighborhoodRadiusMedium = 1;
static const int kRuntimeNative3DAdaptiveNeighborhoodRadiusHigh = 2;

bool RuntimeNative3DAdaptiveSampling_RuntimeEnabled(void) {
    return false;
}

static float runtime_native_3d_adaptive_sampling_clampf(float value,
                                                        float min_value,
                                                        float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float runtime_native_3d_adaptive_sampling_compute_feature_risk(
    const RuntimeNative3DFeatureBuffer* features,
    int x,
    int y,
    int width,
    int height) {
    const size_t pixel_index = (size_t)y * (size_t)width + (size_t)x;
    const size_t normal_base = pixel_index * 3u;
    float risk = 0.0f;

    if (!features || !features->hitMaskBuffer || !features->depthBuffer || !features->normalBuffer ||
        !features->reflectivityBuffer || !features->roughnessBuffer || !features->transparencyBuffer ||
        features->width != width || features->height != height) {
        return 0.0f;
    }
    if (!features->hitMaskBuffer[pixel_index]) {
        return 0.0f;
    }

    {
        const float reflectivity = runtime_native_3d_adaptive_sampling_clampf(
            features->reflectivityBuffer[pixel_index], 0.0f, 1.0f);
        const float roughness = runtime_native_3d_adaptive_sampling_clampf(
            features->roughnessBuffer[pixel_index], 0.0f, 1.0f);
        const float transparency = runtime_native_3d_adaptive_sampling_clampf(
            features->transparencyBuffer[pixel_index], 0.0f, 1.0f);
        const float gloss_risk = reflectivity * (1.0f - roughness);
        risk = fmaxf(risk, gloss_risk);
        risk = fmaxf(risk, transparency);
    }

    if (x + 1 < width) {
        const size_t neighbor_index = pixel_index + 1u;
        if (features->hitMaskBuffer[neighbor_index] != features->hitMaskBuffer[pixel_index]) {
            risk = 1.0f;
        } else if (features->hitMaskBuffer[neighbor_index]) {
            const size_t neighbor_normal_base = neighbor_index * 3u;
            const float dot =
                features->normalBuffer[normal_base] * features->normalBuffer[neighbor_normal_base] +
                features->normalBuffer[normal_base + 1u] *
                    features->normalBuffer[neighbor_normal_base + 1u] +
                features->normalBuffer[normal_base + 2u] *
                    features->normalBuffer[neighbor_normal_base + 2u];
            const float depth = features->depthBuffer[pixel_index];
            const float neighbor_depth = features->depthBuffer[neighbor_index];
            const float depth_delta = fabsf(neighbor_depth - depth);
            const float depth_scale = fmaxf(fmaxf(depth, neighbor_depth), 1.0e-4f);
            if (dot < kRuntimeNative3DAdaptiveNormalEdgeDotThreshold) {
                risk = fmaxf(risk, 0.85f);
            }
            if (depth_delta / depth_scale > kRuntimeNative3DAdaptiveDepthEdgeRatioThreshold) {
                risk = fmaxf(risk, 0.85f);
            }
        }
    }
    if (y + 1 < height) {
        const size_t neighbor_index = pixel_index + (size_t)width;
        if (features->hitMaskBuffer[neighbor_index] != features->hitMaskBuffer[pixel_index]) {
            risk = 1.0f;
        } else if (features->hitMaskBuffer[neighbor_index]) {
            const size_t neighbor_normal_base = neighbor_index * 3u;
            const float dot =
                features->normalBuffer[normal_base] * features->normalBuffer[neighbor_normal_base] +
                features->normalBuffer[normal_base + 1u] *
                    features->normalBuffer[neighbor_normal_base + 1u] +
                features->normalBuffer[normal_base + 2u] *
                    features->normalBuffer[neighbor_normal_base + 2u];
            const float depth = features->depthBuffer[pixel_index];
            const float neighbor_depth = features->depthBuffer[neighbor_index];
            const float depth_delta = fabsf(neighbor_depth - depth);
            const float depth_scale = fmaxf(fmaxf(depth, neighbor_depth), 1.0e-4f);
            if (dot < kRuntimeNative3DAdaptiveNormalEdgeDotThreshold) {
                risk = fmaxf(risk, 0.85f);
            }
            if (depth_delta / depth_scale > kRuntimeNative3DAdaptiveDepthEdgeRatioThreshold) {
                risk = fmaxf(risk, 0.85f);
            }
        }
    }

    return runtime_native_3d_adaptive_sampling_clampf(risk, 0.0f, 1.0f);
}

static bool runtime_native_3d_adaptive_sampling_is_stable_emitter_pixel(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y) {
    RuntimeLightEmitterTrace3DResult trace = {0};
    RuntimeMaterialPayload3D payload = {0};
    Ray3D primary_ray = {0};

    if (!scene || !projector) return false;

    primary_ray = RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
    if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                               &primary_ray,
                                               projector->nearPlane,
                                               1.0e30,
                                               &trace)) {
        return false;
    }
    if (trace.emitterWins) {
        return true;
    }
    if (!trace.geometryHit) {
        return false;
    }
    if (!RuntimeMaterialPayload3D_ResolveFromHit(&trace.geometryHitInfo, &payload)) {
        return false;
    }
    return payload.valid && payload.emissive > 0.0 && payload.transparency <= 0.0;
}

void RuntimeNative3DAdaptiveSamplingMask_Init(RuntimeNative3DAdaptiveSamplingMask* mask) {
    if (!mask) return;
    memset(mask, 0, sizeof(*mask));
}

void RuntimeNative3DAdaptiveSamplingMask_Free(RuntimeNative3DAdaptiveSamplingMask* mask) {
    if (!mask) return;
    free(mask->stableEmitterMask);
    free(mask->activeSampleMask);
    free(mask->scratchSampleMask);
    free(mask->activeTileMask);
    memset(mask, 0, sizeof(*mask));
}

bool RuntimeNative3DAdaptiveSamplingMask_Ensure(RuntimeNative3DAdaptiveSamplingMask* mask,
                                                int width,
                                                int height) {
    uint8_t* stable = NULL;
    uint8_t* active = NULL;
    uint8_t* scratch = NULL;
    size_t count = 0;

    if (!mask || width <= 0 || height <= 0) return false;
    if (mask->stableEmitterMask && mask->activeSampleMask && mask->scratchSampleMask &&
        mask->width == width && mask->height == height) {
        return true;
    }

    count = (size_t)width * (size_t)height;
    stable = (uint8_t*)calloc(count, sizeof(*stable));
    active = (uint8_t*)calloc(count, sizeof(*active));
    scratch = (uint8_t*)calloc(count, sizeof(*scratch));
    if (!stable || !active || !scratch) {
        free(stable);
        free(active);
        free(scratch);
        return false;
    }

    free(mask->stableEmitterMask);
    free(mask->activeSampleMask);
    free(mask->scratchSampleMask);
    mask->stableEmitterMask = stable;
    mask->activeSampleMask = active;
    mask->scratchSampleMask = scratch;
    mask->width = width;
    mask->height = height;
    return true;
}

void RuntimeNative3DAdaptiveSamplingMask_Clear(RuntimeNative3DAdaptiveSamplingMask* mask) {
    size_t count = 0;
    if (!mask || !mask->stableEmitterMask || !mask->activeSampleMask ||
        mask->width <= 0 || mask->height <= 0) {
        return;
    }
    count = (size_t)mask->width * (size_t)mask->height;
    memset(mask->stableEmitterMask, 0, count * sizeof(*mask->stableEmitterMask));
    memset(mask->activeSampleMask, 0, count * sizeof(*mask->activeSampleMask));
    if (mask->scratchSampleMask) {
        memset(mask->scratchSampleMask, 0, count * sizeof(*mask->scratchSampleMask));
    }
    mask->activePixelCount = 0;
    mask->activeTileCount = 0;
    mask->inactiveTileCount = 0;
}

bool RuntimeNative3DAdaptiveSampling_ShouldUse(RayTracing3DIntegratorId integrator_id,
                                               int temporal_frames) {
    return RuntimeNative3DAdaptiveSampling_RuntimeEnabled() &&
           temporal_frames > 1 &&
           (integrator_id == RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY ||
            integrator_id == RAY_TRACING_3D_INTEGRATOR_DISNEY);
}

static int runtime_native_3d_adaptive_sampling_compute_tiles(int extent, int tile_size) {
    if (extent <= 0 || tile_size <= 0) return 0;
    return (extent + tile_size - 1) / tile_size;
}

static bool runtime_native_3d_adaptive_sampling_ensure_tile_mask(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    int tile_count) {
    uint8_t* resized_tiles = NULL;
    size_t count = 0;

    if (!mask || tile_count <= 0) return false;
    if (mask->activeTileMask && mask->tilesX * mask->tilesY == tile_count) {
        return true;
    }

    count = (size_t)tile_count;
    resized_tiles = (uint8_t*)calloc(count, sizeof(*resized_tiles));
    if (!resized_tiles) return false;

    free(mask->activeTileMask);
    mask->activeTileMask = resized_tiles;
    return true;
}

static int runtime_native_3d_adaptive_sampling_extra_hold_subpasses(float risk) {
    if (risk >= kRuntimeNative3DAdaptiveRiskHoldHigh) return 2;
    if (risk >= kRuntimeNative3DAdaptiveRiskHoldMedium) return 1;
    return 0;
}

static float runtime_native_3d_adaptive_sampling_threshold_for_risk(float risk) {
    const float threshold =
        kRuntimeNative3DAdaptiveActivityThreshold -
        ((kRuntimeNative3DAdaptiveActivityThreshold -
          kRuntimeNative3DAdaptiveMinimumActivityThreshold) *
         risk);
    return runtime_native_3d_adaptive_sampling_clampf(threshold,
                                                      kRuntimeNative3DAdaptiveMinimumActivityThreshold,
                                                      kRuntimeNative3DAdaptiveActivityThreshold);
}

static uint8_t runtime_native_3d_adaptive_sampling_seed_strength(
    float pixel_activity,
    float pixel_threshold,
    float pixel_risk) {
    if (pixel_risk >= kRuntimeNative3DAdaptiveRiskHoldHigh ||
        pixel_activity > pixel_threshold * kRuntimeNative3DAdaptiveHighActivityScale) {
        return 2u;
    }
    if (pixel_risk >= kRuntimeNative3DAdaptiveRiskHoldMedium ||
        pixel_activity > pixel_threshold) {
        return 1u;
    }
    return 0u;
}

static bool runtime_native_3d_adaptive_sampling_has_neighbor_seed(
    const RuntimeNative3DAdaptiveSamplingMask* mask,
    int x,
    int y,
    uint8_t minimum_seed,
    int radius) {
    const int start_y = (y - radius > 0) ? (y - radius) : 0;
    const int end_y = (y + radius + 1 < mask->height) ? (y + radius + 1) : mask->height;
    const int start_x = (x - radius > 0) ? (x - radius) : 0;
    const int end_x = (x + radius + 1 < mask->width) ? (x + radius + 1) : mask->width;

    for (int neighbor_y = start_y; neighbor_y < end_y; ++neighbor_y) {
        for (int neighbor_x = start_x; neighbor_x < end_x; ++neighbor_x) {
            const size_t neighbor_index =
                (size_t)neighbor_y * (size_t)mask->width + (size_t)neighbor_x;
            if (mask->scratchSampleMask[neighbor_index] >= minimum_seed) {
                return true;
            }
        }
    }

    return false;
}

bool RuntimeNative3DAdaptiveSampling_BuildStableEmitterMask(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    if (!mask || !mask->stableEmitterMask || !mask->activeSampleMask || !scene || !projector) {
        return false;
    }
    if (start_x >= end_x || start_y >= end_y ||
        end_x - start_x != mask->width ||
        end_y - start_y != mask->height) {
        return false;
    }

    RuntimeNative3DAdaptiveSamplingMask_Clear(mask);
    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        for (int x = start_x; x < end_x; ++x) {
            const int local_x = x - start_x;
            const size_t idx = (size_t)local_y * (size_t)mask->width + (size_t)local_x;
            const bool stable = runtime_native_3d_adaptive_sampling_is_stable_emitter_pixel(
                scene,
                projector,
                (double)x,
                (double)y);
            mask->stableEmitterMask[idx] = stable ? 1u : 0u;
            mask->activeSampleMask[idx] = stable ? 0u : 1u;
            mask->activePixelCount += stable ? 0 : 1;
        }
    }
    return true;
}

bool RuntimeNative3DAdaptiveSampling_BeginTemporalActivityMask(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    int width,
    int height,
    int tile_size,
    int min_subpasses_before_prune) {
    size_t pixel_count = 0;
    const int tiles_x = runtime_native_3d_adaptive_sampling_compute_tiles(width, tile_size);
    const int tiles_y = runtime_native_3d_adaptive_sampling_compute_tiles(height, tile_size);
    const int tile_count = tiles_x * tiles_y;

    if (!mask || width <= 0 || height <= 0 || tile_size <= 0 || min_subpasses_before_prune < 1) {
        return false;
    }
    if (!RuntimeNative3DAdaptiveSamplingMask_Ensure(mask, width, height) ||
        !runtime_native_3d_adaptive_sampling_ensure_tile_mask(mask, tile_count)) {
        return false;
    }

    pixel_count = (size_t)width * (size_t)height;
    memset(mask->stableEmitterMask, 0, pixel_count * sizeof(*mask->stableEmitterMask));
    memset(mask->activeSampleMask, 1, pixel_count * sizeof(*mask->activeSampleMask));
    memset(mask->activeTileMask, 1, (size_t)tile_count * sizeof(*mask->activeTileMask));
    mask->tileSize = tile_size;
    mask->tilesX = tiles_x;
    mask->tilesY = tiles_y;
    mask->minSubpassesBeforePrune = min_subpasses_before_prune;
    mask->activePixelCount = width * height;
    mask->activeTileCount = tile_count;
    mask->inactiveTileCount = 0;
    return true;
}

bool RuntimeNative3DAdaptiveSampling_RefreshTemporalActivityMask(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    const RuntimeNative3DTemporalAccumulation* accumulation,
    const RuntimeNative3DFeatureBuffer* features) {
    if (!mask || !mask->activeSampleMask || !mask->scratchSampleMask || !mask->activeTileMask ||
        !accumulation || !accumulation->activityBuffer || accumulation->width != mask->width ||
        accumulation->height != mask->height || mask->tileSize <= 0 ||
        mask->tilesX <= 0 || mask->tilesY <= 0) {
        return false;
    }

    mask->activePixelCount = 0;
    mask->activeTileCount = 0;
    mask->inactiveTileCount = 0;

    if (accumulation->completedSubpasses < mask->minSubpassesBeforePrune) {
        const size_t pixel_count = (size_t)mask->width * (size_t)mask->height;
        const size_t tile_count = (size_t)mask->tilesX * (size_t)mask->tilesY;
        memset(mask->activeSampleMask, 1, pixel_count * sizeof(*mask->activeSampleMask));
        memset(mask->scratchSampleMask, 0, pixel_count * sizeof(*mask->scratchSampleMask));
        memset(mask->activeTileMask, 1, tile_count * sizeof(*mask->activeTileMask));
        mask->activePixelCount = mask->width * mask->height;
        mask->activeTileCount = (int)tile_count;
        return true;
    }

    memset(mask->activeSampleMask,
           0,
           (size_t)mask->width * (size_t)mask->height * sizeof(*mask->activeSampleMask));
    memset(mask->scratchSampleMask,
           0,
           (size_t)mask->width * (size_t)mask->height * sizeof(*mask->scratchSampleMask));

    for (int tile_y = 0; tile_y < mask->tilesY; ++tile_y) {
        for (int tile_x = 0; tile_x < mask->tilesX; ++tile_x) {
            const int start_x = tile_x * mask->tileSize;
            const int start_y = tile_y * mask->tileSize;
            const int end_x = (start_x + mask->tileSize < mask->width)
                                  ? (start_x + mask->tileSize)
                                  : mask->width;
            const int end_y = (start_y + mask->tileSize < mask->height)
                                  ? (start_y + mask->tileSize)
                                  : mask->height;
            float tile_peak_activity = 0.0f;
            float tile_peak_risk = 0.0f;

            for (int y = start_y; y < end_y; ++y) {
                for (int x = start_x; x < end_x; ++x) {
                    const size_t pixel_index = (size_t)y * (size_t)mask->width + (size_t)x;
                    const float activity = accumulation->activityBuffer[pixel_index];
                    if (activity > tile_peak_activity) {
                        tile_peak_activity = activity;
                    }
                    if (features) {
                        const float pixel_risk =
                            runtime_native_3d_adaptive_sampling_compute_feature_risk(features,
                                                                                    x,
                                                                                    y,
                                                                                    mask->width,
                                                                                    mask->height);
                        if (pixel_risk > tile_peak_risk) {
                            tile_peak_risk = pixel_risk;
                        }
                    }
                }
            }

            for (int y = start_y; y < end_y; ++y) {
                for (int x = start_x; x < end_x; ++x) {
                    const size_t pixel_index = (size_t)y * (size_t)mask->width + (size_t)x;
                    const float pixel_activity = accumulation->activityBuffer[pixel_index];
                    const float pixel_risk =
                        features ? runtime_native_3d_adaptive_sampling_compute_feature_risk(
                                       features,
                                       x,
                                       y,
                                       mask->width,
                                       mask->height)
                                 : 0.0f;
                    const float effective_risk = runtime_native_3d_adaptive_sampling_clampf(
                        fmaxf(pixel_risk, tile_peak_risk * kRuntimeNative3DAdaptiveTileRiskInfluence),
                        0.0f,
                        1.0f);
                    const int min_subpasses =
                        mask->minSubpassesBeforePrune +
                        runtime_native_3d_adaptive_sampling_extra_hold_subpasses(effective_risk);
                    const float pixel_threshold =
                        runtime_native_3d_adaptive_sampling_threshold_for_risk(effective_risk);
                    const bool pixel_active =
                        accumulation->completedSubpasses < min_subpasses ||
                        pixel_activity > pixel_threshold;
                    mask->activeSampleMask[pixel_index] = pixel_active ? 1u : 0u;
                    if (pixel_active) {
                        mask->activePixelCount += 1;
                    }
                    mask->scratchSampleMask[pixel_index] =
                        pixel_active ? runtime_native_3d_adaptive_sampling_seed_strength(
                                           pixel_activity,
                                           pixel_threshold,
                                           effective_risk)
                                     : 0u;
                }
            }
        }
    }

    for (int y = 0; y < mask->height; ++y) {
        for (int x = 0; x < mask->width; ++x) {
            const size_t pixel_index = (size_t)y * (size_t)mask->width + (size_t)x;
            bool pixel_active = mask->activeSampleMask[pixel_index] != 0u;
            if (!pixel_active) {
                if (runtime_native_3d_adaptive_sampling_has_neighbor_seed(
                        mask,
                        x,
                        y,
                        2u,
                        kRuntimeNative3DAdaptiveNeighborhoodRadiusHigh) ||
                    runtime_native_3d_adaptive_sampling_has_neighbor_seed(
                        mask,
                        x,
                        y,
                        1u,
                        kRuntimeNative3DAdaptiveNeighborhoodRadiusMedium)) {
                    mask->activeSampleMask[pixel_index] = 1u;
                    mask->activePixelCount += 1;
                }
            }
        }
    }

    for (int tile_y = 0; tile_y < mask->tilesY; ++tile_y) {
        for (int tile_x = 0; tile_x < mask->tilesX; ++tile_x) {
            const int start_x = tile_x * mask->tileSize;
            const int start_y = tile_y * mask->tileSize;
            const int end_x = (start_x + mask->tileSize < mask->width)
                                  ? (start_x + mask->tileSize)
                                  : mask->width;
            const int end_y = (start_y + mask->tileSize < mask->height)
                                  ? (start_y + mask->tileSize)
                                  : mask->height;
            const size_t tile_index = (size_t)tile_y * (size_t)mask->tilesX + (size_t)tile_x;
            bool tile_active = false;

            for (int y = start_y; !tile_active && y < end_y; ++y) {
                for (int x = start_x; x < end_x; ++x) {
                    const size_t pixel_index = (size_t)y * (size_t)mask->width + (size_t)x;
                    if (mask->activeSampleMask[pixel_index]) {
                        tile_active = true;
                        break;
                    }
                }
            }

            mask->activeTileMask[tile_index] = tile_active ? 1u : 0u;
            if (tile_active) {
                mask->activeTileCount += 1;
            } else {
                mask->inactiveTileCount += 1;
            }
        }
    }

    return true;
}

bool RuntimeNative3DAdaptiveSampling_HasActiveSamples(
    const RuntimeNative3DAdaptiveSamplingMask* mask) {
    size_t count = 0;
    if (!mask || !mask->activeSampleMask || mask->width <= 0 || mask->height <= 0) {
        return false;
    }
    count = (size_t)mask->width * (size_t)mask->height;
    for (size_t i = 0; i < count; ++i) {
        if (mask->activeSampleMask[i]) {
            return true;
        }
    }
    return false;
}

bool RuntimeNative3DAdaptiveSampling_RenderPreparedRegionRadianceRGBMasked(
    float* radiance_buffer,
    int radiance_stride,
    RayTracing3DIntegratorId integrator_id,
    const RuntimeNative3DPreparedFrame* frame,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const uint8_t* active_mask,
    int active_mask_stride,
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderStats stats = {0};

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!active_mask) {
        return RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance_buffer,
                                                              radiance_stride,
                                                              integrator_id,
                                                              frame,
                                                              start_x,
                                                              start_y,
                                                              end_x,
                                                              end_y,
                                                              out_stats);
    }
    if (!radiance_buffer || radiance_stride <= 0 || active_mask_stride <= 0 ||
        !frame || !frame->valid || start_x >= end_x || start_y >= end_y) {
        return false;
    }

    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        int run_start = -1;
        for (int x = start_x; x <= end_x; ++x) {
            const int local_x = x - start_x;
            const bool active =
                x < end_x &&
                active_mask[(size_t)local_y * (size_t)active_mask_stride +
                            (size_t)local_x] != 0u;
            if (active && run_start < 0) {
                run_start = x;
            }
            if ((!active || x == end_x) && run_start >= 0) {
                RuntimeNative3DRenderStats run_stats = {0};
                float* run_buffer = radiance_buffer +
                                    (((size_t)local_y * (size_t)radiance_stride +
                                      (size_t)(run_start - start_x)) *
                                     RUNTIME_NATIVE_3D_RADIANCE_CHANNELS);
                if (!RuntimeNative3DRenderPreparedRegionRadianceRGB(run_buffer,
                                                                    radiance_stride,
                                                                    integrator_id,
                                                                    frame,
                                                                    run_start,
                                                                    y,
                                                                    x,
                                                                    y + 1,
                                                                    &run_stats)) {
                    return false;
                }
                RuntimeNative3DRenderStats_Accumulate(&stats, &run_stats);
                run_start = -1;
            }
        }
    }

    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}
