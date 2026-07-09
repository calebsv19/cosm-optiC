#include "tools/ray_tracing_render_headless_internal.h"

#include <stddef.h>

size_t ray_tracing_headless_count_nonzero_pixels(const uint8_t *pixels,
                                                 int width,
                                                 int height,
                                                 uint8_t *out_max_r,
                                                 uint8_t *out_max_g,
                                                 uint8_t *out_max_b) {
    size_t nonzero = 0u;
    uint8_t max_r = 0u;
    uint8_t max_g = 0u;
    uint8_t max_b = 0u;

    if (!pixels || width <= 0 || height <= 0) {
        if (out_max_r) *out_max_r = 0u;
        if (out_max_g) *out_max_g = 0u;
        if (out_max_b) *out_max_b = 0u;
        return 0u;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t base =
                ((size_t)y * (size_t)width + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            const uint8_t r = pixels[base];
            const uint8_t g = pixels[base + 1u];
            const uint8_t b = pixels[base + 2u];
            if (r > max_r) max_r = r;
            if (g > max_g) max_g = g;
            if (b > max_b) max_b = b;
            if (r > 0u || g > 0u || b > 0u) {
                nonzero += 1u;
            }
        }
    }

    if (out_max_r) *out_max_r = max_r;
    if (out_max_g) *out_max_g = max_g;
    if (out_max_b) *out_max_b = max_b;
    return nonzero;
}

double ray_tracing_headless_frame_normalized_t(const RayTracingAgentRenderRequest *request,
                                               int local_frame) {
    int sampling_offset = 0;
    int sampling_count = 0;
    if (!request) return 0.0;
    sampling_offset = request->has_sampling_window ? request->sampling_frame_offset : 0;
    sampling_count = request->has_sampling_window ? request->sampling_frame_count : request->frame_count;
    if (sampling_count <= 1) return request->normalized_t;
    return (double)(sampling_offset + local_frame) / (double)(sampling_count - 1);
}
