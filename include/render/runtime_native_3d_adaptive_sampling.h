#ifndef RENDER_RUNTIME_NATIVE_3D_ADAPTIVE_SAMPLING_H
#define RENDER_RUNTIME_NATIVE_3D_ADAPTIVE_SAMPLING_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_scene_3d.h"

typedef struct {
    uint8_t* stableEmitterMask;
    uint8_t* activeSampleMask;
    int width;
    int height;
} RuntimeNative3DAdaptiveSamplingMask;

void RuntimeNative3DAdaptiveSamplingMask_Init(RuntimeNative3DAdaptiveSamplingMask* mask);
void RuntimeNative3DAdaptiveSamplingMask_Free(RuntimeNative3DAdaptiveSamplingMask* mask);
bool RuntimeNative3DAdaptiveSamplingMask_Ensure(RuntimeNative3DAdaptiveSamplingMask* mask,
                                                int width,
                                                int height);
void RuntimeNative3DAdaptiveSamplingMask_Clear(RuntimeNative3DAdaptiveSamplingMask* mask);
bool RuntimeNative3DAdaptiveSampling_ShouldUse(RayTracing3DIntegratorId integrator_id,
                                               int temporal_frames);
bool RuntimeNative3DAdaptiveSampling_BuildStableEmitterMask(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    int start_x,
    int start_y,
    int end_x,
    int end_y);
bool RuntimeNative3DAdaptiveSampling_HasActiveSamples(
    const RuntimeNative3DAdaptiveSamplingMask* mask);
bool RuntimeNative3DAdaptiveSampling_RenderPreparedRegionLuminanceMasked(
    float* luminance_buffer,
    int luminance_stride,
    RayTracing3DIntegratorId integrator_id,
    const RuntimeNative3DPreparedFrame* frame,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const uint8_t* active_mask,
    int active_mask_stride,
    RuntimeNative3DRenderStats* out_stats);

#endif
