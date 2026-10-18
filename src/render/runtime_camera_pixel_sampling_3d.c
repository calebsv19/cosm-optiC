#include "render/runtime_native_3d_sampling.h"

#include <stdlib.h>
#include <string.h>

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU;
    x ^= x >> 15; x *= 0x846ca68bU;
    return x ^ (x >> 16);
}

RuntimeCameraPixelSampler3D RuntimeNative3DSampling_ResolveCameraSampler(
    RuntimeCameraPixelSampler3D sampler) {
    if (sampler >= RUNTIME_CAMERA_PIXEL_CENTER && sampler <= RUNTIME_CAMERA_PIXEL_PROGRESSIVE)
        return sampler;
    const char* value = getenv("RAY_TRACING_NATIVE3D_CAMERA_SAMPLER");
    if (value && strcmp(value, "center") == 0) return RUNTIME_CAMERA_PIXEL_CENTER;
    if (value && strcmp(value, "stratified") == 0) return RUNTIME_CAMERA_PIXEL_STRATIFIED;
    return RUNTIME_CAMERA_PIXEL_PROGRESSIVE;
}

/* Nested binary Owen scrambling: a separate deterministic random permutation
 * for each prefix. Preserves the Sobol net's dyadic coverage. */
static uint32_t scramble(uint32_t value, uint32_t seed) {
    uint32_t prefix = 0, result = 0;
    for (unsigned i = 0; i < 32; ++i) {
        uint32_t bit = (value >> (31 - i)) & 1U;
        uint32_t flip = hash32(seed ^ hash32(prefix) ^ (i * 0x9e3779b9U)) & 1U;
        result = (result << 1) | (bit ^ flip);
        prefix = (prefix << 1) | bit;
    }
    return result;
}

static double uniform32(uint32_t bits) {
    return ((double)bits + 0.5) / 4294967296.0;
}

static unsigned gcd(unsigned a, unsigned b) {
    while (b) { unsigned r = a % b; a = b; b = r; }
    return a;
}

void RuntimeNative3DSampling_PixelPosition(const RuntimeNative3DSamplingContext* sampling,
    int x, int y, double* out_x, double* out_y) {
    if (!out_x || !out_y) return;
    *out_x = x; *out_y = y;
    if (!sampling || sampling->temporalSubpassCount <= 1 ||
        sampling->cameraPixelSampler == RUNTIME_CAMERA_PIXEL_CENTER) return;
    const uint32_t index = sampling->temporalSubpassIndex;
    /* Spatial dimensions never consume or depend on the advancing transport RNG. */
    const uint32_t base = sampling->cameraSequenceSeed;
    const uint32_t seed = hash32(base ^ hash32((uint32_t)x) ^
        hash32((uint32_t)y ^ 0x95e1d5e7U) ^ 0x43414d52U);
    double u, v;
    if (sampling->cameraPixelSampler == RUNTIME_CAMERA_PIXEL_STRATIFIED) {
        const unsigned count = sampling->temporalSubpassCount;
        unsigned width = 1;
        for (unsigned i = 1; i * i <= count; ++i) if (count % i == 0) width = i;
        const unsigned height = count / width;
        unsigned stride = (hash32(seed ^ 1U) % count) | 1U;
        while (gcd(stride, count) != 1) stride += 2;
        unsigned cell = (index * stride + hash32(seed ^ 2U) % count) % count;
        u = ((cell % width) + uniform32(hash32(seed ^ hash32(index) ^ 3U))) / width;
        v = ((cell / width) + uniform32(hash32(seed ^ hash32(index) ^ 4U))) / height;
    } else {
        /* First two Sobol dimensions, Gray-code ordering. Dimension two uses
         * the primitive polynomial x+1: v[i+1] = v[i] xor (v[i] >> 1). */
        uint32_t gray = index ^ (index >> 1), a = 0, b = 0;
        uint32_t va = 0x80000000U, vb = 0x80000000U;
        for (unsigned i = 0; i < 32; ++i) {
            if (gray & (1U << i)) { a ^= va; b ^= vb; }
            va >>= 1; vb ^= vb >> 1;
        }
        u = uniform32(scramble(a, hash32(seed ^ 0x75ad47e9U)));
        v = uniform32(scramble(b, hash32(seed ^ 0xb6e32c41U)));
    }
    *out_x = x + u - 0.5; *out_y = y + v - 0.5;
}
