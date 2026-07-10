#include "tools/ray_tracing_render_headless_internal.h"

#include "render/runtime_disney_v2_caustic_sidecar_3d.h"

#include <stdio.h>

static double ray_tracing_headless_caustic_rgb_sum(double r, double g, double b) {
    return r + g + b;
}

static double ray_tracing_headless_caustic_safe_ratio(double numerator, double denominator) {
    return denominator > 0.0 ? numerator / denominator : 0.0;
}

void ray_tracing_headless_write_caustic_state_summary(
    FILE *file,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight) {
    {
        RuntimeCausticReadback3D caustic_readback =
            RuntimeCausticSettings3D_Phase0Readback(&request->caustic_settings,
                                                    request->caustic_sidecar_enabled);
        fprintf(file, "    \"caustic_state\": {\n");
        fprintf(file, "      \"mode\": \"%s\",\n",
                RuntimeCausticMode3D_Label(caustic_readback.mode));
        fprintf(file, "      \"analytic_sidecar_requested\": %s,\n",
                caustic_readback.analyticSidecarRequested ? "true" : "false");
        fprintf(file, "      \"volume_cache_requested\": %s,\n",
                caustic_readback.volumeCacheRequested ? "true" : "false");
        fprintf(file, "      \"surface_cache_requested\": %s,\n",
                caustic_readback.surfaceCacheRequested ? "true" : "false");
        fprintf(file, "      \"cache_grid_mode\": \"%s\",\n",
                RuntimeCausticCacheGridMode3D_Label(request->caustic_settings.cacheGridMode));
        fprintf(file, "      \"surface_radiance_scale\": %.9f,\n",
                request->caustic_settings.surfaceRadianceScale);
        fprintf(file, "      \"surface_footprint_scale\": %.9f,\n",
                request->caustic_settings.surfaceFootprintScale);
        fprintf(file, "      \"surface_receiver_fallback_enabled\": %s,\n",
                request->caustic_settings.surfaceReceiverFallbackEnabled ? "true" : "false");
        fprintf(file, "      \"lens_traversal_profile_override_enabled\": %s,\n",
                request->caustic_settings.hasTraversalProfileOverride ? "true" : "false");
        fprintf(file, "      \"lens_traversal_profile_kind\": %d,\n",
                (int)request->caustic_settings.traversalProfileOverride.kind);
        fprintf(file, "      \"lens_outside_ior\": %.9f,\n",
                request->caustic_settings.traversalProfileOverride.outsideIor);
        fprintf(file, "      \"lens_material_ior\": %.9f,\n",
                request->caustic_settings.traversalProfileOverride.materialIor);
        fprintf(file, "      \"lens_fresnel_scale\": %.9f,\n",
                request->caustic_settings.traversalProfileOverride.fresnelScale);
        fprintf(file, "      \"lens_transmission_scale\": %.9f,\n",
                request->caustic_settings.traversalProfileOverride.transmissionScale);
        fprintf(file,
                "      \"lens_tint\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
                request->caustic_settings.traversalProfileOverride.tint.x,
                request->caustic_settings.traversalProfileOverride.tint.y,
                request->caustic_settings.traversalProfileOverride.tint.z);
        fprintf(file, "      \"lens_absorption_distance\": %.9f,\n",
                request->caustic_settings.traversalProfileOverride.absorptionDistance);
        fprintf(file, "      \"lens_aperture_radius_scale\": %.9f,\n",
                request->caustic_settings.traversalProfileOverride.apertureRadiusScale);
        fprintf(file, "      \"volume_cache_state\": \"%s\",\n",
                RuntimeCausticCacheState3D_Label(caustic_readback.volumeCacheState));
        fprintf(file, "      \"surface_cache_state\": \"%s\",\n",
                RuntimeCausticCacheState3D_Label(caustic_readback.surfaceCacheState));
        fprintf(file, "      \"transport_engine\": \"%s\",\n",
                RuntimeCausticTransportEngine3D_Label(caustic_readback.transportEngine));
        fprintf(file, "      \"path_emission_active\": %s,\n",
                caustic_readback.pathEmissionActive ? "true" : "false");
        fprintf(file, "      \"transport_reserved\": %s,\n",
                caustic_readback.transportReserved ? "true" : "false");
        fprintf(file, "      \"photon_map_requested\": %s,\n",
                caustic_readback.photonMapRequested ? "true" : "false");
        fprintf(file, "      \"photon_map_implemented\": %s,\n",
                caustic_readback.photonMapImplemented ? "true" : "false");
        fprintf(file, "      \"temporary_analytic_bridge\": %s,\n",
                preflight->stats.causticBootstrapTemporaryBridgeActive > 0 ? "true" : "false");
        fprintf(file, "      \"transport_path_emission_active\": %s,\n",
                preflight->stats.causticTransportPathEmissionActive > 0 ? "true" : "false");
        fprintf(file, "      \"volume_cache_suppressed_no_sampleable_volume\": %s,\n",
                preflight->stats.causticVolumeCacheSuppressedNoSampleableVolume > 0 ? "true" : "false");
        fprintf(file, "      \"transport_light_count\": %d,\n",
                preflight->stats.causticTransportLightCount);
        fprintf(file, "      \"transport_evaluated_path_count\": %d,\n",
                preflight->stats.causticTransportEvaluatedPathCount);
        fprintf(file, "      \"transport_emitted_path_count\": %d,\n",
                preflight->stats.causticTransportEmittedPathCount);
        fprintf(file, "      \"transport_analytic_sphere_lens_resolved_count\": %d,\n",
                preflight->stats.causticTransportAnalyticSphereLensResolvedCount);
        fprintf(file, "      \"transport_analytic_sphere_lens_rejected_count\": %d,\n",
                preflight->stats.causticTransportAnalyticSphereLensRejectedCount);
        fprintf(file, "      \"transport_analytic_sphere_lens_evaluated_path_count\": %d,\n",
                preflight->stats.causticTransportAnalyticSphereLensEvaluatedPathCount);
        fprintf(file, "      \"transport_analytic_sphere_lens_emitted_path_count\": %d,\n",
                preflight->stats.causticTransportAnalyticSphereLensEmittedPathCount);
        fprintf(file, "      \"transport_analytic_sphere_lens_sample_weight\": %.9f,\n",
                preflight->stats.causticTransportAnalyticSphereLensSampleWeight);
        fprintf(file, "      \"transport_analytic_sphere_lens_total_sample_weight\": %.9f,\n",
                preflight->stats.causticTransportAnalyticSphereLensTotalSampleWeight);
        fprintf(file, "      \"transport_analytic_cylinder_lens_resolved_count\": %d,\n",
                preflight->stats.causticTransportAnalyticCylinderLensResolvedCount);
        fprintf(file, "      \"transport_analytic_cylinder_lens_rejected_count\": %d,\n",
                preflight->stats.causticTransportAnalyticCylinderLensRejectedCount);
        fprintf(file, "      \"transport_analytic_cylinder_lens_evaluated_path_count\": %d,\n",
                preflight->stats.causticTransportAnalyticCylinderLensEvaluatedPathCount);
        fprintf(file, "      \"transport_analytic_cylinder_lens_emitted_path_count\": %d,\n",
                preflight->stats.causticTransportAnalyticCylinderLensEmittedPathCount);
        fprintf(file, "      \"transport_analytic_cylinder_lens_sample_weight\": %.9f,\n",
                preflight->stats.causticTransportAnalyticCylinderLensSampleWeight);
        fprintf(file, "      \"transport_analytic_cylinder_lens_total_sample_weight\": %.9f,\n",
                preflight->stats.causticTransportAnalyticCylinderLensTotalSampleWeight);
        fprintf(file, "      \"transport_analytic_prism_lens_resolved_count\": %d,\n",
                preflight->stats.causticTransportAnalyticPrismLensResolvedCount);
        fprintf(file, "      \"transport_analytic_prism_lens_rejected_count\": %d,\n",
                preflight->stats.causticTransportAnalyticPrismLensRejectedCount);
        fprintf(file, "      \"transport_analytic_prism_lens_evaluated_path_count\": %d,\n",
                preflight->stats.causticTransportAnalyticPrismLensEvaluatedPathCount);
        fprintf(file, "      \"transport_analytic_prism_lens_emitted_path_count\": %d,\n",
                preflight->stats.causticTransportAnalyticPrismLensEmittedPathCount);
        fprintf(file, "      \"transport_analytic_prism_lens_sample_weight\": %.9f,\n",
                preflight->stats.causticTransportAnalyticPrismLensSampleWeight);
        fprintf(file, "      \"transport_analytic_prism_lens_total_sample_weight\": %.9f,\n",
                preflight->stats.causticTransportAnalyticPrismLensTotalSampleWeight);
        fprintf(file, "      \"transport_analytic_bowl_lens_resolved_count\": %d,\n",
                preflight->stats.causticTransportAnalyticBowlLensResolvedCount);
        fprintf(file, "      \"transport_analytic_bowl_lens_rejected_count\": %d,\n",
                preflight->stats.causticTransportAnalyticBowlLensRejectedCount);
        fprintf(file, "      \"transport_analytic_bowl_lens_evaluated_path_count\": %d,\n",
                preflight->stats.causticTransportAnalyticBowlLensEvaluatedPathCount);
        fprintf(file, "      \"transport_analytic_bowl_lens_emitted_path_count\": %d,\n",
                preflight->stats.causticTransportAnalyticBowlLensEmittedPathCount);
        fprintf(file, "      \"transport_analytic_bowl_lens_sample_weight\": %.9f,\n",
                preflight->stats.causticTransportAnalyticBowlLensSampleWeight);
        fprintf(file, "      \"transport_analytic_bowl_lens_total_sample_weight\": %.9f,\n",
                preflight->stats.causticTransportAnalyticBowlLensTotalSampleWeight);
        fprintf(file, "      \"transport_mesh_dielectric_lens_resolved_count\": %d,\n",
                preflight->stats.causticTransportMeshDielectricLensResolvedCount);
        fprintf(file, "      \"transport_mesh_dielectric_lens_rejected_count\": %d,\n",
                preflight->stats.causticTransportMeshDielectricLensRejectedCount);
        fprintf(file, "      \"transport_mesh_dielectric_lens_evaluated_path_count\": %d,\n",
                preflight->stats.causticTransportMeshDielectricLensEvaluatedPathCount);
        fprintf(file, "      \"transport_mesh_dielectric_lens_emitted_path_count\": %d,\n",
                preflight->stats.causticTransportMeshDielectricLensEmittedPathCount);
        fprintf(file, "      \"transport_mesh_dielectric_lens_sample_weight\": %.9f,\n",
                preflight->stats.causticTransportMeshDielectricLensSampleWeight);
        fprintf(file, "      \"transport_mesh_dielectric_lens_total_sample_weight\": %.9f,\n",
                preflight->stats.causticTransportMeshDielectricLensTotalSampleWeight);
        fprintf(file, "      \"transport_transparent_hit_count\": %d,\n",
                preflight->stats.causticTransportTransparentHitCount);
        fprintf(file, "      \"transport_specular_event_count\": %d,\n",
                preflight->stats.causticTransportSpecularEventCount);
        fprintf(file, "      \"transport_volume_segment_count\": %d,\n",
                preflight->stats.causticTransportVolumeSegmentCount);
        fprintf(file, "      \"transport_surface_receiver_trace_miss_count\": %d,\n",
                preflight->stats.causticTransportSurfaceReceiverTraceMissCount);
        fprintf(file, "      \"transport_surface_receiver_depth_reject_count\": %d,\n",
                preflight->stats.causticTransportSurfaceReceiverDepthRejectCount);
        fprintf(file, "      \"transport_surface_receiver_hit_count\": %d,\n",
                preflight->stats.causticTransportSurfaceReceiverHitCount);
        fprintf(file, "      \"transport_surface_receiver_fallback_count\": %d,\n",
                preflight->stats.causticTransportSurfaceReceiverFallbackCount);
        fprintf(file, "      \"volume_cache_bound\": %s,\n",
                preflight->stats.causticVolumeCacheBound > 0 ? "true" : "false");
        fprintf(file, "      \"volume_cache_allocated\": %s,\n",
                preflight->stats.causticVolumeCacheAllocated > 0 ? "true" : "false");
        fprintf(file, "      \"volume_cache_cell_count\": %d,\n",
                preflight->stats.causticVolumeCacheCellCount);
        fprintf(file, "      \"volume_cache_nonzero_cell_count\": %d,\n",
                preflight->stats.causticVolumeCacheNonZeroCellCount);
        fprintf(file, "      \"volume_cache_deposit_attempt_count\": %d,\n",
                preflight->stats.causticVolumeCacheDepositAttemptCount);
        fprintf(file, "      \"volume_cache_deposit_accepted_count\": %d,\n",
                preflight->stats.causticVolumeCacheDepositAcceptedCount);
        fprintf(file, "      \"volume_cache_deposit_rejected_count\": %d,\n",
                preflight->stats.causticVolumeCacheDepositRejectedCount);
        fprintf(file, "      \"volume_cache_footprint_deposit_count\": %d,\n",
                preflight->stats.causticVolumeCacheFootprintDepositCount);
        fprintf(file, "      \"volume_cache_footprint_cell_contribution_count\": %d,\n",
                preflight->stats.causticVolumeCacheFootprintCellContributionCount);
        fprintf(file, "      \"volume_cache_average_footprint_radius_voxels\": %.9f,\n",
                preflight->stats.causticVolumeCacheAverageFootprintRadiusVoxels);
        fprintf(file, "      \"volume_cache_sample_lookup_count\": %d,\n",
                preflight->stats.causticVolumeCacheSampleLookupCount);
        fprintf(file, "      \"volume_cache_sample_contributing_count\": %d,\n",
                preflight->stats.causticVolumeCacheSampleContributingCount);
        fprintf(file,
                "      \"volume_cache_total_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
                preflight->stats.totalCausticVolumeCacheRadianceR,
                preflight->stats.totalCausticVolumeCacheRadianceG,
                preflight->stats.totalCausticVolumeCacheRadianceB);
        fprintf(file,
                "      \"volume_cache_footprint_input_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
                preflight->stats.totalCausticVolumeCacheFootprintInputRadianceR,
                preflight->stats.totalCausticVolumeCacheFootprintInputRadianceG,
                preflight->stats.totalCausticVolumeCacheFootprintInputRadianceB);
        fprintf(file,
                "      \"volume_cache_footprint_deposited_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
                preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceR,
                preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceG,
                preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceB);
        fprintf(file, "      \"volume_cache_max_cell_radiance\": %.9f,\n",
                preflight->stats.maxCausticVolumeCacheRadiance);
        fprintf(file, "      \"volume_cache_nonzero_cell_ratio\": %.9f,\n",
                preflight->stats.causticVolumeCacheNonZeroCellRatio);
        fprintf(file, "      \"volume_cache_sample_hit_ratio\": %.9f,\n",
                preflight->stats.causticVolumeCacheSampleHitRatio);
        fprintf(file,
                "      \"volume_cache_radiance_centroid\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
                preflight->stats.causticVolumeCacheRadianceCentroidX,
                preflight->stats.causticVolumeCacheRadianceCentroidY,
                preflight->stats.causticVolumeCacheRadianceCentroidZ);
        fprintf(file,
                "      \"volume_cache_nonzero_bounds_min\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
                preflight->stats.causticVolumeCacheNonZeroBoundsMinX,
                preflight->stats.causticVolumeCacheNonZeroBoundsMinY,
                preflight->stats.causticVolumeCacheNonZeroBoundsMinZ);
        fprintf(file,
                "      \"volume_cache_nonzero_bounds_max\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
                preflight->stats.causticVolumeCacheNonZeroBoundsMaxX,
                preflight->stats.causticVolumeCacheNonZeroBoundsMaxY,
                preflight->stats.causticVolumeCacheNonZeroBoundsMaxZ);
        fprintf(file, "      \"surface_cache_bound\": %s,\n",
                preflight->stats.causticSurfaceCacheBound > 0 ? "true" : "false");
        fprintf(file, "      \"surface_cache_allocated\": %s,\n",
                preflight->stats.causticSurfaceCacheAllocated > 0 ? "true" : "false");
        fprintf(file, "      \"surface_cache_record_capacity\": %d,\n",
                preflight->stats.causticSurfaceCacheRecordCapacity);
        fprintf(file, "      \"surface_cache_record_count\": %d,\n",
                preflight->stats.causticSurfaceCacheRecordCount);
        fprintf(file, "      \"surface_cache_deposit_attempt_count\": %d,\n",
                preflight->stats.causticSurfaceCacheDepositAttemptCount);
        fprintf(file, "      \"surface_cache_deposit_accepted_count\": %d,\n",
                preflight->stats.causticSurfaceCacheDepositAcceptedCount);
        fprintf(file, "      \"surface_cache_deposit_rejected_count\": %d,\n",
                preflight->stats.causticSurfaceCacheDepositRejectedCount);
        fprintf(file, "      \"surface_cache_sample_lookup_count\": %d,\n",
                preflight->stats.causticSurfaceCacheSampleLookupCount);
        fprintf(file, "      \"surface_cache_sample_contributing_count\": %d,\n",
                preflight->stats.causticSurfaceCacheSampleContributingCount);
        fprintf(file, "      \"surface_cache_nearest_sample_distance\": %.9f,\n",
                preflight->stats.causticSurfaceCacheNearestSampleDistance);
        fprintf(file, "      \"surface_cache_nearest_sample_radius\": %.9f,\n",
                preflight->stats.causticSurfaceCacheNearestSampleRadius);
        fprintf(file, "      \"surface_cache_nearest_sample_normal_dot\": %.9f,\n",
                preflight->stats.causticSurfaceCacheNearestSampleNormalDot);
        fprintf(file, "      \"surface_cache_nearest_sample_candidate_count\": %.0f,\n",
                preflight->stats.causticSurfaceCacheNearestSampleCandidateCount);
        fprintf(file,
                "      \"surface_cache_total_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
                preflight->stats.totalCausticSurfaceCacheRadianceR,
                preflight->stats.totalCausticSurfaceCacheRadianceG,
                preflight->stats.totalCausticSurfaceCacheRadianceB);
        fprintf(file, "      \"surface_cache_max_record_radiance\": %.9f,\n",
                preflight->stats.maxCausticSurfaceCacheRadiance);
        fprintf(file,
                "      \"surface_caustic_sampled_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
                preflight->stats.totalCausticSurfaceRadianceR,
                preflight->stats.totalCausticSurfaceRadianceG,
                preflight->stats.totalCausticSurfaceRadianceB);
        fprintf(file, "      \"volume_scatter_direct_sample_count\": %d,\n",
                preflight->stats.volumeScatterDirectSampleCount);
        fprintf(file,
                "      \"volume_scatter_direct_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
                preflight->stats.totalDirectVolumeScatterRadianceR,
                preflight->stats.totalDirectVolumeScatterRadianceG,
                preflight->stats.totalDirectVolumeScatterRadianceB);
        fprintf(file, "      \"volume_scatter_caustic_sampling_bound\": %s,\n",
                preflight->stats.causticVolumeScatterSampleCount > 0 ? "true" : "false");
        fprintf(file, "      \"volume_scatter_caustic_sample_count\": %d,\n",
                preflight->stats.causticVolumeScatterSampleCount);
        fprintf(file, "      \"volume_scatter_caustic_contributing_sample_count\": %d,\n",
                preflight->stats.causticVolumeScatterContributingSampleCount);
        fprintf(file,
                "      \"volume_scatter_caustic_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
                preflight->stats.totalCausticVolumeScatterRadianceR,
                preflight->stats.totalCausticVolumeScatterRadianceG,
                preflight->stats.totalCausticVolumeScatterRadianceB);
        {
            const double caustic_samples =
                (double)preflight->stats.causticVolumeScatterContributingSampleCount;
            const double caustic_pixels =
                (double)preflight->stats.causticVolumeScatterContributingPixelCount;
            const double cache_sum = ray_tracing_headless_caustic_rgb_sum(
                preflight->stats.totalCausticVolumeCacheRadianceR,
                preflight->stats.totalCausticVolumeCacheRadianceG,
                preflight->stats.totalCausticVolumeCacheRadianceB);
            const double scatter_sum = ray_tracing_headless_caustic_rgb_sum(
                preflight->stats.totalCausticVolumeScatterRadianceR,
                preflight->stats.totalCausticVolumeScatterRadianceG,
                preflight->stats.totalCausticVolumeScatterRadianceB);
            const double direct_sum = ray_tracing_headless_caustic_rgb_sum(
                preflight->stats.totalDirectVolumeScatterRadianceR,
                preflight->stats.totalDirectVolumeScatterRadianceG,
                preflight->stats.totalDirectVolumeScatterRadianceB);
            const double footprint_input_sum = ray_tracing_headless_caustic_rgb_sum(
                preflight->stats.totalCausticVolumeCacheFootprintInputRadianceR,
                preflight->stats.totalCausticVolumeCacheFootprintInputRadianceG,
                preflight->stats.totalCausticVolumeCacheFootprintInputRadianceB);
            const double footprint_deposited_sum = ray_tracing_headless_caustic_rgb_sum(
                preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceR,
                preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceG,
                preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceB);
            fprintf(file, "      \"volume_cache_total_radiance_sum\": %.9f,\n",
                    cache_sum);
            fprintf(file, "      \"volume_cache_footprint_input_radiance_sum\": %.9f,\n",
                    footprint_input_sum);
            fprintf(file, "      \"volume_cache_footprint_deposited_radiance_sum\": %.9f,\n",
                    footprint_deposited_sum);
            fprintf(file,
                    "      \"volume_cache_footprint_deposited_to_input_ratio\": %.9f,\n",
                    ray_tracing_headless_caustic_safe_ratio(footprint_deposited_sum,
                                                    footprint_input_sum));
            fprintf(file, "      \"volume_scatter_caustic_radiance_sum\": %.9f,\n",
                    scatter_sum);
            fprintf(file, "      \"volume_scatter_direct_radiance_sum\": %.9f,\n",
                    direct_sum);
            fprintf(file,
                    "      \"volume_scatter_caustic_to_cache_radiance_ratio\": %.9f,\n",
                    ray_tracing_headless_caustic_safe_ratio(scatter_sum, cache_sum));
            fprintf(file,
                    "      \"volume_scatter_caustic_to_direct_radiance_ratio\": %.9f,\n",
                    ray_tracing_headless_caustic_safe_ratio(scatter_sum, direct_sum));
            fprintf(file, "      \"volume_scatter_caustic_sampled_cache_radiance_sum\": %.9f,\n",
                    preflight->stats.totalCausticVolumeScatterSampledCacheRadiance);
            fprintf(file, "      \"volume_scatter_caustic_sampled_cache_radiance_avg\": %.9f,\n",
                    ray_tracing_headless_caustic_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterSampledCacheRadiance,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_sampled_cache_radiance_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterSampledCacheRadiance);
            fprintf(file, "      \"volume_scatter_caustic_raw_density_avg\": %.9f,\n",
                    ray_tracing_headless_caustic_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterSampledRawDensity,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_raw_density_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterSampledRawDensity);
            fprintf(file, "      \"volume_scatter_caustic_density_avg\": %.9f,\n",
                    ray_tracing_headless_caustic_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterSampledDensity,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_density_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterSampledDensity);
            fprintf(file, "      \"volume_scatter_caustic_probability_avg\": %.9f,\n",
                    ray_tracing_headless_caustic_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterProbability,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_probability_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterProbability);
            fprintf(file, "      \"volume_scatter_caustic_camera_transmittance_avg\": %.9f,\n",
                    ray_tracing_headless_caustic_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterCameraTransmittance,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_camera_transmittance_min\": %.9f,\n",
                    preflight->stats.minCausticVolumeScatterCameraTransmittance);
            fprintf(file, "      \"volume_scatter_caustic_camera_transmittance_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterCameraTransmittance);
            fprintf(file, "      \"volume_scatter_caustic_visibility_term_avg\": %.9f,\n",
                    ray_tracing_headless_caustic_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterVisibilityTerm,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_visibility_term_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterVisibilityTerm);
            fprintf(file, "      \"volume_scatter_caustic_contributing_pixel_count\": %d,\n",
                    preflight->stats.causticVolumeScatterContributingPixelCount);
            fprintf(file,
                    "      \"volume_scatter_caustic_contributing_pixel_centroid\": { \"x\": %.9f, \"y\": %.9f },\n",
                    ray_tracing_headless_caustic_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterPixelX,
                        caustic_pixels),
                    ray_tracing_headless_caustic_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterPixelY,
                        caustic_pixels));
            fprintf(file,
                    "      \"volume_scatter_caustic_contributing_pixel_bounds_min\": { \"x\": %d, \"y\": %d },\n",
                    preflight->stats.causticVolumeScatterPixelMinX,
                    preflight->stats.causticVolumeScatterPixelMinY);
            fprintf(file,
                    "      \"volume_scatter_caustic_contributing_pixel_bounds_max\": { \"x\": %d, \"y\": %d },\n",
                    preflight->stats.causticVolumeScatterPixelMaxX,
                    preflight->stats.causticVolumeScatterPixelMaxY);
        }
        fprintf(file, "      \"sample_budget\": %d,\n",
                request->caustic_settings.sampleBudget);
        fprintf(file, "      \"max_path_depth\": %d,\n",
                request->caustic_settings.maxPathDepth);
        fprintf(file, "      \"emission_policy\": \"%s\",\n",
                RuntimeCausticTransportEmissionPolicy3D_Label(
                    request->caustic_settings.emissionPolicy));
        fprintf(file, "      \"debug_summary_enabled\": %s,\n",
                request->caustic_settings.debugSummaryEnabled ? "true" : "false");
        fprintf(file, "      \"debug_export_enabled\": %s\n",
                request->caustic_settings.debugExportEnabled ? "true" : "false");
        fprintf(file, "    },\n");
    }
}
