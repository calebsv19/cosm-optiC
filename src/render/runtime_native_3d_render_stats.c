#include "render/runtime_native_3d_render_internal_host.h"

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
    if (src->causticTransportAnalyticSphereLensResolvedCount >
        dst->causticTransportAnalyticSphereLensResolvedCount) {
        dst->causticTransportAnalyticSphereLensResolvedCount =
            src->causticTransportAnalyticSphereLensResolvedCount;
    }
    if (src->causticTransportAnalyticSphereLensRejectedCount >
        dst->causticTransportAnalyticSphereLensRejectedCount) {
        dst->causticTransportAnalyticSphereLensRejectedCount =
            src->causticTransportAnalyticSphereLensRejectedCount;
    }
    if (src->causticTransportAnalyticSphereLensEvaluatedPathCount >
        dst->causticTransportAnalyticSphereLensEvaluatedPathCount) {
        dst->causticTransportAnalyticSphereLensEvaluatedPathCount =
            src->causticTransportAnalyticSphereLensEvaluatedPathCount;
    }
    if (src->causticTransportAnalyticSphereLensEmittedPathCount >
        dst->causticTransportAnalyticSphereLensEmittedPathCount) {
        dst->causticTransportAnalyticSphereLensEmittedPathCount =
            src->causticTransportAnalyticSphereLensEmittedPathCount;
    }
    if (src->causticTransportAnalyticSphereLensSampleWeight >
        dst->causticTransportAnalyticSphereLensSampleWeight) {
        dst->causticTransportAnalyticSphereLensSampleWeight =
            src->causticTransportAnalyticSphereLensSampleWeight;
    }
    if (src->causticTransportAnalyticSphereLensTotalSampleWeight >
        dst->causticTransportAnalyticSphereLensTotalSampleWeight) {
        dst->causticTransportAnalyticSphereLensTotalSampleWeight =
            src->causticTransportAnalyticSphereLensTotalSampleWeight;
    }
    if (src->causticTransportAnalyticCylinderLensResolvedCount >
        dst->causticTransportAnalyticCylinderLensResolvedCount) {
        dst->causticTransportAnalyticCylinderLensResolvedCount =
            src->causticTransportAnalyticCylinderLensResolvedCount;
    }
    if (src->causticTransportAnalyticCylinderLensRejectedCount >
        dst->causticTransportAnalyticCylinderLensRejectedCount) {
        dst->causticTransportAnalyticCylinderLensRejectedCount =
            src->causticTransportAnalyticCylinderLensRejectedCount;
    }
    if (src->causticTransportAnalyticCylinderLensEvaluatedPathCount >
        dst->causticTransportAnalyticCylinderLensEvaluatedPathCount) {
        dst->causticTransportAnalyticCylinderLensEvaluatedPathCount =
            src->causticTransportAnalyticCylinderLensEvaluatedPathCount;
    }
    if (src->causticTransportAnalyticCylinderLensEmittedPathCount >
        dst->causticTransportAnalyticCylinderLensEmittedPathCount) {
        dst->causticTransportAnalyticCylinderLensEmittedPathCount =
            src->causticTransportAnalyticCylinderLensEmittedPathCount;
    }
    if (src->causticTransportAnalyticCylinderLensSampleWeight >
        dst->causticTransportAnalyticCylinderLensSampleWeight) {
        dst->causticTransportAnalyticCylinderLensSampleWeight =
            src->causticTransportAnalyticCylinderLensSampleWeight;
    }
    if (src->causticTransportAnalyticCylinderLensTotalSampleWeight >
        dst->causticTransportAnalyticCylinderLensTotalSampleWeight) {
        dst->causticTransportAnalyticCylinderLensTotalSampleWeight =
            src->causticTransportAnalyticCylinderLensTotalSampleWeight;
    }
    if (src->causticTransportAnalyticPrismLensResolvedCount >
        dst->causticTransportAnalyticPrismLensResolvedCount) {
        dst->causticTransportAnalyticPrismLensResolvedCount =
            src->causticTransportAnalyticPrismLensResolvedCount;
    }
    if (src->causticTransportAnalyticPrismLensRejectedCount >
        dst->causticTransportAnalyticPrismLensRejectedCount) {
        dst->causticTransportAnalyticPrismLensRejectedCount =
            src->causticTransportAnalyticPrismLensRejectedCount;
    }
    if (src->causticTransportAnalyticPrismLensEvaluatedPathCount >
        dst->causticTransportAnalyticPrismLensEvaluatedPathCount) {
        dst->causticTransportAnalyticPrismLensEvaluatedPathCount =
            src->causticTransportAnalyticPrismLensEvaluatedPathCount;
    }
    if (src->causticTransportAnalyticPrismLensEmittedPathCount >
        dst->causticTransportAnalyticPrismLensEmittedPathCount) {
        dst->causticTransportAnalyticPrismLensEmittedPathCount =
            src->causticTransportAnalyticPrismLensEmittedPathCount;
    }
    if (src->causticTransportAnalyticPrismLensSampleWeight >
        dst->causticTransportAnalyticPrismLensSampleWeight) {
        dst->causticTransportAnalyticPrismLensSampleWeight =
            src->causticTransportAnalyticPrismLensSampleWeight;
    }
    if (src->causticTransportAnalyticPrismLensTotalSampleWeight >
        dst->causticTransportAnalyticPrismLensTotalSampleWeight) {
        dst->causticTransportAnalyticPrismLensTotalSampleWeight =
            src->causticTransportAnalyticPrismLensTotalSampleWeight;
    }
    if (src->causticTransportAnalyticBowlLensResolvedCount >
        dst->causticTransportAnalyticBowlLensResolvedCount) {
        dst->causticTransportAnalyticBowlLensResolvedCount =
            src->causticTransportAnalyticBowlLensResolvedCount;
    }
    if (src->causticTransportAnalyticBowlLensRejectedCount >
        dst->causticTransportAnalyticBowlLensRejectedCount) {
        dst->causticTransportAnalyticBowlLensRejectedCount =
            src->causticTransportAnalyticBowlLensRejectedCount;
    }
    if (src->causticTransportAnalyticBowlLensEvaluatedPathCount >
        dst->causticTransportAnalyticBowlLensEvaluatedPathCount) {
        dst->causticTransportAnalyticBowlLensEvaluatedPathCount =
            src->causticTransportAnalyticBowlLensEvaluatedPathCount;
    }
    if (src->causticTransportAnalyticBowlLensEmittedPathCount >
        dst->causticTransportAnalyticBowlLensEmittedPathCount) {
        dst->causticTransportAnalyticBowlLensEmittedPathCount =
            src->causticTransportAnalyticBowlLensEmittedPathCount;
    }
    if (src->causticTransportAnalyticBowlLensSampleWeight >
        dst->causticTransportAnalyticBowlLensSampleWeight) {
        dst->causticTransportAnalyticBowlLensSampleWeight =
            src->causticTransportAnalyticBowlLensSampleWeight;
    }
    if (src->causticTransportAnalyticBowlLensTotalSampleWeight >
        dst->causticTransportAnalyticBowlLensTotalSampleWeight) {
        dst->causticTransportAnalyticBowlLensTotalSampleWeight =
            src->causticTransportAnalyticBowlLensTotalSampleWeight;
    }
    if (src->causticTransportMeshDielectricLensResolvedCount >
        dst->causticTransportMeshDielectricLensResolvedCount) {
        dst->causticTransportMeshDielectricLensResolvedCount =
            src->causticTransportMeshDielectricLensResolvedCount;
    }
    if (src->causticTransportMeshDielectricLensRejectedCount >
        dst->causticTransportMeshDielectricLensRejectedCount) {
        dst->causticTransportMeshDielectricLensRejectedCount =
            src->causticTransportMeshDielectricLensRejectedCount;
    }
    if (src->causticTransportMeshDielectricLensEvaluatedPathCount >
        dst->causticTransportMeshDielectricLensEvaluatedPathCount) {
        dst->causticTransportMeshDielectricLensEvaluatedPathCount =
            src->causticTransportMeshDielectricLensEvaluatedPathCount;
    }
    if (src->causticTransportMeshDielectricLensEmittedPathCount >
        dst->causticTransportMeshDielectricLensEmittedPathCount) {
        dst->causticTransportMeshDielectricLensEmittedPathCount =
            src->causticTransportMeshDielectricLensEmittedPathCount;
    }
    if (src->causticTransportMeshDielectricLensSampleWeight >
        dst->causticTransportMeshDielectricLensSampleWeight) {
        dst->causticTransportMeshDielectricLensSampleWeight =
            src->causticTransportMeshDielectricLensSampleWeight;
    }
    if (src->causticTransportMeshDielectricLensTotalSampleWeight >
        dst->causticTransportMeshDielectricLensTotalSampleWeight) {
        dst->causticTransportMeshDielectricLensTotalSampleWeight =
            src->causticTransportMeshDielectricLensTotalSampleWeight;
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
    dst->causticVolumeScatterContributingPixelCount +=
        src->causticVolumeScatterContributingPixelCount;
    dst->totalCausticVolumeScatterPixelX += src->totalCausticVolumeScatterPixelX;
    dst->totalCausticVolumeScatterPixelY += src->totalCausticVolumeScatterPixelY;
    if (src->causticVolumeScatterContributingPixelCount > 0) {
        if (dst->causticVolumeScatterContributingPixelCount ==
                src->causticVolumeScatterContributingPixelCount ||
            src->causticVolumeScatterPixelMinX < dst->causticVolumeScatterPixelMinX) {
            dst->causticVolumeScatterPixelMinX = src->causticVolumeScatterPixelMinX;
        }
        if (dst->causticVolumeScatterContributingPixelCount ==
                src->causticVolumeScatterContributingPixelCount ||
            src->causticVolumeScatterPixelMinY < dst->causticVolumeScatterPixelMinY) {
            dst->causticVolumeScatterPixelMinY = src->causticVolumeScatterPixelMinY;
        }
        if (src->causticVolumeScatterPixelMaxX > dst->causticVolumeScatterPixelMaxX) {
            dst->causticVolumeScatterPixelMaxX = src->causticVolumeScatterPixelMaxX;
        }
        if (src->causticVolumeScatterPixelMaxY > dst->causticVolumeScatterPixelMaxY) {
            dst->causticVolumeScatterPixelMaxY = src->causticVolumeScatterPixelMaxY;
        }
    }
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
    dst->totalCausticVolumeScatterSampledCacheRadiance +=
        src->totalCausticVolumeScatterSampledCacheRadiance;
    if (src->maxCausticVolumeScatterSampledCacheRadiance >
        dst->maxCausticVolumeScatterSampledCacheRadiance) {
        dst->maxCausticVolumeScatterSampledCacheRadiance =
            src->maxCausticVolumeScatterSampledCacheRadiance;
    }
    dst->totalCausticVolumeScatterSampledRawDensity +=
        src->totalCausticVolumeScatterSampledRawDensity;
    if (src->maxCausticVolumeScatterSampledRawDensity >
        dst->maxCausticVolumeScatterSampledRawDensity) {
        dst->maxCausticVolumeScatterSampledRawDensity =
            src->maxCausticVolumeScatterSampledRawDensity;
    }
    dst->totalCausticVolumeScatterSampledDensity +=
        src->totalCausticVolumeScatterSampledDensity;
    if (src->maxCausticVolumeScatterSampledDensity >
        dst->maxCausticVolumeScatterSampledDensity) {
        dst->maxCausticVolumeScatterSampledDensity =
            src->maxCausticVolumeScatterSampledDensity;
    }
    dst->totalCausticVolumeScatterProbability +=
        src->totalCausticVolumeScatterProbability;
    if (src->maxCausticVolumeScatterProbability >
        dst->maxCausticVolumeScatterProbability) {
        dst->maxCausticVolumeScatterProbability =
            src->maxCausticVolumeScatterProbability;
    }
    dst->totalCausticVolumeScatterCameraTransmittance +=
        src->totalCausticVolumeScatterCameraTransmittance;
    if (src->causticVolumeScatterContributingSampleCount > 0 &&
        (dst->minCausticVolumeScatterCameraTransmittance <= 0.0 ||
         src->minCausticVolumeScatterCameraTransmittance <
             dst->minCausticVolumeScatterCameraTransmittance)) {
        dst->minCausticVolumeScatterCameraTransmittance =
            src->minCausticVolumeScatterCameraTransmittance;
    }
    if (src->maxCausticVolumeScatterCameraTransmittance >
        dst->maxCausticVolumeScatterCameraTransmittance) {
        dst->maxCausticVolumeScatterCameraTransmittance =
            src->maxCausticVolumeScatterCameraTransmittance;
    }
    dst->totalCausticVolumeScatterVisibilityTerm +=
        src->totalCausticVolumeScatterVisibilityTerm;
    if (src->maxCausticVolumeScatterVisibilityTerm >
        dst->maxCausticVolumeScatterVisibilityTerm) {
        dst->maxCausticVolumeScatterVisibilityTerm =
            src->maxCausticVolumeScatterVisibilityTerm;
    }
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
    dst->temporalDirtyPreviewHostPixels += src->temporalDirtyPreviewHostPixels;
    dst->temporalDirtyPreviewHostBytes += src->temporalDirtyPreviewHostBytes;
    dst->temporalFinalResolveHostPixels += src->temporalFinalResolveHostPixels;
    dst->temporalFinalResolveHostBytes += src->temporalFinalResolveHostBytes;
    dst->temporalHistorySeedHostBytes += src->temporalHistorySeedHostBytes;
    dst->temporalHistoryPromoteHostBytes += src->temporalHistoryPromoteHostBytes;
    dst->temporalFinalPreviewPresentHostBytes += src->temporalFinalPreviewPresentHostBytes;
    dst->temporalAdaptiveStateMeasuredPixels += src->temporalAdaptiveStateMeasuredPixels;
    dst->temporalAdaptiveStateStablePixels += src->temporalAdaptiveStateStablePixels;
    dst->temporalAdaptiveStateActivePixels += src->temporalAdaptiveStateActivePixels;
    dst->temporalAdaptiveStateProbePixels += src->temporalAdaptiveStateProbePixels;
    dst->temporalAdaptiveStateHighRiskPixels += src->temporalAdaptiveStateHighRiskPixels;
    dst->temporalAdaptiveStateStableTiles += src->temporalAdaptiveStateStableTiles;
    dst->temporalAdaptiveStateActiveTiles += src->temporalAdaptiveStateActiveTiles;
    dst->temporalAdaptiveStateProbeTiles += src->temporalAdaptiveStateProbeTiles;
    dst->temporalAdaptiveStateHighRiskTiles += src->temporalAdaptiveStateHighRiskTiles;
    dst->temporalAdaptiveStateActivityRiskPixels +=
        src->temporalAdaptiveStateActivityRiskPixels;
    dst->temporalAdaptiveStateMaterialRiskPixels +=
        src->temporalAdaptiveStateMaterialRiskPixels;
    dst->temporalAdaptiveStateTransparentRiskPixels +=
        src->temporalAdaptiveStateTransparentRiskPixels;
    dst->temporalAdaptiveStateGlossyRiskPixels +=
        src->temporalAdaptiveStateGlossyRiskPixels;
    dst->temporalAdaptiveStateGeometryEdgeRiskPixels +=
        src->temporalAdaptiveStateGeometryEdgeRiskPixels;
    dst->temporalAdaptiveStateDirectLightNoTracePixels +=
        src->temporalAdaptiveStateDirectLightNoTracePixels;
    dst->temporalAdaptiveStateDirectLightClearVisiblePixels +=
        src->temporalAdaptiveStateDirectLightClearVisiblePixels;
    dst->temporalAdaptiveStateDirectLightClearBlockedPixels +=
        src->temporalAdaptiveStateDirectLightClearBlockedPixels;
    dst->temporalAdaptiveStateDirectLightStablePartialPixels +=
        src->temporalAdaptiveStateDirectLightStablePartialPixels;
    dst->temporalAdaptiveStateDirectLightMixedPartialPixels +=
        src->temporalAdaptiveStateDirectLightMixedPartialPixels;
    dst->temporalAdaptiveStateDirectLightBoundaryRiskPixels +=
        src->temporalAdaptiveStateDirectLightBoundaryRiskPixels;
    dst->temporalAdaptiveEarlyStopEligiblePixels +=
        src->temporalAdaptiveEarlyStopEligiblePixels;
    dst->temporalAdaptiveEarlyStopHeldPixels +=
        src->temporalAdaptiveEarlyStopHeldPixels;
    dst->temporalAdaptiveEarlyStopHoldProbePixels +=
        src->temporalAdaptiveEarlyStopHoldProbePixels;
    dst->temporalAdaptiveEarlyStopHoldHighRiskPixels +=
        src->temporalAdaptiveEarlyStopHoldHighRiskPixels;
    dst->temporalAdaptiveEarlyStopHoldActivityRiskPixels +=
        src->temporalAdaptiveEarlyStopHoldActivityRiskPixels;
    dst->temporalAdaptiveEarlyStopHoldMaterialRiskPixels +=
        src->temporalAdaptiveEarlyStopHoldMaterialRiskPixels;
    dst->temporalAdaptiveEarlyStopHoldTransparentRiskPixels +=
        src->temporalAdaptiveEarlyStopHoldTransparentRiskPixels;
    dst->temporalAdaptiveEarlyStopHoldGeometryEdgeRiskPixels +=
        src->temporalAdaptiveEarlyStopHoldGeometryEdgeRiskPixels;
    dst->temporalAdaptiveEarlyStopHoldDirectLightRiskPixels +=
        src->temporalAdaptiveEarlyStopHoldDirectLightRiskPixels;
    dst->temporalAdaptiveEarlyStopBaseActivePixels +=
        src->temporalAdaptiveEarlyStopBaseActivePixels;
    dst->temporalAdaptiveEarlyStopPaddingHoldPixels +=
        src->temporalAdaptiveEarlyStopPaddingHoldPixels;
    dst->temporalAdaptiveEarlyStopPaddingHoldHighSeedPixels +=
        src->temporalAdaptiveEarlyStopPaddingHoldHighSeedPixels;
    dst->temporalAdaptiveEarlyStopPaddingHoldMediumSeedPixels +=
        src->temporalAdaptiveEarlyStopPaddingHoldMediumSeedPixels;
    dst->temporalAdaptiveEarlyStopActiveAfterPaddingPixels +=
        src->temporalAdaptiveEarlyStopActiveAfterPaddingPixels;
    for (int i = 0; i < RUNTIME_NATIVE_3D_ADAPTIVE_REGION_COUNT; ++i) {
        dst->temporalAdaptiveEarlyStopEligibleRegionCounts[i] +=
            src->temporalAdaptiveEarlyStopEligibleRegionCounts[i];
        dst->temporalAdaptiveEarlyStopHeldRegionCounts[i] +=
            src->temporalAdaptiveEarlyStopHeldRegionCounts[i];
        dst->temporalAdaptiveEarlyStopPaddingHoldRegionCounts[i] +=
            src->temporalAdaptiveEarlyStopPaddingHoldRegionCounts[i];
    }
    for (int i = 0; i < RUNTIME_NATIVE_3D_TEMPORAL_BUDGET_BUCKET_COUNT; ++i) {
        dst->temporalAdaptiveBudgetBucketPixels[i] +=
            src->temporalAdaptiveBudgetBucketPixels[i];
        dst->temporalAdaptiveBudgetActiveBucketPixels[i] +=
            src->temporalAdaptiveBudgetActiveBucketPixels[i];
        dst->temporalAdaptiveBudgetEligibleBucketPixels[i] +=
            src->temporalAdaptiveBudgetEligibleBucketPixels[i];
        dst->temporalAdaptiveBudgetHeldBucketPixels[i] +=
            src->temporalAdaptiveBudgetHeldBucketPixels[i];
    }
    dst->temporalAdaptiveBudgetClearVisibleEligiblePixels +=
        src->temporalAdaptiveBudgetClearVisibleEligiblePixels;
    dst->temporalAdaptiveBudgetClearVisibleHeldPixels +=
        src->temporalAdaptiveBudgetClearVisibleHeldPixels;
    dst->temporalAdaptiveBudgetPartialHeldPixels +=
        src->temporalAdaptiveBudgetPartialHeldPixels;
    dst->temporalAdaptiveBudgetTransparentHeldPixels +=
        src->temporalAdaptiveBudgetTransparentHeldPixels;
    dst->temporalAdaptiveBudgetGeometryHeldPixels +=
        src->temporalAdaptiveBudgetGeometryHeldPixels;
    dst->temporalAdaptiveBudgetActivityHeldPixels +=
        src->temporalAdaptiveBudgetActivityHeldPixels;
    dst->temporalAdaptiveBudgetHeatmapEnabled =
        dst->temporalAdaptiveBudgetHeatmapEnabled ||
        src->temporalAdaptiveBudgetHeatmapEnabled;
    dst->temporalAdaptiveStateMixedRiskTiles += src->temporalAdaptiveStateMixedRiskTiles;
    dst->temporalAdaptiveStateRiskSum += src->temporalAdaptiveStateRiskSum;
    if (src->temporalAdaptiveStateRiskMax > dst->temporalAdaptiveStateRiskMax) {
        dst->temporalAdaptiveStateRiskMax = src->temporalAdaptiveStateRiskMax;
    }
    if (src->temporalAdaptiveStateMinSampleFloor >
        dst->temporalAdaptiveStateMinSampleFloor) {
        dst->temporalAdaptiveStateMinSampleFloor =
            src->temporalAdaptiveStateMinSampleFloor;
    }
    dst->temporalMeasuredTileJobs += src->temporalMeasuredTileJobs;
    dst->temporalAdaptiveSplitParentCount += src->temporalAdaptiveSplitParentCount;
    dst->temporalAdaptiveChildTileCount += src->temporalAdaptiveChildTileCount;
    dst->temporalTileSchedulerJobArrayOwnerCount +=
        src->temporalTileSchedulerJobArrayOwnerCount;
    dst->temporalTileSchedulerParentMetricArrayOwnerCount +=
        src->temporalTileSchedulerParentMetricArrayOwnerCount;
    dst->temporalTileSchedulerProgressTileArrayOwnerCount +=
        src->temporalTileSchedulerProgressTileArrayOwnerCount;
    dst->temporalTileSchedulerCompletionQueueOwnerCount +=
        src->temporalTileSchedulerCompletionQueueOwnerCount;
    dst->temporalTileSchedulerWorkerPoolOwnerCount +=
        src->temporalTileSchedulerWorkerPoolOwnerCount;
    dst->temporalTileSchedulerCancelTokenBound +=
        src->temporalTileSchedulerCancelTokenBound;
    dst->temporalTileSchedulerCancelCheckCount +=
        src->temporalTileSchedulerCancelCheckCount;
    dst->temporalTileSchedulerCancelRequestedCount +=
        src->temporalTileSchedulerCancelRequestedCount;
    dst->temporalTileSchedulerCancelBeforeDispatchCount +=
        src->temporalTileSchedulerCancelBeforeDispatchCount;
    dst->temporalTileSchedulerCancelDuringWaitCount +=
        src->temporalTileSchedulerCancelDuringWaitCount;
    dst->temporalTileSchedulerCancelBeforeFinalResolveCount +=
        src->temporalTileSchedulerCancelBeforeFinalResolveCount;
    dst->temporalTileSchedulerFinalResolveBlockedByCancelCount +=
        src->temporalTileSchedulerFinalResolveBlockedByCancelCount;
    dst->temporalTileSchedulerWorkerDrainShutdownCount +=
        src->temporalTileSchedulerWorkerDrainShutdownCount;
    dst->temporalTileSchedulerWorkerCancelShutdownCount +=
        src->temporalTileSchedulerWorkerCancelShutdownCount;
    if (src->temporalTileSchedulerCancelGeneration >
        dst->temporalTileSchedulerCancelGeneration) {
        dst->temporalTileSchedulerCancelGeneration =
            src->temporalTileSchedulerCancelGeneration;
    }
    dst->renderUnitScratchOwnerCount += src->renderUnitScratchOwnerCount;
    dst->renderUnitScratchSetupCalls += src->renderUnitScratchSetupCalls;
    dst->renderUnitScratchCacheAcquireHits += src->renderUnitScratchCacheAcquireHits;
    dst->renderUnitScratchCacheAcquireMisses += src->renderUnitScratchCacheAcquireMisses;
    dst->renderUnitRadianceScratchResizeCalls +=
        src->renderUnitRadianceScratchResizeCalls;
    dst->renderUnitRadianceScratchReuseCalls += src->renderUnitRadianceScratchReuseCalls;
    dst->renderUnitRadianceScratchClearBytes += src->renderUnitRadianceScratchClearBytes;
    if (src->renderUnitRadianceScratchRequestedBytesMax >
        dst->renderUnitRadianceScratchRequestedBytesMax) {
        dst->renderUnitRadianceScratchRequestedBytesMax =
            src->renderUnitRadianceScratchRequestedBytesMax;
    }
    if (src->renderUnitRadianceScratchCapacityBytesMax >
        dst->renderUnitRadianceScratchCapacityBytesMax) {
        dst->renderUnitRadianceScratchCapacityBytesMax =
            src->renderUnitRadianceScratchCapacityBytesMax;
    }
    if (src->renderUnitTemporalScratchCapacityBytesMax >
        dst->renderUnitTemporalScratchCapacityBytesMax) {
        dst->renderUnitTemporalScratchCapacityBytesMax =
            src->renderUnitTemporalScratchCapacityBytesMax;
    }
    if (src->renderUnitAdaptiveMaskScratchCapacityBytesMax >
        dst->renderUnitAdaptiveMaskScratchCapacityBytesMax) {
        dst->renderUnitAdaptiveMaskScratchCapacityBytesMax =
            src->renderUnitAdaptiveMaskScratchCapacityBytesMax;
    }
    if (src->renderUnitAdaptiveStateScratchCapacityBytesMax >
        dst->renderUnitAdaptiveStateScratchCapacityBytesMax) {
        dst->renderUnitAdaptiveStateScratchCapacityBytesMax =
            src->renderUnitAdaptiveStateScratchCapacityBytesMax;
    }
    if (src->renderUnitFeatureScratchCapacityBytesMax >
        dst->renderUnitFeatureScratchCapacityBytesMax) {
        dst->renderUnitFeatureScratchCapacityBytesMax =
            src->renderUnitFeatureScratchCapacityBytesMax;
    }
    dst->renderUnitScratchOwnedBytes += src->renderUnitScratchOwnedBytes;
    if (src->renderUnitScratchMaxOwnerBytes > dst->renderUnitScratchMaxOwnerBytes) {
        dst->renderUnitScratchMaxOwnerBytes = src->renderUnitScratchMaxOwnerBytes;
    }
    if (src->renderUnitScratchMaxFrameOwnedBytes >
        dst->renderUnitScratchMaxFrameOwnedBytes) {
        dst->renderUnitScratchMaxFrameOwnedBytes =
            src->renderUnitScratchMaxFrameOwnedBytes;
    }
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

void runtime_native_3d_render_stats_normalize_temporal(
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
    stats->causticVolumeScatterContributingPixelCount =
        runtime_native_3d_render_stats_round_divide(
            stats->causticVolumeScatterContributingPixelCount,
            committed_subpasses);
    stats->volumeScatterDirectSampleCount = runtime_native_3d_render_stats_round_divide(
        stats->volumeScatterDirectSampleCount, committed_subpasses);
    stats->totalDirectVolumeScatterRadianceR /= (double)committed_subpasses;
    stats->totalDirectVolumeScatterRadianceG /= (double)committed_subpasses;
    stats->totalDirectVolumeScatterRadianceB /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterRadianceR /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterRadianceG /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterRadianceB /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterSampledCacheRadiance /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterSampledRawDensity /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterSampledDensity /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterProbability /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterCameraTransmittance /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterVisibilityTerm /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterPixelX /= (double)committed_subpasses;
    stats->totalCausticVolumeScatterPixelY /= (double)committed_subpasses;
    stats->totalCausticSurfaceRadianceR /= (double)committed_subpasses;
    stats->totalCausticSurfaceRadianceG /= (double)committed_subpasses;
    stats->totalCausticSurfaceRadianceB /= (double)committed_subpasses;
    stats->totalMirrorSpecularReflectionRadiance /= (double)committed_subpasses;
    stats->totalMirrorBaseRadianceBeforeAttenuation /= (double)committed_subpasses;
    stats->totalMirrorBaseRadianceAfterAttenuation /= (double)committed_subpasses;
}

void runtime_native_3d_render_stats_record_adaptive_state_summary(
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
    stats->temporalAdaptiveStateActivityRiskPixels += summary->activityRiskPixelCount;
    stats->temporalAdaptiveStateMaterialRiskPixels += summary->materialRiskPixelCount;
    stats->temporalAdaptiveStateTransparentRiskPixels += summary->transparentRiskPixelCount;
    stats->temporalAdaptiveStateGlossyRiskPixels += summary->glossyRiskPixelCount;
    stats->temporalAdaptiveStateGeometryEdgeRiskPixels += summary->geometryEdgeRiskPixelCount;
    stats->temporalAdaptiveStateDirectLightNoTracePixels +=
        summary->directLightNoTracePixelCount;
    stats->temporalAdaptiveStateDirectLightClearVisiblePixels +=
        summary->directLightClearVisiblePixelCount;
    stats->temporalAdaptiveStateDirectLightClearBlockedPixels +=
        summary->directLightClearBlockedPixelCount;
    stats->temporalAdaptiveStateDirectLightStablePartialPixels +=
        summary->directLightStablePartialPixelCount;
    stats->temporalAdaptiveStateDirectLightMixedPartialPixels +=
        summary->directLightMixedPartialPixelCount;
    stats->temporalAdaptiveStateDirectLightBoundaryRiskPixels +=
        summary->directLightBoundaryRiskPixelCount;
    stats->temporalAdaptiveEarlyStopEligiblePixels +=
        summary->earlyStopEligiblePixelCount;
    stats->temporalAdaptiveEarlyStopHeldPixels +=
        summary->earlyStopHeldPixelCount;
    stats->temporalAdaptiveEarlyStopHoldProbePixels +=
        summary->earlyStopHoldProbePixelCount;
    stats->temporalAdaptiveEarlyStopHoldHighRiskPixels +=
        summary->earlyStopHoldHighRiskPixelCount;
    stats->temporalAdaptiveEarlyStopHoldActivityRiskPixels +=
        summary->earlyStopHoldActivityRiskPixelCount;
    stats->temporalAdaptiveEarlyStopHoldMaterialRiskPixels +=
        summary->earlyStopHoldMaterialRiskPixelCount;
    stats->temporalAdaptiveEarlyStopHoldTransparentRiskPixels +=
        summary->earlyStopHoldTransparentRiskPixelCount;
    stats->temporalAdaptiveEarlyStopHoldGeometryEdgeRiskPixels +=
        summary->earlyStopHoldGeometryEdgeRiskPixelCount;
    stats->temporalAdaptiveEarlyStopHoldDirectLightRiskPixels +=
        summary->earlyStopHoldDirectLightRiskPixelCount;
    for (int i = 0; i < RUNTIME_NATIVE_3D_ADAPTIVE_REGION_COUNT; ++i) {
        stats->temporalAdaptiveEarlyStopEligibleRegionCounts[i] +=
            summary->earlyStopEligibleRegionCounts[i];
        stats->temporalAdaptiveEarlyStopHeldRegionCounts[i] +=
            summary->earlyStopHeldRegionCounts[i];
    }
    for (int i = 0; i < RUNTIME_NATIVE_3D_TEMPORAL_BUDGET_BUCKET_COUNT; ++i) {
        stats->temporalAdaptiveBudgetBucketPixels[i] +=
            summary->budgetBucketPixelCounts[i];
        stats->temporalAdaptiveBudgetActiveBucketPixels[i] +=
            summary->budgetActiveBucketPixelCounts[i];
        stats->temporalAdaptiveBudgetEligibleBucketPixels[i] +=
            summary->budgetEligibleBucketPixelCounts[i];
        stats->temporalAdaptiveBudgetHeldBucketPixels[i] +=
            summary->budgetHeldBucketPixelCounts[i];
    }
    stats->temporalAdaptiveBudgetClearVisibleEligiblePixels +=
        summary->budgetClearVisibleEligiblePixelCount;
    stats->temporalAdaptiveBudgetClearVisibleHeldPixels +=
        summary->budgetClearVisibleHeldPixelCount;
    stats->temporalAdaptiveBudgetPartialHeldPixels +=
        summary->budgetPartialHeldPixelCount;
    stats->temporalAdaptiveBudgetTransparentHeldPixels +=
        summary->budgetTransparentHeldPixelCount;
    stats->temporalAdaptiveBudgetGeometryHeldPixels +=
        summary->budgetGeometryHeldPixelCount;
    stats->temporalAdaptiveBudgetActivityHeldPixels +=
        summary->budgetActivityHeldPixelCount;
    stats->temporalAdaptiveStateMixedRiskTiles += summary->mixedRiskTileCount;
    stats->temporalAdaptiveStateRiskSum += summary->riskSum;
    if (summary->riskMax > stats->temporalAdaptiveStateRiskMax) {
        stats->temporalAdaptiveStateRiskMax = summary->riskMax;
    }
    if (summary->minSampleFloor > stats->temporalAdaptiveStateMinSampleFloor) {
        stats->temporalAdaptiveStateMinSampleFloor = summary->minSampleFloor;
    }
}
