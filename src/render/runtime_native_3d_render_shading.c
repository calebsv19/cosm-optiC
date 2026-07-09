#include "render/runtime_native_3d_render_shading_internal.h"




bool runtime_native_3d_render_dispatch_integrator(float* radiance_buffer,
                                                  int radiance_stride,
                                                  RayTracing3DIntegratorId integrator_id,
                                                  const RuntimeNative3DPreparedFrame* frame,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y,
                                                  RuntimeNative3DRenderStats* out_stats) {
    RuntimeCausticVolumeCache3D* caustic_cache = NULL;
    RuntimeCausticSurfaceCache3D* surface_cache = NULL;
    RuntimeCausticVolumeCacheDiagnostics3D cache_diagnostics = {0};
    RuntimeCausticSurfaceCacheDiagnostics3D surface_diagnostics = {0};
    const RuntimeScene3D* scene = NULL;
    bool ok = false;
    if (!frame || !frame->valid) {
        return false;
    }
    scene = frame->traceScene ? frame->traceScene : &frame->scene;
    caustic_cache = (RuntimeCausticVolumeCache3D*)&frame->causticVolumeCache;
    surface_cache = (RuntimeCausticSurfaceCache3D*)&frame->causticSurfaceCache;

    /* Keep the native renderer dispatch explicit so later 3D tiers can add
     * focused shader paths without overloading the entrypoint itself. */
    switch (integrator_id) {
        case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
            ok = runtime_native_3d_render_shade_direct_light(radiance_buffer,
                                                             radiance_stride,
                                                             frame->width,
                                                             frame->height,
                                                             start_x,
                                                             start_y,
                                                             end_x,
                                                             end_y,
                                                             scene,
                                                             &frame->projector,
                                                             &frame->sampling,
                                                             caustic_cache,
                                                             frame->directLightVisibilityAttributionEnabled
                                                                 ? frame->featureAttributionBuffer
                                                                 : NULL,
                                                             frame->featureAttributionStartX,
                                                             frame->featureAttributionStartY,
                                                             out_stats);
            break;
        case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
            ok = runtime_native_3d_render_shade_diffuse_bounce(radiance_buffer,
                                                               radiance_stride,
                                                               frame->width,
                                                               frame->height,
                                                               start_x,
                                                               start_y,
                                                               end_x,
                                                               end_y,
                                                               scene,
                                                               &frame->projector,
                                                               &frame->sampling,
                                                               caustic_cache,
                                                               out_stats);
            break;
        case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
            ok = runtime_native_3d_render_shade_material(radiance_buffer,
                                                         radiance_stride,
                                                         frame->width,
                                                         frame->height,
                                                         start_x,
                                                         start_y,
                                                         end_x,
                                                         end_y,
                                                         scene,
                                                         &frame->projector,
                                                         &frame->sampling,
                                                         caustic_cache,
                                                         out_stats);
            break;
        case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            ok = runtime_native_3d_render_shade_emission_transparency(radiance_buffer,
                                                                      radiance_stride,
                                                                      frame->width,
                                                                      frame->height,
                                                                      start_x,
                                                                      start_y,
                                                                      end_x,
                                                                      end_y,
                                                                      scene,
                                                                      &frame->projector,
                                                                      &frame->sampling,
                                                                      caustic_cache,
                                                                      out_stats);
            break;
        case RAY_TRACING_3D_INTEGRATOR_DISNEY:
            ok = runtime_native_3d_render_shade_disney(radiance_buffer,
                                                       radiance_stride,
                                                       frame->width,
                                                       frame->height,
                                                       start_x,
                                                       start_y,
                                                       end_x,
                                                       end_y,
                                                       scene,
                                                       &frame->projector,
                                                       &frame->sampling,
                                                       caustic_cache,
                                                       out_stats);
            break;
        case RAY_TRACING_3D_INTEGRATOR_DISNEY_V2:
            ok = runtime_native_3d_render_shade_disney_v2(radiance_buffer,
                                                          radiance_stride,
                                                          frame->width,
                                                          frame->height,
                                                          start_x,
                                                          start_y,
                                                          end_x,
                                                          end_y,
                                                          scene,
                                                          &frame->projector,
                                                          &frame->sampling,
                                                          caustic_cache,
                                                          surface_cache,
                                                          frame->causticSidecarProbeValid
                                                              ? &frame->causticSidecarProbe
                                                              : NULL,
                                                          frame->directLightVisibilityAttributionEnabled
                                                              ? frame->featureAttributionBuffer
                                                              : NULL,
                                                          frame->featureAttributionStartX,
                                                          frame->featureAttributionStartY,
                                                          out_stats);
            break;
        default:
            return false;
    }
    if (ok && out_stats) {
        RuntimeCausticVolumeCache3D_SnapshotDiagnostics(caustic_cache, &cache_diagnostics);
        RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(surface_cache, &surface_diagnostics);
        out_stats->causticBootstrapTemporaryBridgeActive =
            frame->causticBootstrapDiagnostics.temporaryAnalyticBridge ? 1 : 0;
        out_stats->causticTransportPathEmissionActive =
            frame->causticTransportDiagnostics.active ? 1 : 0;
        out_stats->causticVolumeCacheSuppressedNoSampleableVolume =
            frame->causticTransportDiagnostics.volumeCacheSuppressedNoSampleableVolume ? 1 : 0;
        out_stats->causticTransportLightCount =
            (int)frame->causticTransportDiagnostics.lightCount;
        out_stats->causticTransportEvaluatedPathCount =
            (int)frame->causticTransportDiagnostics.evaluatedPathCount;
        out_stats->causticTransportEmittedPathCount =
            (int)frame->causticTransportDiagnostics.emittedPathCount;
        out_stats->causticTransportAnalyticSphereLensResolvedCount =
            (int)frame->causticTransportDiagnostics.analyticSphereLensResolvedCount;
        out_stats->causticTransportAnalyticSphereLensRejectedCount =
            (int)frame->causticTransportDiagnostics.analyticSphereLensRejectedCount;
        out_stats->causticTransportAnalyticSphereLensEvaluatedPathCount =
            (int)frame->causticTransportDiagnostics.analyticSphereLensEvaluatedPathCount;
        out_stats->causticTransportAnalyticSphereLensEmittedPathCount =
            (int)frame->causticTransportDiagnostics.analyticSphereLensEmittedPathCount;
        out_stats->causticTransportAnalyticSphereLensSampleWeight =
            frame->causticTransportDiagnostics.analyticSphereLensSampleWeight;
        out_stats->causticTransportAnalyticSphereLensTotalSampleWeight =
            frame->causticTransportDiagnostics.analyticSphereLensTotalSampleWeight;
        out_stats->causticTransportAnalyticCylinderLensResolvedCount =
            (int)frame->causticTransportDiagnostics.analyticCylinderLensResolvedCount;
        out_stats->causticTransportAnalyticCylinderLensRejectedCount =
            (int)frame->causticTransportDiagnostics.analyticCylinderLensRejectedCount;
        out_stats->causticTransportAnalyticCylinderLensEvaluatedPathCount =
            (int)frame->causticTransportDiagnostics.analyticCylinderLensEvaluatedPathCount;
        out_stats->causticTransportAnalyticCylinderLensEmittedPathCount =
            (int)frame->causticTransportDiagnostics.analyticCylinderLensEmittedPathCount;
        out_stats->causticTransportAnalyticCylinderLensSampleWeight =
            frame->causticTransportDiagnostics.analyticCylinderLensSampleWeight;
        out_stats->causticTransportAnalyticCylinderLensTotalSampleWeight =
            frame->causticTransportDiagnostics.analyticCylinderLensTotalSampleWeight;
        out_stats->causticTransportAnalyticPrismLensResolvedCount =
            (int)frame->causticTransportDiagnostics.analyticPrismLensResolvedCount;
        out_stats->causticTransportAnalyticPrismLensRejectedCount =
            (int)frame->causticTransportDiagnostics.analyticPrismLensRejectedCount;
        out_stats->causticTransportAnalyticPrismLensEvaluatedPathCount =
            (int)frame->causticTransportDiagnostics.analyticPrismLensEvaluatedPathCount;
        out_stats->causticTransportAnalyticPrismLensEmittedPathCount =
            (int)frame->causticTransportDiagnostics.analyticPrismLensEmittedPathCount;
        out_stats->causticTransportAnalyticPrismLensSampleWeight =
            frame->causticTransportDiagnostics.analyticPrismLensSampleWeight;
        out_stats->causticTransportAnalyticPrismLensTotalSampleWeight =
            frame->causticTransportDiagnostics.analyticPrismLensTotalSampleWeight;
        out_stats->causticTransportAnalyticBowlLensResolvedCount =
            (int)frame->causticTransportDiagnostics.analyticBowlLensResolvedCount;
        out_stats->causticTransportAnalyticBowlLensRejectedCount =
            (int)frame->causticTransportDiagnostics.analyticBowlLensRejectedCount;
        out_stats->causticTransportAnalyticBowlLensEvaluatedPathCount =
            (int)frame->causticTransportDiagnostics.analyticBowlLensEvaluatedPathCount;
        out_stats->causticTransportAnalyticBowlLensEmittedPathCount =
            (int)frame->causticTransportDiagnostics.analyticBowlLensEmittedPathCount;
        out_stats->causticTransportAnalyticBowlLensSampleWeight =
            frame->causticTransportDiagnostics.analyticBowlLensSampleWeight;
        out_stats->causticTransportAnalyticBowlLensTotalSampleWeight =
            frame->causticTransportDiagnostics.analyticBowlLensTotalSampleWeight;
        out_stats->causticTransportMeshDielectricLensResolvedCount =
            (int)frame->causticTransportDiagnostics.meshDielectricLensResolvedCount;
        out_stats->causticTransportMeshDielectricLensRejectedCount =
            (int)frame->causticTransportDiagnostics.meshDielectricLensRejectedCount;
        out_stats->causticTransportMeshDielectricLensEvaluatedPathCount =
            (int)frame->causticTransportDiagnostics.meshDielectricLensEvaluatedPathCount;
        out_stats->causticTransportMeshDielectricLensEmittedPathCount =
            (int)frame->causticTransportDiagnostics.meshDielectricLensEmittedPathCount;
        out_stats->causticTransportMeshDielectricLensSampleWeight =
            frame->causticTransportDiagnostics.meshDielectricLensSampleWeight;
        out_stats->causticTransportMeshDielectricLensTotalSampleWeight =
            frame->causticTransportDiagnostics.meshDielectricLensTotalSampleWeight;
        out_stats->causticTransportTransparentHitCount =
            (int)frame->causticTransportDiagnostics.transparentHitCount;
        out_stats->causticTransportSpecularEventCount =
            (int)frame->causticTransportDiagnostics.specularEventCount;
        out_stats->causticTransportVolumeSegmentCount =
            (int)frame->causticTransportDiagnostics.volumeSegmentCount;
        out_stats->causticTransportSurfaceReceiverTraceMissCount =
            (int)frame->causticTransportDiagnostics.surfaceReceiverTraceMissCount;
        out_stats->causticTransportSurfaceReceiverDepthRejectCount =
            (int)frame->causticTransportDiagnostics.surfaceReceiverDepthRejectCount;
        out_stats->causticTransportSurfaceReceiverHitCount =
            (int)frame->causticTransportDiagnostics.surfaceReceiverHitCount;
        out_stats->causticTransportSurfaceReceiverFallbackCount =
            (int)frame->causticTransportDiagnostics.surfaceReceiverFallbackCount;
        out_stats->causticVolumeCacheBound =
            RuntimeCausticVolumeCache3D_IsAllocated(caustic_cache) ? 1 : 0;
        out_stats->causticVolumeCacheAllocated = cache_diagnostics.allocated ? 1 : 0;
        out_stats->causticVolumeCacheCellCount = (int)cache_diagnostics.cellCount;
        out_stats->causticVolumeCacheNonZeroCellCount =
            (int)cache_diagnostics.nonZeroCellCount;
        out_stats->causticVolumeCacheDepositAttemptCount =
            (int)cache_diagnostics.depositAttemptCount;
        out_stats->causticVolumeCacheDepositAcceptedCount =
            (int)cache_diagnostics.depositAcceptedCount;
        out_stats->causticVolumeCacheDepositRejectedCount =
            (int)cache_diagnostics.depositRejectedCount;
        out_stats->causticVolumeCacheFootprintDepositCount =
            (int)cache_diagnostics.footprintDepositCount;
        out_stats->causticVolumeCacheFootprintCellContributionCount =
            (int)cache_diagnostics.footprintCellContributionCount;
        out_stats->causticVolumeCacheSampleLookupCount =
            (int)cache_diagnostics.sampleLookupCount;
        out_stats->causticVolumeCacheSampleContributingCount =
            (int)cache_diagnostics.sampleContributingCount;
        out_stats->totalCausticVolumeCacheRadianceR = cache_diagnostics.totalRadianceR;
        out_stats->totalCausticVolumeCacheRadianceG = cache_diagnostics.totalRadianceG;
        out_stats->totalCausticVolumeCacheRadianceB = cache_diagnostics.totalRadianceB;
        out_stats->maxCausticVolumeCacheRadiance = cache_diagnostics.maxCellRadiance;
        out_stats->causticVolumeCacheAverageFootprintRadiusVoxels =
            cache_diagnostics.averageFootprintRadiusVoxels;
        out_stats->totalCausticVolumeCacheFootprintInputRadianceR =
            cache_diagnostics.footprintInputRadianceR;
        out_stats->totalCausticVolumeCacheFootprintInputRadianceG =
            cache_diagnostics.footprintInputRadianceG;
        out_stats->totalCausticVolumeCacheFootprintInputRadianceB =
            cache_diagnostics.footprintInputRadianceB;
        out_stats->totalCausticVolumeCacheFootprintDepositedRadianceR =
            cache_diagnostics.footprintDepositedRadianceR;
        out_stats->totalCausticVolumeCacheFootprintDepositedRadianceG =
            cache_diagnostics.footprintDepositedRadianceG;
        out_stats->totalCausticVolumeCacheFootprintDepositedRadianceB =
            cache_diagnostics.footprintDepositedRadianceB;
        out_stats->causticVolumeCacheNonZeroCellRatio =
            cache_diagnostics.cellCount > 0u
                ? (double)cache_diagnostics.nonZeroCellCount /
                      (double)cache_diagnostics.cellCount
                : 0.0;
        out_stats->causticVolumeCacheSampleHitRatio =
            cache_diagnostics.sampleLookupCount > 0u
                ? (double)cache_diagnostics.sampleContributingCount /
                      (double)cache_diagnostics.sampleLookupCount
                : 0.0;
        out_stats->causticVolumeCacheRadianceCentroidX =
            cache_diagnostics.radianceCentroid.x;
        out_stats->causticVolumeCacheRadianceCentroidY =
            cache_diagnostics.radianceCentroid.y;
        out_stats->causticVolumeCacheRadianceCentroidZ =
            cache_diagnostics.radianceCentroid.z;
        if (cache_diagnostics.hasNonZeroBounds) {
            out_stats->causticVolumeCacheNonZeroBoundsMinX =
                cache_diagnostics.nonZeroBoundsMin.x;
            out_stats->causticVolumeCacheNonZeroBoundsMinY =
                cache_diagnostics.nonZeroBoundsMin.y;
            out_stats->causticVolumeCacheNonZeroBoundsMinZ =
                cache_diagnostics.nonZeroBoundsMin.z;
            out_stats->causticVolumeCacheNonZeroBoundsMaxX =
                cache_diagnostics.nonZeroBoundsMax.x;
            out_stats->causticVolumeCacheNonZeroBoundsMaxY =
                cache_diagnostics.nonZeroBoundsMax.y;
            out_stats->causticVolumeCacheNonZeroBoundsMaxZ =
                cache_diagnostics.nonZeroBoundsMax.z;
        }
        out_stats->causticSurfaceCacheBound =
            RuntimeCausticSurfaceCache3D_IsAllocated(surface_cache) ? 1 : 0;
        out_stats->causticSurfaceCacheAllocated = surface_diagnostics.allocated ? 1 : 0;
        out_stats->causticSurfaceCacheRecordCapacity =
            (int)surface_diagnostics.recordCapacity;
        out_stats->causticSurfaceCacheRecordCount = (int)surface_diagnostics.recordCount;
        out_stats->causticSurfaceCacheDepositAttemptCount =
            (int)surface_diagnostics.depositAttemptCount;
        out_stats->causticSurfaceCacheDepositAcceptedCount =
            (int)surface_diagnostics.depositAcceptedCount;
        out_stats->causticSurfaceCacheDepositRejectedCount =
            (int)surface_diagnostics.depositRejectedCount;
        out_stats->causticSurfaceCacheSampleLookupCount =
            (int)surface_diagnostics.sampleLookupCount;
        out_stats->causticSurfaceCacheSampleContributingCount =
            (int)surface_diagnostics.sampleContributingCount;
        out_stats->causticSurfaceCacheNearestSampleDistance =
            surface_diagnostics.nearestSampleDistance;
        out_stats->causticSurfaceCacheNearestSampleRadius =
            surface_diagnostics.nearestSampleRadius;
        out_stats->causticSurfaceCacheNearestSampleNormalDot =
            surface_diagnostics.nearestSampleNormalDot;
        out_stats->causticSurfaceCacheNearestSampleCandidateCount =
            (double)surface_diagnostics.nearestSampleCandidateCount;
        out_stats->totalCausticSurfaceCacheRadianceR = surface_diagnostics.totalRadianceR;
        out_stats->totalCausticSurfaceCacheRadianceG = surface_diagnostics.totalRadianceG;
        out_stats->totalCausticSurfaceCacheRadianceB = surface_diagnostics.totalRadianceB;
        if (surface_diagnostics.maxRecordRadiance > out_stats->maxCausticSurfaceCacheRadiance) {
            out_stats->maxCausticSurfaceCacheRadiance = surface_diagnostics.maxRecordRadiance;
        }
    }
    return ok;
}
