#ifndef RUNTIME_NATIVE_3D_ADAPTIVE_SAMPLING_INTERNAL_H
#define RUNTIME_NATIVE_3D_ADAPTIVE_SAMPLING_INTERNAL_H

#include "render/runtime_native_3d_adaptive_sampling.h"

enum {
    RUNTIME_NATIVE_3D_ADAPTIVE_STATE_DEFAULT_TILE_SIZE = 16,
    RUNTIME_NATIVE_3D_ADAPTIVE_NEIGHBORHOOD_RADIUS_MEDIUM = 1,
    RUNTIME_NATIVE_3D_ADAPTIVE_NEIGHBORHOOD_RADIUS_HIGH = 2,
    RUNTIME_NATIVE_3D_ADAPTIVE_EARLY_STOP_NEIGHBORHOOD_RADIUS_MEDIUM = 1
};

int runtime_native_3d_adaptive_sampling_compute_tiles(int extent, int tile_size);
bool runtime_native_3d_adaptive_sampling_ensure_tile_mask(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    int tile_count);
int runtime_native_3d_adaptive_sampling_region_index(int x,
                                                     int y,
                                                     int width,
                                                     int height);
bool runtime_native_3d_adaptive_sampling_has_neighbor_seed(
    const RuntimeNative3DAdaptiveSamplingMask* mask,
    int x,
    int y,
    uint8_t minimum_seed,
    int radius);
bool runtime_native_3d_adaptive_sampling_flags_safe_for_early_stop(uint16_t flags);
int runtime_native_3d_adaptive_sampling_budget_bucket(uint16_t sample_count,
                                                      int min_sample_floor);
void runtime_native_3d_adaptive_sampling_note_early_stop_hold_flags(
    RuntimeNative3DAdaptivePixelStateSummary* summary,
    uint16_t flags,
    int region_index,
    int budget_bucket,
    RuntimeNative3DDirectLightVisibilityOutcome visibility);

#endif
