#include "render/runtime_native_3d_blue_noise.h"

static uint32_t runtime_native_3d_blue_noise_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static const uint8_t kRuntimeNative3DBlueNoiseTile[64] = {
    0, 48, 12, 60, 3, 51, 15, 63,
    32, 16, 44, 28, 35, 19, 47, 31,
    8, 56, 4, 52, 11, 59, 7, 55,
    40, 24, 36, 20, 43, 27, 39, 23,
    2, 50, 14, 62, 1, 49, 13, 61,
    34, 18, 46, 30, 33, 17, 45, 29,
    10, 58, 6, 54, 9, 57, 5, 53,
    42, 26, 38, 22, 41, 25, 37, 21
};

void RuntimeNative3DBlueNoise_Jitter2D(uint32_t sample_sequence,
                                       uint32_t base_seed,
                                       uint32_t dimension,
                                       double* out_u,
                                       double* out_v) {
    uint32_t sequence_key = 0u;
    uint32_t tile_index_u = 0u;
    uint32_t tile_index_v = 0u;

    if (!out_u || !out_v) return;

    sequence_key = runtime_native_3d_blue_noise_hash_u32(sample_sequence ^
                                                         base_seed ^
                                                         (dimension * 0x9e3779b9U));
    tile_index_u = sequence_key & 63u;
    tile_index_v =
        runtime_native_3d_blue_noise_hash_u32(sequence_key ^ 0xa511e9b3U ^ base_seed) & 63u;

    *out_u = ((double)kRuntimeNative3DBlueNoiseTile[tile_index_u] + 0.5) / 64.0;
    *out_v = ((double)kRuntimeNative3DBlueNoiseTile[tile_index_v] + 0.5) / 64.0;
}
