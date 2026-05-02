#include "render/runtime_native_3d_denoise.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "render/runtime_native_3d_render.h"

enum {
    RUNTIME_NATIVE_3D_DENOISE_RADIUS = 2
};

static float runtime_native_3d_denoise_spatial_weight(int dx, int dy) {
    const float sigma = 1.25f;
    const float distance_sq = (float)(dx * dx + dy * dy);
    const float denom = 2.0f * sigma * sigma;
    return expf(-distance_sq / denom);
}

static float runtime_native_3d_denoise_depth_relative_delta(float center_depth,
                                                            float sample_depth) {
    const float denom = fmaxf(fmaxf(center_depth, sample_depth), 1e-3f);
    return fabsf(sample_depth - center_depth) / denom;
}

bool RuntimeNative3DDenoise_ShouldApply(RayTracing3DIntegratorId integrator_id,
                                        int temporal_frames,
                                        bool denoise_enabled) {
    return denoise_enabled &&
           integrator_id == RAY_TRACING_3D_INTEGRATOR_DISNEY &&
           temporal_frames > 1;
}

bool RuntimeNative3DDenoise_Apply(float* radiance_buffer,
                                  int radiance_stride,
                                  const RuntimeNative3DFeatureBuffer* features) {
    float* filtered = NULL;
    size_t pixel_count = 0;
    if (!radiance_buffer || radiance_stride <= 0 || !features || !features->normalBuffer ||
        !features->depthBuffer || !features->hitMaskBuffer || features->width <= 0 ||
        features->height <= 0) {
        return false;
    }

    pixel_count = (size_t)features->width * (size_t)features->height;
    filtered = (float*)calloc(pixel_count * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS, sizeof(*filtered));
    if (!filtered) return false;

    for (int y = 0; y < features->height; ++y) {
        for (int x = 0; x < features->width; ++x) {
            const size_t center_index = (size_t)y * (size_t)features->width + (size_t)x;
            const size_t center_normal_base = center_index * 3u;
            const size_t center_radiance_base =
                ((size_t)y * (size_t)radiance_stride + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            float total_weight = 0.0f;
            float accum_r = 0.0f;
            float accum_g = 0.0f;
            float accum_b = 0.0f;

            if (!features->hitMaskBuffer[center_index]) {
                filtered[center_radiance_base] = radiance_buffer[center_radiance_base];
                filtered[center_radiance_base + 1u] = radiance_buffer[center_radiance_base + 1u];
                filtered[center_radiance_base + 2u] = radiance_buffer[center_radiance_base + 2u];
                filtered[center_radiance_base + RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL] =
                    radiance_buffer[center_radiance_base +
                                    RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL];
                continue;
            }

            {
                const float center_depth = features->depthBuffer[center_index];
                const float center_nx = features->normalBuffer[center_normal_base];
                const float center_ny = features->normalBuffer[center_normal_base + 1u];
                const float center_nz = features->normalBuffer[center_normal_base + 2u];

                for (int dy = -RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                     dy <= RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                     ++dy) {
                    const int ny = y + dy;
                    if (ny < 0 || ny >= features->height) continue;
                    for (int dx = -RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                         dx <= RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                         ++dx) {
                        const int nx = x + dx;
                        const size_t sample_index = (size_t)ny * (size_t)features->width +
                                                    (size_t)nx;
                        const size_t sample_normal_base = sample_index * 3u;
                        const size_t sample_radiance_base =
                            ((size_t)ny * (size_t)radiance_stride + (size_t)nx) *
                            (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
                        float ndot = 0.0f;
                        float depth_delta = 0.0f;
                        float weight = 0.0f;
                        if (nx < 0 || nx >= features->width) continue;
                        if (!features->hitMaskBuffer[sample_index]) continue;
                        ndot = center_nx * features->normalBuffer[sample_normal_base] +
                               center_ny * features->normalBuffer[sample_normal_base + 1u] +
                               center_nz * features->normalBuffer[sample_normal_base + 2u];
                        if (ndot < 0.9f) continue;
                        depth_delta = runtime_native_3d_denoise_depth_relative_delta(
                            center_depth,
                            features->depthBuffer[sample_index]);
                        if (depth_delta > 0.08f) continue;
                        weight = runtime_native_3d_denoise_spatial_weight(dx, dy) *
                                 powf(fminf(ndot, 1.0f), 12.0f) *
                                 expf(-(depth_delta * depth_delta) / (2.0f * 0.03f * 0.03f));
                        accum_r += radiance_buffer[sample_radiance_base] * weight;
                        accum_g += radiance_buffer[sample_radiance_base + 1u] * weight;
                        accum_b += radiance_buffer[sample_radiance_base + 2u] * weight;
                        total_weight += weight;
                    }
                }
            }

            if (total_weight > 1e-6f) {
                filtered[center_radiance_base] = accum_r / total_weight;
                filtered[center_radiance_base + 1u] = accum_g / total_weight;
                filtered[center_radiance_base + 2u] = accum_b / total_weight;
                filtered[center_radiance_base + RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL] =
                    radiance_buffer[center_radiance_base +
                                    RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL];
            } else {
                filtered[center_radiance_base] = radiance_buffer[center_radiance_base];
                filtered[center_radiance_base + 1u] = radiance_buffer[center_radiance_base + 1u];
                filtered[center_radiance_base + 2u] = radiance_buffer[center_radiance_base + 2u];
                filtered[center_radiance_base + RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL] =
                    radiance_buffer[center_radiance_base +
                                    RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL];
            }
        }
    }

    memcpy(radiance_buffer,
           filtered,
           pixel_count * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS * sizeof(*filtered));
    free(filtered);
    return true;
}
