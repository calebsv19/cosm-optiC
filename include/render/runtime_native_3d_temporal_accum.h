#ifndef RENDER_RUNTIME_NATIVE_3D_TEMPORAL_ACCUM_H
#define RENDER_RUNTIME_NATIVE_3D_TEMPORAL_ACCUM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum {
    RUNTIME_NATIVE_3D_CONVERGENCE_MIN_SAMPLES = 8,
    RUNTIME_NATIVE_3D_CONVERGENCE_STABLE_CHECKS = 3
};

typedef struct {
    /* Per-pixel mean of accepted samples within the current output frame. */
    float* accumulationBuffer;
    float* activityBuffer;
    /* Unfiltered, unclamped RGB moments drive convergence only. */
    float* rawMeanBuffer;
    float* rawM2Buffer;
    uint8_t* stableSampleStreak;
    uint16_t* sampleCountBuffer;
    int width;
    int height;
    int completedSubpasses;
} RuntimeNative3DTemporalAccumulation;

bool RuntimeNative3DTemporalAccumulation_PixelConverged(
    const RuntimeNative3DTemporalAccumulation* accumulation, size_t pixel);

void RuntimeNative3DTemporalAccumulation_Init(RuntimeNative3DTemporalAccumulation* accumulation);
void RuntimeNative3DTemporalAccumulation_Free(RuntimeNative3DTemporalAccumulation* accumulation);
bool RuntimeNative3DTemporalAccumulation_Ensure(RuntimeNative3DTemporalAccumulation* accumulation,
                                                int width,
                                                int height);
void RuntimeNative3DTemporalAccumulation_Clear(RuntimeNative3DTemporalAccumulation* accumulation);
bool RuntimeNative3DTemporalAccumulation_AddRegion(RuntimeNative3DTemporalAccumulation* accumulation,
                                                   const float* radiance_region,
                                                   int radiance_stride,
                                                   int start_x,
                                                   int start_y,
                                                   int end_x,
                                                   int end_y);
bool RuntimeNative3DTemporalAccumulation_AddRegionSamples(
    RuntimeNative3DTemporalAccumulation* accumulation,
    const float* radiance_region,
    int radiance_stride,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const uint8_t* sample_mask,
    int sample_mask_stride);
void RuntimeNative3DTemporalAccumulation_CommitSubpass(
    RuntimeNative3DTemporalAccumulation* accumulation);
bool RuntimeNative3DTemporalAccumulation_ResolveRegionToRadianceBuffer(
    const RuntimeNative3DTemporalAccumulation* accumulation,
    float* radiance_buffer,
    int radiance_stride,
    int start_x,
    int start_y,
    int end_x,
    int end_y);
void RuntimeNative3DTemporalAccumulation_ResolveRegionToPixelBuffer(
    const RuntimeNative3DTemporalAccumulation* accumulation,
    uint8_t* pixel_buffer,
    int pixel_width,
    int start_x,
    int start_y,
    int end_x,
    int end_y);
void RuntimeNative3DTemporalAccumulation_ResolveToPixelBufferAtOffset(
    const RuntimeNative3DTemporalAccumulation* accumulation,
    uint8_t* pixel_buffer,
    int pixel_width,
    int dst_origin_x,
    int dst_origin_y);

#endif
