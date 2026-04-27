#include "render/runtime_native_3d_temporal_accum.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "render/integrators/hybrid/integrator_tonemap.h"

void RuntimeNative3DTemporalAccumulation_Init(RuntimeNative3DTemporalAccumulation* accumulation) {
    if (!accumulation) return;
    memset(accumulation, 0, sizeof(*accumulation));
}

void RuntimeNative3DTemporalAccumulation_Free(RuntimeNative3DTemporalAccumulation* accumulation) {
    if (!accumulation) return;
    free(accumulation->accumulationBuffer);
    memset(accumulation, 0, sizeof(*accumulation));
}

bool RuntimeNative3DTemporalAccumulation_Ensure(RuntimeNative3DTemporalAccumulation* accumulation,
                                                int width,
                                                int height) {
    float* resized = NULL;
    size_t count = 0;
    if (!accumulation || width <= 0 || height <= 0) return false;
    if (accumulation->accumulationBuffer &&
        accumulation->width == width &&
        accumulation->height == height) {
        return true;
    }

    count = (size_t)width * (size_t)height;
    resized = (float*)calloc(count, sizeof(*resized));
    if (!resized) {
        return false;
    }

    free(accumulation->accumulationBuffer);
    accumulation->accumulationBuffer = resized;
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
    memset(accumulation->accumulationBuffer, 0, count * sizeof(*accumulation->accumulationBuffer));
    accumulation->completedSubpasses = 0;
}

bool RuntimeNative3DTemporalAccumulation_AddRegion(RuntimeNative3DTemporalAccumulation* accumulation,
                                                   const float* luminance_region,
                                                   int luminance_stride,
                                                   int start_x,
                                                   int start_y,
                                                   int end_x,
                                                   int end_y) {
    if (!accumulation || !accumulation->accumulationBuffer || !luminance_region ||
        luminance_stride <= 0) {
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
                (size_t)local_y * (size_t)luminance_stride + (size_t)local_x;
            accumulation->accumulationBuffer[accumulation_index] += luminance_region[region_index];
        }
    }
    return true;
}

void RuntimeNative3DTemporalAccumulation_CommitSubpass(
    RuntimeNative3DTemporalAccumulation* accumulation) {
    if (!accumulation) return;
    accumulation->completedSubpasses += 1;
}

void RuntimeNative3DTemporalAccumulation_ResolveRegionToPixelBuffer(
    const RuntimeNative3DTemporalAccumulation* accumulation,
    uint8_t* pixel_buffer,
    int pixel_stride,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    const float weight =
        (accumulation && accumulation->completedSubpasses > 0)
            ? (1.0f / (float)accumulation->completedSubpasses)
            : 0.0f;
    if (!accumulation || !accumulation->accumulationBuffer || !pixel_buffer || pixel_stride <= 0) {
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
            const size_t pixel_index = (size_t)y * (size_t)pixel_stride + (size_t)x;
            const float resolved_luminance =
                accumulation->accumulationBuffer[accumulation_index] * weight;
            pixel_buffer[pixel_index] = (uint8_t)(TonemapCurve(resolved_luminance) * 255.0f);
        }
    }
}

void RuntimeNative3DTemporalAccumulation_ResolveToPixelBufferAtOffset(
    const RuntimeNative3DTemporalAccumulation* accumulation,
    uint8_t* pixel_buffer,
    int pixel_stride,
    int dst_origin_x,
    int dst_origin_y) {
    const float weight =
        (accumulation && accumulation->completedSubpasses > 0)
            ? (1.0f / (float)accumulation->completedSubpasses)
            : 0.0f;
    if (!accumulation || !accumulation->accumulationBuffer || !pixel_buffer || pixel_stride <= 0) {
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
            const size_t pixel_index = (size_t)dst_y * (size_t)pixel_stride + (size_t)dst_x;
            const float resolved_luminance =
                accumulation->accumulationBuffer[accumulation_index] * weight;
            pixel_buffer[pixel_index] = (uint8_t)(TonemapCurve(resolved_luminance) * 255.0f);
        }
    }
}
