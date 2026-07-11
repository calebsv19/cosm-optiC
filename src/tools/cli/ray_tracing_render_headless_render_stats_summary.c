#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>

static double ray_tracing_headless_render_stats_rgb_sum(double r, double g, double b) {
    return r + g + b;
}

static double ray_tracing_headless_render_stats_safe_ratio(double numerator, double denominator) {
    return denominator > 0.0 ? numerator / denominator : 0.0;
}

static void ray_tracing_headless_render_stats_write_region_counts(FILE* file,
                                                                  const char* name,
                                                                  const int counts[4],
                                                                  bool trailing_comma) {
    fprintf(file,
            "    \"%s\": { \"top_left\": %d, \"top_right\": %d, \"bottom_left\": %d, \"bottom_right\": %d }%s\n",
            name,
            counts ? counts[0] : 0,
            counts ? counts[1] : 0,
            counts ? counts[2] : 0,
            counts ? counts[3] : 0,
            trailing_comma ? "," : "");
}

void ray_tracing_headless_write_render_stats_summary(
    FILE *file,
    const RayTracingHeadlessPreflight *preflight) {
    fprintf(file, "  \"render_stats\": {\n");
    fprintf(file, "    \"hit_pixels\": %d,\n", preflight->stats.hitPixelCount);
    fprintf(file, "    \"visible_pixels\": %d,\n", preflight->stats.visiblePixelCount);
    fprintf(file, "    \"secondary_rays\": %d,\n", preflight->stats.secondaryRayCount);
    fprintf(file, "    \"secondary_hits\": %d,\n", preflight->stats.secondaryHitCount);
    fprintf(file, "    \"emissive_area_candidate_count\": %d,\n",
            preflight->stats.emissiveAreaCandidateCount);
    fprintf(file, "    \"emissive_area_selected_candidates\": %d,\n",
            preflight->stats.emissiveAreaSelectedCandidateCount);
    fprintf(file, "    \"emissive_area_visibility_rays\": %d,\n",
            preflight->stats.emissiveAreaVisibilityRayCount);
    fprintf(file, "    \"emissive_area_primary_samples\": %d,\n",
            preflight->stats.emissiveAreaPrimarySampleCount);
    fprintf(file, "    \"emissive_area_recursive_samples\": %d,\n",
            preflight->stats.emissiveAreaRecursiveSampleCount);
    fprintf(file, "    \"emissive_area_recursive_policy_skips\": %d,\n",
            preflight->stats.emissiveAreaRecursivePolicySkipCount);
    fprintf(file, "    \"emissive_area_recursive_candidate_cap_skips\": %d,\n",
            preflight->stats.emissiveAreaRecursiveCandidateCapSkipCount);
    fprintf(file, "    \"emissive_area_recursive_triangle_cap_skips\": %d,\n",
            preflight->stats.emissiveAreaRecursiveTriangleCapSkipCount);
    fprintf(file, "    \"emissive_area_recursive_candidate_cap\": %d,\n",
            preflight->stats.emissiveAreaRecursiveCandidateCap);
    fprintf(file, "    \"emissive_area_recursive_triangle_cap\": %d,\n",
            preflight->stats.emissiveAreaRecursiveTriangleCap);
    fprintf(file, "    \"emissive_area_full_scan_fallbacks\": %d,\n",
            preflight->stats.emissiveAreaFullScanFallbackCount);
    fprintf(file, "    \"caustic_sidecar_enabled\": %s,\n",
            preflight->stats.causticSidecarEnabled > 0 ? "true" : "false");
    fprintf(file, "    \"caustic_sidecar_samples\": %d,\n",
            preflight->stats.causticSidecarSampleCount);
    fprintf(file, "    \"caustic_sidecar_contributing_samples\": %d,\n",
            preflight->stats.causticSidecarContributingSampleCount);
    fprintf(file, "    \"max_caustic_sidecar_radiance\": %.9f,\n",
            preflight->stats.maxCausticSidecarRadiance);
    fprintf(file, "    \"total_caustic_sidecar_radiance\": %.9f,\n",
            preflight->stats.totalCausticSidecarRadiance);
    fprintf(file, "    \"caustic_bootstrap_temporary_bridge_active\": %s,\n",
            preflight->stats.causticBootstrapTemporaryBridgeActive > 0 ? "true" : "false");
    fprintf(file, "    \"caustic_transport_path_emission_active\": %s,\n",
            preflight->stats.causticTransportPathEmissionActive > 0 ? "true" : "false");
    fprintf(file, "    \"caustic_transport_light_count\": %d,\n",
            preflight->stats.causticTransportLightCount);
    fprintf(file, "    \"caustic_transport_evaluated_path_count\": %d,\n",
            preflight->stats.causticTransportEvaluatedPathCount);
    fprintf(file, "    \"caustic_transport_emitted_path_count\": %d,\n",
            preflight->stats.causticTransportEmittedPathCount);
    fprintf(file, "    \"caustic_transport_analytic_sphere_lens_resolved_count\": %d,\n",
            preflight->stats.causticTransportAnalyticSphereLensResolvedCount);
    fprintf(file, "    \"caustic_transport_analytic_sphere_lens_rejected_count\": %d,\n",
            preflight->stats.causticTransportAnalyticSphereLensRejectedCount);
    fprintf(file, "    \"caustic_transport_analytic_sphere_lens_evaluated_path_count\": %d,\n",
            preflight->stats.causticTransportAnalyticSphereLensEvaluatedPathCount);
    fprintf(file, "    \"caustic_transport_analytic_sphere_lens_emitted_path_count\": %d,\n",
            preflight->stats.causticTransportAnalyticSphereLensEmittedPathCount);
    fprintf(file, "    \"caustic_transport_analytic_sphere_lens_sample_weight\": %.9f,\n",
            preflight->stats.causticTransportAnalyticSphereLensSampleWeight);
    fprintf(file, "    \"caustic_transport_analytic_sphere_lens_total_sample_weight\": %.9f,\n",
            preflight->stats.causticTransportAnalyticSphereLensTotalSampleWeight);
    fprintf(file, "    \"caustic_transport_analytic_cylinder_lens_resolved_count\": %d,\n",
            preflight->stats.causticTransportAnalyticCylinderLensResolvedCount);
    fprintf(file, "    \"caustic_transport_analytic_cylinder_lens_rejected_count\": %d,\n",
            preflight->stats.causticTransportAnalyticCylinderLensRejectedCount);
    fprintf(file, "    \"caustic_transport_analytic_cylinder_lens_evaluated_path_count\": %d,\n",
            preflight->stats.causticTransportAnalyticCylinderLensEvaluatedPathCount);
    fprintf(file, "    \"caustic_transport_analytic_cylinder_lens_emitted_path_count\": %d,\n",
            preflight->stats.causticTransportAnalyticCylinderLensEmittedPathCount);
    fprintf(file, "    \"caustic_transport_analytic_cylinder_lens_sample_weight\": %.9f,\n",
            preflight->stats.causticTransportAnalyticCylinderLensSampleWeight);
    fprintf(file, "    \"caustic_transport_analytic_cylinder_lens_total_sample_weight\": %.9f,\n",
            preflight->stats.causticTransportAnalyticCylinderLensTotalSampleWeight);
    fprintf(file, "    \"caustic_transport_analytic_prism_lens_resolved_count\": %d,\n",
            preflight->stats.causticTransportAnalyticPrismLensResolvedCount);
    fprintf(file, "    \"caustic_transport_analytic_prism_lens_rejected_count\": %d,\n",
            preflight->stats.causticTransportAnalyticPrismLensRejectedCount);
    fprintf(file, "    \"caustic_transport_analytic_prism_lens_evaluated_path_count\": %d,\n",
            preflight->stats.causticTransportAnalyticPrismLensEvaluatedPathCount);
    fprintf(file, "    \"caustic_transport_analytic_prism_lens_emitted_path_count\": %d,\n",
            preflight->stats.causticTransportAnalyticPrismLensEmittedPathCount);
    fprintf(file, "    \"caustic_transport_analytic_prism_lens_sample_weight\": %.9f,\n",
            preflight->stats.causticTransportAnalyticPrismLensSampleWeight);
    fprintf(file, "    \"caustic_transport_analytic_prism_lens_total_sample_weight\": %.9f,\n",
            preflight->stats.causticTransportAnalyticPrismLensTotalSampleWeight);
    fprintf(file, "    \"caustic_transport_analytic_bowl_lens_resolved_count\": %d,\n",
            preflight->stats.causticTransportAnalyticBowlLensResolvedCount);
    fprintf(file, "    \"caustic_transport_analytic_bowl_lens_rejected_count\": %d,\n",
            preflight->stats.causticTransportAnalyticBowlLensRejectedCount);
    fprintf(file, "    \"caustic_transport_analytic_bowl_lens_evaluated_path_count\": %d,\n",
            preflight->stats.causticTransportAnalyticBowlLensEvaluatedPathCount);
    fprintf(file, "    \"caustic_transport_analytic_bowl_lens_emitted_path_count\": %d,\n",
            preflight->stats.causticTransportAnalyticBowlLensEmittedPathCount);
    fprintf(file, "    \"caustic_transport_analytic_bowl_lens_sample_weight\": %.9f,\n",
            preflight->stats.causticTransportAnalyticBowlLensSampleWeight);
    fprintf(file, "    \"caustic_transport_analytic_bowl_lens_total_sample_weight\": %.9f,\n",
            preflight->stats.causticTransportAnalyticBowlLensTotalSampleWeight);
    fprintf(file, "    \"caustic_transport_mesh_dielectric_lens_resolved_count\": %d,\n",
            preflight->stats.causticTransportMeshDielectricLensResolvedCount);
    fprintf(file, "    \"caustic_transport_mesh_dielectric_lens_rejected_count\": %d,\n",
            preflight->stats.causticTransportMeshDielectricLensRejectedCount);
    fprintf(file, "    \"caustic_transport_mesh_dielectric_lens_evaluated_path_count\": %d,\n",
            preflight->stats.causticTransportMeshDielectricLensEvaluatedPathCount);
    fprintf(file, "    \"caustic_transport_mesh_dielectric_lens_emitted_path_count\": %d,\n",
            preflight->stats.causticTransportMeshDielectricLensEmittedPathCount);
    fprintf(file, "    \"caustic_transport_mesh_dielectric_lens_sample_weight\": %.9f,\n",
            preflight->stats.causticTransportMeshDielectricLensSampleWeight);
    fprintf(file, "    \"caustic_transport_mesh_dielectric_lens_total_sample_weight\": %.9f,\n",
            preflight->stats.causticTransportMeshDielectricLensTotalSampleWeight);
    fprintf(file, "    \"caustic_transport_transparent_hit_count\": %d,\n",
            preflight->stats.causticTransportTransparentHitCount);
    fprintf(file, "    \"caustic_transport_specular_event_count\": %d,\n",
            preflight->stats.causticTransportSpecularEventCount);
    fprintf(file, "    \"caustic_transport_volume_segment_count\": %d,\n",
            preflight->stats.causticTransportVolumeSegmentCount);
    fprintf(file, "    \"caustic_transport_surface_receiver_trace_miss_count\": %d,\n",
            preflight->stats.causticTransportSurfaceReceiverTraceMissCount);
    fprintf(file, "    \"caustic_transport_surface_receiver_depth_reject_count\": %d,\n",
            preflight->stats.causticTransportSurfaceReceiverDepthRejectCount);
    fprintf(file, "    \"caustic_transport_surface_receiver_hit_count\": %d,\n",
            preflight->stats.causticTransportSurfaceReceiverHitCount);
    fprintf(file, "    \"caustic_transport_surface_receiver_fallback_count\": %d,\n",
            preflight->stats.causticTransportSurfaceReceiverFallbackCount);
    fprintf(file, "    \"caustic_volume_cache_bound\": %s,\n",
            preflight->stats.causticVolumeCacheBound > 0 ? "true" : "false");
    fprintf(file, "    \"caustic_volume_cache_allocated\": %s,\n",
            preflight->stats.causticVolumeCacheAllocated > 0 ? "true" : "false");
    fprintf(file, "    \"caustic_volume_cache_cell_count\": %d,\n",
            preflight->stats.causticVolumeCacheCellCount);
    fprintf(file, "    \"caustic_volume_cache_nonzero_cell_count\": %d,\n",
            preflight->stats.causticVolumeCacheNonZeroCellCount);
    fprintf(file, "    \"caustic_volume_cache_deposit_attempt_count\": %d,\n",
            preflight->stats.causticVolumeCacheDepositAttemptCount);
    fprintf(file, "    \"caustic_volume_cache_deposit_accepted_count\": %d,\n",
            preflight->stats.causticVolumeCacheDepositAcceptedCount);
    fprintf(file, "    \"caustic_volume_cache_footprint_deposit_count\": %d,\n",
            preflight->stats.causticVolumeCacheFootprintDepositCount);
    fprintf(file, "    \"caustic_volume_cache_footprint_cell_contribution_count\": %d,\n",
            preflight->stats.causticVolumeCacheFootprintCellContributionCount);
    fprintf(file, "    \"caustic_volume_cache_average_footprint_radius_voxels\": %.9f,\n",
            preflight->stats.causticVolumeCacheAverageFootprintRadiusVoxels);
    fprintf(file, "    \"caustic_volume_cache_sample_lookup_count\": %d,\n",
            preflight->stats.causticVolumeCacheSampleLookupCount);
    fprintf(file, "    \"caustic_volume_cache_sample_contributing_count\": %d,\n",
            preflight->stats.causticVolumeCacheSampleContributingCount);
    fprintf(file, "    \"max_caustic_volume_cache_radiance\": %.9f,\n",
            preflight->stats.maxCausticVolumeCacheRadiance);
    fprintf(file,
            "    \"total_caustic_volume_cache_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
            preflight->stats.totalCausticVolumeCacheRadianceR,
            preflight->stats.totalCausticVolumeCacheRadianceG,
            preflight->stats.totalCausticVolumeCacheRadianceB);
    fprintf(file,
            "    \"total_caustic_volume_cache_footprint_input_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
            preflight->stats.totalCausticVolumeCacheFootprintInputRadianceR,
            preflight->stats.totalCausticVolumeCacheFootprintInputRadianceG,
            preflight->stats.totalCausticVolumeCacheFootprintInputRadianceB);
    fprintf(file,
            "    \"total_caustic_volume_cache_footprint_deposited_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
            preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceR,
            preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceG,
            preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceB);
    fprintf(file, "    \"caustic_volume_cache_nonzero_cell_ratio\": %.9f,\n",
            preflight->stats.causticVolumeCacheNonZeroCellRatio);
    fprintf(file, "    \"caustic_volume_cache_sample_hit_ratio\": %.9f,\n",
            preflight->stats.causticVolumeCacheSampleHitRatio);
    fprintf(file,
            "    \"caustic_volume_cache_radiance_centroid\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
            preflight->stats.causticVolumeCacheRadianceCentroidX,
            preflight->stats.causticVolumeCacheRadianceCentroidY,
            preflight->stats.causticVolumeCacheRadianceCentroidZ);
    fprintf(file,
            "    \"caustic_volume_cache_nonzero_bounds_min\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
            preflight->stats.causticVolumeCacheNonZeroBoundsMinX,
            preflight->stats.causticVolumeCacheNonZeroBoundsMinY,
            preflight->stats.causticVolumeCacheNonZeroBoundsMinZ);
    fprintf(file,
            "    \"caustic_volume_cache_nonzero_bounds_max\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
            preflight->stats.causticVolumeCacheNonZeroBoundsMaxX,
            preflight->stats.causticVolumeCacheNonZeroBoundsMaxY,
            preflight->stats.causticVolumeCacheNonZeroBoundsMaxZ);
    fprintf(file, "    \"direct_volume_scatter_samples\": %d,\n",
            preflight->stats.volumeScatterDirectSampleCount);
    fprintf(file,
            "    \"total_direct_volume_scatter_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
            preflight->stats.totalDirectVolumeScatterRadianceR,
            preflight->stats.totalDirectVolumeScatterRadianceG,
            preflight->stats.totalDirectVolumeScatterRadianceB);
    fprintf(file, "    \"caustic_volume_scatter_samples\": %d,\n",
            preflight->stats.causticVolumeScatterSampleCount);
    fprintf(file, "    \"caustic_volume_scatter_contributing_samples\": %d,\n",
            preflight->stats.causticVolumeScatterContributingSampleCount);
    fprintf(file,
            "    \"total_caustic_volume_scatter_radiance\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
            preflight->stats.totalCausticVolumeScatterRadianceR,
            preflight->stats.totalCausticVolumeScatterRadianceG,
            preflight->stats.totalCausticVolumeScatterRadianceB);
    {
        const double caustic_samples =
            (double)preflight->stats.causticVolumeScatterContributingSampleCount;
        const double caustic_pixels =
            (double)preflight->stats.causticVolumeScatterContributingPixelCount;
        const double cache_sum = ray_tracing_headless_render_stats_rgb_sum(
            preflight->stats.totalCausticVolumeCacheRadianceR,
            preflight->stats.totalCausticVolumeCacheRadianceG,
            preflight->stats.totalCausticVolumeCacheRadianceB);
        const double scatter_sum = ray_tracing_headless_render_stats_rgb_sum(
            preflight->stats.totalCausticVolumeScatterRadianceR,
            preflight->stats.totalCausticVolumeScatterRadianceG,
            preflight->stats.totalCausticVolumeScatterRadianceB);
        const double direct_sum = ray_tracing_headless_render_stats_rgb_sum(
            preflight->stats.totalDirectVolumeScatterRadianceR,
            preflight->stats.totalDirectVolumeScatterRadianceG,
            preflight->stats.totalDirectVolumeScatterRadianceB);
        const double footprint_input_sum = ray_tracing_headless_render_stats_rgb_sum(
            preflight->stats.totalCausticVolumeCacheFootprintInputRadianceR,
            preflight->stats.totalCausticVolumeCacheFootprintInputRadianceG,
            preflight->stats.totalCausticVolumeCacheFootprintInputRadianceB);
        const double footprint_deposited_sum = ray_tracing_headless_render_stats_rgb_sum(
            preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceR,
            preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceG,
            preflight->stats.totalCausticVolumeCacheFootprintDepositedRadianceB);
        fprintf(file, "    \"caustic_volume_cache_total_radiance_sum\": %.9f,\n",
                cache_sum);
        fprintf(file, "    \"caustic_volume_cache_footprint_input_radiance_sum\": %.9f,\n",
                footprint_input_sum);
        fprintf(file, "    \"caustic_volume_cache_footprint_deposited_radiance_sum\": %.9f,\n",
                footprint_deposited_sum);
        fprintf(file,
                "    \"caustic_volume_cache_footprint_deposited_to_input_ratio\": %.9f,\n",
                ray_tracing_headless_render_stats_safe_ratio(footprint_deposited_sum,
                                                             footprint_input_sum));
        fprintf(file, "    \"caustic_volume_scatter_radiance_sum\": %.9f,\n",
                scatter_sum);
        fprintf(file, "    \"direct_volume_scatter_radiance_sum\": %.9f,\n",
                direct_sum);
        fprintf(file,
                "    \"caustic_volume_scatter_to_cache_radiance_ratio\": %.9f,\n",
                ray_tracing_headless_render_stats_safe_ratio(scatter_sum, cache_sum));
        fprintf(file,
                "    \"caustic_volume_scatter_to_direct_radiance_ratio\": %.9f,\n",
                ray_tracing_headless_render_stats_safe_ratio(scatter_sum, direct_sum));
        fprintf(file, "    \"caustic_volume_scatter_sampled_cache_radiance_sum\": %.9f,\n",
                preflight->stats.totalCausticVolumeScatterSampledCacheRadiance);
        fprintf(file, "    \"caustic_volume_scatter_sampled_cache_radiance_avg\": %.9f,\n",
                ray_tracing_headless_render_stats_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterSampledCacheRadiance,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_sampled_cache_radiance_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterSampledCacheRadiance);
        fprintf(file, "    \"caustic_volume_scatter_raw_density_avg\": %.9f,\n",
                ray_tracing_headless_render_stats_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterSampledRawDensity,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_raw_density_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterSampledRawDensity);
        fprintf(file, "    \"caustic_volume_scatter_density_avg\": %.9f,\n",
                ray_tracing_headless_render_stats_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterSampledDensity,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_density_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterSampledDensity);
        fprintf(file, "    \"caustic_volume_scatter_probability_avg\": %.9f,\n",
                ray_tracing_headless_render_stats_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterProbability,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_probability_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterProbability);
        fprintf(file, "    \"caustic_volume_scatter_camera_transmittance_avg\": %.9f,\n",
                ray_tracing_headless_render_stats_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterCameraTransmittance,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_camera_transmittance_min\": %.9f,\n",
                preflight->stats.minCausticVolumeScatterCameraTransmittance);
        fprintf(file, "    \"caustic_volume_scatter_camera_transmittance_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterCameraTransmittance);
        fprintf(file, "    \"caustic_volume_scatter_visibility_term_avg\": %.9f,\n",
                ray_tracing_headless_render_stats_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterVisibilityTerm,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_visibility_term_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterVisibilityTerm);
        fprintf(file, "    \"caustic_volume_scatter_contributing_pixel_count\": %d,\n",
                preflight->stats.causticVolumeScatterContributingPixelCount);
        fprintf(file,
                "    \"caustic_volume_scatter_contributing_pixel_centroid\": { \"x\": %.9f, \"y\": %.9f },\n",
                ray_tracing_headless_render_stats_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterPixelX,
                    caustic_pixels),
                ray_tracing_headless_render_stats_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterPixelY,
                    caustic_pixels));
        fprintf(file,
                "    \"caustic_volume_scatter_contributing_pixel_bounds_min\": { \"x\": %d, \"y\": %d },\n",
                preflight->stats.causticVolumeScatterPixelMinX,
                preflight->stats.causticVolumeScatterPixelMinY);
        fprintf(file,
                "    \"caustic_volume_scatter_contributing_pixel_bounds_max\": { \"x\": %d, \"y\": %d },\n",
                preflight->stats.causticVolumeScatterPixelMaxX,
                preflight->stats.causticVolumeScatterPixelMaxY);
    }
    fprintf(file, "    \"mirror_dominant_pixels\": %d,\n",
            preflight->stats.mirrorDominantPixelCount);
    fprintf(file, "    \"mirror_base_attenuated_pixels\": %d,\n",
            preflight->stats.mirrorBaseAttenuatedPixelCount);
    fprintf(file, "    \"mirror_reflection_hit_pixels\": %d,\n",
            preflight->stats.mirrorReflectionHitPixelCount);
    fprintf(file, "    \"mirror_emitter_reflection_pixels\": %d,\n",
            preflight->stats.mirrorEmitterReflectionPixelCount);
    fprintf(file, "    \"mirror_geometry_reflection_pixels\": %d,\n",
            preflight->stats.mirrorGeometryReflectionPixelCount);
    fprintf(file, "    \"max_mirror_dominance\": %.9f,\n",
            preflight->stats.maxMirrorDominance);
    fprintf(file, "    \"max_mirror_specular_reflection_radiance\": %.9f,\n",
            preflight->stats.maxMirrorSpecularReflectionRadiance);
    fprintf(file, "    \"max_mirror_base_radiance_before_attenuation\": %.9f,\n",
            preflight->stats.maxMirrorBaseRadianceBeforeAttenuation);
    fprintf(file, "    \"max_mirror_base_radiance_after_attenuation\": %.9f,\n",
            preflight->stats.maxMirrorBaseRadianceAfterAttenuation);
    fprintf(file, "    \"total_mirror_specular_reflection_radiance\": %.9f,\n",
            preflight->stats.totalMirrorSpecularReflectionRadiance);
    fprintf(file, "    \"total_mirror_base_radiance_before_attenuation\": %.9f,\n",
            preflight->stats.totalMirrorBaseRadianceBeforeAttenuation);
    fprintf(file, "    \"total_mirror_base_radiance_after_attenuation\": %.9f,\n",
            preflight->stats.totalMirrorBaseRadianceAfterAttenuation);
    fprintf(file, "    \"temporal_committed_subpasses\": %d,\n",
            preflight->stats.temporalCommittedSubpasses);
    fprintf(file, "    \"temporal_pixels_rendered\": %d,\n",
            preflight->stats.temporalPixelsRendered);
    fprintf(file, "    \"temporal_pixels_skipped\": %d,\n",
            preflight->stats.temporalPixelsSkipped);
    fprintf(file, "    \"temporal_active_pixels\": %d,\n",
            preflight->stats.temporalActivePixelCount);
    fprintf(file, "    \"temporal_active_tiles\": %d,\n",
            preflight->stats.temporalActiveTileCount);
    fprintf(file, "    \"temporal_inactive_tiles\": %d,\n",
            preflight->stats.temporalInactiveTileCount);
    fprintf(file, "    \"temporal_planned_parent_tiles\": %d,\n",
            preflight->stats.temporalPlannedParentTileCount);
    fprintf(file, "    \"temporal_emitted_tile_jobs\": %d,\n",
            preflight->stats.temporalEmittedTileJobCount);
    fprintf(file, "    \"temporal_occupancy_skipped_tiles\": %d,\n",
            preflight->stats.temporalOccupancySkippedTileCount);
    fprintf(file, "    \"temporal_dispatched_tile_jobs\": %d,\n",
            preflight->stats.temporalDispatchedTileJobCount);
    fprintf(file, "    \"temporal_completed_tile_jobs\": %d,\n",
            preflight->stats.temporalCompletedTileJobCount);
    fprintf(file, "    \"temporal_progress_dirty_tile_batches\": %d,\n",
            preflight->stats.temporalProgressDirtyBatchCount);
    fprintf(file, "    \"temporal_progress_dirty_tiles\": %d,\n",
            preflight->stats.temporalProgressDirtyTileCount);
    fprintf(file, "    \"temporal_tile_scheduler_job_array_owners\": %d,\n",
            preflight->stats.temporalTileSchedulerJobArrayOwnerCount);
    fprintf(file, "    \"temporal_tile_scheduler_parent_metric_array_owners\": %d,\n",
            preflight->stats.temporalTileSchedulerParentMetricArrayOwnerCount);
    fprintf(file, "    \"temporal_tile_scheduler_progress_tile_array_owners\": %d,\n",
            preflight->stats.temporalTileSchedulerProgressTileArrayOwnerCount);
    fprintf(file, "    \"temporal_tile_scheduler_completion_queue_owners\": %d,\n",
            preflight->stats.temporalTileSchedulerCompletionQueueOwnerCount);
    fprintf(file, "    \"temporal_tile_scheduler_worker_pool_owners\": %d,\n",
            preflight->stats.temporalTileSchedulerWorkerPoolOwnerCount);
    fprintf(file, "    \"temporal_tile_scheduler_cancel_token_bound\": %d,\n",
            preflight->stats.temporalTileSchedulerCancelTokenBound);
    fprintf(file, "    \"temporal_tile_scheduler_cancel_checks\": %d,\n",
            preflight->stats.temporalTileSchedulerCancelCheckCount);
    fprintf(file, "    \"temporal_tile_scheduler_cancel_requested\": %d,\n",
            preflight->stats.temporalTileSchedulerCancelRequestedCount);
    fprintf(file, "    \"temporal_tile_scheduler_cancel_before_dispatch\": %d,\n",
            preflight->stats.temporalTileSchedulerCancelBeforeDispatchCount);
    fprintf(file, "    \"temporal_tile_scheduler_cancel_during_wait\": %d,\n",
            preflight->stats.temporalTileSchedulerCancelDuringWaitCount);
    fprintf(file, "    \"temporal_tile_scheduler_cancel_before_final_resolve\": %d,\n",
            preflight->stats.temporalTileSchedulerCancelBeforeFinalResolveCount);
    fprintf(file, "    \"temporal_tile_scheduler_final_resolve_blocked_by_cancel\": %d,\n",
            preflight->stats.temporalTileSchedulerFinalResolveBlockedByCancelCount);
    fprintf(file, "    \"temporal_tile_scheduler_worker_drain_shutdowns\": %d,\n",
            preflight->stats.temporalTileSchedulerWorkerDrainShutdownCount);
    fprintf(file, "    \"temporal_tile_scheduler_worker_cancel_shutdowns\": %d,\n",
            preflight->stats.temporalTileSchedulerWorkerCancelShutdownCount);
    fprintf(file, "    \"temporal_tile_scheduler_cancel_generation\": %llu,\n",
            (unsigned long long)preflight->stats.temporalTileSchedulerCancelGeneration);
    fprintf(file, "    \"temporal_dirty_preview_presents\": %d,\n",
            preflight->stats.temporalDirtyPreviewPresentCount);
    fprintf(file, "    \"temporal_conservative_first_frame_tile_render\": %d,\n",
            preflight->stats.temporalConservativeFirstFrameTileRender);
    fprintf(file, "    \"temporal_final_full_resolve_count\": %d,\n",
            preflight->stats.temporalFinalFullResolveCount);
    fprintf(file, "    \"temporal_host_full_resolve_count\": %d,\n",
            preflight->stats.temporalHostFullResolveCount);
    fprintf(file, "    \"temporal_final_preview_presents\": %d,\n",
            preflight->stats.temporalFinalPreviewPresentCount);
    fprintf(file, "    \"temporal_history_promotes\": %d,\n",
            preflight->stats.temporalHistoryPromoteCount);
    fprintf(file, "    \"temporal_dirty_preview_host_pixels\": %llu,\n",
            (unsigned long long)preflight->stats.temporalDirtyPreviewHostPixels);
    fprintf(file, "    \"temporal_dirty_preview_host_bytes\": %llu,\n",
            (unsigned long long)preflight->stats.temporalDirtyPreviewHostBytes);
    fprintf(file, "    \"temporal_final_resolve_host_pixels\": %llu,\n",
            (unsigned long long)preflight->stats.temporalFinalResolveHostPixels);
    fprintf(file, "    \"temporal_final_resolve_host_bytes\": %llu,\n",
            (unsigned long long)preflight->stats.temporalFinalResolveHostBytes);
    fprintf(file, "    \"temporal_history_seed_host_bytes\": %llu,\n",
            (unsigned long long)preflight->stats.temporalHistorySeedHostBytes);
    fprintf(file, "    \"temporal_history_promote_host_bytes\": %llu,\n",
            (unsigned long long)preflight->stats.temporalHistoryPromoteHostBytes);
    fprintf(file, "    \"temporal_final_preview_present_host_bytes\": %llu,\n",
            (unsigned long long)preflight->stats.temporalFinalPreviewPresentHostBytes);
    fprintf(file, "    \"temporal_adaptive_state_measured_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateMeasuredPixels);
    fprintf(file, "    \"temporal_adaptive_state_stable_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateStablePixels);
    fprintf(file, "    \"temporal_adaptive_state_active_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateActivePixels);
    fprintf(file, "    \"temporal_adaptive_state_probe_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateProbePixels);
    fprintf(file, "    \"temporal_adaptive_state_high_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateHighRiskPixels);
    fprintf(file, "    \"temporal_adaptive_state_stable_tiles\": %d,\n",
            preflight->stats.temporalAdaptiveStateStableTiles);
    fprintf(file, "    \"temporal_adaptive_state_active_tiles\": %d,\n",
            preflight->stats.temporalAdaptiveStateActiveTiles);
    fprintf(file, "    \"temporal_adaptive_state_probe_tiles\": %d,\n",
            preflight->stats.temporalAdaptiveStateProbeTiles);
    fprintf(file, "    \"temporal_adaptive_state_high_risk_tiles\": %d,\n",
            preflight->stats.temporalAdaptiveStateHighRiskTiles);
    fprintf(file, "    \"temporal_adaptive_state_min_sample_floor\": %d,\n",
            preflight->stats.temporalAdaptiveStateMinSampleFloor);
    fprintf(file, "    \"temporal_adaptive_state_activity_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateActivityRiskPixels);
    fprintf(file, "    \"temporal_adaptive_state_material_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateMaterialRiskPixels);
    fprintf(file, "    \"temporal_adaptive_state_transparent_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateTransparentRiskPixels);
    fprintf(file, "    \"temporal_adaptive_state_glossy_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateGlossyRiskPixels);
    fprintf(file, "    \"temporal_adaptive_state_geometry_edge_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateGeometryEdgeRiskPixels);
    fprintf(file, "    \"temporal_adaptive_state_direct_light_no_trace_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateDirectLightNoTracePixels);
    fprintf(file, "    \"temporal_adaptive_state_direct_light_clear_visible_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateDirectLightClearVisiblePixels);
    fprintf(file, "    \"temporal_adaptive_state_direct_light_clear_blocked_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateDirectLightClearBlockedPixels);
    fprintf(file, "    \"temporal_adaptive_state_direct_light_stable_partial_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateDirectLightStablePartialPixels);
    fprintf(file, "    \"temporal_adaptive_state_direct_light_mixed_partial_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateDirectLightMixedPartialPixels);
    fprintf(file, "    \"temporal_adaptive_state_direct_light_boundary_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveStateDirectLightBoundaryRiskPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_eligible_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopEligiblePixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_held_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopHeldPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_hold_probe_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopHoldProbePixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_hold_high_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopHoldHighRiskPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_hold_activity_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopHoldActivityRiskPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_hold_material_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopHoldMaterialRiskPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_hold_transparent_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopHoldTransparentRiskPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_hold_geometry_edge_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopHoldGeometryEdgeRiskPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_hold_direct_light_risk_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopHoldDirectLightRiskPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_base_active_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopBaseActivePixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_padding_hold_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopPaddingHoldPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_padding_hold_high_seed_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopPaddingHoldHighSeedPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_padding_hold_medium_seed_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopPaddingHoldMediumSeedPixels);
    fprintf(file, "    \"temporal_adaptive_early_stop_active_after_padding_pixels\": %d,\n",
            preflight->stats.temporalAdaptiveEarlyStopActiveAfterPaddingPixels);
    ray_tracing_headless_render_stats_write_region_counts(
        file,
        "temporal_adaptive_early_stop_eligible_region_counts",
        preflight->stats.temporalAdaptiveEarlyStopEligibleRegionCounts,
        true);
    ray_tracing_headless_render_stats_write_region_counts(
        file,
        "temporal_adaptive_early_stop_held_region_counts",
        preflight->stats.temporalAdaptiveEarlyStopHeldRegionCounts,
        true);
    ray_tracing_headless_render_stats_write_region_counts(
        file,
        "temporal_adaptive_early_stop_padding_hold_region_counts",
        preflight->stats.temporalAdaptiveEarlyStopPaddingHoldRegionCounts,
        true);
    fprintf(file,
            "    \"temporal_adaptive_budget_buckets\": { "
            "\"floor\": { \"pixels\": %d, \"active\": %d, \"eligible\": %d, \"held\": %d }, "
            "\"three_to_four\": { \"pixels\": %d, \"active\": %d, \"eligible\": %d, \"held\": %d }, "
            "\"five_to_eight\": { \"pixels\": %d, \"active\": %d, \"eligible\": %d, \"held\": %d }, "
            "\"nine_plus\": { \"pixels\": %d, \"active\": %d, \"eligible\": %d, \"held\": %d } },\n",
            preflight->stats.temporalAdaptiveBudgetBucketPixels[0],
            preflight->stats.temporalAdaptiveBudgetActiveBucketPixels[0],
            preflight->stats.temporalAdaptiveBudgetEligibleBucketPixels[0],
            preflight->stats.temporalAdaptiveBudgetHeldBucketPixels[0],
            preflight->stats.temporalAdaptiveBudgetBucketPixels[1],
            preflight->stats.temporalAdaptiveBudgetActiveBucketPixels[1],
            preflight->stats.temporalAdaptiveBudgetEligibleBucketPixels[1],
            preflight->stats.temporalAdaptiveBudgetHeldBucketPixels[1],
            preflight->stats.temporalAdaptiveBudgetBucketPixels[2],
            preflight->stats.temporalAdaptiveBudgetActiveBucketPixels[2],
            preflight->stats.temporalAdaptiveBudgetEligibleBucketPixels[2],
            preflight->stats.temporalAdaptiveBudgetHeldBucketPixels[2],
            preflight->stats.temporalAdaptiveBudgetBucketPixels[3],
            preflight->stats.temporalAdaptiveBudgetActiveBucketPixels[3],
            preflight->stats.temporalAdaptiveBudgetEligibleBucketPixels[3],
            preflight->stats.temporalAdaptiveBudgetHeldBucketPixels[3]);
    fprintf(file,
            "    \"temporal_adaptive_budget_risk_attribution\": { "
            "\"clear_visible_eligible\": %d, "
            "\"clear_visible_held\": %d, "
            "\"partial_held\": %d, "
            "\"transparent_held\": %d, "
            "\"geometry_held\": %d, "
            "\"activity_held\": %d },\n",
            preflight->stats.temporalAdaptiveBudgetClearVisibleEligiblePixels,
            preflight->stats.temporalAdaptiveBudgetClearVisibleHeldPixels,
            preflight->stats.temporalAdaptiveBudgetPartialHeldPixels,
            preflight->stats.temporalAdaptiveBudgetTransparentHeldPixels,
            preflight->stats.temporalAdaptiveBudgetGeometryHeldPixels,
            preflight->stats.temporalAdaptiveBudgetActivityHeldPixels);
    fprintf(file,
            "    \"temporal_adaptive_budget_heatmap_enabled\": %s,\n",
            preflight->stats.temporalAdaptiveBudgetHeatmapEnabled ? "true" : "false");
    fprintf(file, "    \"temporal_adaptive_state_mixed_risk_tiles\": %d,\n",
            preflight->stats.temporalAdaptiveStateMixedRiskTiles);
    fprintf(file, "    \"temporal_adaptive_state_risk_avg\": %.9f,\n",
            preflight->stats.temporalAdaptiveStateMeasuredPixels > 0
                ? preflight->stats.temporalAdaptiveStateRiskSum /
                      (double)preflight->stats.temporalAdaptiveStateMeasuredPixels
                : 0.0);
    fprintf(file, "    \"temporal_adaptive_state_risk_max\": %.9f,\n",
            preflight->stats.temporalAdaptiveStateRiskMax);
    fprintf(file, "    \"denoise_temporal_frame_count\": %d,\n",
            preflight->stats.denoiseTemporalFrameCount);
    fprintf(file, "    \"denoise_raw_pixel_count\": %d,\n",
            preflight->stats.denoiseRawPixelCount);
    fprintf(file, "    \"denoise_reconstructed_pixel_count\": %d,\n",
            preflight->stats.denoiseReconstructedPixelCount);
    fprintf(file, "    \"denoise_stable_interior_sample_count\": %d,\n",
            preflight->stats.denoiseStableInteriorSampleCount);
    fprintf(file, "    \"denoise_rejected_edge_sample_count\": %d,\n",
            preflight->stats.denoiseRejectedEdgeSampleCount);
    fprintf(file, "    \"denoise_preserved_transparent_pixel_count\": %d,\n",
            preflight->stats.denoisePreservedTransparentPixelCount);
    fprintf(file, "    \"denoise_preserved_mirror_glossy_pixel_count\": %d,\n",
            preflight->stats.denoisePreservedMirrorGlossyPixelCount);
    fprintf(file, "    \"denoise_skipped_unstable_temporal_pixel_count\": %d,\n",
            preflight->stats.denoiseSkippedUnstableTemporalPixelCount);
    fprintf(file, "    \"denoise_skipped_invalid_surface_pixel_count\": %d,\n",
            preflight->stats.denoiseSkippedInvalidSurfacePixelCount);
    fprintf(file, "    \"denoise_raw_radiance_luma_total\": %.9f,\n",
            preflight->stats.denoiseRawRadianceLumaTotal);
    fprintf(file, "    \"denoise_reconstructed_radiance_luma_total\": %.9f,\n",
            preflight->stats.denoiseReconstructedRadianceLumaTotal);
    fprintf(file, "    \"denoise_radiance_luma_delta\": %.9f,\n",
            preflight->stats.denoiseReconstructedRadianceLumaTotal -
                preflight->stats.denoiseRawRadianceLumaTotal);
    fprintf(file, "    \"max_radiance\": %.9f,\n", preflight->stats.maxRadiance);
    fprintf(file, "    \"max_bounce_radiance\": %.9f,\n",
            preflight->stats.maxBounceRadiance);
    fprintf(file, "    \"total_bounce_radiance\": %.9f,\n",
            preflight->stats.totalBounceRadiance);
    fprintf(file, "    \"nonzero_pixels\": %llu,\n",
            (unsigned long long)preflight->nonzero_pixels);
    fprintf(file, "    \"max_rgb\": [%u, %u, %u]\n",
            (unsigned)preflight->max_r,
            (unsigned)preflight->max_g,
            (unsigned)preflight->max_b);
    fprintf(file, "  },\n");
}
