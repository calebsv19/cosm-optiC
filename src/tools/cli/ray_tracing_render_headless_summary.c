#include "tools/ray_tracing_render_headless_internal.h"

#include "app/ray_tracing_request_utils.h"
#include "render/runtime_dynamic_geometry_accel_3d.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <json-c/json.h>

static const char *route_family_label(RayTracingRouteFamily family) {
    switch (family) {
        case RAY_TRACING_ROUTE_NATIVE_3D:
            return "native_3d";
        case RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK:
            return "compat_3d_fallback";
        case RAY_TRACING_ROUTE_CANONICAL_2D:
        default:
            return "canonical_2d";
    }
}

static const char *environment_light_mode_label(EnvironmentLightMode mode) {
    switch (animation_config_environment_light_mode_clamp(mode)) {
        case ENVIRONMENT_LIGHT_MODE_TOP_FILL:
            return "top_fill";
        case ENVIRONMENT_LIGHT_MODE_AMBIENT:
            return "ambient";
        case ENVIRONMENT_LIGHT_MODE_OFF:
        default:
            return "off";
    }
}

static double ray_tracing_headless_rgb_sum(double r, double g, double b) {
    return r + g + b;
}

static double ray_tracing_headless_safe_ratio(double numerator, double denominator) {
    return denominator > 0.0 ? numerator / denominator : 0.0;
}

static double ray_tracing_headless_nonnegative(double value) {
    return value > 0.0 ? value : 0.0;
}

static double ray_tracing_headless_percent_of(double value, double total) {
    return total > 0.0 ? (ray_tracing_headless_nonnegative(value) * 100.0) / total : 0.0;
}

static const char* ray_tracing_headless_mesh_asset_persistent_cache_mode_label(int mode) {
    switch ((RayTracingRuntimeMeshAssetPersistentCacheMode)mode) {
        case RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_DISABLED:
            return "disabled";
        case RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_READ_ONLY:
            return "read_only";
        case RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_REFRESH:
            return "refresh";
        case RAY_TRACING_RUNTIME_MESH_ASSET_PERSISTENT_CACHE_READ_WRITE:
        default:
            return "read_write";
    }
}

static void ray_tracing_headless_write_startup_timing_stage(FILE* file,
                                                            const char* name,
                                                            double ms,
                                                            double total_ms,
                                                            bool comma) {
    if (!file || !name) return;
    fprintf(file,
            "      { \"stage\": ");
    RayTracingJsonWriteString(file, name);
    fprintf(file,
            ", \"ms\": %.6f, \"percent_of_total\": %.6f }%s\n",
            ray_tracing_headless_nonnegative(ms),
            ray_tracing_headless_percent_of(ms, total_ms),
            comma ? "," : "");
}

static void ray_tracing_headless_write_startup_load_timing_matrix(
    FILE* file,
    const RayTracingHeadlessPreflight* preflight) {
    double total_reference_ms = 0.0;
    double flattened_triangle_append_ms = 0.0;
    double unaccounted_prepare_ms = 0.0;
    if (!file || !preflight) return;

    total_reference_ms = preflight->native_prepare_frame_ms > 0.0
                             ? preflight->native_prepare_frame_ms
                             : preflight->total_run_ms;
    flattened_triangle_append_ms =
        preflight->scene_builder_timing_stats.mesh_append_total_ms -
        preflight->scene_builder_timing_stats.mesh_append_reserve_ms;
    unaccounted_prepare_ms =
        total_reference_ms -
        (preflight->mesh_asset_timing_stats.total_ms +
         preflight->scene_builder_timing_stats.total_ms +
         preflight->scene_acceleration_stats.blasBuildMs +
         preflight->scene_acceleration_stats.blasPersistentCacheReadMs +
         preflight->scene_acceleration_stats.blasPersistentCacheWriteMs +
         preflight->scene_acceleration_stats.tlasBuildMs +
         preflight->scene_acceleration_stats.tlasBindMs +
         preflight->dynamic_water_cache_stats.geometryStoreMs);

    fprintf(file, "  \"startup_load_timing_matrix\": {\n");
    fprintf(file, "    \"total_reference_ms\": %.6f,\n",
            ray_tracing_headless_nonnegative(total_reference_ms));
    fprintf(file, "    \"total_reference_source\": ");
    RayTracingJsonWriteString(file,
                              preflight->native_prepare_frame_ms > 0.0
                                  ? "native_prepare_frame_ms"
                                  : "total_run_ms");
    fprintf(file, ",\n");
    fprintf(file, "    \"stages\": [\n");
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "sidecar_path_resolution",
        preflight->mesh_asset_timing_stats.sidecar_path_resolution_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "runtime_mesh_scene_read",
        preflight->mesh_asset_timing_stats.scene_read_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "runtime_mesh_scene_parse",
        preflight->mesh_asset_timing_stats.scene_parse_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "runtime_mesh_document_load_parse",
        preflight->mesh_asset_timing_stats.asset_runtime_document_load_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "runtime_mesh_document_pack_read",
        preflight->mesh_asset_timing_stats.asset_persistent_cache_read_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "runtime_mesh_document_pack_write",
        preflight->mesh_asset_timing_stats.asset_persistent_cache_write_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "mesh_asset_document_copy",
        preflight->mesh_asset_timing_stats.asset_document_copy_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "mesh_instance_expansion",
        preflight->scene_builder_timing_stats.mesh_append_expand_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "flattened_triangle_append",
        flattened_triangle_append_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "flattened_bvh_build",
        preflight->scene_builder_timing_stats.bvh_rebuild_wall_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "mesh_local_blas_build",
        preflight->scene_acceleration_stats.blasBuildMs,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "mesh_local_blas_persistent_cache_read",
        preflight->scene_acceleration_stats.blasPersistentCacheReadMs,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "mesh_local_blas_persistent_cache_write",
        preflight->scene_acceleration_stats.blasPersistentCacheWriteMs,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "tlas_build",
        preflight->scene_acceleration_stats.tlasBuildMs,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "tlas_bind",
        preflight->scene_acceleration_stats.tlasBindMs,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "dynamic_water_cache_store",
        preflight->dynamic_water_cache_stats.geometryStoreMs,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "dynamic_water_bvh_build",
        preflight->dynamic_water_cache_stats.geometryBVHBuildMs,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "caustic_cache_prep",
        preflight->caustic_cache_prep_ms,
        total_reference_ms,
        true);
    ray_tracing_headless_write_startup_timing_stage(
        file,
        "unaccounted_prepare_or_runtime_init",
        unaccounted_prepare_ms,
        total_reference_ms,
        false);
    fprintf(file, "    ],\n");
    fprintf(file, "    \"cache_counts\": {\n");
    fprintf(file, "      \"runtime_mesh_document_cache_hits\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_cache_hits);
    fprintf(file, "      \"runtime_mesh_document_cache_misses\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_cache_misses);
    fprintf(file, "      \"blas_cache_hits\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCacheHits);
    fprintf(file, "      \"blas_cache_misses\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCacheMisses);
    fprintf(file, "      \"blas_cache_invalidations\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCacheInvalidations);
    fprintf(file, "      \"dynamic_water_geometry_stores\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.geometryStores);
    fprintf(file, "      \"dynamic_water_geometry_store_failures\": %llu\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.geometryStoreFailures);
    fprintf(file, "    },\n");
    fprintf(file, "    \"notes\": ");
    RayTracingJsonWriteString(
        file,
        "caustic_cache_prep measures prepared-frame sidecar probe and caustic cache population");
    fprintf(file, "\n");
    fprintf(file, "  },\n");
}

static void ray_tracing_headless_write_render_trace_cost_ledger(
    FILE* file,
    const RayTracingHeadlessPreflight* preflight) {
    const RuntimeRenderTraceCostLedger3D* ledger =
        preflight ? &preflight->render_trace_cost_ledger : NULL;
    fprintf(file, "  \"render_trace_cost_ledger\": {\n");
    fprintf(file, "    \"enabled\": %s,\n",
            (ledger && ledger->enabled) ? "true" : "false");
    fprintf(file, "    \"total_rays\": %llu,\n",
            (unsigned long long)(ledger ? ledger->totalRays : 0u));
    fprintf(file, "    \"ray_class_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT; ++i) {
        fprintf(file,
                "      \"%s\": %llu%s\n",
                RuntimeRenderTraceCostRayClass3DLabel((RuntimeRenderTraceCostRayClass3D)i),
                (unsigned long long)(ledger ? ledger->rayClassCounts[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT ? "," : "");
    }
    fprintf(file, "    },\n");
    fprintf(file, "    \"path_depth_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT; ++i) {
        fprintf(file,
                "      \"%s\": %llu%s\n",
                RuntimeRenderTraceCostPathDepthBucket3DLabel(
                    (RuntimeRenderTraceCostPathDepthBucket3D)i),
                (unsigned long long)(ledger ? ledger->pathDepthCounts[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT ? "," : "");
    }
    fprintf(file, "    },\n");
    fprintf(file, "    \"ray_class_depth_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT; ++i) {
        fprintf(file,
                "      \"%s\": {",
                RuntimeRenderTraceCostRayClass3DLabel((RuntimeRenderTraceCostRayClass3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostPathDepthBucket3DLabel(
                        (RuntimeRenderTraceCostPathDepthBucket3D)j),
                    (unsigned long long)(ledger ? ledger->rayClassDepthCounts[i][j] : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT ? "," : "");
        }
        fprintf(file, " }%s\n", i + 1 < RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT ? "," : "");
    }
    fprintf(file, "    },\n");
    fprintf(file, "    \"material_family_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT; ++i) {
        fprintf(file,
                "      \"%s\": %llu%s\n",
                RuntimeRenderTraceCostMaterialFamily3DLabel(
                    (RuntimeRenderTraceCostMaterialFamily3D)i),
                (unsigned long long)(ledger ? ledger->materialFamilyCounts[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT ? "," : "");
    }
    fprintf(file, "    },\n");
    fprintf(file, "    \"direct_light_visibility_policy\": {\n");
    fprintf(file, "      \"source_evaluations\": %llu,\n",
            (unsigned long long)(ledger ? ledger->directLightVisibilityPolicy.sourceEvaluations
                                        : 0u));
    fprintf(file, "      \"evaluated_samples\": %llu,\n",
            (unsigned long long)(ledger ? ledger->directLightVisibilityPolicy.evaluatedSamples
                                        : 0u));
    fprintf(file, "      \"visibility_sample_queries\": %llu,\n",
            (unsigned long long)(ledger ? ledger->directLightVisibilityPolicy.visibilityTraces
                                        : 0u));
    fprintf(file, "      \"luma_range_count\": %llu,\n",
            (unsigned long long)(ledger ? ledger->directLightVisibilityPolicy.lumaRangeCount
                                        : 0u));
    fprintf(file, "      \"luma_min_observed\": %.6f,\n",
            ledger && ledger->directLightVisibilityPolicy.lumaRangeCount > 0u
                ? ledger->directLightVisibilityPolicy.lumaMinObserved
                : 0.0);
    fprintf(file, "      \"luma_max_observed\": %.6f,\n",
            ledger && ledger->directLightVisibilityPolicy.lumaRangeCount > 0u
                ? ledger->directLightVisibilityPolicy.lumaMaxObserved
                : 0.0);
    fprintf(file, "      \"luma_span_avg\": %.6f,\n",
            ledger && ledger->directLightVisibilityPolicy.lumaRangeCount > 0u
                ? ledger->directLightVisibilityPolicy.lumaSpanSum /
                      (double)ledger->directLightVisibilityPolicy.lumaRangeCount
                : 0.0);
    fprintf(file, "      \"luma_span_max\": %.6f,\n",
            ledger ? ledger->directLightVisibilityPolicy.lumaSpanMax : 0.0);
    fprintf(file, "      \"caller_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightCaller3DLabel(
                    (RuntimeRenderTraceCostDirectLightCaller3D)i),
                (unsigned long long)(ledger ? ledger->directLightVisibilityPolicy.callerCounts[i]
                                            : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_kind_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightSourceKind3DLabel(
                    (RuntimeRenderTraceCostDirectLightSourceKind3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy.sourceKindCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_origin_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightSourceOrigin3DLabel(
                    (RuntimeRenderTraceCostDirectLightSourceOrigin3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy.sourceOriginCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"emission_profile_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightEmissionProfile3DLabel(
                    (RuntimeRenderTraceCostDirectLightEmissionProfile3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy
                                               .emissionProfileCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"outcome_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightOutcome3DLabel(
                    (RuntimeRenderTraceCostDirectLightOutcome3D)i),
                (unsigned long long)(ledger ? ledger->directLightVisibilityPolicy.outcomeCounts[i]
                                            : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"stop_reason_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightStopReason3DLabel(
                    (RuntimeRenderTraceCostDirectLightStopReason3D)i),
                (unsigned long long)(ledger ? ledger->directLightVisibilityPolicy.stopReasonCounts[i]
                                            : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"sample_bucket_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightSampleBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightSampleBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy.sampleBucketCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"distance_bucket_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightDistanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightDistanceBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy.distanceBucketCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"importance_bucket_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightImportanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightImportanceBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy
                                               .importanceBucketCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"material_emitter_rect_policy\": {\n");
    fprintf(file, "        \"source_evaluations\": %llu,\n",
            (unsigned long long)(ledger ? ledger->directLightVisibilityPolicy
                                              .materialEmitterRectEvaluations
                                        : 0u));
    fprintf(file, "        \"evaluated_samples\": %llu,\n",
            (unsigned long long)(ledger ? ledger->directLightVisibilityPolicy
                                              .materialEmitterRectEvaluatedSamples
                                        : 0u));
    fprintf(file, "        \"visibility_sample_queries\": %llu,\n",
            (unsigned long long)(ledger ? ledger->directLightVisibilityPolicy
                                              .materialEmitterRectVisibilityTraces
                                        : 0u));
    fprintf(file, "        \"distance_bucket_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT; ++i) {
        fprintf(file,
                "          \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightDistanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightDistanceBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy
                                               .materialEmitterRectDistanceCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT ? "," : "");
    }
    fprintf(file, "        },\n");
    fprintf(file, "        \"importance_bucket_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT; ++i) {
        fprintf(file,
                "          \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightImportanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightImportanceBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy
                                               .materialEmitterRectImportanceCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT ? "," : "");
    }
    fprintf(file, "        },\n");
    fprintf(file, "        \"evaluated_samples_by_distance\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT; ++i) {
        fprintf(file,
                "          \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightDistanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightDistanceBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy
                                               .materialEmitterRectEvaluatedSamplesByDistance[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT ? "," : "");
    }
    fprintf(file, "        },\n");
    fprintf(file, "        \"visibility_sample_queries_by_distance\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT; ++i) {
        fprintf(file,
                "          \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightDistanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightDistanceBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy
                                               .materialEmitterRectVisibilityTracesByDistance[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT ? "," : "");
    }
    fprintf(file, "        },\n");
    fprintf(file, "        \"evaluated_samples_by_importance\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT; ++i) {
        fprintf(file,
                "          \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightImportanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightImportanceBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy
                                               .materialEmitterRectEvaluatedSamplesByImportance[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT ? "," : "");
    }
    fprintf(file, "        },\n");
    fprintf(file, "        \"visibility_sample_queries_by_importance\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT; ++i) {
        fprintf(file,
                "          \"%s\": %llu%s\n",
                RuntimeRenderTraceCostDirectLightImportanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightImportanceBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->directLightVisibilityPolicy
                                               .materialEmitterRectVisibilityTracesByImportance[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT ? "," : "");
    }
    fprintf(file, "        },\n");
    fprintf(file, "        \"distance_importance_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT; ++i) {
        fprintf(file,
                "          \"%s\": {",
                RuntimeRenderTraceCostDirectLightDistanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightDistanceBucket3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostDirectLightImportanceBucket3DLabel(
                        (RuntimeRenderTraceCostDirectLightImportanceBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->directLightVisibilityPolicy
                                                   .materialEmitterRectDistanceImportanceCounts[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT ? "," : "");
    }
    fprintf(file, "        }\n");
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_kind_outcome_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostDirectLightSourceKind3DLabel(
                    (RuntimeRenderTraceCostDirectLightSourceKind3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostDirectLightOutcome3DLabel(
                        (RuntimeRenderTraceCostDirectLightOutcome3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->directLightVisibilityPolicy
                                                   .sourceKindOutcomeCounts[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_kind_stop_reason_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostDirectLightSourceKind3DLabel(
                    (RuntimeRenderTraceCostDirectLightSourceKind3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostDirectLightStopReason3DLabel(
                        (RuntimeRenderTraceCostDirectLightStopReason3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->directLightVisibilityPolicy
                                                   .sourceKindStopReasonCounts[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT ? "," : "");
    }
    fprintf(file, "      }\n");
    fprintf(file, "    }\n");
    fprintf(file, "  },\n");
}

static void ray_tracing_headless_write_dynamic_geometry_acceleration_summary(
    FILE *file,
    const RayTracingHeadlessPreflight *preflight) {
    RuntimeDynamicGeometryAcceleration3DInput input = {0};
    RuntimeDynamicGeometryAcceleration3DClassification classification = {0};
    if (!file || !preflight) return;

    input.static_mesh_loaded_assets = preflight->mesh_asset_timing_stats.loaded_assets;
    input.static_mesh_loaded_instances =
        preflight->mesh_asset_timing_stats.loaded_instances;
    input.static_mesh_loaded_triangles =
        preflight->mesh_asset_timing_stats.loaded_triangles;
    input.static_blas_cached_asset_count =
        preflight->scene_acceleration_stats.blasCachedAssetCount;
    input.static_tlas_instance_count =
        preflight->scene_acceleration_stats.tlasInstanceCount;
    input.water_surface_source_found = preflight->water_surface_source_found;
    input.water_surface_loaded = preflight->water_surface_loaded;
    input.water_surface_frame_selection_built =
        preflight->water_surface_frame_selection_built;
    input.water_surface_frame_selection_dynamic =
        preflight->water_surface_frame_selection_dynamic;
    input.water_surface_mesh_attached = preflight->water_surface_mesh_attached;
    input.water_surface_first_frame_index =
        preflight->water_surface_loaded_first_frame_index;
    input.water_surface_last_frame_index =
        preflight->water_surface_loaded_last_frame_index;
    input.water_surface_first_grid_w = preflight->water_surface_grid_w;
    input.water_surface_first_grid_d = preflight->water_surface_grid_d;
    input.water_surface_first_sample_count = preflight->water_surface_sample_count;
    input.water_surface_last_grid_w = preflight->water_surface_last_grid_w;
    input.water_surface_last_grid_d = preflight->water_surface_last_grid_d;
    input.water_surface_last_sample_count =
        preflight->water_surface_last_sample_count;
    input.water_surface_triangle_count = preflight->water_surface_triangle_count;
    input.mesh_emissive_light_count =
        preflight->registered_light_mesh_emissive_count;
    input.mesh_area_sampler_only_count =
        preflight->registered_light_mesh_area_sampler_only_count;

    RuntimeDynamicGeometryAcceleration3D_Classify(&input, &classification);

    fprintf(file, "  \"dynamic_geometry_acceleration\": {\n");
    fprintf(file, "    \"static_mesh_asset_policy\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeDynamicGeometryAcceleration3D_StaticMeshPolicyLabel(&classification));
    fprintf(file, ",\n");
    fprintf(file, "    \"static_mesh_asset_present\": %s,\n",
            classification.static_mesh_asset_present ? "true" : "false");
    fprintf(file, "    \"static_mesh_asset_blas_active\": %s,\n",
            classification.static_mesh_asset_blas_active ? "true" : "false");
    fprintf(file, "    \"static_mesh_asset_loaded_assets\": %d,\n",
            classification.static_mesh_asset_loaded_assets);
    fprintf(file, "    \"static_mesh_asset_loaded_instances\": %d,\n",
            classification.static_mesh_asset_loaded_instances);
    fprintf(file, "    \"static_mesh_asset_loaded_triangles\": %llu,\n",
            (unsigned long long)classification.static_mesh_asset_loaded_triangles);
    fprintf(file, "    \"water_surface_policy\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeDynamicGeometryAcceleration3D_WaterSurfacePolicyLabel(
            classification.water_surface_decision));
    fprintf(file, ",\n");
    fprintf(file, "    \"water_surface_accel_action\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeDynamicGeometryAcceleration3D_WaterSurfaceActionLabel(
            classification.water_surface_decision));
    fprintf(file, ",\n");
    fprintf(file, "    \"water_surface_present\": %s,\n",
            classification.water_surface_present ? "true" : "false");
    fprintf(file, "    \"water_surface_mesh_attached\": %s,\n",
            classification.water_surface_mesh_attached ? "true" : "false");
    fprintf(file, "    \"water_surface_frame_selection_dynamic\": %s,\n",
            classification.water_surface_frame_selection_dynamic ? "true" : "false");
    fprintf(file, "    \"water_surface_frame_stamp_changed\": %s,\n",
            classification.water_surface_frame_stamp_changed ? "true" : "false");
    fprintf(file, "    \"water_surface_topology_comparable\": %s,\n",
            classification.water_surface_topology_comparable ? "true" : "false");
    fprintf(file, "    \"water_surface_topology_stable\": %s,\n",
            classification.water_surface_topology_stable ? "true" : "false");
    fprintf(file, "    \"water_surface_first_grid_w\": %u,\n",
            classification.water_surface_first_grid_w);
    fprintf(file, "    \"water_surface_first_grid_d\": %u,\n",
            classification.water_surface_first_grid_d);
    fprintf(file, "    \"water_surface_first_sample_count\": %llu,\n",
            (unsigned long long)classification.water_surface_first_sample_count);
    fprintf(file, "    \"water_surface_last_grid_w\": %u,\n",
            classification.water_surface_last_grid_w);
    fprintf(file, "    \"water_surface_last_grid_d\": %u,\n",
            classification.water_surface_last_grid_d);
    fprintf(file, "    \"water_surface_last_sample_count\": %llu,\n",
            (unsigned long long)classification.water_surface_last_sample_count);
    fprintf(file, "    \"water_surface_triangle_count\": %d,\n",
            classification.water_surface_triangle_count);
    fprintf(file, "    \"mesh_emissive_policy\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeDynamicGeometryAcceleration3D_MeshEmissivePolicyLabel(&classification));
    fprintf(file, ",\n");
    fprintf(file, "    \"mesh_emissive_internal_present\": %s,\n",
            classification.mesh_emissive_internal_present ? "true" : "false");
    fprintf(file, "    \"mesh_emissive_light_count\": %d,\n",
            classification.mesh_emissive_light_count);
    fprintf(file, "    \"mesh_area_sampler_only_count\": %d,\n",
            classification.mesh_area_sampler_only_count);
    fprintf(file, "    \"generated_runtime_mesh_policy\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeDynamicGeometryAcceleration3D_GeneratedRuntimeMeshPolicyLabel());
    fprintf(file, ",\n");
    fprintf(file, "    \"deforming_mesh_policy\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeDynamicGeometryAcceleration3D_DeformingMeshPolicyLabel());
    fprintf(file, ",\n");
    fprintf(file, "    \"next_dynamic_accel_contract\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeDynamicGeometryAcceleration3D_NextContractLabel());
    fprintf(file, "\n");
    fprintf(file, "  },\n");
}

static void ray_tracing_headless_write_dynamic_water_acceleration_cache_summary(
    FILE *file,
    const RayTracingHeadlessPreflight *preflight) {
    if (!file || !preflight) return;
    fprintf(file, "  \"dynamic_water_acceleration_cache\": {\n");
    fprintf(file, "    \"valid\": %s,\n",
            preflight->dynamic_water_cache_stats.valid ? "true" : "false");
    fprintf(file, "    \"cache_ready\": %s,\n",
            preflight->dynamic_water_cache_stats.cacheReady ? "true" : "false");
    fprintf(file, "    \"last_status\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeDynamicGeometryAcceleration3D_WaterCacheStatusLabel(
            preflight->dynamic_water_cache_stats.lastStatus));
    fprintf(file, ",\n");
    fprintf(file, "    \"last_policy\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeDynamicGeometryAcceleration3D_WaterSurfacePolicyLabel(
            preflight->dynamic_water_cache_stats.lastDecision));
    fprintf(file, ",\n");
    fprintf(file, "    \"observed_frames\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.observedFrames);
    fprintf(file, "    \"rebuilds\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.rebuilds);
    fprintf(file, "    \"reuses\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.reuses);
    fprintf(file, "    \"refits\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.refits);
    fprintf(file, "    \"fallbacks\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.fallbacks);
    fprintf(file, "    \"last_frame_index\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.lastFrameIndex);
    fprintf(file, "    \"cached_grid_w\": %u,\n",
            preflight->dynamic_water_cache_stats.cachedGridW);
    fprintf(file, "    \"cached_grid_d\": %u,\n",
            preflight->dynamic_water_cache_stats.cachedGridD);
    fprintf(file, "    \"cached_sample_count\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.cachedSampleCount);
    fprintf(file, "    \"cached_triangle_count\": %d,\n",
            preflight->dynamic_water_cache_stats.cachedTriangleCount);
    fprintf(file, "    \"geometry_cache_ready\": %s,\n",
            preflight->dynamic_water_cache_stats.geometryCacheReady ? "true" : "false");
    fprintf(file, "    \"geometry_bvh_ready\": %s,\n",
            preflight->dynamic_water_cache_stats.geometryBVHReady ? "true" : "false");
    fprintf(file, "    \"geometry_stores\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.geometryStores);
    fprintf(file, "    \"geometry_rebuild_stores\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.geometryRebuildStores);
    fprintf(file, "    \"geometry_refit_stores\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.geometryRefitStores);
    fprintf(file, "    \"geometry_reuse_stores\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.geometryReuseStores);
    fprintf(file, "    \"geometry_store_failures\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.geometryStoreFailures);
    fprintf(file, "    \"geometry_bvh_node_count\": %d,\n",
            preflight->dynamic_water_cache_stats.geometryBVHNodeCount);
    fprintf(file, "    \"geometry_bvh_leaf_count\": %d,\n",
            preflight->dynamic_water_cache_stats.geometryBVHLeafCount);
    fprintf(file, "    \"geometry_store_ms\": %.6f,\n",
            preflight->dynamic_water_cache_stats.geometryStoreMs);
    fprintf(file, "    \"geometry_bvh_build_ms\": %.6f,\n",
            preflight->dynamic_water_cache_stats.geometryBVHBuildMs);
    fprintf(file, "    \"route_trace_calls\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.routeTraceCalls);
    fprintf(file, "    \"route_trace_hits\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.routeTraceHits);
    fprintf(file, "    \"route_trace_misses\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.routeTraceMisses);
    fprintf(file, "    \"route_trace_errors\": %llu,\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.routeTraceErrors);
    fprintf(file, "    \"route_trace_unavailable\": %llu\n",
            (unsigned long long)preflight->dynamic_water_cache_stats.routeTraceUnavailable);
    fprintf(file, "  },\n");
}

void ray_tracing_render_headless_write_summary(
    FILE *file,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight) {
    if (!file || !request || !preflight) return;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_headless_summary_v1\",\n");
    fprintf(file, "  \"run_id\": ");
    RayTracingJsonWriteString(file, request->run_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"request_loaded\": %s,\n", preflight->request_loaded ? "true" : "false");
    fprintf(file, "  \"scene_applied\": %s,\n", preflight->scene_applied ? "true" : "false");
    fprintf(file, "  \"volume_attached\": %s,\n", preflight->volume_attached ? "true" : "false");
    fprintf(file, "  \"volume_summary_built\": %s,\n",
            preflight->volume_summary_built ? "true" : "false");
    fprintf(file, "  \"water_surface_source_found\": %s,\n",
            preflight->water_surface_source_found ? "true" : "false");
    fprintf(file, "  \"water_surface_loaded\": %s,\n",
            preflight->water_surface_loaded ? "true" : "false");
    fprintf(file, "  \"water_surface_mesh_attached\": %s,\n",
            preflight->water_surface_mesh_attached ? "true" : "false");
    fprintf(file, "  \"route_family\": ");
    RayTracingJsonWriteString(file, route_family_label(preflight->route.routeFamily));
    fprintf(file, ",\n");
    fprintf(file, "  \"route_native_3d\": %s,\n", preflight->route_native_3d ? "true" : "false");
    fprintf(file, "  \"prepared_frame\": %s,\n", preflight->prepared_frame ? "true" : "false");
    fprintf(file, "  \"rendered_frames\": %s,\n", preflight->rendered_frames ? "true" : "false");
    fprintf(file, "  \"frames_rendered\": %d,\n", preflight->frames_rendered);
    fprintf(file, "  \"runtime_scene_path\": ");
    RayTracingJsonWriteString(file, request->runtime_scene_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"volume_source_path\": ");
    RayTracingJsonWriteString(file, request->volume_source_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"volume_source_kind\": ");
    RayTracingJsonWriteString(file,
                      ray_tracing_agent_render_request_volume_kind_label(
                          request->volume_source_kind));
    fprintf(file, ",\n");
    fprintf(file, "  \"volume_visible\": %s,\n",
            request->volume_visible ? "true" : "false");
    fprintf(file, "  \"integrator_3d\": ");
    RayTracingJsonWriteString(file,
                      ray_tracing_agent_render_request_integrator_label(
                          request->integrator_3d));
    fprintf(file, ",\n");
    fprintf(file, "  \"render\": {\n");
    fprintf(file, "    \"start_frame\": %d,\n", request->start_frame);
    fprintf(file, "    \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "    \"width\": %d,\n", request->width);
    fprintf(file, "    \"height\": %d,\n", request->height);
    fprintf(file, "    \"normalized_t\": %.9f,\n", request->normalized_t);
    fprintf(file, "    \"temporal_frames\": %d,\n", request->temporal_frames);
    fprintf(file, "    \"has_denoise_enabled_override\": %s,\n",
            request->has_denoise_enabled_override ? "true" : "false");
    fprintf(file, "    \"denoise_enabled\": %s\n",
            preflight->denoise_enabled ? "true" : "false");
    fprintf(file, "  },\n");
    fprintf(file, "  \"resources\": {\n");
    fprintf(file, "    \"has_budget\": %s,\n",
            request->has_resource_budget ? "true" : "false");
    fprintf(file, "    \"cpu_percent\": %d,\n", request->resource_cpu_percent);
    fprintf(file, "    \"max_workers\": %d,\n", request->resource_max_workers);
    fprintf(file, "    \"reserve_cpu_count\": %d\n", request->resource_reserve_cpu_count);
    fprintf(file, "  },\n");
    fprintf(file, "  \"denoise\": {\n");
    fprintf(file, "    \"has_request_override\": %s,\n",
            request->has_denoise_enabled_override ? "true" : "false");
    fprintf(file, "    \"enabled\": %s,\n",
            preflight->denoise_enabled ? "true" : "false");
    fprintf(file, "    \"applied\": %s\n",
            preflight->stats.denoiseRawPixelCount > 0 ? "true" : "false");
    fprintf(file, "  },\n");
    fprintf(file, "  \"inspection\": {\n");
    fprintf(file, "    \"preset\": ");
    RayTracingJsonWriteString(file,
                      ray_tracing_agent_render_request_inspection_preset_label(
                          request->inspection_preset));
    fprintf(file, ",\n");
    fprintf(file, "    \"has_camera_zoom_override\": %s,\n",
            request->has_camera_zoom_override ? "true" : "false");
    fprintf(file, "    \"camera_zoom\": %.9f,\n", request->camera_zoom_override);
    fprintf(file, "    \"has_camera_position_override\": %s,\n",
            request->has_camera_position_override ? "true" : "false");
    fprintf(file, "    \"camera_position\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
            request->camera_position_x,
            request->camera_position_y,
            request->camera_position_z);
    fprintf(file, "    \"has_camera_look_at_override\": %s,\n",
            request->has_camera_look_at_override ? "true" : "false");
    fprintf(file, "    \"camera_look_at\": { \"x\": %.9f, \"y\": %.9f, \"z\": %.9f },\n",
            request->camera_look_at_x,
            request->camera_look_at_y,
            request->camera_look_at_z);
    fprintf(file, "    \"has_environment_brightness_override\": %s,\n",
            request->has_environment_brightness_override ? "true" : "false");
    fprintf(file, "    \"environment_brightness\": %.9f,\n",
            request->environment_brightness_override);
    fprintf(file, "    \"has_ambient_strength_override\": %s,\n",
            request->has_ambient_strength_override ? "true" : "false");
    fprintf(file, "    \"ambient_strength\": %.9f,\n", request->ambient_strength_override);
    fprintf(file, "    \"has_environment_light_mode_override\": %s,\n",
            request->has_environment_light_mode_override ? "true" : "false");
    fprintf(file, "    \"environment_light_mode\": ");
    RayTracingJsonWriteString(file, environment_light_mode_label(request->environment_light_mode_override));
    fprintf(file, ",\n");
    fprintf(file, "    \"has_environment_preset_override\": %s,\n",
            request->has_environment_preset_override ? "true" : "false");
    fprintf(file, "    \"environment_preset\": ");
    RayTracingJsonWriteString(file,
                      RuntimeEnvironment3DPresetLabel(
                          (EnvironmentPreset)request->environment_preset_override));
    fprintf(file, ",\n");
    fprintf(file, "    \"has_background_brightness_override\": %s,\n",
            request->has_background_brightness_override ? "true" : "false");
    fprintf(file, "    \"background_brightness\": %.9f,\n",
            request->background_brightness_override);
    fprintf(file, "    \"has_background_color_override\": %s,\n",
            request->has_background_color_override ? "true" : "false");
    fprintf(file, "    \"background_color\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
            request->background_color_r,
            request->background_color_g,
            request->background_color_b);
    fprintf(file, "    \"has_top_fill_strength_override\": %s,\n",
            request->has_top_fill_strength_override ? "true" : "false");
    fprintf(file, "    \"top_fill_strength\": %.9f,\n", request->top_fill_strength_override);
    fprintf(file, "    \"has_light_intensity_override\": %s,\n",
            request->has_light_intensity_override ? "true" : "false");
    fprintf(file, "    \"light_intensity\": %.9f,\n", request->light_intensity_override);
    fprintf(file, "    \"has_light_radius_override\": %s,\n",
            request->has_light_radius_override ? "true" : "false");
    fprintf(file, "    \"light_radius\": %.9f,\n", request->light_radius_override);
    fprintf(file, "    \"has_forward_decay_override\": %s,\n",
            request->has_forward_decay_override ? "true" : "false");
    fprintf(file, "    \"forward_decay\": %.9f,\n", request->forward_decay_override);
    fprintf(file, "    \"has_volume_scatter_gain_override\": %s,\n",
            request->has_volume_scatter_gain_override ? "true" : "false");
    fprintf(file, "    \"volume_scatter_gain\": %.9f,\n",
            request->volume_scatter_gain_override);
    fprintf(file, "    \"has_caustic_volume_scatter_gain_override\": %s,\n",
            request->has_caustic_volume_scatter_gain_override ? "true" : "false");
    fprintf(file, "    \"caustic_volume_scatter_gain\": %.9f,\n",
            request->caustic_volume_scatter_gain_override);
    fprintf(file, "    \"has_volume_density_scale_override\": %s,\n",
            request->has_volume_density_scale_override ? "true" : "false");
    fprintf(file, "    \"volume_density_scale\": %.9f,\n",
            request->volume_density_scale_override);
    fprintf(file, "    \"has_volume_density_gamma_override\": %s,\n",
            request->has_volume_density_gamma_override ? "true" : "false");
    fprintf(file, "    \"volume_density_gamma\": %.9f,\n",
            request->volume_density_gamma_override);
    fprintf(file, "    \"has_volume_absorption_gain_override\": %s,\n",
            request->has_volume_absorption_gain_override ? "true" : "false");
    fprintf(file, "    \"volume_absorption_gain\": %.9f,\n",
            request->volume_absorption_gain_override);
    fprintf(file, "    \"has_volume_opacity_clamp_override\": %s,\n",
            request->has_volume_opacity_clamp_override ? "true" : "false");
    fprintf(file, "    \"volume_opacity_clamp\": %.9f,\n",
            request->volume_opacity_clamp_override);
    fprintf(file, "    \"has_volume_step_scale_override\": %s,\n",
            request->has_volume_step_scale_override ? "true" : "false");
    fprintf(file, "    \"volume_step_scale\": %.9f,\n",
            request->volume_step_scale_override);
    fprintf(file, "    \"has_secondary_diffuse_samples_3d_override\": %s,\n",
            request->has_secondary_diffuse_samples_3d_override ? "true" : "false");
    fprintf(file, "    \"secondary_diffuse_samples_3d\": %d,\n",
            request->secondary_diffuse_samples_3d_override);
    fprintf(file, "    \"has_transmission_samples_3d_override\": %s,\n",
            request->has_transmission_samples_3d_override ? "true" : "false");
    fprintf(file, "    \"transmission_samples_3d\": %d,\n",
            request->transmission_samples_3d_override);
    fprintf(file, "    \"has_trace_route_override\": %s,\n",
            request->has_trace_route_override ? "true" : "false");
    fprintf(file, "    \"trace_route\": ");
    RayTracingJsonWriteString(file, RuntimeRay3DTraceRouteLabel(request->trace_route));
    fprintf(file, ",\n");
    fprintf(file, "    \"has_caustic_mode_override\": %s,\n",
            request->has_caustic_mode_override ? "true" : "false");
    fprintf(file, "    \"caustic_mode\": \"%s\",\n",
            RuntimeDisneyV2_3D_CausticModeLabel(request->caustic_mode));
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
        fprintf(file, "      \"volume_cache_state\": \"%s\",\n",
                RuntimeCausticCacheState3D_Label(caustic_readback.volumeCacheState));
        fprintf(file, "      \"surface_cache_state\": \"%s\",\n",
                RuntimeCausticCacheState3D_Label(caustic_readback.surfaceCacheState));
        fprintf(file, "      \"path_emission_active\": %s,\n",
                caustic_readback.pathEmissionActive ? "true" : "false");
        fprintf(file, "      \"transport_reserved\": %s,\n",
                caustic_readback.transportReserved ? "true" : "false");
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
            const double cache_sum = ray_tracing_headless_rgb_sum(
                preflight->stats.totalCausticVolumeCacheRadianceR,
                preflight->stats.totalCausticVolumeCacheRadianceG,
                preflight->stats.totalCausticVolumeCacheRadianceB);
            const double scatter_sum = ray_tracing_headless_rgb_sum(
                preflight->stats.totalCausticVolumeScatterRadianceR,
                preflight->stats.totalCausticVolumeScatterRadianceG,
                preflight->stats.totalCausticVolumeScatterRadianceB);
            const double direct_sum = ray_tracing_headless_rgb_sum(
                preflight->stats.totalDirectVolumeScatterRadianceR,
                preflight->stats.totalDirectVolumeScatterRadianceG,
                preflight->stats.totalDirectVolumeScatterRadianceB);
            const double footprint_input_sum = ray_tracing_headless_rgb_sum(
                preflight->stats.totalCausticVolumeCacheFootprintInputRadianceR,
                preflight->stats.totalCausticVolumeCacheFootprintInputRadianceG,
                preflight->stats.totalCausticVolumeCacheFootprintInputRadianceB);
            const double footprint_deposited_sum = ray_tracing_headless_rgb_sum(
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
                    ray_tracing_headless_safe_ratio(footprint_deposited_sum,
                                                    footprint_input_sum));
            fprintf(file, "      \"volume_scatter_caustic_radiance_sum\": %.9f,\n",
                    scatter_sum);
            fprintf(file, "      \"volume_scatter_direct_radiance_sum\": %.9f,\n",
                    direct_sum);
            fprintf(file,
                    "      \"volume_scatter_caustic_to_cache_radiance_ratio\": %.9f,\n",
                    ray_tracing_headless_safe_ratio(scatter_sum, cache_sum));
            fprintf(file,
                    "      \"volume_scatter_caustic_to_direct_radiance_ratio\": %.9f,\n",
                    ray_tracing_headless_safe_ratio(scatter_sum, direct_sum));
            fprintf(file, "      \"volume_scatter_caustic_sampled_cache_radiance_sum\": %.9f,\n",
                    preflight->stats.totalCausticVolumeScatterSampledCacheRadiance);
            fprintf(file, "      \"volume_scatter_caustic_sampled_cache_radiance_avg\": %.9f,\n",
                    ray_tracing_headless_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterSampledCacheRadiance,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_sampled_cache_radiance_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterSampledCacheRadiance);
            fprintf(file, "      \"volume_scatter_caustic_raw_density_avg\": %.9f,\n",
                    ray_tracing_headless_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterSampledRawDensity,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_raw_density_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterSampledRawDensity);
            fprintf(file, "      \"volume_scatter_caustic_density_avg\": %.9f,\n",
                    ray_tracing_headless_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterSampledDensity,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_density_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterSampledDensity);
            fprintf(file, "      \"volume_scatter_caustic_probability_avg\": %.9f,\n",
                    ray_tracing_headless_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterProbability,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_probability_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterProbability);
            fprintf(file, "      \"volume_scatter_caustic_camera_transmittance_avg\": %.9f,\n",
                    ray_tracing_headless_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterCameraTransmittance,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_camera_transmittance_min\": %.9f,\n",
                    preflight->stats.minCausticVolumeScatterCameraTransmittance);
            fprintf(file, "      \"volume_scatter_caustic_camera_transmittance_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterCameraTransmittance);
            fprintf(file, "      \"volume_scatter_caustic_visibility_term_avg\": %.9f,\n",
                    ray_tracing_headless_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterVisibilityTerm,
                        caustic_samples));
            fprintf(file, "      \"volume_scatter_caustic_visibility_term_max\": %.9f,\n",
                    preflight->stats.maxCausticVolumeScatterVisibilityTerm);
            fprintf(file, "      \"volume_scatter_caustic_contributing_pixel_count\": %d,\n",
                    preflight->stats.causticVolumeScatterContributingPixelCount);
            fprintf(file,
                    "      \"volume_scatter_caustic_contributing_pixel_centroid\": { \"x\": %.9f, \"y\": %.9f },\n",
                    ray_tracing_headless_safe_ratio(
                        preflight->stats.totalCausticVolumeScatterPixelX,
                        caustic_pixels),
                    ray_tracing_headless_safe_ratio(
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
    fprintf(file, "    \"has_caustic_sidecar_enabled_override\": %s,\n",
            request->has_caustic_sidecar_enabled_override ? "true" : "false");
    fprintf(file, "    \"caustic_sidecar_enabled\": %s,\n",
            request->caustic_sidecar_enabled ? "true" : "false");
    fprintf(file, "    \"has_caustic_sidecar_strength_override\": %s,\n",
            request->has_caustic_sidecar_strength_override ? "true" : "false");
    fprintf(file, "    \"caustic_sidecar_strength\": %.9f,\n",
            request->caustic_sidecar_strength);
    fprintf(file, "    \"has_volume_tint_override\": %s,\n",
            request->has_volume_tint_override ? "true" : "false");
    fprintf(file, "    \"volume_tint\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f },\n",
            request->volume_tint_r,
            request->volume_tint_g,
            request->volume_tint_b);
    fprintf(file, "    \"has_volume_albedo_override\": %s,\n",
            request->has_volume_albedo_override ? "true" : "false");
    fprintf(file, "    \"volume_albedo\": { \"r\": %.9f, \"g\": %.9f, \"b\": %.9f }\n",
            request->volume_albedo_r,
            request->volume_albedo_g,
            request->volume_albedo_b);
    fprintf(file, "  },\n");
    {
        const RuntimeEnvironment3D *environment = &preflight->environment_summary;
        const double ambient_strength =
            RuntimeEnvironment3D_AmbientStrength(environment);
        const double background_brightness =
            RuntimeEnvironment3D_BackgroundBrightness(environment);
        const bool ambient_contributes =
            preflight->environment_summary_built &&
            environment->lightMode == ENVIRONMENT_LIGHT_MODE_AMBIENT &&
            ambient_strength > 0.0;
        const bool background_contributes =
            preflight->environment_summary_built &&
            environment->lightMode == ENVIRONMENT_LIGHT_MODE_AMBIENT &&
            background_brightness > 0.0;
        const bool top_fill_contributes =
            preflight->environment_summary_built &&
            environment->lightMode == ENVIRONMENT_LIGHT_MODE_TOP_FILL &&
            environment->topFillIntensity > 0.0;
        fprintf(file, "  \"environment_lighting\": {\n");
        fprintf(file, "    \"built\": %s,\n",
                preflight->environment_summary_built ? "true" : "false");
        fprintf(file, "    \"mode\": ");
        RayTracingJsonWriteString(file, environment_light_mode_label(environment->lightMode));
        fprintf(file, ",\n");
        fprintf(file, "    \"preset\": ");
        RayTracingJsonWriteString(file, RuntimeEnvironment3DPresetLabel(environment->preset));
        fprintf(file, ",\n");
        fprintf(file, "    \"ambient_strength\": %.9f,\n", ambient_strength);
        fprintf(file, "    \"ambient_color\": [%.9f, %.9f, %.9f],\n",
                environment->ambientColor.x,
                environment->ambientColor.y,
                environment->ambientColor.z);
        fprintf(file, "    \"background_brightness\": %.9f,\n", background_brightness);
        fprintf(file, "    \"background_brightness_source\": ");
        RayTracingJsonWriteString(file,
                          environment->backgroundIntensityDerivedFromAmbient
                              ? "ambient_strength_compat"
                              : "background_brightness");
        fprintf(file, ",\n");
        fprintf(file, "    \"background_color\": [%.9f, %.9f, %.9f],\n",
                environment->backgroundColor.x,
                environment->backgroundColor.y,
                environment->backgroundColor.z);
        fprintf(file, "    \"background_top_color\": [%.9f, %.9f, %.9f],\n",
                environment->backgroundTopColor.x,
                environment->backgroundTopColor.y,
                environment->backgroundTopColor.z);
        fprintf(file, "    \"background_bottom_color\": [%.9f, %.9f, %.9f],\n",
                environment->backgroundBottomColor.x,
                environment->backgroundBottomColor.y,
                environment->backgroundBottomColor.z);
        fprintf(file, "    \"top_fill_strength\": %.9f,\n",
                environment->topFillIntensity);
        fprintf(file, "    \"ambient_surface_fill_contributes\": %s,\n",
                ambient_contributes ? "true" : "false");
        fprintf(file, "    \"background_miss_contributes\": %s,\n",
                background_contributes ? "true" : "false");
        fprintf(file, "    \"top_fill_contributes\": %s,\n",
                top_fill_contributes ? "true" : "false");
        fprintf(file, "    \"authored_direct_light_count\": %d\n",
                preflight->scene_summary.light_count);
        fprintf(file, "  },\n");
    }
    fprintf(file, "  \"registered_lights\": {\n");
    fprintf(file, "    \"light_count\": %d,\n", preflight->registered_light_count);
    fprintf(file, "    \"enabled_count\": %d,\n", preflight->registered_enabled_light_count);
    fprintf(file, "    \"shape_counts\": { \"point\": %d, \"sphere\": %d, \"disk\": %d, \"rect\": %d, \"mesh_emissive\": %d },\n",
            preflight->registered_light_point_count,
            preflight->registered_light_sphere_count,
            preflight->registered_light_disk_count,
            preflight->registered_light_rect_count,
            preflight->registered_light_mesh_emissive_count);
    fprintf(file, "    \"source_counts\": { \"authored\": %d, \"compatibility\": %d, \"material_emitter\": %d },\n",
            preflight->registered_light_authored_count,
            preflight->registered_light_compatibility_count,
            preflight->registered_light_material_emitter_count);
    fprintf(file, "    \"material_emitter_enabled_count\": %d,\n",
            preflight->registered_light_material_emitter_enabled_count);
    fprintf(file, "    \"mesh_area_sampler_only_count\": %d,\n",
            preflight->registered_light_mesh_area_sampler_only_count);
    fprintf(file, "    \"emission_profile_counts\": { \"omni\": %d, \"one_sided\": %d, \"two_sided\": %d },\n",
            preflight->registered_light_emission_omni_count,
            preflight->registered_light_emission_one_sided_count,
            preflight->registered_light_emission_two_sided_count);
    fprintf(file, "    \"emissive_candidate_count\": %d,\n",
            preflight->registered_light_emissive_candidate_count);
    fprintf(file, "    \"emissive_area\": %.9f,\n",
            preflight->registered_light_emissive_area);
    fprintf(file, "    \"emissive_weight\": %.9f,\n",
            preflight->registered_light_emissive_weight);
    fprintf(file, "    \"emissive_proxy_radius_max\": %.9f,\n",
            preflight->registered_light_emissive_proxy_radius_max);
    fprintf(file, "    \"first_color\": [%.9f, %.9f, %.9f]\n",
            preflight->registered_light_first_color_r,
            preflight->registered_light_first_color_g,
            preflight->registered_light_first_color_b);
    fprintf(file, "  },\n");
    fprintf(file, "  \"object_audit_summary\": {\n");
    fprintf(file, "    \"enabled\": %s,\n", preflight->object_audit_enabled ? "true" : "false");
    fprintf(file, "    \"requested_max_dimension\": %d,\n", request->object_audit_max_dimension);
    fprintf(file, "    \"width\": %d,\n", preflight->object_audit_width);
    fprintf(file, "    \"height\": %d,\n", preflight->object_audit_height);
    fprintf(file, "    \"stride_x\": %d,\n", preflight->object_audit_stride_x);
    fprintf(file, "    \"stride_y\": %d,\n", preflight->object_audit_stride_y);
    fprintf(file, "    \"scale_factor\": %d,\n", preflight->object_audit_scale_factor);
    fprintf(file, "    \"sample_count\": %d,\n", preflight->object_audit_sample_count);
    fprintf(file, "    \"full_resolution_pixel_count\": %llu\n",
            (unsigned long long)((request->width > 0 && request->height > 0)
                                     ? ((unsigned long long)request->width *
                                        (unsigned long long)request->height)
                                     : 0ull));
    fprintf(file, "  },\n");
    fprintf(file, "  \"outputs\": {\n");
    fprintf(file, "    \"root\": ");
    RayTracingJsonWriteString(file, request->output_root);
    fprintf(file, ",\n");
    fprintf(file, "    \"frame_dir\": ");
    RayTracingJsonWriteString(file, preflight->frame_dir);
    fprintf(file, ",\n");
    fprintf(file, "    \"first_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->first_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"last_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->last_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"video_enabled\": %s,\n", request->video_enabled ? "true" : "false");
    fprintf(file, "    \"video_path\": ");
    RayTracingJsonWriteString(file, request->video_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"video_fps\": %d", request->video_fps);
    fprintf(file, "\n");
    fprintf(file, "  },\n");
    fprintf(file, "  \"scene_summary\": {\n");
    fprintf(file, "    \"valid_contract\": %s,\n",
            preflight->scene_summary.valid_contract ? "true" : "false");
    fprintf(file, "    \"scene_id\": ");
    RayTracingJsonWriteString(file, preflight->scene_summary.scene_id);
    fprintf(file, ",\n");
    fprintf(file, "    \"object_count\": %d,\n", preflight->scene_summary.object_count);
    fprintf(file, "    \"material_count\": %d,\n", preflight->scene_summary.material_count);
    fprintf(file, "    \"light_count\": %d,\n", preflight->scene_summary.light_count);
    fprintf(file, "    \"camera_count\": %d\n", preflight->scene_summary.camera_count);
    fprintf(file, "  },\n");
    fprintf(file, "  \"volume_summary\": {\n");
    fprintf(file, "    \"enabled\": %s,\n", preflight->volume_summary.enabled ? "true" : "false");
    fprintf(file, "    \"debug_overlay_enabled\": %s,\n",
            preflight->volume_summary.debugOverlayEnabled ? "true" : "false");
    fprintf(file, "    \"has_data\": %s,\n", preflight->volume_summary.hasData ? "true" : "false");
    fprintf(file, "    \"layout_valid\": %s,\n",
            preflight->volume_summary.layoutValid ? "true" : "false");
    fprintf(file, "    \"has_density\": %s,\n",
            preflight->volume_summary.hasDensity ? "true" : "false");
    fprintf(file, "    \"has_velocity\": %s,\n",
            preflight->volume_summary.hasVelocity ? "true" : "false");
    fprintf(file, "    \"has_pressure\": %s,\n",
            preflight->volume_summary.hasPressure ? "true" : "false");
    fprintf(file, "    \"has_solid_mask\": %s,\n",
            preflight->volume_summary.hasSolidMask ? "true" : "false");
    fprintf(file, "    \"grid_w\": %u,\n", preflight->volume_summary.gridW);
    fprintf(file, "    \"grid_h\": %u,\n", preflight->volume_summary.gridH);
    fprintf(file, "    \"grid_d\": %u,\n", preflight->volume_summary.gridD);
    fprintf(file, "    \"cell_count\": %llu,\n",
            (unsigned long long)preflight->volume_summary.cellCount);
    fprintf(file, "    \"density_non_zero_cell_count\": %llu,\n",
            (unsigned long long)preflight->volume_summary.densityNonZeroCellCount);
    fprintf(file, "    \"density_min\": %.9f,\n", preflight->volume_summary.densityMin);
    fprintf(file, "    \"density_max\": %.9f\n", preflight->volume_summary.densityMax);
    fprintf(file, "  },\n");
    fprintf(file, "  \"volume_frame_selection\": {\n");
    fprintf(file, "    \"built\": %s,\n",
            preflight->volume_frame_selection_built ? "true" : "false");
    fprintf(file, "    \"dynamic\": %s,\n",
            preflight->volume_frame_selection_dynamic ? "true" : "false");
    fprintf(file, "    \"requested_first_frame_index\": %d,\n",
            preflight->volume_requested_first_frame_index);
    fprintf(file, "    \"requested_last_frame_index\": %d,\n",
            preflight->volume_requested_last_frame_index);
    fprintf(file, "    \"loaded_first_frame_index\": %llu,\n",
            (unsigned long long)preflight->volume_loaded_first_frame_index);
    fprintf(file, "    \"loaded_last_frame_index\": %llu,\n",
            (unsigned long long)preflight->volume_loaded_last_frame_index);
    fprintf(file, "    \"selected_first_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->volume_selected_first_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"selected_last_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->volume_selected_last_frame_path);
    fprintf(file, "\n");
    fprintf(file, "  },\n");
    fprintf(file, "  \"water_surface\": {\n");
    fprintf(file, "    \"source_found\": %s,\n",
            preflight->water_surface_source_found ? "true" : "false");
    fprintf(file, "    \"loaded\": %s,\n",
            preflight->water_surface_loaded ? "true" : "false");
    fprintf(file, "    \"frame_selection_built\": %s,\n",
            preflight->water_surface_frame_selection_built ? "true" : "false");
    fprintf(file, "    \"dynamic\": %s,\n",
            preflight->water_surface_frame_selection_dynamic ? "true" : "false");
    fprintf(file, "    \"mesh_attached\": %s,\n",
            preflight->water_surface_mesh_attached ? "true" : "false");
    fprintf(file, "    \"triangle_count\": %d,\n",
            preflight->water_surface_triangle_count);
    fprintf(file, "    \"requested_first_frame_index\": %d,\n",
            preflight->water_surface_requested_first_frame_index);
    fprintf(file, "    \"requested_last_frame_index\": %d,\n",
            preflight->water_surface_requested_last_frame_index);
    fprintf(file, "    \"loaded_first_frame_index\": %llu,\n",
            (unsigned long long)preflight->water_surface_loaded_first_frame_index);
    fprintf(file, "    \"loaded_last_frame_index\": %llu,\n",
            (unsigned long long)preflight->water_surface_loaded_last_frame_index);
    fprintf(file, "    \"manifest_path\": ");
    RayTracingJsonWriteString(file, preflight->water_surface_manifest_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"selected_first_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->water_surface_selected_first_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"selected_last_frame_path\": ");
    RayTracingJsonWriteString(file, preflight->water_surface_selected_last_frame_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"surface_axis\": ");
    RayTracingJsonWriteString(file, preflight->water_surface_axis);
    fprintf(file, ",\n");
    fprintf(file, "    \"grid_w\": %u,\n", preflight->water_surface_grid_w);
    fprintf(file, "    \"grid_d\": %u,\n", preflight->water_surface_grid_d);
    fprintf(file, "    \"sample_count\": %llu,\n",
            (unsigned long long)preflight->water_surface_sample_count);
    fprintf(file, "    \"wet_columns\": %u,\n", preflight->water_surface_wet_columns);
    fprintf(file, "    \"dry_columns\": %u,\n", preflight->water_surface_dry_columns);
    fprintf(file, "    \"solid_columns\": %u,\n", preflight->water_surface_solid_columns);
    fprintf(file, "    \"water_cells\": %u,\n", preflight->water_surface_water_cells);
    fprintf(file, "    \"surface_min_y\": %.9f,\n", preflight->water_surface_min_y);
    fprintf(file, "    \"surface_max_y\": %.9f,\n", preflight->water_surface_max_y);
    fprintf(file, "    \"surface_avg_y\": %.9f,\n", preflight->water_surface_avg_y);
    fprintf(file, "    \"max_slope\": %.9f,\n", preflight->water_surface_max_slope);
    fprintf(file, "    \"finite_normals\": %s,\n",
            preflight->water_surface_finite_normals ? "true" : "false");
    fprintf(file, "    \"material\": {\n");
    fprintf(file, "      \"ior\": %.9f,\n", preflight->water_surface_material_ior);
    fprintf(file, "      \"absorption_distance_m\": %.9f,\n",
            preflight->water_surface_absorption_distance_m);
    fprintf(file, "      \"absorption_rgb\": [%.9f, %.9f, %.9f],\n",
            preflight->water_surface_absorption_r,
            preflight->water_surface_absorption_g,
            preflight->water_surface_absorption_b);
    fprintf(file, "      \"reflectivity\": %.9f,\n",
            preflight->water_surface_material_reflectivity);
    fprintf(file, "      \"roughness\": %.9f\n",
            preflight->water_surface_material_roughness);
    fprintf(file, "    },\n");
    fprintf(file, "    \"payload\": {\n");
    fprintf(file, "      \"applied\": %s,\n",
            preflight->water_surface_material_payload_applied ? "true" : "false");
    fprintf(file, "      \"ior\": %.9f,\n", preflight->water_surface_payload_ior);
    fprintf(file, "      \"absorption_distance_m\": %.9f,\n",
            preflight->water_surface_payload_absorption_distance_m);
    fprintf(file, "      \"transparency\": %.9f,\n",
            preflight->water_surface_payload_transparency);
    fprintf(file, "      \"reflectivity\": %.9f,\n",
            preflight->water_surface_payload_reflectivity);
    fprintf(file, "      \"roughness\": %.9f,\n",
            preflight->water_surface_payload_roughness);
    fprintf(file, "      \"tint_rgb\": [%.9f, %.9f, %.9f]\n",
            preflight->water_surface_payload_tint_r,
            preflight->water_surface_payload_tint_g,
            preflight->water_surface_payload_tint_b);
    fprintf(file, "    }\n");
    fprintf(file, "  },\n");
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
        const double cache_sum = ray_tracing_headless_rgb_sum(
            preflight->stats.totalCausticVolumeCacheRadianceR,
            preflight->stats.totalCausticVolumeCacheRadianceG,
            preflight->stats.totalCausticVolumeCacheRadianceB);
        const double scatter_sum = ray_tracing_headless_rgb_sum(
            preflight->stats.totalCausticVolumeScatterRadianceR,
            preflight->stats.totalCausticVolumeScatterRadianceG,
            preflight->stats.totalCausticVolumeScatterRadianceB);
        const double direct_sum = ray_tracing_headless_rgb_sum(
            preflight->stats.totalDirectVolumeScatterRadianceR,
            preflight->stats.totalDirectVolumeScatterRadianceG,
            preflight->stats.totalDirectVolumeScatterRadianceB);
        const double footprint_input_sum = ray_tracing_headless_rgb_sum(
            preflight->stats.totalCausticVolumeCacheFootprintInputRadianceR,
            preflight->stats.totalCausticVolumeCacheFootprintInputRadianceG,
            preflight->stats.totalCausticVolumeCacheFootprintInputRadianceB);
        const double footprint_deposited_sum = ray_tracing_headless_rgb_sum(
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
                ray_tracing_headless_safe_ratio(footprint_deposited_sum,
                                                footprint_input_sum));
        fprintf(file, "    \"caustic_volume_scatter_radiance_sum\": %.9f,\n",
                scatter_sum);
        fprintf(file, "    \"direct_volume_scatter_radiance_sum\": %.9f,\n",
                direct_sum);
        fprintf(file,
                "    \"caustic_volume_scatter_to_cache_radiance_ratio\": %.9f,\n",
                ray_tracing_headless_safe_ratio(scatter_sum, cache_sum));
        fprintf(file,
                "    \"caustic_volume_scatter_to_direct_radiance_ratio\": %.9f,\n",
                ray_tracing_headless_safe_ratio(scatter_sum, direct_sum));
        fprintf(file, "    \"caustic_volume_scatter_sampled_cache_radiance_sum\": %.9f,\n",
                preflight->stats.totalCausticVolumeScatterSampledCacheRadiance);
        fprintf(file, "    \"caustic_volume_scatter_sampled_cache_radiance_avg\": %.9f,\n",
                ray_tracing_headless_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterSampledCacheRadiance,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_sampled_cache_radiance_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterSampledCacheRadiance);
        fprintf(file, "    \"caustic_volume_scatter_raw_density_avg\": %.9f,\n",
                ray_tracing_headless_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterSampledRawDensity,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_raw_density_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterSampledRawDensity);
        fprintf(file, "    \"caustic_volume_scatter_density_avg\": %.9f,\n",
                ray_tracing_headless_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterSampledDensity,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_density_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterSampledDensity);
        fprintf(file, "    \"caustic_volume_scatter_probability_avg\": %.9f,\n",
                ray_tracing_headless_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterProbability,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_probability_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterProbability);
        fprintf(file, "    \"caustic_volume_scatter_camera_transmittance_avg\": %.9f,\n",
                ray_tracing_headless_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterCameraTransmittance,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_camera_transmittance_min\": %.9f,\n",
                preflight->stats.minCausticVolumeScatterCameraTransmittance);
        fprintf(file, "    \"caustic_volume_scatter_camera_transmittance_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterCameraTransmittance);
        fprintf(file, "    \"caustic_volume_scatter_visibility_term_avg\": %.9f,\n",
                ray_tracing_headless_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterVisibilityTerm,
                    caustic_samples));
        fprintf(file, "    \"caustic_volume_scatter_visibility_term_max\": %.9f,\n",
                preflight->stats.maxCausticVolumeScatterVisibilityTerm);
        fprintf(file, "    \"caustic_volume_scatter_contributing_pixel_count\": %d,\n",
                preflight->stats.causticVolumeScatterContributingPixelCount);
        fprintf(file,
                "    \"caustic_volume_scatter_contributing_pixel_centroid\": { \"x\": %.9f, \"y\": %.9f },\n",
                ray_tracing_headless_safe_ratio(
                    preflight->stats.totalCausticVolumeScatterPixelX,
                    caustic_pixels),
                ray_tracing_headless_safe_ratio(
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
    fprintf(file, "  \"timing_breakdown\": {\n");
    fprintf(file, "    \"runtime_scene_apply_ms\": %.6f,\n",
            preflight->runtime_scene_apply_ms);
    fprintf(file, "    \"runtime_scene_preflight_ms\": %.6f,\n",
            preflight->runtime_scene_preflight_ms);
    fprintf(file, "    \"native_prepare_frame_ms\": %.6f,\n",
            preflight->native_prepare_frame_ms);
    fprintf(file, "    \"object_audit_ms\": %.6f,\n", preflight->object_audit_ms);
    fprintf(file, "    \"render_frames_ms\": %.6f,\n", preflight->render_frames_ms);
    fprintf(file, "    \"render_trace_ms\": %.6f,\n", preflight->render_trace_ms);
    fprintf(file, "    \"frame_analysis_ms\": %.6f,\n", preflight->frame_analysis_ms);
    fprintf(file, "    \"frame_write_ms\": %.6f,\n", preflight->frame_write_ms);
    fprintf(file, "    \"video_encode_ms\": %.6f,\n", preflight->video_encode_ms);
    fprintf(file, "    \"total_run_ms\": %.6f,\n", preflight->total_run_ms);
    fprintf(file, "    \"mesh_asset_loader\": {\n");
    fprintf(file, "      \"total_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.total_ms);
    fprintf(file, "      \"scene_read_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.scene_read_ms);
    fprintf(file, "      \"scene_parse_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.scene_parse_ms);
    fprintf(file, "      \"sidecar_path_resolution_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.sidecar_path_resolution_ms);
    fprintf(file, "      \"asset_load_total_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.asset_load_total_ms);
    fprintf(file, "      \"asset_runtime_document_load_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.asset_runtime_document_load_ms);
    fprintf(file, "      \"asset_persistent_cache_mode\": ");
    RayTracingJsonWriteString(
        file,
        ray_tracing_headless_mesh_asset_persistent_cache_mode_label(
            preflight->mesh_asset_timing_stats.asset_persistent_cache_mode));
    fprintf(file, ",\n");
    fprintf(file, "      \"asset_persistent_cache_read_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.asset_persistent_cache_read_ms);
    fprintf(file, "      \"asset_persistent_cache_write_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.asset_persistent_cache_write_ms);
    fprintf(file, "      \"asset_document_copy_ms\": %.6f,\n",
            preflight->mesh_asset_timing_stats.asset_document_copy_ms);
    fprintf(file, "      \"asset_load_calls\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_load_calls);
    fprintf(file, "      \"asset_cache_hits\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_cache_hits);
    fprintf(file, "      \"asset_cache_misses\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_cache_misses);
    fprintf(file, "      \"asset_persistent_cache_hits\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_persistent_cache_hits);
    fprintf(file, "      \"asset_persistent_cache_misses\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_persistent_cache_misses);
    fprintf(file, "      \"asset_persistent_cache_writes\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_persistent_cache_writes);
    fprintf(file, "      \"asset_persistent_cache_invalidations\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_persistent_cache_invalidations);
    fprintf(file, "      \"asset_persistent_cache_refreshes\": %d,\n",
            preflight->mesh_asset_timing_stats.asset_persistent_cache_refreshes);
    fprintf(file, "      \"loaded_assets\": %d,\n",
            preflight->mesh_asset_timing_stats.loaded_assets);
    fprintf(file, "      \"loaded_instances\": %d,\n",
            preflight->mesh_asset_timing_stats.loaded_instances);
    fprintf(file, "      \"loaded_asset_bytes\": %llu,\n",
            preflight->mesh_asset_timing_stats.loaded_asset_bytes);
    fprintf(file, "      \"loaded_vertices\": %llu,\n",
            preflight->mesh_asset_timing_stats.loaded_vertices);
    fprintf(file, "      \"loaded_triangles\": %llu\n",
            preflight->mesh_asset_timing_stats.loaded_triangles);
    fprintf(file, "    },\n");
    fprintf(file, "    \"scene_builder\": {\n");
    fprintf(file, "      \"total_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.total_ms);
    fprintf(file, "      \"primitive_seed_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.primitive_seed_ms);
    fprintf(file, "      \"mesh_append_total_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.mesh_append_total_ms);
    fprintf(file, "      \"mesh_append_reserve_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.mesh_append_reserve_ms);
    fprintf(file, "      \"mesh_append_expand_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.mesh_append_expand_ms);
    fprintf(file, "      \"bvh_rebuild_wall_ms\": %.6f,\n",
            preflight->scene_builder_timing_stats.bvh_rebuild_wall_ms);
    fprintf(file, "      \"mesh_append_calls\": %d,\n",
            preflight->scene_builder_timing_stats.mesh_append_calls);
    fprintf(file, "      \"mesh_append_assets\": %d,\n",
            preflight->scene_builder_timing_stats.mesh_append_assets);
    fprintf(file, "      \"mesh_append_instances\": %d,\n",
            preflight->scene_builder_timing_stats.mesh_append_instances);
    fprintf(file, "      \"mesh_append_triangles_expected\": %d,\n",
            preflight->scene_builder_timing_stats.mesh_append_triangles_expected);
    fprintf(file, "      \"mesh_append_triangles_appended\": %d\n",
            preflight->scene_builder_timing_stats.mesh_append_triangles_appended);
    fprintf(file, "    }\n");
    fprintf(file, "  },\n");
    ray_tracing_headless_write_startup_load_timing_matrix(file, preflight);
    fprintf(file, "  \"prepared_scene_cache\": {\n");
    fprintf(file, "    \"valid\": %s,\n",
            preflight->prepared_scene_cache_stats.valid ? "true" : "false");
    fprintf(file, "    \"generation\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.generation);
    fprintf(file, "    \"cached_generation\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.cachedGeneration);
    fprintf(file, "    \"hits\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.hits);
    fprintf(file, "    \"misses\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.misses);
    fprintf(file, "    \"stores\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.stores);
    fprintf(file, "    \"invalidations\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.invalidations);
    fprintf(file, "    \"static_geometry_reuse_enabled\": %s,\n",
            preflight->prepared_scene_cache_stats.staticGeometryReuseEnabled ? "true"
                                                                             : "false");
    fprintf(file, "    \"time_independent_hits\": %llu,\n",
            (unsigned long long)preflight->prepared_scene_cache_stats.timeIndependentHits);
    fprintf(file, "    \"cached_normalized_t\": %.9f,\n",
            preflight->prepared_scene_cache_stats.cachedNormalizedT);
    fprintf(file, "    \"last_requested_normalized_t\": %.9f,\n",
            preflight->prepared_scene_cache_stats.lastRequestedNormalizedT);
    fprintf(file, "    \"cached_primitive_count\": %d,\n",
            preflight->prepared_scene_cache_stats.cachedPrimitiveCount);
    fprintf(file, "    \"cached_triangle_count\": %d,\n",
            preflight->prepared_scene_cache_stats.cachedTriangleCount);
    fprintf(file, "    \"cached_bvh_node_count\": %d,\n",
            preflight->prepared_scene_cache_stats.cachedBVHNodeCount);
    fprintf(file, "    \"cached_bvh_leaf_count\": %d\n",
            preflight->prepared_scene_cache_stats.cachedBVHLeafCount);
    fprintf(file, "  },\n");
    fprintf(file, "  \"prepared_acceleration\": {\n");
    fprintf(file, "    \"enabled\": %s,\n",
            preflight->scene_acceleration_stats.enabled ? "true" : "false");
    fprintf(file, "    \"prepared_accel_reuse_status\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeSceneAcceleration3DReuseStatusLabel(
            preflight->scene_acceleration_stats.reuseStatus));
    fprintf(file, ",\n");
    fprintf(file, "    \"blas_prepare_calls\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasPrepareCalls);
    fprintf(file, "    \"blas_cache_hits\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCacheHits);
    fprintf(file, "    \"blas_cache_misses\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCacheMisses);
    fprintf(file, "    \"blas_cache_invalidations\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCacheInvalidations);
    fprintf(file, "    \"blas_full_rebuilds\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasFullRebuilds);
    fprintf(file, "    \"blas_cached_asset_count\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCachedAssetCount);
    fprintf(file, "    \"blas_persistent_cache_hits\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasPersistentCacheHits);
    fprintf(file, "    \"blas_persistent_cache_misses\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasPersistentCacheMisses);
    fprintf(file, "    \"blas_persistent_cache_writes\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasPersistentCacheWrites);
    fprintf(file, "    \"blas_persistent_cache_invalidations\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasPersistentCacheInvalidations);
    fprintf(file, "    \"blas_persistent_cache_refreshes\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasPersistentCacheRefreshes);
    fprintf(file, "    \"blas_build_ms\": %.6f,\n",
            preflight->scene_acceleration_stats.blasBuildMs);
    fprintf(file, "    \"blas_persistent_cache_read_ms\": %.6f,\n",
            preflight->scene_acceleration_stats.blasPersistentCacheReadMs);
    fprintf(file, "    \"blas_persistent_cache_write_ms\": %.6f,\n",
            preflight->scene_acceleration_stats.blasPersistentCacheWriteMs);
    fprintf(file, "    \"tlas_node_count\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.tlasNodeCount);
    fprintf(file, "    \"tlas_instance_count\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.tlasInstanceCount);
    fprintf(file, "    \"tlas_rebuilds\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.tlasRebuilds);
    fprintf(file, "    \"tlas_refits\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.tlasRefits);
    fprintf(file, "    \"tlas_build_ms\": %.6f,\n",
            preflight->scene_acceleration_stats.tlasBuildMs);
    fprintf(file, "    \"tlas_bind_ms\": %.6f,\n",
            preflight->scene_acceleration_stats.tlasBindMs);
    fprintf(file, "    \"active_trace_route\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeRay3DTraceRouteLabel(preflight->ray_trace_route_stats.activeRoute));
    fprintf(file, ",\n");
    fprintf(file, "    \"requested_trace_route\": ");
    RayTracingJsonWriteString(
        file,
        RuntimeRay3DTraceRouteLabel(preflight->ray_trace_route_stats.requestedRoute));
    fprintf(file, ",\n");
    fprintf(file, "    \"route_trace_calls\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.traceCalls);
    fprintf(file, "    \"route_flattened_trace_calls\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.flattenedTraceCalls);
    fprintf(file, "    \"route_tlas_trace_calls\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.tlasTraceCalls);
    fprintf(file, "    \"route_tlas_trace_hits\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.tlasTraceHits);
    fprintf(file, "    \"route_tlas_trace_misses\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.tlasTraceMisses);
    fprintf(file, "    \"route_tlas_trace_unready\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.tlasTraceUnready);
    fprintf(file, "    \"route_tlas_trace_unsupported\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.tlasTraceUnsupported);
    fprintf(file, "    \"route_tlas_trace_errors\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.tlasTraceErrors);
    fprintf(file, "    \"route_flattened_fallback_calls\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.flattenedFallbackCalls);
    fprintf(file, "    \"route_parity_checked_rays\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.parityCheckedRays);
    fprintf(file, "    \"route_parity_mismatches\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.parityMismatches);
    fprintf(file, "    \"last_parity_mismatch_reason\": ");
    RayTracingJsonWriteString(
        file,
        preflight->ray_trace_route_stats.lastParityMismatchReason);
    fprintf(file, "\n");
    fprintf(file, "  },\n");
    ray_tracing_headless_write_dynamic_geometry_acceleration_summary(file, preflight);
    ray_tracing_headless_write_dynamic_water_acceleration_cache_summary(file, preflight);
    ray_tracing_headless_write_render_trace_cost_ledger(file, preflight);
    fprintf(file, "  \"bvh_summary\": {\n");
    fprintf(file, "    \"ready\": %s,\n",
            preflight->bvh_build_stats.ready ? "true" : "false");
    fprintf(file, "    \"triangle_count\": %d,\n",
            preflight->bvh_build_stats.triangleCount);
    fprintf(file, "    \"node_count\": %d,\n", preflight->bvh_build_stats.nodeCount);
    fprintf(file, "    \"leaf_count\": %d,\n", preflight->bvh_build_stats.leafCount);
    fprintf(file, "    \"max_depth\": %d,\n", preflight->bvh_build_stats.maxDepth);
    fprintf(file, "    \"leaf_size\": %d,\n", preflight->bvh_build_stats.leafSize);
    fprintf(file, "    \"max_leaf_triangle_count\": %d,\n",
            preflight->bvh_build_stats.maxLeafTriangleCount);
    fprintf(file, "    \"build_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.buildCpuMs);
    fprintf(file, "    \"allocation_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.allocationCpuMs);
    fprintf(file, "    \"centroid_build_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.centroidBuildCpuMs);
    fprintf(file, "    \"tree_build_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.treeBuildCpuMs);
    fprintf(file, "    \"range_bounds_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.rangeBoundsCpuMs);
    fprintf(file, "    \"sort_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.sortCpuMs);
    fprintf(file, "    \"node_append_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.nodeAppendCpuMs);
    fprintf(file, "    \"final_stats_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.finalStatsCpuMs);
    fprintf(file, "    \"build_unaccounted_cpu_ms\": %.6f,\n",
            preflight->bvh_build_stats.buildUnaccountedCpuMs);
    fprintf(file, "    \"range_bounds_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.rangeBoundsCalls);
    fprintf(file, "    \"sort_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.sortCalls);
    fprintf(file, "    \"node_append_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.nodeAppendCalls);
    fprintf(file, "    \"max_range_bounds_count\": %d,\n",
            preflight->bvh_build_stats.maxRangeBoundsCount);
    fprintf(file, "    \"max_sort_count\": %d,\n",
            preflight->bvh_build_stats.maxSortCount);
    fprintf(file, "    \"node_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.nodeBytes);
    fprintf(file, "    \"index_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.indexBytes);
    fprintf(file, "    \"centroid_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.centroidBytes);
    fprintf(file, "    \"triangle_bounds_min_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.triangleBoundsMinBytes);
    fprintf(file, "    \"triangle_bounds_max_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.triangleBoundsMaxBytes);
    fprintf(file, "    \"sort_scratch_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.sortScratchBytes);
    fprintf(file, "    \"build_scratch_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.buildScratchBytes);
    fprintf(file, "    \"total_bytes\": %llu,\n",
            (unsigned long long)preflight->bvh_build_stats.totalBytes);
    fprintf(file, "    \"trace_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceCalls);
    fprintf(file, "    \"trace_hits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceHits);
    fprintf(file, "    \"trace_misses\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceMisses);
    fprintf(file, "    \"trace_overflows\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.traceOverflows);
    fprintf(file, "    \"flat_fallback_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.flatFallbackCalls);
    fprintf(file, "    \"overflow_fallback_calls\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.overflowFallbackCalls);
    fprintf(file, "    \"node_visits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.nodeVisits);
    fprintf(file, "    \"leaf_visits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.leafVisits);
    fprintf(file, "    \"aabb_tests\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.aabbTests);
    fprintf(file, "    \"aabb_hits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.aabbHits);
    fprintf(file, "    \"triangle_tests\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.triangleTests);
    fprintf(file, "    \"triangle_hits\": %llu,\n",
            (unsigned long long)preflight->bvh_trace_stats.triangleHits);
    fprintf(file, "    \"max_stack_depth\": %llu\n",
            (unsigned long long)preflight->bvh_trace_stats.maxStackDepth);
    fprintf(file, "  },\n");
    fprintf(file, "  \"object_audit\": [\n");
    {
        int emitted = 0;
        for (int i = 0; i < RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX; ++i) {
            const RayTracingHeadlessObjectAuditEntry *entry = &preflight->object_audit[i];
            if (!entry->used) continue;
            if (emitted > 0) {
                fprintf(file, ",\n");
            }
            fprintf(file, "    {\n");
            fprintf(file, "      \"scene_object_index\": %d,\n", entry->scene_object_index);
            fprintf(file, "      \"object_id\": ");
            RayTracingJsonWriteString(file, entry->object_id);
            fprintf(file, ",\n");
            fprintf(file, "      \"object_type\": ");
            RayTracingJsonWriteString(file, entry->object_type);
            fprintf(file, ",\n");
            fprintf(file, "      \"material_id\": %d,\n", entry->material_id);
            fprintf(file, "      \"alpha\": %.6f,\n", entry->alpha);
            fprintf(file, "      \"reflectivity\": %.6f,\n", entry->reflectivity);
            fprintf(file, "      \"roughness\": %.6f,\n", entry->roughness);
            fprintf(file, "      \"emissive_strength\": %.6f,\n", entry->emissive_strength);
            fprintf(file, "      \"texture_id\": %d,\n", entry->texture_id);
            fprintf(file, "      \"texture_strength\": %.6f,\n", entry->texture_strength);
            fprintf(file, "      \"texture_scale\": %.6f,\n", entry->texture_scale);
            fprintf(file, "      \"texture_offset_u\": %.6f,\n", entry->texture_offset_u);
            fprintf(file, "      \"texture_offset_v\": %.6f,\n", entry->texture_offset_v);
            fprintf(file, "      \"texture_seed\": %d,\n", entry->texture_seed);
            fprintf(file, "      \"texture_pattern_mode\": %d,\n", entry->texture_pattern_mode);
            fprintf(file, "      \"texture_coverage\": %.6f,\n", entry->texture_coverage);
            fprintf(file, "      \"texture_grain\": %.6f,\n", entry->texture_grain);
            fprintf(file, "      \"texture_edge_softness\": %.6f,\n", entry->texture_edge_softness);
            fprintf(file, "      \"texture_contrast\": %.6f,\n", entry->texture_contrast);
            fprintf(file, "      \"texture_flow\": %.6f,\n", entry->texture_flow);
            fprintf(file, "      \"texture_color_depth\": %.6f,\n", entry->texture_color_depth);
            fprintf(file, "      \"texture_surface_damage\": %.6f,\n", entry->texture_surface_damage);
            fprintf(file, "      \"packed_color\": %d,\n", entry->packed_color);
            fprintf(file, "      \"primitive_count\": %d,\n", entry->primitive_count);
            fprintf(file, "      \"triangle_count\": %d,\n", entry->triangle_count);
            fprintf(file, "      \"primary_hit_pixels\": %d,\n", entry->primary_hit_pixels);
            fprintf(file, "      \"center\": { \"x\": %.6f, \"y\": %.6f, \"z\": %.6f },\n",
                    entry->center_x,
                    entry->center_y,
                    entry->center_z);
            fprintf(file, "      \"center_projectable\": %s,\n",
                    entry->center_projectable ? "true" : "false");
            fprintf(file, "      \"center_inside_viewport\": %s,\n",
                    entry->center_inside_viewport ? "true" : "false");
            fprintf(file,
                    "      \"center_screen\": { \"x\": %.6f, \"y\": %.6f, \"camera_depth\": %.6f }\n",
                    entry->center_screen_x,
                    entry->center_screen_y,
                    entry->center_camera_depth);
            fprintf(file, "    }");
            emitted += 1;
        }
        if (emitted > 0) {
            fprintf(file, "\n");
        }
    }
    fprintf(file, "  ],\n");
    fprintf(file, "  \"diagnostics\": ");
    RayTracingJsonWriteString(file, preflight->diagnostics);
    fprintf(file, "\n");
    fprintf(file, "}\n");
}

bool ray_tracing_render_headless_write_summary_file(
    const char *path,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight) {
    FILE *file = NULL;
    if (!path || !path[0]) return true;
    file = fopen(path, "wb");
    if (!file) return false;
    ray_tracing_render_headless_write_summary(file, request, preflight);
    fclose(file);
    return true;
}

static bool utc_now_string(char *out, size_t out_size) {
    time_t now = 0;
    struct tm tm_utc;
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1) return false;
    if (gmtime_r(&now, &tm_utc) == NULL) return false;
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) > 0u;
}

static bool ensure_directory_exists(const char *path) {
    char tmp[PATH_MAX];
    size_t len = 0u;

    if (!path || !path[0]) return false;
    len = strlen(path);
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1u);

    for (size_t i = 1u; i < len; ++i) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, 0700) != 0 && errno != EEXIST) {
            return false;
        }
        tmp[i] = '/';
    }

    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static bool ensure_parent_directory_exists(const char *path) {
    const char *slash = NULL;
    char dir[PATH_MAX];
    size_t len = 0u;
    if (!path || !path[0]) return false;
    slash = strrchr(path, '/');
    if (!slash) return true;
    len = (size_t)(slash - path);
    if (len == 0u) {
        return true;
    }
    if (len >= sizeof(dir)) return false;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return ensure_directory_exists(dir);
}

static bool write_progress_file(const char *path,
                                const RayTracingAgentRenderRequest *request,
                                const char *stage,
                                int frame_index,
                                int frames_completed,
                                int temporal_subpasses_started,
                                int temporal_subpasses_completed,
                                int temporal_subpasses_total,
                                size_t completed_tiles_in_subpass,
                                size_t total_tiles_in_subpass,
                                double elapsed_seconds,
                                double estimated_remaining_seconds,
                                const char *state,
                                const char *diagnostics) {
    FILE *file = NULL;
    char updated_at_utc[32] = {0};
    if (!path || !path[0] || !request) return true;
    if (!ensure_parent_directory_exists(path)) return false;
    utc_now_string(updated_at_utc, sizeof(updated_at_utc));
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_render_progress_v1\",\n");
    fprintf(file, "  \"run_id\": ");
    RayTracingJsonWriteString(file, request->run_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    RayTracingJsonWriteString(file, stage ? stage : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    RayTracingJsonWriteString(file, state ? state : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"frame_index\": %d,\n", frame_index);
    fprintf(file, "  \"frames_completed\": %d,\n", frames_completed);
    fprintf(file, "  \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"temporal_subpasses_started\": %d,\n", temporal_subpasses_started);
    fprintf(file, "  \"temporal_subpasses_completed\": %d,\n", temporal_subpasses_completed);
    fprintf(file, "  \"temporal_subpasses_total\": %d,\n", temporal_subpasses_total);
    fprintf(file, "  \"completed_tiles_in_subpass\": %zu,\n", completed_tiles_in_subpass);
    fprintf(file, "  \"total_tiles_in_subpass\": %zu,\n", total_tiles_in_subpass);
    fprintf(file, "  \"elapsed_seconds\": %.6f,\n", elapsed_seconds > 0.0 ? elapsed_seconds : 0.0);
    fprintf(file, "  \"estimated_remaining_seconds\": %.6f,\n",
            estimated_remaining_seconds >= 0.0 ? estimated_remaining_seconds : -1.0);
    fprintf(file,
            "  \"progress_ratio\": %.6f,\n",
            RayTracingProgressRatioActive(frames_completed,
                                          request->frame_count,
                                          temporal_subpasses_started,
                                          temporal_subpasses_completed,
                                          temporal_subpasses_total,
                                          completed_tiles_in_subpass,
                                          total_tiles_in_subpass));
    fprintf(file, "  \"updated_at_utc\": ");
    RayTracingJsonWriteString(file, updated_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"diagnostics\": ");
    RayTracingJsonWriteString(file, diagnostics ? diagnostics : "");
    fprintf(file, "\n}\n");
    fclose(file);
    return true;
}

bool ray_tracing_render_headless_write_progress_and_job_status(
    const char *progress_path,
    const RayTracingAgentRenderRequest *request,
    const char *stage,
    int frame_index,
    int frames_completed,
    int temporal_subpasses_started,
    int temporal_subpasses_completed,
    int temporal_subpasses_total,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    double elapsed_seconds,
    double estimated_remaining_seconds,
    const char *state,
    const char *diagnostics,
    const char *job_status_path,
    const char *job_id,
    const char *request_path,
    int exit_code) {
    if (!write_progress_file(progress_path,
                             request,
                             stage,
                             frame_index,
                             frames_completed,
                             temporal_subpasses_started,
                             temporal_subpasses_completed,
                             temporal_subpasses_total,
                             completed_tiles_in_subpass,
                             total_tiles_in_subpass,
                             elapsed_seconds,
                             estimated_remaining_seconds,
                             state,
                             diagnostics)) {
        return false;
    }
    if (job_status_path && job_status_path[0] && job_id && job_id[0]) {
        if (!ray_tracing_render_headless_write_job_status_file(job_status_path,
                                                               job_id,
                                                               request_path,
                                                               request,
                                                               state,
                                                               stage,
                                                               exit_code,
                                                               frame_index,
                                                               frames_completed,
                                                               temporal_subpasses_started,
                                                               temporal_subpasses_completed,
                                                               temporal_subpasses_total,
                                                               completed_tiles_in_subpass,
                                                               total_tiles_in_subpass,
                                                               elapsed_seconds,
                                                               estimated_remaining_seconds,
                                                               diagnostics)) {
            return false;
        }
    }
    return true;
}

static void load_existing_job_status_times(const char *path,
                                           char *out_submitted_at_utc,
                                           size_t submitted_size,
                                           char *out_started_at_utc,
                                           size_t started_size) {
    json_object *root = NULL;
    json_object *value = NULL;
    const char *text_value = NULL;
    if (out_submitted_at_utc && submitted_size > 0u) out_submitted_at_utc[0] = '\0';
    if (out_started_at_utc && started_size > 0u) out_started_at_utc[0] = '\0';
    if (!path || !path[0]) return;
    root = json_object_from_file(path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return;
    }
    if (out_submitted_at_utc && submitted_size > 0u &&
        json_object_object_get_ex(root, "submitted_at_utc", &value) &&
        json_object_is_type(value, json_type_string)) {
        text_value = json_object_get_string(value);
        if (text_value) snprintf(out_submitted_at_utc, submitted_size, "%s", text_value);
    }
    if (out_started_at_utc && started_size > 0u &&
        json_object_object_get_ex(root, "started_at_utc", &value) &&
        json_object_is_type(value, json_type_string)) {
        text_value = json_object_get_string(value);
        if (text_value) snprintf(out_started_at_utc, started_size, "%s", text_value);
    }
    json_object_put(root);
}

bool ray_tracing_render_headless_write_job_status_file(
    const char *path,
    const char *job_id,
    const char *request_path,
    const RayTracingAgentRenderRequest *request,
    const char *state,
    const char *stage,
    int exit_code,
    int frame_index,
    int frames_completed,
    int temporal_subpasses_started,
    int temporal_subpasses_completed,
    int temporal_subpasses_total,
    size_t completed_tiles_in_subpass,
    size_t total_tiles_in_subpass,
    double elapsed_seconds,
    double estimated_remaining_seconds,
    const char *diagnostics) {
    FILE *file = NULL;
    char updated_at_utc[32] = {0};
    char started_at_utc[32] = {0};
    char finished_at_utc[32] = {0};
    char submitted_at_utc[32] = {0};
    char stdout_path[PATH_MAX] = {0};
    char stderr_path[PATH_MAX] = {0};
    char pid_path[PATH_MAX] = {0};
    char job_root[PATH_MAX] = {0};
    char overwrite_policy[32] = {0};
    const char *slash = NULL;
    size_t job_root_len = 0u;
    if (!path || !path[0] || !job_id || !job_id[0] || !request) return true;
    if (!ensure_parent_directory_exists(path)) return false;
    load_existing_job_status_times(path,
                                   submitted_at_utc,
                                   sizeof(submitted_at_utc),
                                   started_at_utc,
                                   sizeof(started_at_utc));
    slash = strrchr(path, '/');
    if (slash) {
        job_root_len = (size_t)(slash - path);
        if (job_root_len > 0u && job_root_len < sizeof(job_root)) {
            memcpy(job_root, path, job_root_len);
            job_root[job_root_len] = '\0';
            snprintf(stdout_path, sizeof(stdout_path), "%s/stdout.log", job_root);
            snprintf(stderr_path, sizeof(stderr_path), "%s/stderr.log", job_root);
            snprintf(pid_path, sizeof(pid_path), "%s/pid.txt", job_root);
        }
    }
    utc_now_string(updated_at_utc, sizeof(updated_at_utc));
    if ((strcmp(state ? state : "", "running") == 0 ||
         strcmp(state ? state : "", "completed") == 0 ||
         strcmp(state ? state : "", "failed") == 0) &&
        started_at_utc[0] == '\0') {
        snprintf(started_at_utc, sizeof(started_at_utc), "%s", updated_at_utc);
    }
    if (strcmp(state ? state : "", "completed") == 0 ||
        strcmp(state ? state : "", "failed") == 0 ||
        strcmp(state ? state : "", "cancelled") == 0) {
        snprintf(finished_at_utc, sizeof(finished_at_utc), "%s", updated_at_utc);
    }
    if (submitted_at_utc[0] == '\0') {
        snprintf(submitted_at_utc, sizeof(submitted_at_utc), "%s", updated_at_utc);
    }
    if (request->overwrite) {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "overwrite");
    } else if (request->has_sampling_window && request->sampling_frame_offset > 0) {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "resume");
    } else {
        snprintf(overwrite_policy, sizeof(overwrite_policy), "fail_if_exists");
    }
    file = fopen(path, "wb");
    if (!file) return false;
    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": \"ray_tracing_detached_job_status_v1\",\n");
    fprintf(file, "  \"program\": \"ray_tracing\",\n");
    fprintf(file, "  \"tool\": \"ray_tracing_render_headless\",\n");
    fprintf(file, "  \"job_id\": ");
    RayTracingJsonWriteString(file, job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    RayTracingJsonWriteString(file, state ? state : "unknown");
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    RayTracingJsonWriteString(file, stage ? stage : "");
    fprintf(file, ",\n");
    fprintf(file, "  \"request_path\": ");
    RayTracingJsonWriteString(file, request_path ? request_path : "");
    fprintf(file, ",\n");
    fprintf(file, "  \"output_root\": ");
    RayTracingJsonWriteString(file, request->output_root);
    fprintf(file, ",\n");
    fprintf(file, "  \"progress_path\": ");
    RayTracingJsonWriteString(file, request->progress_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"summary_path\": ");
    RayTracingJsonWriteString(file, request->summary_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stdout_path\": ");
    RayTracingJsonWriteString(file, stdout_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"stderr_path\": ");
    RayTracingJsonWriteString(file, stderr_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"pid_path\": ");
    RayTracingJsonWriteString(file, pid_path);
    fprintf(file, ",\n");
    fprintf(file, "  \"pid\": %ld,\n", (long)getpid());
    fprintf(file, "  \"exit_code\": %d,\n", exit_code);
    fprintf(file, "  \"overwrite_policy\": ");
    RayTracingJsonWriteString(file, overwrite_policy);
    fprintf(file, ",\n");
    fprintf(file, "  \"requested_start_frame\": %d,\n",
            request->start_frame - (request->has_sampling_window ? request->sampling_frame_offset : 0));
    fprintf(file, "  \"requested_frame_count\": %d,\n",
            request->has_sampling_window ? request->sampling_frame_count : request->frame_count);
    fprintf(file, "  \"effective_start_frame\": %d,\n", request->start_frame);
    fprintf(file, "  \"effective_frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"frame_index\": %d,\n", frame_index);
    fprintf(file, "  \"frames_completed\": %d,\n", frames_completed);
    fprintf(file, "  \"frame_count\": %d,\n", request->frame_count);
    fprintf(file, "  \"temporal_subpasses_started\": %d,\n", temporal_subpasses_started);
    fprintf(file, "  \"temporal_subpasses_completed\": %d,\n", temporal_subpasses_completed);
    fprintf(file, "  \"temporal_subpasses_total\": %d,\n", temporal_subpasses_total);
    fprintf(file, "  \"completed_tiles_in_subpass\": %zu,\n", completed_tiles_in_subpass);
    fprintf(file, "  \"total_tiles_in_subpass\": %zu,\n", total_tiles_in_subpass);
    fprintf(file, "  \"elapsed_seconds\": %.6f,\n", elapsed_seconds > 0.0 ? elapsed_seconds : 0.0);
    fprintf(file, "  \"estimated_remaining_seconds\": %.6f,\n",
            estimated_remaining_seconds >= 0.0 ? estimated_remaining_seconds : -1.0);
    fprintf(file,
            "  \"progress_ratio\": %.6f,\n",
            RayTracingProgressRatioActive(frames_completed,
                                          request->frame_count,
                                          temporal_subpasses_started,
                                          temporal_subpasses_completed,
                                          temporal_subpasses_total,
                                          completed_tiles_in_subpass,
                                          total_tiles_in_subpass));
    fprintf(file, "  \"submitted_at_utc\": ");
    RayTracingJsonWriteString(file, submitted_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"started_at_utc\": ");
    RayTracingJsonWriteString(file, started_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"finished_at_utc\": ");
    RayTracingJsonWriteString(file, finished_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"updated_at_utc\": ");
    RayTracingJsonWriteString(file, updated_at_utc);
    fprintf(file, ",\n");
    fprintf(file, "  \"diagnostics\": ");
    RayTracingJsonWriteString(file, diagnostics ? diagnostics : "");
    fprintf(file, "\n}\n");
    fclose(file);
    return true;
}
