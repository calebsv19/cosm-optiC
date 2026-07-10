#include "runtime_native_3d_adaptive_sampling_internal.h"

#include <string.h>

static uint8_t runtime_native_3d_adaptive_sampling_early_stop_seed_from_flags(
    uint16_t flags,
    bool active) {
    const uint16_t high_seed_flags =
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_HIGH_RISK |
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_MATERIAL_RISK |
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_TRANSPARENT_RISK |
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_GEOMETRY_EDGE_RISK |
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_DIRECT_LIGHT_RISK;
    if ((flags & high_seed_flags) != 0u) {
        return 2u;
    }
    return (active && (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVITY_RISK) != 0u) ? 1u : 0u;
}

bool runtime_native_3d_adaptive_sampling_flags_safe_for_early_stop(uint16_t flags) {
    const uint16_t hold_flags =
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_PROBE |
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_HIGH_RISK |
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVITY_RISK |
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_MATERIAL_RISK |
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_TRANSPARENT_RISK |
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_GEOMETRY_EDGE_RISK |
        RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_DIRECT_LIGHT_RISK;
    return (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_STABLE) != 0u &&
           (flags & hold_flags) == 0u;
}

bool RuntimeNative3DAdaptiveSampling_FlagsSafeForEarlyStop(uint16_t flags) {
    return runtime_native_3d_adaptive_sampling_flags_safe_for_early_stop(flags);
}

int runtime_native_3d_adaptive_sampling_budget_bucket(uint16_t sample_count,
                                                      int min_sample_floor) {
    const int floor = (min_sample_floor > 0) ? min_sample_floor : 2;
    if ((int)sample_count <= floor) return 0;
    if (sample_count <= 4u) return 1;
    if (sample_count <= 8u) return 2;
    return 3;
}

int RuntimeNative3DAdaptiveSampling_BudgetBucket(uint16_t sample_count,
                                                 int min_sample_floor) {
    return runtime_native_3d_adaptive_sampling_budget_bucket(sample_count, min_sample_floor);
}

void runtime_native_3d_adaptive_sampling_note_early_stop_hold_flags(
    RuntimeNative3DAdaptivePixelStateSummary* summary,
    uint16_t flags,
    int region_index,
    int budget_bucket,
    RuntimeNative3DDirectLightVisibilityOutcome visibility) {
    const bool safe_for_early_stop =
        runtime_native_3d_adaptive_sampling_flags_safe_for_early_stop(flags);
    if (!summary) return;
    if (region_index < 0 || region_index >= RUNTIME_NATIVE_3D_ADAPTIVE_REGION_COUNT) {
        region_index = 0;
    }
    if (budget_bucket < 0 || budget_bucket >= RUNTIME_NATIVE_3D_TEMPORAL_BUDGET_BUCKET_COUNT) {
        budget_bucket = 0;
    }

    summary->budgetBucketPixelCounts[budget_bucket] += 1;
    summary->budgetActiveBucketPixelCounts[budget_bucket] +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVE) ? 1 : 0;

    if (safe_for_early_stop) {
        summary->earlyStopEligiblePixelCount += 1;
        summary->earlyStopEligibleRegionCounts[region_index] += 1;
        summary->budgetEligibleBucketPixelCounts[budget_bucket] += 1;
        summary->budgetClearVisibleEligiblePixelCount +=
            (visibility == RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_CLEAR_VISIBLE) ? 1 : 0;
        return;
    }
    summary->earlyStopHeldPixelCount += 1;
    summary->earlyStopHeldRegionCounts[region_index] += 1;
    summary->budgetHeldBucketPixelCounts[budget_bucket] += 1;
    summary->budgetClearVisibleHeldPixelCount +=
        (visibility == RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_CLEAR_VISIBLE) ? 1 : 0;
    summary->budgetPartialHeldPixelCount +=
        (visibility == RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_STABLE_PARTIAL ||
         visibility == RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_MIXED_PARTIAL)
            ? 1
            : 0;
    summary->budgetTransparentHeldPixelCount +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_TRANSPARENT_RISK) ? 1 : 0;
    summary->budgetGeometryHeldPixelCount +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_GEOMETRY_EDGE_RISK) ? 1 : 0;
    summary->budgetActivityHeldPixelCount +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVITY_RISK) ? 1 : 0;
    summary->earlyStopHoldProbePixelCount +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_PROBE) ? 1 : 0;
    summary->earlyStopHoldHighRiskPixelCount +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_HIGH_RISK) ? 1 : 0;
    summary->earlyStopHoldActivityRiskPixelCount +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVITY_RISK) ? 1 : 0;
    summary->earlyStopHoldMaterialRiskPixelCount +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_MATERIAL_RISK) ? 1 : 0;
    summary->earlyStopHoldTransparentRiskPixelCount +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_TRANSPARENT_RISK) ? 1 : 0;
    summary->earlyStopHoldGeometryEdgeRiskPixelCount +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_GEOMETRY_EDGE_RISK) ? 1 : 0;
    summary->earlyStopHoldDirectLightRiskPixelCount +=
        (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_DIRECT_LIGHT_RISK) ? 1 : 0;
}

bool RuntimeNative3DAdaptiveSampling_RefreshActivityMaskFromPixelState(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    const RuntimeNative3DAdaptivePixelStateBuffer* state,
    int tile_size) {
    const int resolved_tile_size =
        (tile_size > 0) ? tile_size : RUNTIME_NATIVE_3D_ADAPTIVE_STATE_DEFAULT_TILE_SIZE;
    const int width = state ? state->width : 0;
    const int height = state ? state->height : 0;
    const int tiles_x =
        runtime_native_3d_adaptive_sampling_compute_tiles(width, resolved_tile_size);
    const int tiles_y =
        runtime_native_3d_adaptive_sampling_compute_tiles(height, resolved_tile_size);
    const int tile_count = tiles_x * tiles_y;
    const size_t pixel_count = (size_t)width * (size_t)height;

    if (!mask || !state || !state->pixels || width <= 0 || height <= 0 ||
        tiles_x <= 0 || tiles_y <= 0) {
        return false;
    }
    if (!RuntimeNative3DAdaptiveSamplingMask_Ensure(mask, width, height) ||
        !runtime_native_3d_adaptive_sampling_ensure_tile_mask(mask, tile_count)) {
        return false;
    }

    memset(mask->activeSampleMask, 0, pixel_count * sizeof(*mask->activeSampleMask));
    memset(mask->scratchSampleMask, 0, pixel_count * sizeof(*mask->scratchSampleMask));
    memset(mask->activeTileMask, 0, (size_t)tile_count * sizeof(*mask->activeTileMask));
    mask->tileSize = resolved_tile_size;
    mask->tilesX = tiles_x;
    mask->tilesY = tiles_y;
    mask->minSubpassesBeforePrune = state->summary.minSampleFloor;
    mask->activePixelCount = 0;
    mask->activeTileCount = 0;
    mask->inactiveTileCount = 0;

    for (size_t i = 0; i < pixel_count; ++i) {
        const uint16_t flags = state->pixels[i].flags;
        const bool high_risk =
            (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_HIGH_RISK) != 0u;
        const bool active =
            (flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVE) != 0u;
        mask->activeSampleMask[i] = active ? 1u : 0u;
        mask->scratchSampleMask[i] = high_risk ? 2u : (active ? 1u : 0u);
        if (active) {
            mask->activePixelCount += 1;
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t pixel_index = (size_t)y * (size_t)width + (size_t)x;
            if (mask->activeSampleMask[pixel_index]) {
                continue;
            }
            if (runtime_native_3d_adaptive_sampling_has_neighbor_seed(
                    mask,
                    x,
                    y,
                    2u,
                    RUNTIME_NATIVE_3D_ADAPTIVE_NEIGHBORHOOD_RADIUS_HIGH) ||
                runtime_native_3d_adaptive_sampling_has_neighbor_seed(
                    mask,
                    x,
                    y,
                    1u,
                    RUNTIME_NATIVE_3D_ADAPTIVE_NEIGHBORHOOD_RADIUS_MEDIUM)) {
                mask->activeSampleMask[pixel_index] = 1u;
                mask->activePixelCount += 1;
            }
        }
    }

    for (int tile_y = 0; tile_y < tiles_y; ++tile_y) {
        for (int tile_x = 0; tile_x < tiles_x; ++tile_x) {
            const int start_x = tile_x * resolved_tile_size;
            const int start_y = tile_y * resolved_tile_size;
            const int end_x = (start_x + resolved_tile_size < width)
                                  ? (start_x + resolved_tile_size)
                                  : width;
            const int end_y = (start_y + resolved_tile_size < height)
                                  ? (start_y + resolved_tile_size)
                                  : height;
            const size_t tile_index = (size_t)tile_y * (size_t)tiles_x + (size_t)tile_x;
            bool tile_active = false;

            for (int y = start_y; !tile_active && y < end_y; ++y) {
                for (int x = start_x; x < end_x; ++x) {
                    const size_t pixel_index = (size_t)y * (size_t)width + (size_t)x;
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

bool RuntimeNative3DAdaptiveSampling_RefreshConservativeEarlyStopMaskFromPixelState(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    const RuntimeNative3DAdaptivePixelStateBuffer* state,
    int tile_size) {
    const int resolved_tile_size =
        (tile_size > 0) ? tile_size : RUNTIME_NATIVE_3D_ADAPTIVE_STATE_DEFAULT_TILE_SIZE;
    const int width = state ? state->width : 0;
    const int height = state ? state->height : 0;
    const int tiles_x =
        runtime_native_3d_adaptive_sampling_compute_tiles(width, resolved_tile_size);
    const int tiles_y =
        runtime_native_3d_adaptive_sampling_compute_tiles(height, resolved_tile_size);
    const int tile_count = tiles_x * tiles_y;
    const size_t pixel_count = (size_t)width * (size_t)height;

    if (!mask || !state || !state->pixels || width <= 0 || height <= 0 ||
        tiles_x <= 0 || tiles_y <= 0) {
        return false;
    }
    if (!RuntimeNative3DAdaptiveSamplingMask_Ensure(mask, width, height) ||
        !runtime_native_3d_adaptive_sampling_ensure_tile_mask(mask, tile_count)) {
        return false;
    }

    memset(mask->activeSampleMask, 0, pixel_count * sizeof(*mask->activeSampleMask));
    memset(mask->scratchSampleMask, 0, pixel_count * sizeof(*mask->scratchSampleMask));
    memset(mask->activeTileMask, 0, (size_t)tile_count * sizeof(*mask->activeTileMask));
    mask->tileSize = resolved_tile_size;
    mask->tilesX = tiles_x;
    mask->tilesY = tiles_y;
    mask->minSubpassesBeforePrune = state->summary.minSampleFloor;
    mask->activePixelCount = 0;
    mask->activeTileCount = 0;
    mask->inactiveTileCount = 0;
    mask->conservativeEarlyStopEligiblePixelCount = 0;
    mask->conservativeEarlyStopBaseActivePixelCount = 0;
    mask->conservativeEarlyStopPaddingHoldPixelCount = 0;
    mask->conservativeEarlyStopPaddingHoldHighSeedPixelCount = 0;
    mask->conservativeEarlyStopPaddingHoldMediumSeedPixelCount = 0;
    memset(mask->conservativeEarlyStopPaddingHoldRegionCounts,
           0,
           sizeof(mask->conservativeEarlyStopPaddingHoldRegionCounts));

    for (size_t i = 0; i < pixel_count; ++i) {
        const uint16_t flags = state->pixels[i].flags;
        const bool active =
            !runtime_native_3d_adaptive_sampling_flags_safe_for_early_stop(flags);
        if (active) {
            mask->conservativeEarlyStopBaseActivePixelCount += 1;
        } else {
            mask->conservativeEarlyStopEligiblePixelCount += 1;
        }
        mask->activeSampleMask[i] = active ? 1u : 0u;
        mask->scratchSampleMask[i] =
            runtime_native_3d_adaptive_sampling_early_stop_seed_from_flags(flags, active);
        if (active) {
            mask->activePixelCount += 1;
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t pixel_index = (size_t)y * (size_t)width + (size_t)x;
            if (mask->activeSampleMask[pixel_index]) {
                continue;
            }
            if (runtime_native_3d_adaptive_sampling_has_neighbor_seed(
                    mask,
                    x,
                    y,
                    2u,
                    RUNTIME_NATIVE_3D_ADAPTIVE_NEIGHBORHOOD_RADIUS_HIGH)) {
                mask->activeSampleMask[pixel_index] = 1u;
                mask->activePixelCount += 1;
                mask->conservativeEarlyStopPaddingHoldPixelCount += 1;
                mask->conservativeEarlyStopPaddingHoldHighSeedPixelCount += 1;
                mask->conservativeEarlyStopPaddingHoldRegionCounts
                    [runtime_native_3d_adaptive_sampling_region_index(x, y, width, height)] += 1;
            } else if (runtime_native_3d_adaptive_sampling_has_neighbor_seed(
                           mask,
                           x,
                           y,
                           1u,
                           RUNTIME_NATIVE_3D_ADAPTIVE_EARLY_STOP_NEIGHBORHOOD_RADIUS_MEDIUM)) {
                mask->activeSampleMask[pixel_index] = 1u;
                mask->activePixelCount += 1;
                mask->conservativeEarlyStopPaddingHoldPixelCount += 1;
                mask->conservativeEarlyStopPaddingHoldMediumSeedPixelCount += 1;
                mask->conservativeEarlyStopPaddingHoldRegionCounts
                    [runtime_native_3d_adaptive_sampling_region_index(x, y, width, height)] += 1;
            }
        }
    }

    for (int tile_y = 0; tile_y < tiles_y; ++tile_y) {
        for (int tile_x = 0; tile_x < tiles_x; ++tile_x) {
            const int start_x = tile_x * resolved_tile_size;
            const int start_y = tile_y * resolved_tile_size;
            const int end_x = (start_x + resolved_tile_size < width)
                                  ? (start_x + resolved_tile_size)
                                  : width;
            const int end_y = (start_y + resolved_tile_size < height)
                                  ? (start_y + resolved_tile_size)
                                  : height;
            const size_t tile_index = (size_t)tile_y * (size_t)tiles_x + (size_t)tile_x;
            bool tile_active = false;

            for (int y = start_y; !tile_active && y < end_y; ++y) {
                for (int x = start_x; x < end_x; ++x) {
                    const size_t pixel_index = (size_t)y * (size_t)width + (size_t)x;
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
