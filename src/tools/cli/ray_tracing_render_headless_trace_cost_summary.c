#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>

void ray_tracing_headless_write_render_trace_cost_ledger(
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
    fprintf(file, "    \"transmission_path_policy\": {\n");
    fprintf(file, "      \"path_evaluations\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy.pathEvaluations : 0u));
    fprintf(file, "      \"requested_samples\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy.requestedSamples : 0u));
    fprintf(file, "      \"sample_evaluations\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy.sampleEvaluations : 0u));
    fprintf(file, "      \"contributing_samples\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy.contributingSamples : 0u));
    fprintf(file, "      \"receiver_samples\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy.receiverSamples : 0u));
    fprintf(file, "      \"ray_traces\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy.rayTraces : 0u));
    fprintf(file, "      \"hit_surfaces\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy.hitSurfaces : 0u));
    fprintf(file, "      \"transparent_surface_hits\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy.transparentSurfaceHits
                                        : 0u));
    fprintf(file, "      \"receiver_hits\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy.receiverHits : 0u));
    fprintf(file, "      \"avg_ray_traces_per_sample\": %.6f,\n",
            ledger && ledger->transmissionPathPolicy.sampleEvaluations > 0u
                ? (double)ledger->transmissionPathPolicy.totalRayTracesPerSample /
                      (double)ledger->transmissionPathPolicy.sampleEvaluations
                : 0.0);
    fprintf(file, "      \"max_ray_traces_in_sample\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy.maxRayTracesInSample
                                        : 0u));
    fprintf(file, "      \"avg_transparent_surfaces_per_sample\": %.6f,\n",
            ledger && ledger->transmissionPathPolicy.sampleEvaluations > 0u
                ? (double)ledger->transmissionPathPolicy.totalTransparentSurfacesPerSample /
                      (double)ledger->transmissionPathPolicy.sampleEvaluations
                : 0.0);
    fprintf(file, "      \"max_transparent_surfaces_in_sample\": %llu,\n",
            (unsigned long long)(ledger ? ledger->transmissionPathPolicy
                                              .maxTransparentSurfacesInSample
                                        : 0u));
    fprintf(file, "      \"source_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i),
                (unsigned long long)(ledger ? ledger->transmissionPathPolicy.sourceCounts[i]
                                            : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_sample_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy.sourceSampleCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"termination_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionTermination3DLabel(
                    (RuntimeRenderTraceCostTransmissionTermination3D)i),
                (unsigned long long)(ledger ? ledger->transmissionPathPolicy
                                                  .terminationCounts[i]
                                            : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_termination_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionTermination3DLabel(
                        (RuntimeRenderTraceCostTransmissionTermination3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceTerminationCounts[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"surface_kind_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionSurfaceKind3DLabel(
                    (RuntimeRenderTraceCostTransmissionSurfaceKind3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy.surfaceKindCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"transparent_surface_material_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostMaterialFamily3DLabel(
                    (RuntimeRenderTraceCostMaterialFamily3D)i),
                (unsigned long long)(ledger ? ledger->transmissionPathPolicy
                                                  .transparentSurfaceMaterialCounts[i]
                                            : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"receiver_material_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostMaterialFamily3DLabel(
                    (RuntimeRenderTraceCostMaterialFamily3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy.receiverMaterialCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"surface_kind_material_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSurfaceKind3DLabel(
                    (RuntimeRenderTraceCostTransmissionSurfaceKind3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostMaterialFamily3DLabel(
                        (RuntimeRenderTraceCostMaterialFamily3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .surfaceKindMaterialCounts[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"terminal_depth_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostPathDepthBucket3DLabel(
                    (RuntimeRenderTraceCostPathDepthBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy.terminalDepthCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"ray_depth_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostPathDepthBucket3DLabel(
                    (RuntimeRenderTraceCostPathDepthBucket3D)i),
                (unsigned long long)(ledger ? ledger->transmissionPathPolicy.rayDepthCounts[i]
                                            : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"throughput_bucket_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_THROUGHPUT_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostThroughputBucket3DLabel(
                    (RuntimeRenderTraceCostThroughputBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy.throughputBucketCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_THROUGHPUT_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"contribution_bucket_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_THROUGHPUT_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostThroughputBucket3DLabel(
                    (RuntimeRenderTraceCostThroughputBucket3D)i),
                (unsigned long long)(ledger ? ledger->transmissionPathPolicy
                                                  .contributionBucketCounts[i]
                                            : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_THROUGHPUT_COUNT ? "," : "");
    }
    fprintf(file, "      }\n");
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
