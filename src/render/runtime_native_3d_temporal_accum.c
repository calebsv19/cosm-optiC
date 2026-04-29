#include "render/runtime_native_3d_temporal_accum.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/runtime_native_3d_render.h"

static const float kRuntimeNative3DTemporalEMAAlpha = 0.25f;
static const float kRuntimeNative3DTemporalClampScale = 3.0f;
static const float kRuntimeNative3DTemporalClampMinLuma = 1.0f;

static float runtime_native_3d_temporal_accum_luma(float r, float g, float b) {
    return (0.2126f * r) + (0.7152f * g) + (0.0722f * b);
}

static float runtime_native_3d_temporal_accum_bias_correction(uint16_t sample_count) {
    if (sample_count == 0u) return 0.0f;
    return 1.0f - powf(1.0f - kRuntimeNative3DTemporalEMAAlpha, (float)sample_count);
}

static void runtime_native_3d_temporal_accum_resolve_history_rgb(
    const RuntimeNative3DTemporalAccumulation* accumulation,
    size_t accumulation_base,
    uint16_t sample_count,
    float* out_r,
    float* out_g,
    float* out_b) {
    float correction = runtime_native_3d_temporal_accum_bias_correction(sample_count);
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;

    if (sample_count > 0u && accumulation && accumulation->accumulationBuffer) {
        r = accumulation->accumulationBuffer[accumulation_base];
        g = accumulation->accumulationBuffer[accumulation_base + 1u];
        b = accumulation->accumulationBuffer[accumulation_base + 2u];
        if (correction > 1e-6f) {
            r /= correction;
            g /= correction;
            b /= correction;
        }
    }

    if (out_r) *out_r = r;
    if (out_g) *out_g = g;
    if (out_b) *out_b = b;
}

static void runtime_native_3d_temporal_accum_clamp_sample_to_history(float history_r,
                                                                     float history_g,
                                                                     float history_b,
                                                                     float* io_sample_r,
                                                                     float* io_sample_g,
                                                                     float* io_sample_b) {
    float sample_luma = 0.0f;
    float history_luma = 0.0f;
    float luma_limit = 0.0f;
    float scale = 1.0f;

    if (!io_sample_r || !io_sample_g || !io_sample_b) return;

    sample_luma = runtime_native_3d_temporal_accum_luma(*io_sample_r, *io_sample_g, *io_sample_b);
    history_luma = runtime_native_3d_temporal_accum_luma(history_r, history_g, history_b);
    luma_limit = fmaxf(kRuntimeNative3DTemporalClampMinLuma,
                       (history_luma * kRuntimeNative3DTemporalClampScale) + 0.25f);
    if (sample_luma <= luma_limit || sample_luma <= 1e-6f) {
        return;
    }

    scale = luma_limit / sample_luma;
    *io_sample_r *= scale;
    *io_sample_g *= scale;
    *io_sample_b *= scale;
}

void RuntimeNative3DTemporalAccumulation_Init(RuntimeNative3DTemporalAccumulation* accumulation) {
    if (!accumulation) return;
    memset(accumulation, 0, sizeof(*accumulation));
}

void RuntimeNative3DTemporalAccumulation_Free(RuntimeNative3DTemporalAccumulation* accumulation) {
    if (!accumulation) return;
    free(accumulation->accumulationBuffer);
    free(accumulation->sampleCountBuffer);
    memset(accumulation, 0, sizeof(*accumulation));
}

bool RuntimeNative3DTemporalAccumulation_Ensure(RuntimeNative3DTemporalAccumulation* accumulation,
                                                int width,
                                                int height) {
    float* resized = NULL;
    uint16_t* resized_counts = NULL;
    size_t count = 0;
    if (!accumulation || width <= 0 || height <= 0) return false;
    if (accumulation->accumulationBuffer &&
        accumulation->sampleCountBuffer &&
        accumulation->width == width &&
        accumulation->height == height) {
        return true;
    }

    count = (size_t)width * (size_t)height;
    resized = (float*)calloc(count * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS, sizeof(*resized));
    resized_counts = (uint16_t*)calloc(count, sizeof(*resized_counts));
    if (!resized || !resized_counts) {
        free(resized);
        free(resized_counts);
        return false;
    }

    free(accumulation->accumulationBuffer);
    free(accumulation->sampleCountBuffer);
    accumulation->accumulationBuffer = resized;
    accumulation->sampleCountBuffer = resized_counts;
    accumulation->width = width;
    accumulation->height = height;
    accumulation->completedSubpasses = 0;
    return true;
}

void RuntimeNative3DTemporalAccumulation_Clear(RuntimeNative3DTemporalAccumulation* accumulation) {
    size_t count = 0;
    if (!accumulation || !accumulation->accumulationBuffer || accumulation->width <= 0 ||
        accumulation->height <= 0) {
        return;
    }
    count = (size_t)accumulation->width * (size_t)accumulation->height;
    memset(accumulation->accumulationBuffer,
           0,
           count * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS *
               sizeof(*accumulation->accumulationBuffer));
    if (accumulation->sampleCountBuffer) {
        memset(accumulation->sampleCountBuffer, 0, count * sizeof(*accumulation->sampleCountBuffer));
    }
    accumulation->completedSubpasses = 0;
}

bool RuntimeNative3DTemporalAccumulation_AddRegion(RuntimeNative3DTemporalAccumulation* accumulation,
                                                   const float* radiance_region,
                                                   int radiance_stride,
                                                   int start_x,
                                                   int start_y,
                                                   int end_x,
                                                   int end_y) {
    return RuntimeNative3DTemporalAccumulation_AddRegionSamples(accumulation,
                                                                radiance_region,
                                                                radiance_stride,
                                                                start_x,
                                                                start_y,
                                                                end_x,
                                                                end_y,
                                                                NULL,
                                                                0);
}

bool RuntimeNative3DTemporalAccumulation_AddRegionSamples(
    RuntimeNative3DTemporalAccumulation* accumulation,
    const float* radiance_region,
    int radiance_stride,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const uint8_t* sample_mask,
    int sample_mask_stride) {
    if (!accumulation || !accumulation->accumulationBuffer || !accumulation->sampleCountBuffer ||
        !radiance_region || radiance_stride <= 0) {
        return false;
    }
    if (sample_mask && sample_mask_stride <= 0) {
        return false;
    }
    if (start_x < 0 || start_y < 0 || end_x > accumulation->width || end_y > accumulation->height ||
        start_x >= end_x || start_y >= end_y) {
        return false;
    }

    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        for (int x = start_x; x < end_x; ++x) {
            const int local_x = x - start_x;
            const size_t accumulation_index =
                (size_t)y * (size_t)accumulation->width + (size_t)x;
            const size_t region_index =
                (size_t)local_y * (size_t)radiance_stride + (size_t)local_x;
            if (sample_mask) {
                const size_t mask_index =
                    (size_t)local_y * (size_t)sample_mask_stride + (size_t)local_x;
                if (!sample_mask[mask_index]) {
                    continue;
                }
            }
            {
                const size_t accumulation_base =
                    accumulation_index * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
                const size_t region_base =
                    region_index * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
                const uint16_t sample_count = accumulation->sampleCountBuffer[accumulation_index];
                float sample_r = radiance_region[region_base];
                float sample_g = radiance_region[region_base + 1u];
                float sample_b = radiance_region[region_base + 2u];

                if (sample_count > 0u) {
                    float history_r = 0.0f;
                    float history_g = 0.0f;
                    float history_b = 0.0f;
                    runtime_native_3d_temporal_accum_resolve_history_rgb(accumulation,
                                                                         accumulation_base,
                                                                         sample_count,
                                                                         &history_r,
                                                                         &history_g,
                                                                         &history_b);
                    runtime_native_3d_temporal_accum_clamp_sample_to_history(history_r,
                                                                             history_g,
                                                                             history_b,
                                                                             &sample_r,
                                                                             &sample_g,
                                                                             &sample_b);
                    accumulation->accumulationBuffer[accumulation_base] +=
                        kRuntimeNative3DTemporalEMAAlpha *
                        (sample_r - accumulation->accumulationBuffer[accumulation_base]);
                    accumulation->accumulationBuffer[accumulation_base + 1u] +=
                        kRuntimeNative3DTemporalEMAAlpha *
                        (sample_g - accumulation->accumulationBuffer[accumulation_base + 1u]);
                    accumulation->accumulationBuffer[accumulation_base + 2u] +=
                        kRuntimeNative3DTemporalEMAAlpha *
                        (sample_b - accumulation->accumulationBuffer[accumulation_base + 2u]);
                } else {
                    accumulation->accumulationBuffer[accumulation_base] =
                        kRuntimeNative3DTemporalEMAAlpha * sample_r;
                    accumulation->accumulationBuffer[accumulation_base + 1u] =
                        kRuntimeNative3DTemporalEMAAlpha * sample_g;
                    accumulation->accumulationBuffer[accumulation_base + 2u] =
                        kRuntimeNative3DTemporalEMAAlpha * sample_b;
                }
            }
            if (accumulation->sampleCountBuffer[accumulation_index] < UINT16_MAX) {
                accumulation->sampleCountBuffer[accumulation_index] += 1u;
            }
        }
    }
    return true;
}

void RuntimeNative3DTemporalAccumulation_CommitSubpass(
    RuntimeNative3DTemporalAccumulation* accumulation) {
    if (!accumulation) return;
    accumulation->completedSubpasses += 1;
}

bool RuntimeNative3DTemporalAccumulation_ResolveRegionToRadianceBuffer(
    const RuntimeNative3DTemporalAccumulation* accumulation,
    float* radiance_buffer,
    int radiance_stride,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    if (!accumulation || !accumulation->accumulationBuffer || !radiance_buffer ||
        radiance_stride <= 0) {
        return false;
    }
    if (start_x < 0 || start_y < 0 || end_x > accumulation->width || end_y > accumulation->height ||
        start_x >= end_x || start_y >= end_y) {
        return false;
    }

    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        for (int x = start_x; x < end_x; ++x) {
            const int local_x = x - start_x;
            const size_t accumulation_index =
                (size_t)y * (size_t)accumulation->width + (size_t)x;
            const size_t accumulation_base =
                accumulation_index * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            const size_t radiance_base =
                ((size_t)local_y * (size_t)radiance_stride + (size_t)local_x) *
                (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            const uint16_t sample_count =
                accumulation->sampleCountBuffer ? accumulation->sampleCountBuffer[accumulation_index]
                                                : 0u;
            float resolved_r = 0.0f;
            float resolved_g = 0.0f;
            float resolved_b = 0.0f;
            runtime_native_3d_temporal_accum_resolve_history_rgb(accumulation,
                                                                 accumulation_base,
                                                                 sample_count,
                                                                 &resolved_r,
                                                                 &resolved_g,
                                                                 &resolved_b);
            radiance_buffer[radiance_base] = resolved_r;
            radiance_buffer[radiance_base + 1u] = resolved_g;
            radiance_buffer[radiance_base + 2u] = resolved_b;
        }
    }
    return true;
}

void RuntimeNative3DTemporalAccumulation_ResolveRegionToPixelBuffer(
    const RuntimeNative3DTemporalAccumulation* accumulation,
    uint8_t* pixel_buffer,
    int pixel_width,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    const uint8_t environment = RuntimeNative3DResolveEnvironmentByte();
    if (!accumulation || !accumulation->accumulationBuffer || !pixel_buffer || pixel_width <= 0) {
        return;
    }
    if (start_x < 0 || start_y < 0 || end_x > accumulation->width || end_y > accumulation->height ||
        start_x >= end_x || start_y >= end_y) {
        return;
    }

    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            const size_t accumulation_index =
                (size_t)y * (size_t)accumulation->width + (size_t)x;
            const size_t accumulation_base =
                accumulation_index * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            const size_t pixel_base =
                ((size_t)y * (size_t)pixel_width + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            const uint16_t sample_count =
                accumulation->sampleCountBuffer ? accumulation->sampleCountBuffer[accumulation_index]
                                                : 0u;
            float resolved_r = 0.0f;
            float resolved_g = 0.0f;
            float resolved_b = 0.0f;
            runtime_native_3d_temporal_accum_resolve_history_rgb(accumulation,
                                                                 accumulation_base,
                                                                 sample_count,
                                                                 &resolved_r,
                                                                 &resolved_g,
                                                                 &resolved_b);
            pixel_buffer[pixel_base] = TonemapCurveToByteWithFloor(resolved_r, environment);
            pixel_buffer[pixel_base + 1u] = TonemapCurveToByteWithFloor(resolved_g, environment);
            pixel_buffer[pixel_base + 2u] = TonemapCurveToByteWithFloor(resolved_b, environment);
            pixel_buffer[pixel_base + 3u] = 0xFFu;
        }
    }
}

void RuntimeNative3DTemporalAccumulation_ResolveToPixelBufferAtOffset(
    const RuntimeNative3DTemporalAccumulation* accumulation,
    uint8_t* pixel_buffer,
    int pixel_width,
    int dst_origin_x,
    int dst_origin_y) {
    const uint8_t environment = RuntimeNative3DResolveEnvironmentByte();
    if (!accumulation || !accumulation->accumulationBuffer || !pixel_buffer || pixel_width <= 0) {
        return;
    }
    if (dst_origin_x < 0 || dst_origin_y < 0) {
        return;
    }

    for (int y = 0; y < accumulation->height; ++y) {
        const int dst_y = dst_origin_y + y;
        for (int x = 0; x < accumulation->width; ++x) {
            const int dst_x = dst_origin_x + x;
            const size_t accumulation_index =
                (size_t)y * (size_t)accumulation->width + (size_t)x;
            const size_t accumulation_base =
                accumulation_index * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            const size_t pixel_base =
                ((size_t)dst_y * (size_t)pixel_width + (size_t)dst_x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            const uint16_t sample_count =
                accumulation->sampleCountBuffer ? accumulation->sampleCountBuffer[accumulation_index]
                                                : 0u;
            float resolved_r = 0.0f;
            float resolved_g = 0.0f;
            float resolved_b = 0.0f;
            runtime_native_3d_temporal_accum_resolve_history_rgb(accumulation,
                                                                 accumulation_base,
                                                                 sample_count,
                                                                 &resolved_r,
                                                                 &resolved_g,
                                                                 &resolved_b);
            pixel_buffer[pixel_base] = TonemapCurveToByteWithFloor(resolved_r, environment);
            pixel_buffer[pixel_base + 1u] = TonemapCurveToByteWithFloor(resolved_g, environment);
            pixel_buffer[pixel_base + 2u] = TonemapCurveToByteWithFloor(resolved_b, environment);
            pixel_buffer[pixel_base + 3u] = 0xFFu;
        }
    }
}
