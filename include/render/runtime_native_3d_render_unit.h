#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_UNIT_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_UNIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_temporal_accum.h"

typedef struct {
    const RuntimeNative3DPreparedFrame* frame;
    RuntimeNative3DSamplingContext baseSampling;
    RuntimeNative3DTemporalAccumulation accumulation;
    RuntimeNative3DAdaptiveSamplingMask adaptiveMask;
    RuntimeNative3DAdaptivePixelStateBuffer adaptivePixelState;
    RuntimeNative3DFeatureBuffer featureBuffer;
    float* subpassRadiance;
    float* resolvedRadiance;
    RayTracing3DIntegratorId integratorId;
    int startX;
    int startY;
    int endX;
    int endY;
    int width;
    int height;
    int temporalFrames;
    int committedSubpasses;
    size_t radianceCapacity;
    uint64_t scratchSetupCalls;
    uint64_t scratchCacheAcquireHits;
    uint64_t scratchCacheAcquireMisses;
    uint64_t radianceScratchResizeCalls;
    uint64_t radianceScratchReuseCalls;
    uint64_t radianceScratchClearBytes;
    uint64_t radianceScratchRequestedBytesMax;
    uint64_t radianceScratchCapacityBytesMax;
    bool useAdaptiveSampling;
    bool useDisneyTemporalPruning;
    bool useDenoise;
    bool measureAdaptiveState;
    bool featuresPrepared;
} RuntimeNative3DRenderUnit;

void RuntimeNative3DRenderUnit_Init(RuntimeNative3DRenderUnit* unit);
void RuntimeNative3DRenderUnit_Free(RuntimeNative3DRenderUnit* unit);
void RuntimeNative3DRenderUnit_TakeReusable(RuntimeNative3DRenderUnit* unit);
void RuntimeNative3DRenderUnit_ReturnReusable(RuntimeNative3DRenderUnit* unit);
void RuntimeNative3DRenderUnit_ReleaseReusableScratchCache(void);
bool RuntimeNative3DRenderUnit_Setup(RuntimeNative3DRenderUnit* unit,
                                     RayTracing3DIntegratorId integrator_id,
                                     const RuntimeNative3DPreparedFrame* frame,
                                     int start_x,
                                     int start_y,
                                     int end_x,
                                     int end_y,
                                     const RuntimeNative3DSamplingContext* sampling,
                                     int temporal_frames,
                                     bool disney_denoise_enabled);
bool RuntimeNative3DRenderUnit_ShouldRenderSubpass(const RuntimeNative3DRenderUnit* unit,
                                                   int subpass_index);
bool RuntimeNative3DRenderUnit_RenderSubpass(RuntimeNative3DRenderUnit* unit,
                                             int subpass_index,
                                             RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderUnit_ResolveCurrentToPixels(const RuntimeNative3DRenderUnit* unit,
                                                      uint8_t* pixel_buffer,
                                                      int pixel_width);
bool RuntimeNative3DRenderUnit_ResolveCurrentRawToPixels(const RuntimeNative3DRenderUnit* unit,
                                                         uint8_t* pixel_buffer,
                                                         int pixel_width);
bool RuntimeNative3DRenderUnit_ResolveTemporalBudgetHeatmapToPixels(
    const RuntimeNative3DRenderUnit* unit,
    uint8_t* pixel_buffer,
    int pixel_width);
bool RuntimeNative3DRenderUnit_ResolveCurrentToPixelsWithStats(
    const RuntimeNative3DRenderUnit* unit,
    uint8_t* pixel_buffer,
    int pixel_width,
    RuntimeNative3DRenderStats* out_stats);
int RuntimeNative3DRenderUnit_CommittedSubpasses(const RuntimeNative3DRenderUnit* unit);
void RuntimeNative3DRenderUnit_GetActivityCounts(const RuntimeNative3DRenderUnit* unit,
                                                 int* out_active_pixels,
                                                 int* out_active_tiles,
                                                 int* out_inactive_tiles);
void RuntimeNative3DRenderUnit_GetAdaptiveStateSummary(
    const RuntimeNative3DRenderUnit* unit,
    RuntimeNative3DAdaptivePixelStateSummary* out_summary);
void RuntimeNative3DRenderUnit_RecordScratchStats(const RuntimeNative3DRenderUnit* unit,
                                                  RuntimeNative3DRenderStats* out_stats);

#endif
