#ifndef RENDER_RUNTIME_NATIVE_3D_FRAME_DENOISE_H
#define RENDER_RUNTIME_NATIVE_3D_FRAME_DENOISE_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_render_unit.h"

typedef struct RuntimeNative3DGuideSpan {
    size_t start, end, localStart;
    const RuntimeNative3DFeatureBuffer* source;
} RuntimeNative3DGuideSpan;

typedef struct RuntimeNative3DFrameDenoise {
    RuntimeNative3DGuideSpan* guideSpans;
    size_t* guideRowOffsets;
    size_t guideSpanCount, guideSpanCapacity;
    bool materialGuides, guideSpansReady;
    /* Scheduler may omit proven-empty occupancy tiles; their cleared hit mask
     * must remain zero. Ordinary callers require complete nonoverlapping coverage. */
    bool allowSparseGuides;
    float* radianceBuffer;
    float* temporalActivityBuffer;
    RuntimeNative3DFeatureBuffer featureBuffer;
    RayTracing3DIntegratorId integratorId;
    int width;
    int height;
    int temporalFrames;
    bool applied;
} RuntimeNative3DFrameDenoise;

void RuntimeNative3DFrameDenoise_Init(RuntimeNative3DFrameDenoise* frame_denoise);
void RuntimeNative3DFrameDenoise_Free(RuntimeNative3DFrameDenoise* frame_denoise);
bool RuntimeNative3DFrameDenoise_Prepare(RuntimeNative3DFrameDenoise* frame_denoise,
                                         int width,
                                         int height,
                                         RayTracing3DIntegratorId integrator_id,
                                         int temporal_frames);
bool RuntimeNative3DFrameDenoise_PrepareWithGuides(
    RuntimeNative3DFrameDenoise* frame_denoise, int width, int height,
    RayTracing3DIntegratorId integrator_id, int temporal_frames, bool material_guides);
bool RuntimeNative3DFrameDenoise_GatherUnit(RuntimeNative3DFrameDenoise* frame_denoise,
                                            RuntimeNative3DRenderUnit* unit);
bool RuntimeNative3DFrameDenoise_Apply(RuntimeNative3DFrameDenoise* frame_denoise,
                                       RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DFrameDenoise_ResolveUnitToPixels(
    const RuntimeNative3DFrameDenoise* frame_denoise,
    RuntimeNative3DRenderUnit* unit,
    uint8_t* pixel_buffer,
    int pixel_width);

#endif
