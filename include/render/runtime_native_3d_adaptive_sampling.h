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
#ifndef RUNTIME_NATIVE_3D_ADAPTIVE_REGION_COUNT
#define RUNTIME_NATIVE_3D_ADAPTIVE_REGION_COUNT 4
#endif

bool RuntimeNative3DAdaptiveSampling_RuntimeEnabled(void);
void RuntimeNative3DAdaptiveSampling_SetRuntimeOverride(bool has_override, bool enabled);
bool RuntimeNative3DAdaptiveSampling_RiskEarlyStopEnabled(void);
void RuntimeNative3DAdaptiveSampling_SetRiskEarlyStopOverride(bool has_override, bool enabled);
bool RuntimeNative3DAdaptiveSampling_TemporalBudgetHeatmapEnabled(void);
bool RuntimeNative3DAdaptiveSampling_FlagsSafeForEarlyStop(uint16_t flags);
int RuntimeNative3DAdaptiveSampling_BudgetBucket(uint16_t sample_count, int min_sample_floor);

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
    int conservativeEarlyStopEligiblePixelCount;
    int conservativeEarlyStopBaseActivePixelCount;
    int conservativeEarlyStopPaddingHoldPixelCount;
    int conservativeEarlyStopPaddingHoldHighSeedPixelCount;
    int conservativeEarlyStopPaddingHoldMediumSeedPixelCount;
    int conservativeEarlyStopPaddingHoldRegionCounts[RUNTIME_NATIVE_3D_ADAPTIVE_REGION_COUNT];
} RuntimeNative3DAdaptiveSamplingMask;

enum {
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_STABLE = 1u << 0,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVE = 1u << 1,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_PROBE = 1u << 2,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_HIGH_RISK = 1u << 3,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVITY_RISK = 1u << 4,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_MATERIAL_RISK = 1u << 5,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_TRANSPARENT_RISK = 1u << 6,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_GEOMETRY_EDGE_RISK = 1u << 7,
    RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_DIRECT_LIGHT_RISK = 1u << 8
};

typedef struct {
    uint16_t sampleCount;
    uint16_t probeCountdown;
    float meanLuma;
    float radianceDelta;
    float risk;
    uint16_t flags;
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
    int activityRiskPixelCount;
    int materialRiskPixelCount;
    int transparentRiskPixelCount;
    int glossyRiskPixelCount;
    int geometryEdgeRiskPixelCount;
    int directLightNoTracePixelCount;
    int directLightClearVisiblePixelCount;
    int directLightClearBlockedPixelCount;
    int directLightStablePartialPixelCount;
    int directLightMixedPartialPixelCount;
    int directLightBoundaryRiskPixelCount;
    int earlyStopEligiblePixelCount;
    int earlyStopHeldPixelCount;
    int earlyStopHoldProbePixelCount;
    int earlyStopHoldHighRiskPixelCount;
    int earlyStopHoldActivityRiskPixelCount;
    int earlyStopHoldMaterialRiskPixelCount;
    int earlyStopHoldTransparentRiskPixelCount;
    int earlyStopHoldGeometryEdgeRiskPixelCount;
    int earlyStopHoldDirectLightRiskPixelCount;
    int earlyStopEligibleRegionCounts[RUNTIME_NATIVE_3D_ADAPTIVE_REGION_COUNT];
    int earlyStopHeldRegionCounts[RUNTIME_NATIVE_3D_ADAPTIVE_REGION_COUNT];
    int budgetBucketPixelCounts[RUNTIME_NATIVE_3D_TEMPORAL_BUDGET_BUCKET_COUNT];
    int budgetActiveBucketPixelCounts[RUNTIME_NATIVE_3D_TEMPORAL_BUDGET_BUCKET_COUNT];
    int budgetEligibleBucketPixelCounts[RUNTIME_NATIVE_3D_TEMPORAL_BUDGET_BUCKET_COUNT];
    int budgetHeldBucketPixelCounts[RUNTIME_NATIVE_3D_TEMPORAL_BUDGET_BUCKET_COUNT];
    int budgetClearVisibleEligiblePixelCount;
    int budgetClearVisibleHeldPixelCount;
    int budgetPartialHeldPixelCount;
    int budgetTransparentHeldPixelCount;
    int budgetGeometryHeldPixelCount;
    int budgetActivityHeldPixelCount;
    int mixedRiskTileCount;
    int minSampleFloor;
    double riskSum;
    double riskMax;
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
bool RuntimeNative3DAdaptiveSampling_RefreshConservativeEarlyStopMaskFromPixelState(
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
