#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_caustic_bootstrap_3d.h"
#include "render/runtime_caustic_surface_cache_3d.h"
#include "render/runtime_caustic_transport_3d.h"
#include "render/runtime_caustic_volume_cache_3d.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"
#include "render/runtime_native_3d_prepare_cache.h"
#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_native_3d_tile_occupancy.h"
#include "render/runtime_scene_3d.h"
#include "render/ray_tracing_integrator_catalog.h"

#define RUNTIME_NATIVE_3D_RADIANCE_COLOR_CHANNELS 3
#define RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL 3
#define RUNTIME_NATIVE_3D_RADIANCE_CHANNELS 4
#define RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES 4

typedef struct {
    int hitPixelCount;
    int visiblePixelCount;
    int bouncePixelCount;
    int secondaryRayCount;
    int secondaryHitCount;
    int secondaryContributingHitCount;
    int emissiveAreaCandidateCount;
    int emissiveAreaSelectedCandidateCount;
    int emissiveAreaVisibilityRayCount;
    int emissiveAreaPrimarySampleCount;
    int emissiveAreaRecursiveSampleCount;
    int emissiveAreaRecursivePolicySkipCount;
    int emissiveAreaRecursiveCandidateCapSkipCount;
    int emissiveAreaRecursiveTriangleCapSkipCount;
    int emissiveAreaRecursiveCandidateCap;
    int emissiveAreaRecursiveTriangleCap;
    int emissiveAreaFullScanFallbackCount;
    int causticSidecarEnabled;
    int causticSidecarSampleCount;
    int causticSidecarContributingSampleCount;
    int causticBootstrapTemporaryBridgeActive;
    int causticTransportPathEmissionActive;
    int causticVolumeCacheSuppressedNoSampleableVolume;
    int causticTransportLightCount;
    int causticTransportEvaluatedPathCount;
    int causticTransportEmittedPathCount;
    int causticTransportAnalyticSphereLensResolvedCount;
    int causticTransportAnalyticSphereLensRejectedCount;
    int causticTransportAnalyticSphereLensEvaluatedPathCount;
    int causticTransportAnalyticSphereLensEmittedPathCount;
    double causticTransportAnalyticSphereLensSampleWeight;
    double causticTransportAnalyticSphereLensTotalSampleWeight;
    int causticTransportAnalyticCylinderLensResolvedCount;
    int causticTransportAnalyticCylinderLensRejectedCount;
    int causticTransportAnalyticCylinderLensEvaluatedPathCount;
    int causticTransportAnalyticCylinderLensEmittedPathCount;
    double causticTransportAnalyticCylinderLensSampleWeight;
    double causticTransportAnalyticCylinderLensTotalSampleWeight;
    int causticTransportAnalyticPrismLensResolvedCount;
    int causticTransportAnalyticPrismLensRejectedCount;
    int causticTransportAnalyticPrismLensEvaluatedPathCount;
    int causticTransportAnalyticPrismLensEmittedPathCount;
    double causticTransportAnalyticPrismLensSampleWeight;
    double causticTransportAnalyticPrismLensTotalSampleWeight;
    int causticTransportAnalyticBowlLensResolvedCount;
    int causticTransportAnalyticBowlLensRejectedCount;
    int causticTransportAnalyticBowlLensEvaluatedPathCount;
    int causticTransportAnalyticBowlLensEmittedPathCount;
    double causticTransportAnalyticBowlLensSampleWeight;
    double causticTransportAnalyticBowlLensTotalSampleWeight;
    int causticTransportTransparentHitCount;
    int causticTransportSpecularEventCount;
    int causticTransportVolumeSegmentCount;
    int causticTransportSurfaceReceiverTraceMissCount;
    int causticTransportSurfaceReceiverDepthRejectCount;
    int causticTransportSurfaceReceiverHitCount;
    int causticTransportSurfaceReceiverFallbackCount;
    int causticVolumeCacheBound;
    int causticVolumeCacheAllocated;
    int causticVolumeCacheCellCount;
    int causticVolumeCacheNonZeroCellCount;
    int causticVolumeCacheDepositAttemptCount;
    int causticVolumeCacheDepositAcceptedCount;
    int causticVolumeCacheDepositRejectedCount;
    int causticVolumeCacheFootprintDepositCount;
    int causticVolumeCacheFootprintCellContributionCount;
    int causticVolumeCacheSampleLookupCount;
    int causticVolumeCacheSampleContributingCount;
    int causticSurfaceCacheBound;
    int causticSurfaceCacheAllocated;
    int causticSurfaceCacheRecordCapacity;
    int causticSurfaceCacheRecordCount;
    int causticSurfaceCacheDepositAttemptCount;
    int causticSurfaceCacheDepositAcceptedCount;
    int causticSurfaceCacheDepositRejectedCount;
    int causticSurfaceCacheSampleLookupCount;
    int causticSurfaceCacheSampleContributingCount;
    int volumeScatterDirectSampleCount;
    int causticVolumeScatterSampleCount;
    int causticVolumeScatterContributingSampleCount;
    int causticVolumeScatterContributingPixelCount;
    int causticVolumeScatterPixelMinX;
    int causticVolumeScatterPixelMinY;
    int causticVolumeScatterPixelMaxX;
    int causticVolumeScatterPixelMaxY;
    int mirrorDominantPixelCount;
    int mirrorBaseAttenuatedPixelCount;
    int mirrorReflectionHitPixelCount;
    int mirrorEmitterReflectionPixelCount;
    int mirrorGeometryReflectionPixelCount;
    int temporalCommittedSubpasses;
    int temporalPixelsRendered;
    int temporalPixelsSkipped;
    int temporalActivePixelCount;
    int temporalActiveTileCount;
    int temporalInactiveTileCount;
    int temporalPlannedParentTileCount;
    int temporalEmittedTileJobCount;
    int temporalOccupancySkippedTileCount;
    int temporalDispatchedTileJobCount;
    int temporalCompletedTileJobCount;
    int temporalProgressDirtyBatchCount;
    int temporalProgressDirtyTileCount;
    int temporalDirtyPreviewPresentCount;
    int temporalConservativeFirstFrameTileRender;
    int temporalFinalFullResolveCount;
    int temporalHostFullResolveCount;
    int temporalFinalPreviewPresentCount;
    int temporalHistoryPromoteCount;
    int temporalAdaptiveStateMeasuredPixels;
    int temporalAdaptiveStateStablePixels;
    int temporalAdaptiveStateActivePixels;
    int temporalAdaptiveStateProbePixels;
    int temporalAdaptiveStateHighRiskPixels;
    int temporalAdaptiveStateStableTiles;
    int temporalAdaptiveStateActiveTiles;
    int temporalAdaptiveStateProbeTiles;
    int temporalAdaptiveStateHighRiskTiles;
    int temporalAdaptiveStateMinSampleFloor;
    int temporalMeasuredTileJobs;
    int temporalAdaptiveSplitParentCount;
    int temporalAdaptiveChildTileCount;
    int temporalSlowTileOriginX;
    int temporalSlowTileOriginY;
    int temporalSlowTileWidth;
    int temporalSlowTileHeight;
    int denoiseTemporalFrameCount;
    int denoiseRawPixelCount;
    int denoiseReconstructedPixelCount;
    int denoiseStableInteriorSampleCount;
    int denoiseRejectedEdgeSampleCount;
    int denoisePreservedTransparentPixelCount;
    int denoisePreservedMirrorGlossyPixelCount;
    int denoiseSkippedUnstableTemporalPixelCount;
    int denoiseSkippedInvalidSurfacePixelCount;
    double maxRadiance;
    double maxBounceRadiance;
    double maxMirrorDominance;
    double maxMirrorSpecularReflectionRadiance;
    double maxMirrorBaseRadianceBeforeAttenuation;
    double maxMirrorBaseRadianceAfterAttenuation;
    double totalBounceRadiance;
    double maxCausticSidecarRadiance;
    double totalCausticSidecarRadiance;
    double maxCausticVolumeCacheRadiance;
    double causticVolumeCacheNonZeroCellRatio;
    double causticVolumeCacheSampleHitRatio;
    double causticVolumeScatterToCacheRadianceRatio;
    double causticVolumeCacheAverageFootprintRadiusVoxels;
    double causticVolumeCacheRadianceCentroidX;
    double causticVolumeCacheRadianceCentroidY;
    double causticVolumeCacheRadianceCentroidZ;
    double causticVolumeCacheNonZeroBoundsMinX;
    double causticVolumeCacheNonZeroBoundsMinY;
    double causticVolumeCacheNonZeroBoundsMinZ;
    double causticVolumeCacheNonZeroBoundsMaxX;
    double causticVolumeCacheNonZeroBoundsMaxY;
    double causticVolumeCacheNonZeroBoundsMaxZ;
    double totalCausticVolumeCacheRadianceR;
    double totalCausticVolumeCacheRadianceG;
    double totalCausticVolumeCacheRadianceB;
    double totalCausticVolumeCacheFootprintInputRadianceR;
    double totalCausticVolumeCacheFootprintInputRadianceG;
    double totalCausticVolumeCacheFootprintInputRadianceB;
    double totalCausticVolumeCacheFootprintDepositedRadianceR;
    double totalCausticVolumeCacheFootprintDepositedRadianceG;
    double totalCausticVolumeCacheFootprintDepositedRadianceB;
    double maxCausticSurfaceCacheRadiance;
    double causticSurfaceCacheNearestSampleDistance;
    double causticSurfaceCacheNearestSampleRadius;
    double causticSurfaceCacheNearestSampleNormalDot;
    double causticSurfaceCacheNearestSampleCandidateCount;
    double totalCausticSurfaceCacheRadianceR;
    double totalCausticSurfaceCacheRadianceG;
    double totalCausticSurfaceCacheRadianceB;
    double totalCausticSurfaceRadianceR;
    double totalCausticSurfaceRadianceG;
    double totalCausticSurfaceRadianceB;
    double totalDirectVolumeScatterRadianceR;
    double totalDirectVolumeScatterRadianceG;
    double totalDirectVolumeScatterRadianceB;
    double totalCausticVolumeScatterRadianceR;
    double totalCausticVolumeScatterRadianceG;
    double totalCausticVolumeScatterRadianceB;
    double totalCausticVolumeScatterSampledCacheRadiance;
    double maxCausticVolumeScatterSampledCacheRadiance;
    double totalCausticVolumeScatterSampledRawDensity;
    double maxCausticVolumeScatterSampledRawDensity;
    double totalCausticVolumeScatterSampledDensity;
    double maxCausticVolumeScatterSampledDensity;
    double totalCausticVolumeScatterProbability;
    double maxCausticVolumeScatterProbability;
    double totalCausticVolumeScatterCameraTransmittance;
    double minCausticVolumeScatterCameraTransmittance;
    double maxCausticVolumeScatterCameraTransmittance;
    double totalCausticVolumeScatterVisibilityTerm;
    double maxCausticVolumeScatterVisibilityTerm;
    double totalCausticVolumeScatterPixelX;
    double totalCausticVolumeScatterPixelY;
    double totalMirrorSpecularReflectionRadiance;
    double totalMirrorBaseRadianceBeforeAttenuation;
    double totalMirrorBaseRadianceAfterAttenuation;
    double temporalTotalTileMs;
    double temporalMaxTileMs;
    double temporalAverageTileMs;
    double temporalMaxTileSubpassMs;
    double denoiseRawRadianceLumaTotal;
    double denoiseReconstructedRadianceLumaTotal;
} RuntimeNative3DRenderStats;

typedef struct {
    RuntimeScene3D scene;
    /* Stable scene backing for shallow per-subpass frame copies. */
    const RuntimeScene3D* traceScene;
    RuntimeCameraProjector3D projector;
    RuntimeNative3DTileOccupancy tileOccupancy;
    RuntimeNative3DSamplingContext sampling;
    RuntimeCausticVolumeCache3D causticVolumeCache;
    RuntimeCausticSurfaceCache3D causticSurfaceCache;
    RuntimeDisneyV2CausticSidecarProbe3D causticSidecarProbe;
    RuntimeCausticBootstrap3DDiagnostics causticBootstrapDiagnostics;
    RuntimeCausticTransport3DDiagnostics causticTransportDiagnostics;
    double causticCachePrepMs;
    int width;
    int height;
    bool tileOccupancyConservativeAllTiles;
    bool causticSidecarProbeValid;
    bool valid;
} RuntimeNative3DPreparedFrame;

typedef struct {
    int cpuPercent;
    int maxWorkerThreads;
    int reserveCpuCount;
} RuntimeNative3DResourceBudget;

typedef void (*RuntimeNative3DTemporalProgressCallback)(int started_subpasses,
                                                        int completed_subpasses,
                                                        int total_subpasses,
                                                        void* user_data);
typedef void (*RuntimeNative3DTemporalTileProgressCallback)(
    int started_subpasses,
    int completed_subpasses,
    int total_subpasses,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    void* user_data);

void RuntimeNative3DRenderStats_Accumulate(RuntimeNative3DRenderStats* dst,
                                           const RuntimeNative3DRenderStats* src);
void RuntimeNative3DRender_ResetInspectionCameraOverrides(void);
void RuntimeNative3DRender_SetInspectionCameraPosition(Vec3 position);
void RuntimeNative3DRender_SetInspectionCameraLookAt(Vec3 target);
const char* RuntimeNative3DPrepareFrameLastDiagnostics(void);
bool RuntimeNative3DPrepareFrame(RuntimeNative3DPreparedFrame* out_frame,
                                 int width,
                                 int height,
                                 double normalized_t,
                                 double live_light_x,
                                 double live_light_y);
bool RuntimeNative3DPrepareFrameAtFrameIndex(RuntimeNative3DPreparedFrame* out_frame,
                                             int width,
                                             int height,
                                             double normalized_t,
                                             int frame_index,
                                             double live_light_x,
                                             double live_light_y);
bool RuntimeNative3DPrepareFrameWithSampling(RuntimeNative3DPreparedFrame* out_frame,
                                             int width,
                                             int height,
                                             double normalized_t,
                                             double live_light_x,
                                             double live_light_y,
                                             const RuntimeNative3DSamplingContext* sampling);
bool RuntimeNative3DPrepareFrameWithSamplingAtFrameIndex(
    RuntimeNative3DPreparedFrame* out_frame,
    int width,
    int height,
    double normalized_t,
    int frame_index,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling);
void RuntimeNative3DPreparedFrame_Free(RuntimeNative3DPreparedFrame* frame);
bool RuntimeNative3DRenderPreparedRegion(uint8_t* pixel_buffer,
                                         RayTracing3DIntegratorId integrator_id,
                                         const RuntimeNative3DPreparedFrame* frame,
                                         int start_x,
                                         int start_y,
                                         int end_x,
                                         int end_y,
                                         RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderPreparedRegionRadianceRGB(float* radiance_buffer,
                                                    int radiance_stride,
                                                    RayTracing3DIntegratorId integrator_id,
                                                    const RuntimeNative3DPreparedFrame* frame,
                                                    int start_x,
                                                    int start_y,
                                                    int end_x,
                                                    int end_y,
                                                    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderPreparedRegionLuminance(float* luminance_buffer,
                                                  int luminance_stride,
                                                  RayTracing3DIntegratorId integrator_id,
                                                  const RuntimeNative3DPreparedFrame* frame,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y,
                                                  RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DPrepareFrameTileOccupancy(RuntimeNative3DPreparedFrame* frame, int tile_size);
bool RuntimeNative3DPreparedRegionMayContainGeometry(const RuntimeNative3DPreparedFrame* frame,
                                                     int start_x,
                                                     int start_y,
                                                     int end_x,
                                                     int end_y);
void RuntimeNative3DResolveRadianceRegionToPixels(uint8_t* pixel_buffer,
                                                  int pixel_width,
                                                  const float* radiance_buffer,
                                                  int radiance_stride,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y);
bool RuntimeNative3DRenderToPixelBuffer(uint8_t* pixel_buffer,
                                        RayTracing3DIntegratorId integrator_id,
                                        int width,
                                        int height,
                                        double normalized_t,
                                        double live_light_x,
                                        double live_light_y,
                                        RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderToPixelBufferWithSampling(uint8_t* pixel_buffer,
                                                    RayTracing3DIntegratorId integrator_id,
                                                    int width,
                                                    int height,
                                                    double normalized_t,
                                                    double live_light_x,
                                                    double live_light_y,
                                                    const RuntimeNative3DSamplingContext* sampling,
                                                    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgress(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgressAtFrameIndex(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    int frame_index,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalDetailedProgressAtFrameIndex(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    int frame_index,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DTemporalTileProgressCallback tile_progress_callback,
    void* tile_progress_user_data,
    RuntimeNative3DRenderStats* out_stats);
bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporalDetailedProgressBudgetedAtFrameIndex(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    int frame_index,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DTemporalTileProgressCallback tile_progress_callback,
    void* tile_progress_user_data,
    const RuntimeNative3DResourceBudget* resource_budget,
    RuntimeNative3DRenderStats* out_stats);
uint8_t RuntimeNative3DResolveEnvironmentByte(void);
void RuntimeNative3DFillPixelBufferEnvironment(uint8_t* pixel_buffer, size_t pixel_count);
void RuntimeNative3DFillPixelBufferBackground(uint8_t* pixel_buffer,
                                              int width,
                                              int height,
                                              const RuntimeScene3D* scene,
                                              const RuntimeCameraProjector3D* projector);

#endif
