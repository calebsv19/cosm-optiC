#include "tools/ray_tracing_render_headless_internal.h"

#include "app/ray_tracing_request_utils.h"
#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"

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

static void ray_tracing_headless_write_transmission_receiver_object_hits(
    FILE* file,
    const RuntimeRenderTraceCostLedger3D* ledger) {
    bool first = true;
    fprintf(file, "      \"receiver_object_hits\": [\n");
    if (ledger) {
        for (int i = 0; i < MAX_OBJECTS; ++i) {
            char object_id[64] = {0};
            const uint64_t hit_count =
                ledger->transmissionPathPolicy.receiverObjectHitCounts[i];
            if (hit_count == 0u) continue;
            if (!first) {
                fprintf(file, ",\n");
            }
            first = false;
            if (!runtime_scene_bridge_get_last_object_id_for_scene_index(i,
                                                                         object_id,
                                                                         sizeof(object_id))) {
                object_id[0] = '\0';
            }
            fprintf(file, "        {\n");
            fprintf(file, "          \"scene_object_index\": %d,\n", i);
            fprintf(file, "          \"object_id\": ");
            RayTracingJsonWriteString(file, object_id);
            fprintf(file, ",\n");
            fprintf(file, "          \"object_type\": ");
            if (i >= 0 && i < sceneSettings.objectCount) {
                RayTracingJsonWriteString(file, sceneSettings.sceneObjects[i].type);
            } else {
                RayTracingJsonWriteString(file, "");
            }
            fprintf(file, ",\n");
            const uint64_t contribution_count =
                ledger->transmissionPathPolicy.receiverObjectContributionCounts[i];
            const double contribution_r =
                ledger->transmissionPathPolicy.receiverObjectContributionR[i];
            const double contribution_g =
                ledger->transmissionPathPolicy.receiverObjectContributionG[i];
            const double contribution_b =
                ledger->transmissionPathPolicy.receiverObjectContributionB[i];
            const double divisor = contribution_count > 0u ? (double)contribution_count : 1.0;
            fprintf(file, "          \"hit_count\": %llu,\n", (unsigned long long)hit_count);
            fprintf(file,
                    "          \"contribution_count\": %llu,\n",
                    (unsigned long long)contribution_count);
            fprintf(file, "          \"contribution_sum_r\": %.9f,\n", contribution_r);
            fprintf(file, "          \"contribution_sum_g\": %.9f,\n", contribution_g);
            fprintf(file, "          \"contribution_sum_b\": %.9f,\n", contribution_b);
            fprintf(file, "          \"contribution_avg_r\": %.9f,\n", contribution_r / divisor);
            fprintf(file, "          \"contribution_avg_g\": %.9f,\n", contribution_g / divisor);
            fprintf(file, "          \"contribution_avg_b\": %.9f\n", contribution_b / divisor);
            fprintf(file, "        }");
        }
    }
    fprintf(file, "\n      ],\n");
}

static void ray_tracing_headless_write_transmission_ior_diagnostics(
    FILE* file,
    const RuntimeRenderTraceCostLedger3D* ledger) {
    const RuntimeRenderTraceCostTransmissionPathPolicy3D* policy =
        ledger ? &ledger->transmissionPathPolicy : NULL;
    const double angle_count =
        policy && policy->refractionAngleDeltaCount > 0u
            ? (double)policy->refractionAngleDeltaCount
            : 1.0;
    const double water_normal_count =
        policy && policy->waterSurfaceNormalSampleCount > 0u
            ? (double)policy->waterSurfaceNormalSampleCount
            : 1.0;
    const double receiver_position_count =
        policy && policy->receiverPositionSampleCount > 0u
            ? (double)policy->receiverPositionSampleCount
            : 1.0;

    fprintf(file, "      \"ior_diagnostics\": {\n");
    fprintf(file,
            "        \"refraction_event_count\": %llu,\n",
            (unsigned long long)(policy ? policy->refractionEventCount : 0u));
    fprintf(file,
            "        \"thin_walled_straight_through_count\": %llu,\n",
            (unsigned long long)(policy ? policy->thinWalledStraightThroughCount : 0u));
    fprintf(file,
            "        \"transparent_physical_hits_without_refraction_count\": %llu,\n",
            (unsigned long long)(policy
                                     ? policy->transparentPhysicalHitsWithoutRefractionCount
                                     : 0u));
    fprintf(file,
            "        \"direction_changed_count\": %llu,\n",
            (unsigned long long)(policy ? policy->directionChangedCount : 0u));
    fprintf(file,
            "        \"direction_unchanged_count\": %llu,\n",
            (unsigned long long)(policy ? policy->directionUnchangedCount : 0u));
    fprintf(file, "        \"refraction_angle_delta_deg\": {\n");
    fprintf(file,
            "          \"count\": %llu,\n",
            (unsigned long long)(policy ? policy->refractionAngleDeltaCount : 0u));
    fprintf(file,
            "          \"min\": %.9f,\n",
            policy && policy->refractionAngleDeltaCount > 0u
                ? policy->refractionAngleDeltaMinDeg
                : 0.0);
    fprintf(file,
            "          \"mean\": %.9f,\n",
            policy && policy->refractionAngleDeltaCount > 0u
                ? policy->refractionAngleDeltaSumDeg / angle_count
                : 0.0);
    fprintf(file,
            "          \"max\": %.9f\n",
            policy && policy->refractionAngleDeltaCount > 0u
                ? policy->refractionAngleDeltaMaxDeg
                : 0.0);
    fprintf(file, "        },\n");
    fprintf(file, "        \"refraction_material_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_MATERIAL_CLASS_COUNT; ++i) {
        fprintf(file,
                "          \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionMaterialClass3DLabel(
                    (RuntimeRenderTraceCostTransmissionMaterialClass3D)i),
                (unsigned long long)(policy ? policy->refractionMaterialCounts[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_MATERIAL_CLASS_COUNT ? "," : "");
    }
    fprintf(file, "        },\n");
    fprintf(file, "        \"eta_pair_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ETA_PAIR_COUNT; ++i) {
        fprintf(file,
                "          \"%s\": %llu%s\n",
                RuntimeRenderTraceCostTransmissionEtaPair3DLabel(
                    (RuntimeRenderTraceCostTransmissionEtaPair3D)i),
                (unsigned long long)(policy ? policy->etaPairCounts[i] : 0u),
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ETA_PAIR_COUNT ? "," : "");
    }
    fprintf(file, "        },\n");
    fprintf(file, "        \"material_eta_pair_counts\": {\n");
    for (int i = 0; i < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_MATERIAL_CLASS_COUNT; ++i) {
        fprintf(file,
                "          \"%s\": {",
                RuntimeRenderTraceCostTransmissionMaterialClass3DLabel(
                    (RuntimeRenderTraceCostTransmissionMaterialClass3D)i));
        for (int j = 0; j < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ETA_PAIR_COUNT; ++j) {
            fprintf(file,
                    " \"%s\": %llu%s",
                    RuntimeRenderTraceCostTransmissionEtaPair3DLabel(
                        (RuntimeRenderTraceCostTransmissionEtaPair3D)j),
                    (unsigned long long)(policy ? policy->materialEtaPairCounts[i][j] : 0u),
                    j + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ETA_PAIR_COUNT ? "," : "");
        }
        fprintf(file,
                " }%s\n",
                i + 1 < RUNTIME_RENDER_TRACE_COST_TRANSMISSION_MATERIAL_CLASS_COUNT ? "," : "");
    }
    fprintf(file, "        },\n");
    fprintf(file, "        \"water_surface_sampled_normal_stats\": {\n");
    fprintf(file,
            "          \"count\": %llu,\n",
            (unsigned long long)(policy ? policy->waterSurfaceNormalSampleCount : 0u));
    fprintf(file,
            "          \"normal_y_min\": %.9f,\n",
            policy && policy->waterSurfaceNormalSampleCount > 0u
                ? policy->waterSurfaceNormalYMin
                : 0.0);
    fprintf(file,
            "          \"normal_y_mean\": %.9f,\n",
            policy && policy->waterSurfaceNormalSampleCount > 0u
                ? policy->waterSurfaceNormalYSum / water_normal_count
                : 0.0);
    fprintf(file,
            "          \"normal_y_max\": %.9f,\n",
            policy && policy->waterSurfaceNormalSampleCount > 0u
                ? policy->waterSurfaceNormalYMax
                : 0.0);
    fprintf(file,
            "          \"normal_z_min\": %.9f,\n",
            policy && policy->waterSurfaceNormalSampleCount > 0u
                ? policy->waterSurfaceNormalZMin
                : 0.0);
    fprintf(file,
            "          \"normal_z_mean\": %.9f,\n",
            policy && policy->waterSurfaceNormalSampleCount > 0u
                ? policy->waterSurfaceNormalZSum / water_normal_count
                : 0.0);
    fprintf(file,
            "          \"normal_z_max\": %.9f\n",
            policy && policy->waterSurfaceNormalSampleCount > 0u
                ? policy->waterSurfaceNormalZMax
                : 0.0);
    fprintf(file, "        },\n");
    fprintf(file, "        \"terminal_receiver_position_stats\": {\n");
    fprintf(file,
            "          \"count\": %llu,\n",
            (unsigned long long)(policy ? policy->receiverPositionSampleCount : 0u));
    fprintf(file,
            "          \"x_min\": %.9f,\n",
            policy && policy->receiverPositionSampleCount > 0u
                ? policy->receiverPositionXMin
                : 0.0);
    fprintf(file,
            "          \"x_mean\": %.9f,\n",
            policy && policy->receiverPositionSampleCount > 0u
                ? policy->receiverPositionXSum / receiver_position_count
                : 0.0);
    fprintf(file,
            "          \"x_max\": %.9f,\n",
            policy && policy->receiverPositionSampleCount > 0u
                ? policy->receiverPositionXMax
                : 0.0);
    fprintf(file,
            "          \"y_min\": %.9f,\n",
            policy && policy->receiverPositionSampleCount > 0u
                ? policy->receiverPositionYMin
                : 0.0);
    fprintf(file,
            "          \"y_mean\": %.9f,\n",
            policy && policy->receiverPositionSampleCount > 0u
                ? policy->receiverPositionYSum / receiver_position_count
                : 0.0);
    fprintf(file,
            "          \"y_max\": %.9f,\n",
            policy && policy->receiverPositionSampleCount > 0u
                ? policy->receiverPositionYMax
                : 0.0);
    fprintf(file,
            "          \"z_min\": %.9f,\n",
            policy && policy->receiverPositionSampleCount > 0u
                ? policy->receiverPositionZMin
                : 0.0);
    fprintf(file,
            "          \"z_mean\": %.9f,\n",
            policy && policy->receiverPositionSampleCount > 0u
                ? policy->receiverPositionZSum / receiver_position_count
                : 0.0);
    fprintf(file,
            "          \"z_max\": %.9f\n",
            policy && policy->receiverPositionSampleCount > 0u
                ? policy->receiverPositionZMax
                : 0.0);
    fprintf(file, "        }\n");
    fprintf(file, "      },\n");
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
    ray_tracing_headless_write_transmission_ior_diagnostics(file, ledger);
    ray_tracing_headless_write_transmission_receiver_object_hits(file, ledger);
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
