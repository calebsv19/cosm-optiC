#ifndef RENDER_RUNTIME_NATIVE_3D_ADAPTIVE_SAMPLING_H
#define RENDER_RUNTIME_NATIVE_3D_ADAPTIVE_SAMPLING_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_scene_3d.h"

#define RUNTIME_NATIVE_3D_ADAPTIVE_TILE_SIZE 16
#define RUNTIME_NATIVE_3D_ADAPTIVE_MIN_SUBPASSES 2

bool RuntimeNative3DAdaptiveSampling_RuntimeEnabled(void);
void RuntimeNative3DAdaptiveSampling_SetRuntimeOverride(bool has_override, bool enabled);

typedef struct {
    uint8_t* stableEmitterMask;
    uint8_t* activeSampleMask;
    uint8_t* scratchSampleMask;
    uint8_t* activeTileMask;
    int width;
    int height;
    int tileSize;
    int tilesX;
    int tilesY;
    int minSubpassesBeforePrune;
    int activePixelCount;
    int activeTileCount;
    int inactiveTileCount;
} RuntimeNative3DAdaptiveSamplingMask;

enum {
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_STABLE = 1u << 0,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVE = 1u << 1,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_PROBE = 1u << 2,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_HIGH_RISK = 1u << 3
};

typedef struct {
    uint16_t sampleCount;
    uint16_t probeCountdown;
    float meanLuma;
    float radianceDelta;
    float risk;
    uint8_t flags;
} RuntimeNative3DAdaptivePixelState;

typedef struct {
    int stablePixelCount;
    int activePixelCount;
    int probePixelCount;
    int highRiskPixelCount;
    int stableTileCount;
    int activeTileCount;
    int probeTileCount;
    int highRiskTileCount;
    int measuredPixelCount;
    int minSampleFloor;
} RuntimeNative3DAdaptivePixelStateSummary;

typedef struct {
    RuntimeNative3DAdaptivePixelState* pixels;
    RuntimeNative3DAdaptivePixelStateSummary summary;
    int width;
    int height;
    int tileSize;
    int tilesX;
    int tilesY;
} RuntimeNative3DAdaptivePixelStateBuffer;

void RuntimeNative3DAdaptiveSamplingMask_Init(RuntimeNative3DAdaptiveSamplingMask* mask);
void RuntimeNative3DAdaptiveSamplingMask_Free(RuntimeNative3DAdaptiveSamplingMask* mask);
bool RuntimeNative3DAdaptiveSamplingMask_Ensure(RuntimeNative3DAdaptiveSamplingMask* mask,
                                                int width,
                                                int height);
void RuntimeNative3DAdaptiveSamplingMask_Clear(RuntimeNative3DAdaptiveSamplingMask* mask);
void RuntimeNative3DAdaptivePixelStateBuffer_Init(RuntimeNative3DAdaptivePixelStateBuffer* state);
void RuntimeNative3DAdaptivePixelStateBuffer_Free(RuntimeNative3DAdaptivePixelStateBuffer* state);
bool RuntimeNative3DAdaptivePixelStateBuffer_Ensure(RuntimeNative3DAdaptivePixelStateBuffer* state,
                                                    int width,
                                                    int height);
void RuntimeNative3DAdaptivePixelStateBuffer_Clear(RuntimeNative3DAdaptivePixelStateBuffer* state);
bool RuntimeNative3DAdaptiveSampling_MeasurePixelState(
    RuntimeNative3DAdaptivePixelStateBuffer* state,
    const RuntimeNative3DTemporalAccumulation* accumulation,
    const RuntimeNative3DFeatureBuffer* features,
    int tile_size,
    int min_sample_floor,
    int probe_period);
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
bool RuntimeNative3DAdaptiveSampling_BeginTemporalActivityMask(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    int width,
    int height,
    int tile_size,
    int min_subpasses_before_prune);
bool RuntimeNative3DAdaptiveSampling_RefreshTemporalActivityMask(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    const RuntimeNative3DTemporalAccumulation* accumulation,
    const RuntimeNative3DFeatureBuffer* features);
bool RuntimeNative3DAdaptiveSampling_RefreshActivityMaskFromPixelState(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    const RuntimeNative3DAdaptivePixelStateBuffer* state,
    int tile_size);
bool RuntimeNative3DAdaptiveSampling_HasActiveSamples(
    const RuntimeNative3DAdaptiveSamplingMask* mask);
bool RuntimeNative3DAdaptiveSampling_RenderPreparedRegionRadianceRGBMasked(
    float* radiance_buffer,
    int radiance_stride,
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
