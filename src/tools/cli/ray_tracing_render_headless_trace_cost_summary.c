#include "tools/ray_tracing_render_headless_internal.h"

#include <stdio.h>

static void ray_tracing_headless_write_transmission_screen_region_map(
    FILE* file,
    const char* key,
    const uint64_t* values,
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": %llu%s\n",
                indent,
                RuntimeRenderTraceCostTransmissionScreenRegion3DLabel(
                    (RuntimeRenderTraceCostTransmissionScreenRegion3D)i),
                (unsigned long long)(values ? values[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_transmission_pixel_stability_map(
    FILE* file,
    const char* key,
    const uint64_t* values,
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": %llu%s\n",
                indent,
                RuntimeRenderTraceCostTransmissionPixelStability3DLabel(
                    (RuntimeRenderTraceCostTransmissionPixelStability3D)i),
                (unsigned long long)(values ? values[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_transmission_source_screen_region_map(
    FILE* file,
    const char* key,
    const uint64_t values[RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
                         [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT],
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": {",
                indent,
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionScreenRegion3DLabel(
                        (RuntimeRenderTraceCostTransmissionScreenRegion3D)j),
                    (unsigned long long)(values ? values[i][j] : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

static void ray_tracing_headless_write_transmission_source_pixel_stability_map(
    FILE* file,
    const char* key,
    const uint64_t values[RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
                         [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT],
    const char* indent,
    const char* suffix) {
    fprintf(file, "%s\"%s\": {\n", indent, key);
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "%s  \"%s\": {",
                indent,
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionPixelStability3DLabel(
                        (RuntimeRenderTraceCostTransmissionPixelStability3D)j),
                    (unsigned long long)(values ? values[i][j] : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "%s}%s\n", indent, suffix ? suffix : "");
}

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
    fprintf(file, "      \"source_ray_traces\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i),
                (unsigned long long)(ledger ? ledger->transmissionPathPolicy.sourceRayTraces[i]
                                            : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"sample_index_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
                    (RuntimeRenderTraceCostTransmissionSampleIndexBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy.sampleIndexCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_sample_index_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
                        (RuntimeRenderTraceCostTransmissionSampleIndexBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceSampleIndexCounts[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"contributing_samples_by_index\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
                    (RuntimeRenderTraceCostTransmissionSampleIndexBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy
                                               .contributingSamplesByIndex[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"receiver_samples_by_index\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
                    (RuntimeRenderTraceCostTransmissionSampleIndexBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy
                                               .receiverSamplesByIndex[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"no_hit_samples_by_index\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
                    (RuntimeRenderTraceCostTransmissionSampleIndexBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy
                                               .noHitSamplesByIndex[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"zero_contribution_samples_by_index\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
                    (RuntimeRenderTraceCostTransmissionSampleIndexBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy
                                               .zeroContributionSamplesByIndex[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_contributing_samples_by_index\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
                        (RuntimeRenderTraceCostTransmissionSampleIndexBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceContributingSamplesByIndex[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_receiver_samples_by_index\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
                        (RuntimeRenderTraceCostTransmissionSampleIndexBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceReceiverSamplesByIndex[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_no_hit_samples_by_index\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
                        (RuntimeRenderTraceCostTransmissionSampleIndexBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceNoHitSamplesByIndex[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_zero_contribution_samples_by_index\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
                        (RuntimeRenderTraceCostTransmissionSampleIndexBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceZeroContributionSamplesByIndex[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"alignment_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
                    (RuntimeRenderTraceCostTransmissionAlignmentBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy.alignmentCounts[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_alignment_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
                        (RuntimeRenderTraceCostTransmissionAlignmentBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceAlignmentCounts[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"contributing_samples_by_alignment\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
                    (RuntimeRenderTraceCostTransmissionAlignmentBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy
                                               .contributingSamplesByAlignment[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"receiver_samples_by_alignment\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
                    (RuntimeRenderTraceCostTransmissionAlignmentBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy
                                               .receiverSamplesByAlignment[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"no_hit_samples_by_alignment\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
                    (RuntimeRenderTraceCostTransmissionAlignmentBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy
                                               .noHitSamplesByAlignment[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"zero_contribution_samples_by_alignment\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
                    (RuntimeRenderTraceCostTransmissionAlignmentBucket3D)i),
                (unsigned long long)(ledger
                                         ? ledger->transmissionPathPolicy
                                               .zeroContributionSamplesByAlignment[i]
                                         : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_contributing_samples_by_alignment\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
                        (RuntimeRenderTraceCostTransmissionAlignmentBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceContributingSamplesByAlignment[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_receiver_samples_by_alignment\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
                        (RuntimeRenderTraceCostTransmissionAlignmentBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceReceiverSamplesByAlignment[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_no_hit_samples_by_alignment\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
                        (RuntimeRenderTraceCostTransmissionAlignmentBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceNoHitSamplesByAlignment[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    fprintf(file, "      \"source_zero_contribution_samples_by_alignment\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
                        (RuntimeRenderTraceCostTransmissionAlignmentBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceZeroContributionSamplesByAlignment[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
    }
    fprintf(file, "      },\n");
    ray_tracing_headless_write_transmission_screen_region_map(
        file,
        "screen_region_counts",
        ledger ? ledger->transmissionPathPolicy.screenRegionCounts : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_source_screen_region_map(
        file,
        "source_screen_region_counts",
        ledger ? ledger->transmissionPathPolicy.sourceScreenRegionCounts : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_screen_region_map(
        file,
        "contributing_samples_by_screen_region",
        ledger ? ledger->transmissionPathPolicy.contributingSamplesByScreenRegion : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_screen_region_map(
        file,
        "receiver_samples_by_screen_region",
        ledger ? ledger->transmissionPathPolicy.receiverSamplesByScreenRegion : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_screen_region_map(
        file,
        "no_hit_samples_by_screen_region",
        ledger ? ledger->transmissionPathPolicy.noHitSamplesByScreenRegion : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_screen_region_map(
        file,
        "zero_contribution_samples_by_screen_region",
        ledger ? ledger->transmissionPathPolicy.zeroContributionSamplesByScreenRegion : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_source_screen_region_map(
        file,
        "source_contributing_samples_by_screen_region",
        ledger ? ledger->transmissionPathPolicy.sourceContributingSamplesByScreenRegion : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_source_screen_region_map(
        file,
        "source_receiver_samples_by_screen_region",
        ledger ? ledger->transmissionPathPolicy.sourceReceiverSamplesByScreenRegion : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_source_screen_region_map(
        file,
        "source_no_hit_samples_by_screen_region",
        ledger ? ledger->transmissionPathPolicy.sourceNoHitSamplesByScreenRegion : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_source_screen_region_map(
        file,
        "source_zero_contribution_samples_by_screen_region",
        ledger ? ledger->transmissionPathPolicy.sourceZeroContributionSamplesByScreenRegion : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_pixel_stability_map(
        file,
        "pixel_stability_counts",
        ledger ? ledger->transmissionPathPolicy.pixelStabilityCounts : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_source_pixel_stability_map(
        file,
        "source_pixel_stability_counts",
        ledger ? ledger->transmissionPathPolicy.sourcePixelStabilityCounts : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_pixel_stability_map(
        file,
        "contributing_samples_by_pixel_stability",
        ledger ? ledger->transmissionPathPolicy.contributingSamplesByPixelStability : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_pixel_stability_map(
        file,
        "receiver_samples_by_pixel_stability",
        ledger ? ledger->transmissionPathPolicy.receiverSamplesByPixelStability : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_pixel_stability_map(
        file,
        "no_hit_samples_by_pixel_stability",
        ledger ? ledger->transmissionPathPolicy.noHitSamplesByPixelStability : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_pixel_stability_map(
        file,
        "zero_contribution_samples_by_pixel_stability",
        ledger ? ledger->transmissionPathPolicy.zeroContributionSamplesByPixelStability : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_source_pixel_stability_map(
        file,
        "source_contributing_samples_by_pixel_stability",
        ledger ? ledger->transmissionPathPolicy.sourceContributingSamplesByPixelStability : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_source_pixel_stability_map(
        file,
        "source_receiver_samples_by_pixel_stability",
        ledger ? ledger->transmissionPathPolicy.sourceReceiverSamplesByPixelStability : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_source_pixel_stability_map(
        file,
        "source_no_hit_samples_by_pixel_stability",
        ledger ? ledger->transmissionPathPolicy.sourceNoHitSamplesByPixelStability : NULL,
        "      ",
        ",");
    ray_tracing_headless_write_transmission_source_pixel_stability_map(
        file,
        "source_zero_contribution_samples_by_pixel_stability",
        ledger ? ledger->transmissionPathPolicy.sourceZeroContributionSamplesByPixelStability : NULL,
        "      ",
        ",");
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
    fprintf(file, "      \"source_ray_depth_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT; ++i) {
        fprintf(file,
                "        \"%s\": {",
                RuntimeRenderTraceCostTransmissionSource3DLabel(
                    (RuntimeRenderTraceCostTransmissionSource3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostPathDepthBucket3DLabel(
                        (RuntimeRenderTraceCostPathDepthBucket3D)j),
                    (unsigned long long)(ledger
                                             ? ledger->transmissionPathPolicy
                                                   .sourceRayDepthCounts[i][j]
                                             : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT ? "," : "");
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
    ray_tracing_headless_write_direct_light_visibility_policy(file, ledger);
    fprintf(file, "  },\n");
}
