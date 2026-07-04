#include "render/runtime_native_3d_render.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_native_3d_render_internal.h"
#include "render/runtime_native_3d_render_unit.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_scene_3d_samples.h"

static double runtime_native_3d_render_clamp_unit(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

uint8_t RuntimeNative3DResolveEnvironmentByte(void) {
    RuntimeEnvironment3D environment;
    double value = 0.0;
    RuntimeEnvironment3D_ResolveFromAnimationConfig(&environment, &animSettings);
    value = RuntimeEnvironment3D_BackgroundBrightness(&environment) * 255.0;
    if (value < 0.0) value = 0.0;
    if (value > 255.0) value = 255.0;
    return (uint8_t)lround(value);
}

void RuntimeNative3DFillPixelBufferEnvironment(uint8_t* pixel_buffer, size_t pixel_count) {
    const uint8_t environment = RuntimeNative3DResolveEnvironmentByte();
    if (!pixel_buffer) return;

    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t base = i * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        pixel_buffer[base] = environment;
        pixel_buffer[base + 1u] = environment;
        pixel_buffer[base + 2u] = environment;
        pixel_buffer[base + 3u] = 0xFFu;
    }
}

void RuntimeNative3DFillPixelBufferBackground(uint8_t* pixel_buffer,
                                              int width,
                                              int height,
                                              const RuntimeScene3D* scene,
                                              const RuntimeCameraProjector3D* projector) {
    if (!pixel_buffer || width <= 0 || height <= 0) return;
    if (!scene || !projector) {
        RuntimeNative3DFillPixelBufferEnvironment(pixel_buffer, (size_t)width * (size_t)height);
        return;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double pixel_x = (double)x;
            const double pixel_y = (double)y;
            const Ray3D primary_ray =
                RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
            double radiance_r = 0.0;
            double radiance_g = 0.0;
            double radiance_b = 0.0;
            const size_t base =
                ((size_t)y * (size_t)width + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;

            runtime_native_3d_render_background_rgb(scene,
                                                    &primary_ray,
                                                    &radiance_r,
                                                    &radiance_g,
                                                    &radiance_b);
            pixel_buffer[base] = TonemapCurveToByteWithFloor((float)radiance_r, 0u);
            pixel_buffer[base + 1u] = TonemapCurveToByteWithFloor((float)radiance_g, 0u);
            pixel_buffer[base + 2u] = TonemapCurveToByteWithFloor((float)radiance_b, 0u);
            pixel_buffer[base + 3u] = 0xFFu;
        }
    }
}

void RuntimeNative3DResolveRadianceRegionToPixels(
    uint8_t* pixel_buffer,
    int pixel_width,
    const float* radiance_buffer,
    int radiance_stride,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    if (!pixel_buffer || !radiance_buffer || pixel_width <= 0 || radiance_stride <= 0) return;
    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        for (int x = start_x; x < end_x; ++x) {
            const int local_x = x - start_x;
            const size_t pixel_base =
                ((size_t)y * (size_t)pixel_width + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            const size_t radiance_base =
                ((size_t)local_y * (size_t)radiance_stride + (size_t)local_x) *
                (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            const uint8_t environment = (uint8_t)lround(
                runtime_native_3d_render_clamp_unit(
                    radiance_buffer[radiance_base +
                                    RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL]) *
                255.0);
            pixel_buffer[pixel_base] = TonemapCurveToByteWithFloor(
                radiance_buffer[radiance_base], environment);
            pixel_buffer[pixel_base + 1u] = TonemapCurveToByteWithFloor(
                radiance_buffer[radiance_base + 1u], environment);
            pixel_buffer[pixel_base + 2u] = TonemapCurveToByteWithFloor(
                radiance_buffer[radiance_base + 2u], environment);
            pixel_buffer[pixel_base + 3u] = 0xFFu;
        }
    }
}

void RuntimeNative3DRenderStats_Accumulate(RuntimeNative3DRenderStats* dst,
                                           const RuntimeNative3DRenderStats* src) {
    if (!dst || !src) return;
    dst->hitPixelCount += src->hitPixelCount;
    dst->visiblePixelCount += src->visiblePixelCount;
    dst->bouncePixelCount += src->bouncePixelCount;
    dst->secondaryRayCount += src->secondaryRayCount;
    dst->secondaryHitCount += src->secondaryHitCount;
    dst->secondaryContributingHitCount += src->secondaryContributingHitCount;
    if (src->emissiveAreaCandidateCount > dst->emissiveAreaCandidateCount) {
        dst->emissiveAreaCandidateCount = src->emissiveAreaCandidateCount;
    }
    dst->emissiveAreaSelectedCandidateCount += src->emissiveAreaSelectedCandidateCount;
    dst->emissiveAreaVisibilityRayCount += src->emissiveAreaVisibilityRayCount;
    dst->emissiveAreaPrimarySampleCount += src->emissiveAreaPrimarySampleCount;
    dst->emissiveAreaRecursiveSampleCount += src->emissiveAreaRecursiveSampleCount;
    dst->emissiveAreaRecursivePolicySkipCount +=
        src->emissiveAreaRecursivePolicySkipCount;
    dst->emissiveAreaRecursiveCandidateCapSkipCount +=
        src->emissiveAreaRecursiveCandidateCapSkipCount;
    dst->emissiveAreaRecursiveTriangleCapSkipCount +=
        src->emissiveAreaRecursiveTriangleCapSkipCount;
    if (src->emissiveAreaRecursiveCandidateCap >
        dst->emissiveAreaRecursiveCandidateCap) {
        dst->emissiveAreaRecursiveCandidateCap =
            src->emissiveAreaRecursiveCandidateCap;
    }
    if (src->emissiveAreaRecursiveTriangleCap > dst->emissiveAreaRecursiveTriangleCap) {
        dst->emissiveAreaRecursiveTriangleCap = src->emissiveAreaRecursiveTriangleCap;
    }
    dst->emissiveAreaFullScanFallbackCount += src->emissiveAreaFullScanFallbackCount;
    if (src->causticSidecarEnabled > dst->causticSidecarEnabled) {
        dst->causticSidecarEnabled = src->causticSidecarEnabled;
    }
    dst->causticSidecarSampleCount += src->causticSidecarSampleCount;
    dst->causticSidecarContributingSampleCount +=
        src->causticSidecarContributingSampleCount;
    if (src->causticBootstrapTemporaryBridgeActive >
        dst->causticBootstrapTemporaryBridgeActive) {
        dst->causticBootstrapTemporaryBridgeActive =
            src->causticBootstrapTemporaryBridgeActive;
    }
    if (src->causticTransportPathEmissionActive >
        dst->causticTransportPathEmissionActive) {
        dst->causticTransportPathEmissionActive =
            src->causticTransportPathEmissionActive;
    }
    if (src->causticVolumeCacheSuppressedNoSampleableVolume >
        dst->causticVolumeCacheSuppressedNoSampleableVolume) {
        dst->causticVolumeCacheSuppressedNoSampleableVolume =
            src->causticVolumeCacheSuppressedNoSampleableVolume;
    }
    if (src->causticTransportLightCount > dst->causticTransportLightCount) {
        dst->causticTransportLightCount = src->causticTransportLightCount;
    }
    if (src->causticTransportEvaluatedPathCount >
        dst->causticTransportEvaluatedPathCount) {
        dst->causticTransportEvaluatedPathCount =
            src->causticTransportEvaluatedPathCount;
    }
    if (src->causticTransportEmittedPathCount >
        dst->causticTransportEmittedPathCount) {
        dst->causticTransportEmittedPathCount =
            src->causticTransportEmittedPathCount;
    }
    if (src->causticTransportTransparentHitCount >
        dst->causticTransportTransparentHitCount) {
        dst->causticTransportTransparentHitCount =
            src->causticTransportTransparentHitCount;
    }
    if (src->causticTransportSpecularEventCount >
        dst->causticTransportSpecularEventCount) {
        dst->causticTransportSpecularEventCount =
            src->causticTransportSpecularEventCount;
    }
    if (src->causticTransportVolumeSegmentCount >
        dst->causticTransportVolumeSegmentCount) {
        dst->causticTransportVolumeSegmentCount =
            src->causticTransportVolumeSegmentCount;
    }
    if (src->causticTransportSurfaceReceiverTraceMissCount >
        dst->causticTransportSurfaceReceiverTraceMissCount) {
        dst->causticTransportSurfaceReceiverTraceMissCount =
            src->causticTransportSurfaceReceiverTraceMissCount;
    }
    if (src->causticTransportSurfaceReceiverDepthRejectCount >
        dst->causticTransportSurfaceReceiverDepthRejectCount) {
        dst->causticTransportSurfaceReceiverDepthRejectCount =
            src->causticTransportSurfaceReceiverDepthRejectCount;
    }
    if (src->causticTransportSurfaceReceiverHitCount >
        dst->causticTransportSurfaceReceiverHitCount) {
        dst->causticTransportSurfaceReceiverHitCount =
            src->causticTransportSurfaceReceiverHitCount;
    }
    if (src->causticTransportSurfaceReceiverFallbackCount >
        dst->causticTransportSurfaceReceiverFallbackCount) {
        dst->causticTransportSurfaceReceiverFallbackCount =
            src->causticTransportSurfaceReceiverFallbackCount;
    }
    if (src->causticVolumeCacheBound > dst->causticVolumeCacheBound) {
        dst->causticVolumeCacheBound = src->causticVolumeCacheBound;
    }
    if (src->causticVolumeCacheAllocated > dst->causticVolumeCacheAllocated) {
        dst->causticVolumeCacheAllocated = src->causticVolumeCacheAllocated;
    }
    if (src->causticVolumeCacheCellCount > dst->causticVolumeCacheCellCount) {
        dst->causticVolumeCacheCellCount = src->causticVolumeCacheCellCount;
    }
    if (src->causticVolumeCacheNonZeroCellCount > dst->causticVolumeCacheNonZeroCellCount) {
        dst->causticVolumeCacheNonZeroCellCount = src->causticVolumeCacheNonZeroCellCount;
    }
    if (src->causticVolumeCacheDepositAttemptCount >
        dst->causticVolumeCacheDepositAttemptCount) {
        dst->causticVolumeCacheDepositAttemptCount =
            src->causticVolumeCacheDepositAttemptCount;
    }
    if (src->causticVolumeCacheDepositAcceptedCount >
        dst->causticVolumeCacheDepositAcceptedCount) {
        dst->causticVolumeCacheDepositAcceptedCount =
            src->causticVolumeCacheDepositAcceptedCount;
    }
    if (src->causticVolumeCacheDepositRejectedCount >
        dst->causticVolumeCacheDepositRejectedCount) {
        dst->causticVolumeCacheDepositRejectedCount =
            src->causticVolumeCacheDepositRejectedCount;
    }
    if (src->causticVolumeCacheFootprintDepositCount >
        dst->causticVolumeCacheFootprintDepositCount) {
        dst->causticVolumeCacheFootprintDepositCount =
            src->causticVolumeCacheFootprintDepositCount;
    }
    if (src->causticVolumeCacheFootprintCellContributionCount >
        dst->causticVolumeCacheFootprintCellContributionCount) {
        dst->causticVolumeCacheFootprintCellContributionCount =
            src->causticVolumeCacheFootprintCellContributionCount;
    }
    dst->causticVolumeCacheSampleLookupCount += src->causticVolumeCacheSampleLookupCount;
    dst->causticVolumeCacheSampleContributingCount +=
        src->causticVolumeCacheSampleContributingCount;
    if (src->causticSurfaceCacheBound > dst->causticSurfaceCacheBound) {
        dst->causticSurfaceCacheBound = src->causticSurfaceCacheBound;
    }
    if (src->causticSurfaceCacheAllocated > dst->causticSurfaceCacheAllocated) {
        dst->causticSurfaceCacheAllocated = src->causticSurfaceCacheAllocated;
    }
    if (src->causticSurfaceCacheRecordCapacity >
        dst->causticSurfaceCacheRecordCapacity) {
        dst->causticSurfaceCacheRecordCapacity =
            src->causticSurfaceCacheRecordCapacity;
    }
    if (src->causticSurfaceCacheRecordCount > dst->causticSurfaceCacheRecordCount) {
        dst->causticSurfaceCacheRecordCount = src->causticSurfaceCacheRecordCount;
    }
    if (src->causticSurfaceCacheDepositAttemptCount >
        dst->causticSurfaceCacheDepositAttemptCount) {
        dst->causticSurfaceCacheDepositAttemptCount =
            src->causticSurfaceCacheDepositAttemptCount;
    }
    if (src->causticSurfaceCacheDepositAcceptedCount >
        dst->causticSurfaceCacheDepositAcceptedCount) {
        dst->causticSurfaceCacheDepositAcceptedCount =
            src->causticSurfaceCacheDepositAcceptedCount;
    }
    if (src->causticSurfaceCacheDepositRejectedCount >
        dst->causticSurfaceCacheDepositRejectedCount) {
        dst->causticSurfaceCacheDepositRejectedCount =
            src->causticSurfaceCacheDepositRejectedCount;
    }
    dst->causticSurfaceCacheSampleLookupCount +=
        src->causticSurfaceCacheSampleLookupCount;
    dst->causticSurfaceCacheSampleContributingCount +=
        src->causticSurfaceCacheSampleContributingCount;
    if (src->causticSurfaceCacheNearestSampleCandidateCount > 0.0 &&
        (dst->causticSurfaceCacheNearestSampleCandidateCount <= 0.0 ||
         src->causticSurfaceCacheNearestSampleDistance <
             dst->causticSurfaceCacheNearestSampleDistance)) {
        dst->causticSurfaceCacheNearestSampleDistance =
            src->causticSurfaceCacheNearestSampleDistance;
        dst->causticSurfaceCacheNearestSampleRadius =
            src->causticSurfaceCacheNearestSampleRadius;
        dst->causticSurfaceCacheNearestSampleNormalDot =
            src->causticSurfaceCacheNearestSampleNormalDot;
    }
    dst->causticSurfaceCacheNearestSampleCandidateCount +=
        src->causticSurfaceCacheNearestSampleCandidateCount;
    dst->causticVolumeScatterSampleCount += src->causticVolumeScatterSampleCount;
    dst->causticVolumeScatterContributingSampleCount +=
        src->causticVolumeScatterContributingSampleCount;
    if (src->maxCausticSidecarRadiance > dst->maxCausticSidecarRadiance) {
        dst->maxCausticSidecarRadiance = src->maxCausticSidecarRadiance;
    }
    dst->totalCausticSidecarRadiance += src->totalCausticSidecarRadiance;
    if (src->maxCausticVolumeCacheRadiance > dst->maxCausticVolumeCacheRadiance) {
        dst->maxCausticVolumeCacheRadiance = src->maxCausticVolumeCacheRadiance;
    }
    if (src->causticVolumeCacheNonZeroCellRatio >
        dst->causticVolumeCacheNonZeroCellRatio) {
        dst->causticVolumeCacheNonZeroCellRatio =
            src->causticVolumeCacheNonZeroCellRatio;
    }
    if (src->causticVolumeCacheSampleHitRatio >
        dst->causticVolumeCacheSampleHitRatio) {
        dst->causticVolumeCacheSampleHitRatio =
            src->causticVolumeCacheSampleHitRatio;
    }
    if (src->causticVolumeCacheAverageFootprintRadiusVoxels >
        dst->causticVolumeCacheAverageFootprintRadiusVoxels) {
        dst->causticVolumeCacheAverageFootprintRadiusVoxels =
            src->causticVolumeCacheAverageFootprintRadiusVoxels;
    }
    if (src->causticVolumeCacheNonZeroCellCount > 0 &&
        (dst->causticVolumeCacheNonZeroCellCount == src->causticVolumeCacheNonZeroCellCount ||
         dst->causticVolumeCacheRadianceCentroidX == 0.0)) {
        dst->causticVolumeCacheRadianceCentroidX =
            src->causticVolumeCacheRadianceCentroidX;
        dst->causticVolumeCacheRadianceCentroidY =
            src->causticVolumeCacheRadianceCentroidY;
        dst->causticVolumeCacheRadianceCentroidZ =
            src->causticVolumeCacheRadianceCentroidZ;
        dst->causticVolumeCacheNonZeroBoundsMinX =
            src->causticVolumeCacheNonZeroBoundsMinX;
        dst->causticVolumeCacheNonZeroBoundsMinY =
            src->causticVolumeCacheNonZeroBoundsMinY;
        dst->causticVolumeCacheNonZeroBoundsMinZ =
            src->causticVolumeCacheNonZeroBoundsMinZ;
        dst->causticVolumeCacheNonZeroBoundsMaxX =
            src->causticVolumeCacheNonZeroBoundsMaxX;
        dst->causticVolumeCacheNonZeroBoundsMaxY =
            src->causticVolumeCacheNonZeroBoundsMaxY;
        dst->causticVolumeCacheNonZeroBoundsMaxZ =
            src->causticVolumeCacheNonZeroBoundsMaxZ;
    }
    dst->totalCausticVolumeCacheRadianceR += src->totalCausticVolumeCacheRadianceR;
    dst->totalCausticVolumeCacheRadianceG += src->totalCausticVolumeCacheRadianceG;
    dst->totalCausticVolumeCacheRadianceB += src->totalCausticVolumeCacheRadianceB;
    if (src->totalCausticVolumeCacheFootprintInputRadianceR >
        dst->totalCausticVolumeCacheFootprintInputRadianceR) {
        dst->totalCausticVolumeCacheFootprintInputRadianceR =
            src->totalCausticVolumeCacheFootprintInputRadianceR;
        dst->totalCausticVolumeCacheFootprintInputRadianceG =
            src->totalCausticVolumeCacheFootprintInputRadianceG;
        dst->totalCausticVolumeCacheFootprintInputRadianceB =
            src->totalCausticVolumeCacheFootprintInputRadianceB;
    }
    if (src->totalCausticVolumeCacheFootprintDepositedRadianceR >
        dst->totalCausticVolumeCacheFootprintDepositedRadianceR) {
        dst->totalCausticVolumeCacheFootprintDepositedRadianceR =
            src->totalCausticVolumeCacheFootprintDepositedRadianceR;
        dst->totalCausticVolumeCacheFootprintDepositedRadianceG =
            src->totalCausticVolumeCacheFootprintDepositedRadianceG;
        dst->totalCausticVolumeCacheFootprintDepositedRadianceB =
            src->totalCausticVolumeCacheFootprintDepositedRadianceB;
    }
    if (src->maxCausticSurfaceCacheRadiance > dst->maxCausticSurfaceCacheRadiance) {
        dst->maxCausticSurfaceCacheRadiance = src->maxCausticSurfaceCacheRadiance;
    }
    dst->totalCausticSurfaceCacheRadianceR += src->totalCausticSurfaceCacheRadianceR;
    dst->totalCausticSurfaceCacheRadianceG += src->totalCausticSurfaceCacheRadianceG;
    dst->totalCausticSurfaceCacheRadianceB += src->totalCausticSurfaceCacheRadianceB;
    dst->totalCausticSurfaceRadianceR += src->totalCausticSurfaceRadianceR;
    dst->totalCausticSurfaceRadianceG += src->totalCausticSurfaceRadianceG;
    dst->totalCausticSurfaceRadianceB += src->totalCausticSurfaceRadianceB;
    dst->volumeScatterDirectSampleCount += src->volumeScatterDirectSampleCount;
    dst->totalDirectVolumeScatterRadianceR += src->totalDirectVolumeScatterRadianceR;
    dst->totalDirectVolumeScatterRadianceG += src->totalDirectVolumeScatterRadianceG;
    dst->totalDirectVolumeScatterRadianceB += src->totalDirectVolumeScatterRadianceB;
    dst->totalCausticVolumeScatterRadianceR += src->totalCausticVolumeScatterRadianceR;
    dst->totalCausticVolumeScatterRadianceG += src->totalCausticVolumeScatterRadianceG;
    dst->totalCausticVolumeScatterRadianceB += src->totalCausticVolumeScatterRadianceB;
    dst->mirrorDominantPixelCount += src->mirrorDominantPixelCount;
    dst->mirrorBaseAttenuatedPixelCount += src->mirrorBaseAttenuatedPixelCount;
    dst->mirrorReflectionHitPixelCount += src->mirrorReflectionHitPixelCount;
    dst->mirrorEmitterReflectionPixelCount += src->mirrorEmitterReflectionPixelCount;
    dst->mirrorGeometryReflectionPixelCount += src->mirrorGeometryReflectionPixelCount;
    dst->temporalCommittedSubpasses += src->temporalCommittedSubpasses;
    dst->temporalPixelsRendered += src->temporalPixelsRendered;
    dst->temporalPixelsSkipped += src->temporalPixelsSkipped;
    dst->temporalActivePixelCount += src->temporalActivePixelCount;
    dst->temporalActiveTileCount += src->temporalActiveTileCount;
    dst->temporalInactiveTileCount += src->temporalInactiveTileCount;
    dst->temporalPlannedParentTileCount += src->temporalPlannedParentTileCount;
    dst->temporalEmittedTileJobCount += src->temporalEmittedTileJobCount;
    dst->temporalOccupancySkippedTileCount += src->temporalOccupancySkippedTileCount;
    dst->temporalDispatchedTileJobCount += src->temporalDispatchedTileJobCount;
    dst->temporalCompletedTileJobCount += src->temporalCompletedTileJobCount;
    dst->temporalProgressDirtyBatchCount += src->temporalProgressDirtyBatchCount;
    dst->temporalProgressDirtyTileCount += src->temporalProgressDirtyTileCount;
    dst->temporalDirtyPreviewPresentCount += src->temporalDirtyPreviewPresentCount;
    if (src->temporalConservativeFirstFrameTileRender >
        dst->temporalConservativeFirstFrameTileRender) {
        dst->temporalConservativeFirstFrameTileRender =
            src->temporalConservativeFirstFrameTileRender;
    }
    dst->temporalFinalFullResolveCount += src->temporalFinalFullResolveCount;
    dst->temporalHostFullResolveCount += src->temporalHostFullResolveCount;
    dst->temporalFinalPreviewPresentCount += src->temporalFinalPreviewPresentCount;
    dst->temporalHistoryPromoteCount += src->temporalHistoryPromoteCount;
    dst->temporalAdaptiveStateMeasuredPixels += src->temporalAdaptiveStateMeasuredPixels;
    dst->temporalAdaptiveStateStablePixels += src->temporalAdaptiveStateStablePixels;
    dst->temporalAdaptiveStateActivePixels += src->temporalAdaptiveStateActivePixels;
    dst->temporalAdaptiveStateProbePixels += src->temporalAdaptiveStateProbePixels;
    dst->temporalAdaptiveStateHighRiskPixels += src->temporalAdaptiveStateHighRiskPixels;
    dst->temporalAdaptiveStateStableTiles += src->temporalAdaptiveStateStableTiles;
    dst->temporalAdaptiveStateActiveTiles += src->temporalAdaptiveStateActiveTiles;
    dst->temporalAdaptiveStateProbeTiles += src->temporalAdaptiveStateProbeTiles;
    dst->temporalAdaptiveStateHighRiskTiles += src->temporalAdaptiveStateHighRiskTiles;
    if (src->temporalAdaptiveStateMinSampleFloor >
        dst->temporalAdaptiveStateMinSampleFloor) {
        dst->temporalAdaptiveStateMinSampleFloor =
            src->temporalAdaptiveStateMinSampleFloor;
    }
    dst->temporalMeasuredTileJobs += src->temporalMeasuredTileJobs;
    dst->temporalAdaptiveSplitParentCount += src->temporalAdaptiveSplitParentCount;
    dst->temporalAdaptiveChildTileCount += src->temporalAdaptiveChildTileCount;
    if (src->denoiseTemporalFrameCount > dst->denoiseTemporalFrameCount) {
        dst->denoiseTemporalFrameCount = src->denoiseTemporalFrameCount;
    }
    dst->denoiseRawPixelCount += src->denoiseRawPixelCount;
    dst->denoiseReconstructedPixelCount += src->denoiseReconstructedPixelCount;
    dst->denoiseStableInteriorSampleCount += src->denoiseStableInteriorSampleCount;
    dst->denoiseRejectedEdgeSampleCount += src->denoiseRejectedEdgeSampleCount;
    dst->denoisePreservedTransparentPixelCount += src->denoisePreservedTransparentPixelCount;
    dst->denoisePreservedMirrorGlossyPixelCount += src->denoisePreservedMirrorGlossyPixelCount;
    dst->denoiseSkippedUnstableTemporalPixelCount +=
        src->denoiseSkippedUnstableTemporalPixelCount;
    dst->denoiseSkippedInvalidSurfacePixelCount += src->denoiseSkippedInvalidSurfacePixelCount;
    if (src->maxRadiance > dst->maxRadiance) {
        dst->maxRadiance = src->maxRadiance;
    }
    if (src->maxBounceRadiance > dst->maxBounceRadiance) {
        dst->maxBounceRadiance = src->maxBounceRadiance;
    }
    if (src->maxMirrorDominance > dst->maxMirrorDominance) {
        dst->maxMirrorDominance = src->maxMirrorDominance;
    }
    if (src->maxMirrorSpecularReflectionRadiance > dst->maxMirrorSpecularReflectionRadiance) {
        dst->maxMirrorSpecularReflectionRadiance = src->maxMirrorSpecularReflectionRadiance;
    }
    if (src->maxMirrorBaseRadianceBeforeAttenuation >
        dst->maxMirrorBaseRadianceBeforeAttenuation) {
        dst->maxMirrorBaseRadianceBeforeAttenuation =
            src->maxMirrorBaseRadianceBeforeAttenuation;
    }
    if (src->maxMirrorBaseRadianceAfterAttenuation >
        dst->maxMirrorBaseRadianceAfterAttenuation) {
        dst->maxMirrorBaseRadianceAfterAttenuation =
            src->maxMirrorBaseRadianceAfterAttenuation;
    }
    dst->totalBounceRadiance += src->totalBounceRadiance;
    dst->totalMirrorSpecularReflectionRadiance += src->totalMirrorSpecularReflectionRadiance;
    dst->totalMirrorBaseRadianceBeforeAttenuation +=
        src->totalMirrorBaseRadianceBeforeAttenuation;
    dst->totalMirrorBaseRadianceAfterAttenuation +=
        src->totalMirrorBaseRadianceAfterAttenuation;
    dst->temporalTotalTileMs += src->temporalTotalTileMs;
    dst->denoiseRawRadianceLumaTotal += src->denoiseRawRadianceLumaTotal;
    dst->denoiseReconstructedRadianceLumaTotal += src->denoiseReconstructedRadianceLumaTotal;
    if (src->temporalMaxTileMs > dst->temporalMaxTileMs) {
        dst->temporalMaxTileMs = src->temporalMaxTileMs;
        dst->temporalSlowTileOriginX = src->temporalSlowTileOriginX;
        dst->temporalSlowTileOriginY = src->temporalSlowTileOriginY;
        dst->temporalSlowTileWidth = src->temporalSlowTileWidth;
        dst->temporalSlowTileHeight = src->temporalSlowTileHeight;
    }
    if (src->temporalMaxTileSubpassMs > dst->temporalMaxTileSubpassMs) {
        dst->temporalMaxTileSubpassMs = src->temporalMaxTileSubpassMs;
    }
    if (dst->temporalMeasuredTileJobs > 0) {
        dst->temporalAverageTileMs =
            dst->temporalTotalTileMs / (double)dst->temporalMeasuredTileJobs;
    }
}

static int runtime_native_3d_render_stats_round_divide(int value, int divisor) {
    if (divisor <= 1) return value;
    return (int)lround((double)value / (double)divisor);
}

static void runtime_native_3d_render_stats_normalize_temporal(
    RuntimeNative3DRenderStats* stats,
    int committed_subpasses) {
    if (!stats || committed_subpasses <= 1) return;
    stats->hitPixelCount =
        runtime_native_3d_render_stats_round_divide(stats->hitPixelCount, committed_subpasses);
    stats->visiblePixelCount =
        runtime_native_3d_render_stats_round_divide(stats->visiblePixelCount, committed_subpasses);
    stats->bouncePixelCount =
        runtime_native_3d_render_stats_round_divide(stats->bouncePixelCount, committed_subpasses);
    stats->secondaryRayCount =
        runtime_native_3d_render_stats_round_divide(stats->secondaryRayCount, committed_subpasses);
    stats->secondaryHitCount =
        runtime_native_3d_render_stats_round_divide(stats->secondaryHitCount, committed_subpasses);
    stats->secondaryContributingHitCount = runtime_native_3d_render_stats_round_divide(
        stats->secondaryContributingHitCount, committed_subpasses);
    stats->mirrorDominantPixelCount = runtime_native_3d_render_stats_round_divide(
        stats->mirrorDominantPixelCount, committed_subpasses);
    stats->mirrorBaseAttenuatedPixelCount = runtime_native_3d_render_stats_round_divide(
        stats->mirrorBaseAttenuatedPixelCount, committed_subpasses);
    stats->mirrorReflectionHitPixelCount = runtime_native_3d_render_stats_round_divide(
        stats->mirrorReflectionHitPixelCount, committed_subpasses);
    stats->mirrorEmitterReflectionPixelCount = runtime_native_3d_render_stats_round_divide(
        stats->mirrorEmitterReflectionPixelCount, committed_subpasses);
    stats->mirrorGeometryReflectionPixelCount = runtime_native_3d_render_stats_round_divide(
        stats->mirrorGeometryReflectionPixelCount, committed_subpasses);
    stats->totalBounceRadiance /= (double)committed_subpasses;
    stats->causticVolumeCacheSampleLookupCount = runtime_native_3d_render_stats_round_divide(
        stats->causticVolumeCacheSampleLookupCount, committed_subpasses);
    stats->causticVolumeCacheSampleContributingCount =
        runtime_native_3d_render_stats_round_divide(
            stats->causticVolumeCacheSampleContributingCount,
            committed_subpasses);
    stats->causticSurfaceCacheSampleLookupCount = runtime_native_3d_render_stats_round_divide(
        stats->causticSurfaceCacheSampleLookupCount, committed_subpasses);
    stats->causticSurfaceCacheSampleContributingCount =
        runtime_native_3d_render_stats_round_divide(
            stats->causticSurfaceCacheSampleContributingCount,
            committed_subpasses);
    stats->causticVolumeScatterSampleCount = runtime_native_3d_render_stats_round_divide(
        stats->causticVolumeScatterSampleCount, committed_subpasses);
    stats->causticVolumeScatterContributingSampleCount =
        runtime_native_3d_render_stats_round_divide(
            stats->causticVolumeScatterContributingSampleCount,
            committed_subpasses);
    stats->volumeScatterDirectSampleCount = runtime_native_3d_render_stats_round_divide(
        stats->volumeScatterDirectSampleCount, committed_subpasses);
    stats->totalDirectVolumeScatterRadianceR /= (double)committed_subpasses;
    stats->totalDirectVolumeScatterRadianceG /= (double)committed_subpasses;
    stats->totalDirectVolumeScatterRadianceB /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterRadianceR /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterRadianceG /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterRadianceB /= (double)committed_subpasses;
    stats->totalCausticSurfaceRadianceR /= (double)committed_subpasses;
    stats->totalCausticSurfaceRadianceG /= (double)committed_subpasses;
    stats->totalCausticSurfaceRadianceB /= (double)committed_subpasses;
    stats->totalMirrorSpecularReflectionRadiance /= (double)committed_subpasses;
    stats->totalMirrorBaseRadianceBeforeAttenuation /= (double)committed_subpasses;
    stats->totalMirrorBaseRadianceAfterAttenuation /= (double)committed_subpasses;
}

static void runtime_native_3d_render_stats_record_adaptive_state_summary(
    RuntimeNative3DRenderStats* stats,
    const RuntimeNative3DAdaptivePixelStateSummary* summary) {
    if (!stats || !summary) return;
    stats->temporalAdaptiveStateMeasuredPixels += summary->measuredPixelCount;
    stats->temporalAdaptiveStateStablePixels += summary->stablePixelCount;
    stats->temporalAdaptiveStateActivePixels += summary->activePixelCount;
    stats->temporalAdaptiveStateProbePixels += summary->probePixelCount;
    stats->temporalAdaptiveStateHighRiskPixels += summary->highRiskPixelCount;
    stats->temporalAdaptiveStateStableTiles += summary->stableTileCount;
    stats->temporalAdaptiveStateActiveTiles += summary->activeTileCount;
    stats->temporalAdaptiveStateProbeTiles += summary->probeTileCount;
    stats->temporalAdaptiveStateHighRiskTiles += summary->highRiskTileCount;
    if (summary->minSampleFloor > stats->temporalAdaptiveStateMinSampleFloor) {
        stats->temporalAdaptiveStateMinSampleFloor = summary->minSampleFloor;
    }
}

static bool runtime_native_3d_render_prepared_frame_serial(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    const RuntimeNative3DPreparedFrame* frame,
    int temporal_frames,
    RuntimeNative3DTemporalProgressCallback progress_callback,
    void* progress_user_data,
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderUnit render_unit = {0};
    bool ok = false;

    RuntimeNative3DRenderUnit_Init(&render_unit);
    ok = RuntimeNative3DRenderUnit_Setup(&render_unit,
                                         integrator_id,
                                         frame,
                                         0,
                                         0,
                                         frame->width,
                                         frame->height,
                                         &frame->sampling,
                                         temporal_frames,
                                         animSettings.disneyDenoiseEnabled);
    if (!ok) {
        RuntimeNative3DRenderUnit_Free(&render_unit);
        return false;
    }

    for (int subpass = 0; subpass < temporal_frames; ++subpass) {
        RuntimeNative3DRenderStats subpass_stats = {0};
        const int started_subpasses = subpass + 1;
        if (!RuntimeNative3DRenderUnit_ShouldRenderSubpass(&render_unit, subpass)) {
            break;
        }
        if (progress_callback) {
            progress_callback(started_subpasses,
                              RuntimeNative3DRenderUnit_CommittedSubpasses(&render_unit),
                              temporal_frames,
                              progress_user_data);
        }
        ok = RuntimeNative3DRenderUnit_RenderSubpass(&render_unit, subpass, &subpass_stats);
        if (!ok) {
            break;
        }
        if (progress_callback) {
            progress_callback(started_subpasses,
                              RuntimeNative3DRenderUnit_CommittedSubpasses(&render_unit),
                              temporal_frames,
                              progress_user_data);
        }
        if (out_stats) {
            RuntimeNative3DRenderStats_Accumulate(out_stats, &subpass_stats);
        }
    }

    if (ok) {
        RuntimeNative3DRenderStats resolve_stats = {0};
        ok = RuntimeNative3DRenderUnit_ResolveCurrentToPixelsWithStats(&render_unit,
                                                                       pixel_buffer,
                                                                       frame->width,
                                                                       &resolve_stats);
        if (ok && out_stats) {
            RuntimeNative3DRenderStats_Accumulate(out_stats, &resolve_stats);
        }
    }
    if (ok && out_stats) {
        RuntimeNative3DAdaptivePixelStateSummary adaptive_summary = {0};
        out_stats->temporalCommittedSubpasses =
            RuntimeNative3DRenderUnit_CommittedSubpasses(&render_unit);
        RuntimeNative3DRenderUnit_GetActivityCounts(&render_unit,
                                                    &out_stats->temporalActivePixelCount,
                                                    &out_stats->temporalActiveTileCount,
                                                    &out_stats->temporalInactiveTileCount);
        RuntimeNative3DRenderUnit_GetAdaptiveStateSummary(&render_unit, &adaptive_summary);
        runtime_native_3d_render_stats_record_adaptive_state_summary(out_stats,
                                                                     &adaptive_summary);
    }

    RuntimeNative3DRenderUnit_Free(&render_unit);
    return ok;
}

static bool runtime_native_3d_render_should_use_tile_scheduler(int width, int height) {
    const int tile_size = RuntimeNative3DTileSchedulerResolveTileSizeForScale(
        animSettings.tileSize,
        animSettings.renderScale3D);
    if (!animSettings.useTiledRenderer) {
        return false;
    }
    return width > tile_size || height > tile_size;
}


void RuntimeNative3DPreparedFrame_Free(RuntimeNative3DPreparedFrame* frame) {
    if (!frame) return;
    RuntimeNative3DTileOccupancy_Free(&frame->tileOccupancy);
    RuntimeCausticVolumeCache3D_Free(&frame->causticVolumeCache);
    RuntimeCausticSurfaceCache3D_Free(&frame->causticSurfaceCache);
    RuntimeScene3D_Free(&frame->scene);
    memset(frame, 0, sizeof(*frame));
}

bool RuntimeNative3DRenderPreparedRegion(uint8_t* pixel_buffer,
                                         RayTracing3DIntegratorId integrator_id,
                                         const RuntimeNative3DPreparedFrame* frame,
                                         int start_x,
                                         int start_y,
                                         int end_x,
                                         int end_y,
                                         RuntimeNative3DRenderStats* out_stats) {
    float* radiance_buffer = NULL;
    const int region_width = end_x - start_x;
    const int region_height = end_y - start_y;
    bool ok = false;

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!pixel_buffer || !frame || !frame->valid) return false;
    if (region_width <= 0 || region_height <= 0) return true;
    radiance_buffer = (float*)calloc((size_t)region_width * (size_t)region_height *
                                         RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                     sizeof(*radiance_buffer));
    if (!radiance_buffer) return false;
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance_buffer,
                                                        region_width,
                                                        integrator_id,
                                                        frame,
                                                        start_x,
                                                        start_y,
                                                        end_x,
                                                        end_y,
                                                        out_stats);
    if (ok) {
        RuntimeNative3DResolveRadianceRegionToPixels(pixel_buffer,
                                                     frame->width,
                                                     radiance_buffer,
                                                     region_width,
                                                     start_x,
                                                     start_y,
                                                     end_x,
                                                     end_y);
    }
    free(radiance_buffer);
    return ok;
}

bool RuntimeNative3DRenderPreparedRegionRadianceRGB(float* radiance_buffer,
                                                    int radiance_stride,
                                                    RayTracing3DIntegratorId integrator_id,
                                                    const RuntimeNative3DPreparedFrame* frame,
                                                    int start_x,
                                                    int start_y,
                                                    int end_x,
                                                    int end_y,
                                                    RuntimeNative3DRenderStats* out_stats) {
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!radiance_buffer || radiance_stride <= 0 || !frame || !frame->valid) return false;
    return runtime_native_3d_render_dispatch_integrator(radiance_buffer,
                                                        radiance_stride,
                                                        integrator_id,
                                                        frame,
                                                        start_x,
                                                        start_y,
                                                        end_x,
                                                        end_y,
                                                        out_stats);
}

bool RuntimeNative3DRenderPreparedRegionLuminance(float* luminance_buffer,
                                                  int luminance_stride,
                                                  RayTracing3DIntegratorId integrator_id,
                                                  const RuntimeNative3DPreparedFrame* frame,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y,
                                                  RuntimeNative3DRenderStats* out_stats) {
    float* radiance_buffer = NULL;
    const int region_width = end_x - start_x;
    const int region_height = end_y - start_y;
    bool ok = false;
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!luminance_buffer || luminance_stride <= 0 || !frame || !frame->valid) return false;
    if (region_width <= 0 || region_height <= 0) return true;
    radiance_buffer = (float*)calloc((size_t)region_width * (size_t)region_height *
                                         RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                     sizeof(*radiance_buffer));
    if (!radiance_buffer) return false;
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance_buffer,
                                                        region_width,
                                                        integrator_id,
                                                        frame,
                                                        start_x,
                                                        start_y,
                                                        end_x,
                                                        end_y,
                                                        out_stats);
    if (ok) {
        for (int y = 0; y < region_height; ++y) {
            for (int x = 0; x < region_width; ++x) {
                const size_t dst_index = (size_t)y * (size_t)luminance_stride + (size_t)x;
                const size_t src_base =
                    ((size_t)y * (size_t)region_width + (size_t)x) *
                    (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
                luminance_buffer[dst_index] = 0.2126f * radiance_buffer[src_base] +
                                              0.7152f * radiance_buffer[src_base + 1u] +
                                              0.0722f * radiance_buffer[src_base + 2u];
            }
        }
    }
    free(radiance_buffer);
    return ok;
}

bool RuntimeNative3DPrepareFrameTileOccupancy(RuntimeNative3DPreparedFrame* frame, int tile_size) {
    if (!frame || !frame->valid) return false;
    return RuntimeNative3DTileOccupancy_Build(&frame->tileOccupancy,
                                              &frame->scene,
                                              &frame->projector,
                                              tile_size);
}

bool RuntimeNative3DPreparedRegionMayContainGeometry(const RuntimeNative3DPreparedFrame* frame,
                                                     int start_x,
                                                     int start_y,
                                                     int end_x,
                                                     int end_y) {
    if (!frame || !frame->valid) return true;
    if (frame->tileOccupancyConservativeAllTiles) return true;
    if (RuntimeVolume3D_HasActiveExtinction(&frame->scene.volume)) {
        return true;
    }
    return RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&frame->tileOccupancy,
                                                                 start_x,
                                                                 start_y,
                                                                 end_x,
                                                                 end_y);
}

bool RuntimeNative3DRenderToPixelBuffer(uint8_t* pixel_buffer,
                                        RayTracing3DIntegratorId integrator_id,
                                        int width,
                                        int height,
                                        double normalized_t,
                                        double live_light_x,
                                        double live_light_y,
                                        RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(pixel_buffer,
                                                                  integrator_id,
                                                                  width,
                                                                  height,
                                                                  normalized_t,
                                                                  live_light_x,
                                                                  live_light_y,
                                                                  NULL,
                                                                  1,
                                                                  out_stats);
}

bool RuntimeNative3DRenderToPixelBufferWithSampling(uint8_t* pixel_buffer,
                                                    RayTracing3DIntegratorId integrator_id,
                                                    int width,
                                                    int height,
                                                    double normalized_t,
                                                    double live_light_x,
                                                    double live_light_y,
                                                    const RuntimeNative3DSamplingContext* sampling,
                                                    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(pixel_buffer,
                                                                  integrator_id,
                                                                  width,
                                                                  height,
                                                                  normalized_t,
                                                                  live_light_x,
                                                                  live_light_y,
                                                                  sampling,
                                                                  1,
                                                                  out_stats);
}

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
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgressAtFrameIndex(
        pixel_buffer,
        integrator_id,
        width,
        height,
        normalized_t,
        0,
        live_light_x,
        live_light_y,
        sampling,
        temporal_frames,
        NULL,
        NULL,
        out_stats);
}

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
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporalProgressAtFrameIndex(
        pixel_buffer,
        integrator_id,
        width,
        height,
        normalized_t,
        0,
        live_light_x,
        live_light_y,
        sampling,
        temporal_frames,
        progress_callback,
        progress_user_data,
        out_stats);
}

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
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporalDetailedProgressAtFrameIndex(
        pixel_buffer,
        integrator_id,
        width,
        height,
        normalized_t,
        frame_index,
        live_light_x,
        live_light_y,
        sampling,
        temporal_frames,
        progress_callback,
        progress_user_data,
        NULL,
        NULL,
        out_stats);
}

typedef struct RuntimeNative3DTileProgressAdapter {
    RuntimeNative3DTemporalTileProgressCallback callback;
    void* user_data;
} RuntimeNative3DTileProgressAdapter;

static bool runtime_native_3d_render_tile_progress_adapter(
    const RuntimeNative3DTileSchedulerProgress* progress,
    void* user_data) {
    RuntimeNative3DTileProgressAdapter* adapter =
        (RuntimeNative3DTileProgressAdapter*)user_data;
    if (!progress || !adapter || !adapter->callback) {
        return false;
    }
    adapter->callback(progress->startedSubpasses,
                      progress->completedSubpasses,
                      progress->totalSubpasses,
                      progress->completedTilesInSubpass,
                      progress->totalTilesInSubpass,
                      adapter->user_data);
    return true;
}

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
    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporalDetailedProgressBudgetedAtFrameIndex(
        pixel_buffer,
        integrator_id,
        width,
        height,
        normalized_t,
        frame_index,
        live_light_x,
        live_light_y,
        sampling,
        temporal_frames,
        progress_callback,
        progress_user_data,
        tile_progress_callback,
        tile_progress_user_data,
        NULL,
        out_stats);
}

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
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DTileProgressAdapter tile_progress_adapter = {0};
    bool ok = false;
    const int effective_temporal_frames = (temporal_frames <= 1) ? 1 : temporal_frames;

    if (!pixel_buffer || width <= 0 || height <= 0) return false;
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }

    ok = RuntimeNative3DPrepareFrameWithSamplingAtFrameIndex(&frame,
                                                             width,
                                                             height,
                                                             normalized_t,
                                                             frame_index,
                                                             live_light_x,
                                                             live_light_y,
                                                             sampling);
    if (!ok) {
        RuntimeNative3DFillPixelBufferEnvironment(pixel_buffer, (size_t)width * (size_t)height);
        return false;
    }
    RuntimeNative3DFillPixelBufferBackground(pixel_buffer,
                                             width,
                                             height,
                                             &frame.scene,
                                             &frame.projector);
    if (runtime_native_3d_render_should_use_tile_scheduler(width, height)) {
        if (tile_progress_callback) {
            tile_progress_adapter.callback = tile_progress_callback;
            tile_progress_adapter.user_data = tile_progress_user_data;
            ok = RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressAndBudget(
                pixel_buffer,
                integrator_id,
                &frame,
                effective_temporal_frames,
                progress_callback,
                progress_user_data,
                runtime_native_3d_render_tile_progress_adapter,
                &tile_progress_adapter,
                resource_budget,
                out_stats);
        } else {
            ok = RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressAndBudget(
                pixel_buffer,
                integrator_id,
                &frame,
                effective_temporal_frames,
                progress_callback,
                progress_user_data,
                NULL,
                NULL,
                resource_budget,
                out_stats);
        }
    } else {
        ok = runtime_native_3d_render_prepared_frame_serial(pixel_buffer,
                                                            integrator_id,
                                                            &frame,
                                                            effective_temporal_frames,
                                                            progress_callback,
                                                            progress_user_data,
                                                            out_stats);
    }
    if (ok && out_stats) {
        runtime_native_3d_render_stats_normalize_temporal(
            out_stats,
            out_stats->temporalCommittedSubpasses);
    }
    RuntimeNative3DPreparedFrame_Free(&frame);
    return ok;
}
