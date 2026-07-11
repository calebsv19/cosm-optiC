#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>

static void ray_tracing_headless_write_direct_light_source_kind_map(
    FILE* file,
    const char* key,
    const uint64_t* values,
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": %llu%s\n",
                indent,
                RuntimeRenderTraceCostDirectLightSourceKind3DLabel(
                    (RuntimeRenderTraceCostDirectLightSourceKind3D)i),
                (unsigned long long)(values ? values[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_direct_light_origin_map(
    FILE* file,
    const char* key,
    const uint64_t* values,
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": %llu%s\n",
                indent,
                RuntimeRenderTraceCostDirectLightSourceOrigin3DLabel(
                    (RuntimeRenderTraceCostDirectLightSourceOrigin3D)i),
                (unsigned long long)(values ? values[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_direct_light_emission_map(
    FILE* file,
    const char* key,
    const uint64_t* values,
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": %llu%s\n",
                indent,
                RuntimeRenderTraceCostDirectLightEmissionProfile3DLabel(
                    (RuntimeRenderTraceCostDirectLightEmissionProfile3D)i),
                (unsigned long long)(values ? values[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_direct_light_outcome_map(
    FILE* file,
    const char* key,
    const uint64_t* values,
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": %llu%s\n",
                indent,
                RuntimeRenderTraceCostDirectLightOutcome3DLabel(
                    (RuntimeRenderTraceCostDirectLightOutcome3D)i),
                (unsigned long long)(values ? values[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_direct_light_stop_map(
    FILE* file,
    const char* key,
    const uint64_t* values,
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": %llu%s\n",
                indent,
                RuntimeRenderTraceCostDirectLightStopReason3DLabel(
                    (RuntimeRenderTraceCostDirectLightStopReason3D)i),
                (unsigned long long)(values ? values[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_direct_light_sample_map(
    FILE* file,
    const char* key,
    const uint64_t* values,
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": %llu%s\n",
                indent,
                RuntimeRenderTraceCostDirectLightSampleBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightSampleBucket3D)i),
                (unsigned long long)(values ? values[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_direct_light_distance_map(
    FILE* file,
    const char* key,
    const uint64_t* values,
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": %llu%s\n",
                indent,
                RuntimeRenderTraceCostDirectLightDistanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightDistanceBucket3D)i),
                (unsigned long long)(values ? values[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_direct_light_importance_map(
    FILE* file,
    const char* key,
    const uint64_t* values,
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": %llu%s\n",
                indent,
                RuntimeRenderTraceCostDirectLightImportanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightImportanceBucket3D)i),
                (unsigned long long)(values ? values[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_direct_light_distance_importance_matrix(
    FILE* file,
    const char* key,
    const uint64_t values[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT]
                         [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT],
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": {",
                indent,
                RuntimeRenderTraceCostDirectLightDistanceBucket3DLabel(
                    (RuntimeRenderTraceCostDirectLightDistanceBucket3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostDirectLightImportanceBucket3DLabel(
                        (RuntimeRenderTraceCostDirectLightImportanceBucket3D)j),
                    (unsigned long long)(values ? values[i][j] : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

void ray_tracing_headless_write_direct_light_visibility_policy(
    FILE* file,
    const RuntimeRenderTraceCostLedger3D* ledger) {
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
    fprintf(file, "      \"attribution\": {\n");
    ray_tracing_headless_write_direct_light_source_kind_map(
        file,
        "evaluated_samples_by_source_kind",
        ledger ? ledger->directLightVisibilityPolicy.evaluatedSamplesBySourceKind : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_source_kind_map(
        file,
        "visibility_sample_queries_by_source_kind",
        ledger ? ledger->directLightVisibilityPolicy.visibilityTracesBySourceKind : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_origin_map(
        file,
        "evaluated_samples_by_source_origin",
        ledger ? ledger->directLightVisibilityPolicy.evaluatedSamplesBySourceOrigin : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_origin_map(
        file,
        "visibility_sample_queries_by_source_origin",
        ledger ? ledger->directLightVisibilityPolicy.visibilityTracesBySourceOrigin : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_emission_map(
        file,
        "evaluated_samples_by_emission_profile",
        ledger ? ledger->directLightVisibilityPolicy.evaluatedSamplesByEmissionProfile : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_emission_map(
        file,
        "visibility_sample_queries_by_emission_profile",
        ledger ? ledger->directLightVisibilityPolicy.visibilityTracesByEmissionProfile : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_outcome_map(
        file,
        "evaluated_samples_by_outcome",
        ledger ? ledger->directLightVisibilityPolicy.evaluatedSamplesByOutcome : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_outcome_map(
        file,
        "visibility_sample_queries_by_outcome",
        ledger ? ledger->directLightVisibilityPolicy.visibilityTracesByOutcome : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_stop_map(
        file,
        "evaluated_samples_by_stop_reason",
        ledger ? ledger->directLightVisibilityPolicy.evaluatedSamplesByStopReason : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_stop_map(
        file,
        "visibility_sample_queries_by_stop_reason",
        ledger ? ledger->directLightVisibilityPolicy.visibilityTracesByStopReason : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_sample_map(
        file,
        "evaluated_samples_by_sample_bucket",
        ledger ? ledger->directLightVisibilityPolicy.evaluatedSamplesBySampleBucket : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_sample_map(
        file,
        "visibility_sample_queries_by_sample_bucket",
        ledger ? ledger->directLightVisibilityPolicy.visibilityTracesBySampleBucket : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_distance_map(
        file,
        "evaluated_samples_by_distance",
        ledger ? ledger->directLightVisibilityPolicy.evaluatedSamplesByDistance : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_distance_map(
        file,
        "visibility_sample_queries_by_distance",
        ledger ? ledger->directLightVisibilityPolicy.visibilityTracesByDistance : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_importance_map(
        file,
        "evaluated_samples_by_importance",
        ledger ? ledger->directLightVisibilityPolicy.evaluatedSamplesByImportance : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_importance_map(
        file,
        "visibility_sample_queries_by_importance",
        ledger ? ledger->directLightVisibilityPolicy.visibilityTracesByImportance : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_distance_importance_matrix(
        file,
        "distance_importance_counts",
        ledger ? ledger->directLightVisibilityPolicy.distanceImportanceCounts : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_distance_importance_matrix(
        file,
        "evaluated_samples_by_distance_importance",
        ledger ? ledger->directLightVisibilityPolicy.evaluatedSamplesByDistanceImportance
               : NULL,
        "        ",
        ",");
    ray_tracing_headless_write_direct_light_distance_importance_matrix(
        file,
        "visibility_sample_queries_by_distance_importance",
        ledger ? ledger->directLightVisibilityPolicy.visibilityTracesByDistanceImportance
               : NULL,
        "        ",
        "");
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
}
