#include "render/runtime_native_3d_sampling.h"

#include <math.h>

#include "render/runtime_native_3d_blue_noise.h"

static uint32_t runtime_native_3d_sampling_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static double runtime_native_3d_sampling_hash01(uint32_t seed, uint32_t salt) {
    const uint32_t bits = runtime_native_3d_sampling_hash_u32(seed ^ salt);
    return (double)bits / 4294967295.0;
}

static int runtime_native_3d_sampling_resolve_subpass_count(
    const RuntimeNative3DSamplingContext* sampling) {
    if (!sampling || sampling->temporalSubpassCount == 0u) {
        return 1;
    }
    return (int)sampling->temporalSubpassCount;
}

static int runtime_native_3d_sampling_resolve_subpass_index(
    const RuntimeNative3DSamplingContext* sampling,
    int subpass_count) {
    int subpass_index = 0;

    if (!sampling || subpass_count <= 1) {
        return 0;
    }
    subpass_index = (int)sampling->temporalSubpassIndex;
    if (subpass_index < 0) subpass_index = 0;
    if (subpass_index >= subpass_count) subpass_index = subpass_count - 1;
    return subpass_index;
}

void RuntimeNative3DSampling_Stratified2D(const RuntimeNative3DSamplingContext* sampling,
                                          uint32_t base_seed,
                                          int sample_count,
                                          int sample_index,
                                          uint32_t dimension,
                                          double* out_u,
                                          double* out_v) {
    int strata_count = sample_count;
    int subpass_count = runtime_native_3d_sampling_resolve_subpass_count(sampling);
    int subpass_index = runtime_native_3d_sampling_resolve_subpass_index(sampling, subpass_count);
    int total_count = 0;
    int global_index = 0;
    int grid_width = 0;
    int grid_height = 0;
    int stratum_x = 0;
    int stratum_y = 0;
    double jitter_x = 0.5;
    double jitter_y = 0.5;
    uint32_t sequence = sampling ? sampling->sampleSequence : 1u;

    if (!out_u || !out_v) return;

    if (strata_count < 1) strata_count = 1;
    if (sample_index < 0) sample_index = 0;
    if (sample_index >= strata_count) sample_index = strata_count - 1;

    total_count = strata_count * subpass_count;
    if (total_count < 1) total_count = 1;
    global_index = (subpass_index * strata_count) + sample_index;
    if (global_index < 0) global_index = 0;
    if (global_index >= total_count) global_index = total_count - 1;

    grid_width = (int)ceil(sqrt((double)total_count));
    if (grid_width < 1) grid_width = 1;
    grid_height = (total_count + grid_width - 1) / grid_width;
    if (grid_height < 1) grid_height = 1;

    stratum_x = global_index % grid_width;
    stratum_y = global_index / grid_width;
    RuntimeNative3DBlueNoise_Jitter2D(sequence + (uint32_t)global_index,
                                      base_seed,
                                      dimension,
                                      &jitter_x,
                                      &jitter_y);
    jitter_x = fmod(jitter_x +
                        runtime_native_3d_sampling_hash01(base_seed,
                                                          runtime_native_3d_sampling_hash_u32(
                                                              dimension ^
                                                              ((uint32_t)global_index * 2u + 1u))) /
                            64.0,
                    1.0);
    jitter_y = fmod(jitter_y +
                        runtime_native_3d_sampling_hash01(base_seed,
                                                          runtime_native_3d_sampling_hash_u32(
                                                              dimension ^
                                                              ((uint32_t)global_index * 2u + 2u))) /
                            64.0,
                    1.0);

    *out_u = ((double)stratum_x + jitter_x) / (double)grid_width;
    *out_v = ((double)stratum_y + jitter_y) / (double)grid_height;
}
