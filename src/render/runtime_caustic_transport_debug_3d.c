#include "render/runtime_caustic_transport_debug_3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_file_io.h"
#include "render/runtime_caustic_settings_3d.h"

enum {
    RUNTIME_CAUSTIC_TRANSPORT_DEBUG_MAX_RECORDS = 4096
};

static RuntimeCausticTransportDebug3DState g_debug_state = {0};
static RuntimeCausticTransportDebugPath3D* g_debug_paths = NULL;
static size_t g_debug_path_count = 0u;
static size_t g_debug_path_capacity = 0u;
static RuntimeCausticTransport3DRequestState g_last_request_state = {0};
static RuntimeCausticTransport3DDiagnostics g_last_diagnostics = {0};
static bool g_has_last_snapshot = false;
static bool g_has_camera_stats = false;
static int g_camera_contributing_sample_count = 0;
static int g_camera_contributing_pixel_count = 0;
static double g_camera_total_pixel_x = 0.0;
static double g_camera_total_pixel_y = 0.0;
static int g_camera_pixel_min_x = 0;
static int g_camera_pixel_min_y = 0;
static int g_camera_pixel_max_x = 0;
static int g_camera_pixel_max_y = 0;
static double g_camera_sampled_cache_radiance_sum = 0.0;
static double g_camera_sampled_cache_radiance_max = 0.0;

static void runtime_caustic_transport_debug_write_string(FILE* file, const char* text) {
    if (!file) return;
    fputc('"', file);
    if (text) {
        for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
            switch (*p) {
                case '"':
                    fputs("\\\"", file);
                    break;
                case '\\':
                    fputs("\\\\", file);
                    break;
                case '\n':
                    fputs("\\n", file);
                    break;
                case '\r':
                    fputs("\\r", file);
                    break;
                case '\t':
                    fputs("\\t", file);
                    break;
                default:
                    if (*p < 0x20u) {
                        fprintf(file, "\\u%04x", (unsigned int)*p);
                    } else {
                        fputc((int)*p, file);
                    }
                    break;
            }
        }
    }
    fputc('"', file);
}

static void runtime_caustic_transport_debug_write_vec3(FILE* file, Vec3 value) {
    fprintf(file,
            "{ \"x\": %.9f, \"y\": %.9f, \"z\": %.9f }",
            value.x,
            value.y,
            value.z);
}

static void runtime_caustic_transport_debug_write_path(FILE* file,
                                                       const RuntimeCausticTransportDebugPath3D* path,
                                                       bool pretty) {
    const char* sep = pretty ? "\n" : "";
    const char* indent = pretty ? "  " : "";
    if (!file || !path) return;
    fprintf(file, "{%s", sep);
#define WRITE_FIELD_PREFIX(name) do { fprintf(file, "%s\"", indent); fputs(name, file); fputs("\": ", file); } while (0)
#define WRITE_FIELD_COMMA() do { fputs(pretty ? ",\n" : ", ", file); } while (0)
    WRITE_FIELD_PREFIX("path_id"); fprintf(file, "%llu", (unsigned long long)path->pathId); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("emission_policy"); runtime_caustic_transport_debug_write_string(file, path->emissionPolicy); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("light_index"); fprintf(file, "%d", path->lightIndex); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("light_id"); runtime_caustic_transport_debug_write_string(file, path->lightId); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("light_kind"); runtime_caustic_transport_debug_write_string(file, path->lightKind); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("light_position"); runtime_caustic_transport_debug_write_vec3(file, path->lightPosition); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("light_radius"); fprintf(file, "%.9f", path->lightRadius); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("light_intensity"); fprintf(file, "%.9f", path->lightIntensity); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("light_color"); runtime_caustic_transport_debug_write_vec3(file, path->lightColor); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("target_triangle_index"); fprintf(file, "%d", path->targetTriangleIndex); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("target_primitive_index"); fprintf(file, "%d", path->targetPrimitiveIndex); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("target_scene_object_index"); fprintf(file, "%d", path->targetSceneObjectIndex); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("target_sample_index"); fprintf(file, "%d", path->targetSampleIndex); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("target_position"); runtime_caustic_transport_debug_write_vec3(file, path->targetPosition); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("target_distance"); fprintf(file, "%.9f", path->targetDistance); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("first_hit_position"); runtime_caustic_transport_debug_write_vec3(file, path->firstHitPosition); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("first_hit_geometric_normal"); runtime_caustic_transport_debug_write_vec3(file, path->firstHitGeometricNormal); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("first_hit_oriented_normal"); runtime_caustic_transport_debug_write_vec3(file, path->firstHitOrientedNormal); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("material_id"); fprintf(file, "%d", path->materialId); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("transparency"); fprintf(file, "%.9f", path->transparency); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("optical_ior"); fprintf(file, "%.9f", path->opticalIor); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("bsdf_ior"); fprintf(file, "%.9f", path->bsdfIor); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("roughness"); fprintf(file, "%.9f", path->roughness); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("reflectivity"); fprintf(file, "%.9f", path->reflectivity); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("eligible"); fprintf(file, "%s", path->eligible ? "true" : "false"); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("event_type"); runtime_caustic_transport_debug_write_string(file, path->eventType); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("outgoing_direction"); runtime_caustic_transport_debug_write_vec3(file, path->outgoingDirection); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("throughput"); runtime_caustic_transport_debug_write_vec3(file, path->throughput); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("initial_radiance"); runtime_caustic_transport_debug_write_vec3(file, path->initialRadiance); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_shape_kind"); runtime_caustic_transport_debug_write_string(file, path->lensShapeKind); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_scene_object_index"); fprintf(file, "%d", path->lensSceneObjectIndex); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_primitive_index"); fprintf(file, "%d", path->lensPrimitiveIndex); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_interface_event_count"); fprintf(file, "%u", path->lensInterfaceEventCount); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_entry_position"); runtime_caustic_transport_debug_write_vec3(file, path->lensEntryPosition); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_entry_normal"); runtime_caustic_transport_debug_write_vec3(file, path->lensEntryNormal); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_entry_incident_direction"); runtime_caustic_transport_debug_write_vec3(file, path->lensEntryIncidentDirection); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_entry_outgoing_direction"); runtime_caustic_transport_debug_write_vec3(file, path->lensEntryOutgoingDirection); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_entry_eta_from"); fprintf(file, "%.9f", path->lensEntryEtaFrom); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_entry_eta_to"); fprintf(file, "%.9f", path->lensEntryEtaTo); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_entry_fresnel"); fprintf(file, "%.9f", path->lensEntryFresnel); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_entry_total_internal_reflection"); fprintf(file, "%s", path->lensEntryTotalInternalReflection ? "true" : "false"); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_exit_position"); runtime_caustic_transport_debug_write_vec3(file, path->lensExitPosition); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_exit_normal"); runtime_caustic_transport_debug_write_vec3(file, path->lensExitNormal); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_exit_incident_direction"); runtime_caustic_transport_debug_write_vec3(file, path->lensExitIncidentDirection); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_exit_outgoing_direction"); runtime_caustic_transport_debug_write_vec3(file, path->lensExitOutgoingDirection); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_exit_eta_from"); fprintf(file, "%.9f", path->lensExitEtaFrom); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_exit_eta_to"); fprintf(file, "%.9f", path->lensExitEtaTo); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_exit_fresnel"); fprintf(file, "%.9f", path->lensExitFresnel); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_exit_total_internal_reflection"); fprintf(file, "%s", path->lensExitTotalInternalReflection ? "true" : "false"); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_post_exit_origin"); runtime_caustic_transport_debug_write_vec3(file, path->lensPostExitOrigin); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_post_exit_direction"); runtime_caustic_transport_debug_write_vec3(file, path->lensPostExitDirection); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_receiver_crossing"); runtime_caustic_transport_debug_write_vec3(file, path->lensReceiverCrossing); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_inside_distance"); fprintf(file, "%.9f", path->lensInsideDistance); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_sample_weight"); fprintf(file, "%.9f", path->lensSampleWeight); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_path_pdf"); fprintf(file, "%.9f", path->lensPathPdf); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("lens_total_internal_reflection"); fprintf(file, "%s", path->lensTotalInternalReflection ? "true" : "false"); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("sphere_lens_entry_position"); runtime_caustic_transport_debug_write_vec3(file, path->sphereLensEntryPosition); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("sphere_lens_exit_position"); runtime_caustic_transport_debug_write_vec3(file, path->sphereLensExitPosition); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("sphere_lens_receiver_crossing"); runtime_caustic_transport_debug_write_vec3(file, path->sphereLensReceiverCrossing); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("sphere_lens_inside_distance"); fprintf(file, "%.9f", path->sphereLensInsideDistance); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("inside_specular_object_after_event"); fprintf(file, "%s", path->insideSpecularObjectAfterEvent ? "true" : "false"); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("continuation_event_count"); fprintf(file, "%llu", (unsigned long long)path->continuationEventCount); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("exited_specular_object_before_volume_deposit"); fprintf(file, "%s", path->exitedSpecularObjectBeforeVolumeDeposit ? "true" : "false"); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("medium_exit_scene_object_index"); fprintf(file, "%d", path->mediumExitSceneObjectIndex); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("medium_exit_position"); runtime_caustic_transport_debug_write_vec3(file, path->mediumExitPosition); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("medium_exit_direction"); runtime_caustic_transport_debug_write_vec3(file, path->mediumExitDirection); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("volume_clip_hit"); fprintf(file, "%s", path->volumeClipHit ? "true" : "false"); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("volume_t_enter"); fprintf(file, "%.9f", path->volumeTEnter); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("volume_t_exit"); fprintf(file, "%.9f", path->volumeTExit); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("volume_step_count"); fprintf(file, "%d", path->volumeStepCount); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("volume_first_deposit_position"); runtime_caustic_transport_debug_write_vec3(file, path->volumeFirstDepositPosition); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("volume_last_deposit_position"); runtime_caustic_transport_debug_write_vec3(file, path->volumeLastDepositPosition); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("footprint_radius_min"); fprintf(file, "%.9f", path->footprintRadiusMin); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("footprint_radius_max"); fprintf(file, "%.9f", path->footprintRadiusMax); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("volume_deposit_accepted_count"); fprintf(file, "%llu", (unsigned long long)path->volumeDepositAcceptedCount); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("volume_deposit_rejected_count"); fprintf(file, "%llu", (unsigned long long)path->volumeDepositRejectedCount); WRITE_FIELD_COMMA();
    WRITE_FIELD_PREFIX("volume_deposited_radiance"); runtime_caustic_transport_debug_write_vec3(file, path->volumeDepositedRadiance);
    fprintf(file, "%s}", sep);
#undef WRITE_FIELD_PREFIX
#undef WRITE_FIELD_COMMA
}

void RuntimeCausticTransportDebug3D_Reset(void) {
    free(g_debug_paths);
    g_debug_paths = NULL;
    g_debug_path_count = 0u;
    g_debug_path_capacity = 0u;
    memset(&g_debug_state, 0, sizeof(g_debug_state));
}

void RuntimeCausticTransportDebug3D_SetEnabled(bool enabled) {
    g_debug_state.enabled = enabled;
}

void RuntimeCausticTransportDebug3D_SetOutputRoot(const char* output_root) {
    if (!output_root || !output_root[0]) {
        g_debug_state.outputRoot[0] = '\0';
        g_debug_state.summaryPath[0] = '\0';
        g_debug_state.pathsPath[0] = '\0';
        return;
    }
    if (snprintf(g_debug_state.outputRoot,
                 sizeof(g_debug_state.outputRoot),
                 "%s",
                 output_root) >= (int)sizeof(g_debug_state.outputRoot)) {
        g_debug_state.outputRoot[0] = '\0';
    }
    if (snprintf(g_debug_state.summaryPath,
                 sizeof(g_debug_state.summaryPath),
                 "%s/caustic_transport_debug.json",
                 output_root) >= (int)sizeof(g_debug_state.summaryPath)) {
        g_debug_state.summaryPath[0] = '\0';
    }
    if (snprintf(g_debug_state.pathsPath,
                 sizeof(g_debug_state.pathsPath),
                 "%s/caustic_transport_debug_paths.jsonl",
                 output_root) >= (int)sizeof(g_debug_state.pathsPath)) {
        g_debug_state.pathsPath[0] = '\0';
    }
}

bool RuntimeCausticTransportDebug3D_IsEnabled(void) {
    return g_debug_state.enabled && g_debug_state.summaryPath[0] && g_debug_state.pathsPath[0];
}

void RuntimeCausticTransportDebug3D_BeginFrame(void) {
    g_debug_path_count = 0u;
    g_debug_state.recordedPathCount = 0u;
    g_debug_state.droppedPathCount = 0u;
    g_has_last_snapshot = false;
    g_has_camera_stats = false;
}

void RuntimeCausticTransportDebug3D_RecordPath(
    const RuntimeCausticTransportDebugPath3D* path) {
    RuntimeCausticTransportDebugPath3D* next_paths = NULL;
    size_t next_capacity = 0u;
    if (!RuntimeCausticTransportDebug3D_IsEnabled() || !path) return;
    if (g_debug_path_count >= RUNTIME_CAUSTIC_TRANSPORT_DEBUG_MAX_RECORDS) {
        g_debug_state.droppedPathCount += 1u;
        return;
    }
    if (g_debug_path_count >= g_debug_path_capacity) {
        next_capacity = g_debug_path_capacity == 0u ? 64u : g_debug_path_capacity * 2u;
        if (next_capacity > RUNTIME_CAUSTIC_TRANSPORT_DEBUG_MAX_RECORDS) {
            next_capacity = RUNTIME_CAUSTIC_TRANSPORT_DEBUG_MAX_RECORDS;
        }
        next_paths = (RuntimeCausticTransportDebugPath3D*)realloc(
            g_debug_paths, next_capacity * sizeof(*g_debug_paths));
        if (!next_paths) {
            g_debug_state.droppedPathCount += 1u;
            return;
        }
        g_debug_paths = next_paths;
        g_debug_path_capacity = next_capacity;
    }
    g_debug_paths[g_debug_path_count++] = *path;
    g_debug_state.recordedPathCount = (uint64_t)g_debug_path_count;
}

bool RuntimeCausticTransportDebug3D_WriteArtifacts(
    const RuntimeCausticTransport3DRequestState* request_state,
    const RuntimeCausticTransport3DDiagnostics* diagnostics) {
    FILE* summary = NULL;
    FILE* paths = NULL;

    if (!RuntimeCausticTransportDebug3D_IsEnabled() || !request_state || !diagnostics) {
        return false;
    }
    g_last_request_state = *request_state;
    g_last_diagnostics = *diagnostics;
    g_has_last_snapshot = true;
    if (!config_io_ensure_directory_exists(g_debug_state.outputRoot)) {
        return false;
    }
    paths = fopen(g_debug_state.pathsPath, "w");
    if (!paths) return false;
    for (size_t i = 0u; i < g_debug_path_count; ++i) {
        runtime_caustic_transport_debug_write_path(paths, &g_debug_paths[i], false);
        fputc('\n', paths);
    }
    fclose(paths);

    summary = fopen(g_debug_state.summaryPath, "w");
    if (!summary) return false;
    fprintf(summary, "{\n");
    fprintf(summary, "  \"enabled\": true,\n");
    fprintf(summary, "  \"paths_path\": ");
    runtime_caustic_transport_debug_write_string(summary, g_debug_state.pathsPath);
    fprintf(summary, ",\n");
    fprintf(summary, "  \"recorded_path_count\": %llu,\n",
            (unsigned long long)g_debug_state.recordedPathCount);
    fprintf(summary, "  \"dropped_path_count\": %llu,\n",
            (unsigned long long)g_debug_state.droppedPathCount);
    fprintf(summary, "  \"request\": {\n");
    fprintf(summary, "    \"mode\": ");
    runtime_caustic_transport_debug_write_string(
        summary, RuntimeCausticMode3D_Label(request_state->mode));
    fprintf(summary, ",\n");
    fprintf(summary, "    \"volume_cache_requested\": %s,\n",
            request_state->volumeCacheRequested ? "true" : "false");
    fprintf(summary, "    \"surface_cache_requested\": %s,\n",
            request_state->surfaceCacheRequested ? "true" : "false");
    fprintf(summary, "    \"emission_policy\": ");
    runtime_caustic_transport_debug_write_string(
        summary, RuntimeCausticTransportEmissionPolicy3D_Label(request_state->emissionPolicy));
    fprintf(summary, ",\n");
    fprintf(summary, "    \"sample_budget\": %d,\n", request_state->sampleBudget);
    fprintf(summary, "    \"max_path_depth\": %d,\n", request_state->maxPathDepth);
    fprintf(summary, "    \"surface_receiver_fallback_enabled\": %s\n",
            request_state->surfaceReceiverFallbackEnabled ? "true" : "false");
    fprintf(summary, "  },\n");
    fprintf(summary, "  \"transport\": {\n");
    fprintf(summary, "    \"active\": %s,\n", diagnostics->active ? "true" : "false");
    fprintf(summary, "    \"light_count\": %llu,\n",
            (unsigned long long)diagnostics->lightCount);
    fprintf(summary, "    \"evaluated_path_count\": %llu,\n",
            (unsigned long long)diagnostics->evaluatedPathCount);
    fprintf(summary, "    \"emitted_path_count\": %llu,\n",
            (unsigned long long)diagnostics->emittedPathCount);
    fprintf(summary, "    \"analytic_sphere_lens_resolved_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticSphereLensResolvedCount);
    fprintf(summary, "    \"analytic_sphere_lens_rejected_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticSphereLensRejectedCount);
    fprintf(summary, "    \"analytic_sphere_lens_evaluated_path_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticSphereLensEvaluatedPathCount);
    fprintf(summary, "    \"analytic_sphere_lens_emitted_path_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticSphereLensEmittedPathCount);
    fprintf(summary, "    \"analytic_sphere_lens_sample_weight\": %.9f,\n",
            diagnostics->analyticSphereLensSampleWeight);
    fprintf(summary, "    \"analytic_sphere_lens_total_sample_weight\": %.9f,\n",
            diagnostics->analyticSphereLensTotalSampleWeight);
    fprintf(summary, "    \"analytic_cylinder_lens_resolved_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticCylinderLensResolvedCount);
    fprintf(summary, "    \"analytic_cylinder_lens_rejected_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticCylinderLensRejectedCount);
    fprintf(summary, "    \"analytic_cylinder_lens_evaluated_path_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticCylinderLensEvaluatedPathCount);
    fprintf(summary, "    \"analytic_cylinder_lens_emitted_path_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticCylinderLensEmittedPathCount);
    fprintf(summary, "    \"analytic_cylinder_lens_sample_weight\": %.9f,\n",
            diagnostics->analyticCylinderLensSampleWeight);
    fprintf(summary, "    \"analytic_cylinder_lens_total_sample_weight\": %.9f,\n",
            diagnostics->analyticCylinderLensTotalSampleWeight);
    fprintf(summary, "    \"analytic_prism_lens_resolved_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticPrismLensResolvedCount);
    fprintf(summary, "    \"analytic_prism_lens_rejected_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticPrismLensRejectedCount);
    fprintf(summary, "    \"analytic_prism_lens_evaluated_path_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticPrismLensEvaluatedPathCount);
    fprintf(summary, "    \"analytic_prism_lens_emitted_path_count\": %llu,\n",
            (unsigned long long)diagnostics->analyticPrismLensEmittedPathCount);
    fprintf(summary, "    \"analytic_prism_lens_sample_weight\": %.9f,\n",
            diagnostics->analyticPrismLensSampleWeight);
    fprintf(summary, "    \"analytic_prism_lens_total_sample_weight\": %.9f,\n",
            diagnostics->analyticPrismLensTotalSampleWeight);
    fprintf(summary, "    \"transparent_hit_count\": %llu,\n",
            (unsigned long long)diagnostics->transparentHitCount);
    fprintf(summary, "    \"specular_event_count\": %llu,\n",
            (unsigned long long)diagnostics->specularEventCount);
    fprintf(summary, "    \"volume_segment_count\": %llu,\n",
            (unsigned long long)diagnostics->volumeSegmentCount);
    fprintf(summary, "    \"surface_receiver_trace_miss_count\": %llu,\n",
            (unsigned long long)diagnostics->surfaceReceiverTraceMissCount);
    fprintf(summary, "    \"surface_receiver_depth_reject_count\": %llu,\n",
            (unsigned long long)diagnostics->surfaceReceiverDepthRejectCount);
    fprintf(summary, "    \"surface_receiver_hit_count\": %llu,\n",
            (unsigned long long)diagnostics->surfaceReceiverHitCount);
    fprintf(summary, "    \"surface_receiver_fallback_count\": %llu\n",
            (unsigned long long)diagnostics->surfaceReceiverFallbackCount);
    fprintf(summary, "  },\n");
    fprintf(summary, "  \"volume_cache\": {\n");
    fprintf(summary, "    \"allocated\": %s,\n",
            diagnostics->cache.allocated ? "true" : "false");
    fprintf(summary, "    \"grid_w\": %u,\n", diagnostics->cache.gridW);
    fprintf(summary, "    \"grid_h\": %u,\n", diagnostics->cache.gridH);
    fprintf(summary, "    \"grid_d\": %u,\n", diagnostics->cache.gridD);
    fprintf(summary, "    \"voxel_size\": %.9f,\n", diagnostics->cache.voxelSize);
    fprintf(summary, "    \"nonzero_cell_count\": %llu,\n",
            (unsigned long long)diagnostics->cache.nonZeroCellCount);
    fprintf(summary, "    \"radiance_centroid\": ");
    runtime_caustic_transport_debug_write_vec3(summary, diagnostics->cache.radianceCentroid);
    fprintf(summary, ",\n");
    fprintf(summary, "    \"nonzero_bounds_min\": ");
    runtime_caustic_transport_debug_write_vec3(summary, diagnostics->cache.nonZeroBoundsMin);
    fprintf(summary, ",\n");
    fprintf(summary, "    \"nonzero_bounds_max\": ");
    runtime_caustic_transport_debug_write_vec3(summary, diagnostics->cache.nonZeroBoundsMax);
    fprintf(summary, ",\n");
    fprintf(summary, "    \"average_footprint_radius_voxels\": %.9f,\n",
            diagnostics->cache.averageFootprintRadiusVoxels);
    fprintf(summary, "    \"footprint_deposited_to_input_ratio\": %.9f\n",
            (diagnostics->cache.footprintInputRadianceR +
             diagnostics->cache.footprintInputRadianceG +
             diagnostics->cache.footprintInputRadianceB) > 0.0
                ? (diagnostics->cache.footprintDepositedRadianceR +
                   diagnostics->cache.footprintDepositedRadianceG +
                   diagnostics->cache.footprintDepositedRadianceB) /
                      (diagnostics->cache.footprintInputRadianceR +
                       diagnostics->cache.footprintInputRadianceG +
                       diagnostics->cache.footprintInputRadianceB)
                : 0.0);
    fprintf(summary, "  },\n");
    fprintf(summary, "  \"camera\": {\n");
    fprintf(summary, "    \"available\": %s,\n", g_has_camera_stats ? "true" : "false");
    fprintf(summary, "    \"caustic_volume_scatter_contributing_samples\": %d,\n",
            g_camera_contributing_sample_count);
    fprintf(summary, "    \"caustic_volume_scatter_contributing_pixels\": %d,\n",
            g_camera_contributing_pixel_count);
    fprintf(summary, "    \"contributing_pixel_centroid\": { \"x\": %.9f, \"y\": %.9f },\n",
            g_camera_contributing_pixel_count > 0
                ? g_camera_total_pixel_x / (double)g_camera_contributing_pixel_count
                : 0.0,
            g_camera_contributing_pixel_count > 0
                ? g_camera_total_pixel_y / (double)g_camera_contributing_pixel_count
                : 0.0);
    fprintf(summary,
            "    \"contributing_pixel_bounds_min\": { \"x\": %d, \"y\": %d },\n",
            g_camera_pixel_min_x,
            g_camera_pixel_min_y);
    fprintf(summary,
            "    \"contributing_pixel_bounds_max\": { \"x\": %d, \"y\": %d },\n",
            g_camera_pixel_max_x,
            g_camera_pixel_max_y);
    fprintf(summary, "    \"sampled_cache_radiance_sum\": %.9f,\n",
            g_camera_sampled_cache_radiance_sum);
    fprintf(summary, "    \"sampled_cache_radiance_avg\": %.9f,\n",
            g_camera_contributing_sample_count > 0
                ? g_camera_sampled_cache_radiance_sum /
                      (double)g_camera_contributing_sample_count
                : 0.0);
    fprintf(summary, "    \"sampled_cache_radiance_max\": %.9f\n",
            g_camera_sampled_cache_radiance_max);
    fprintf(summary, "  }\n");
    fprintf(summary, "}\n");
    fclose(summary);
    return true;
}

bool RuntimeCausticTransportDebug3D_WriteCameraStats(int contributing_sample_count,
                                                     int contributing_pixel_count,
                                                     double total_pixel_x,
                                                     double total_pixel_y,
                                                     int pixel_min_x,
                                                     int pixel_min_y,
                                                     int pixel_max_x,
                                                     int pixel_max_y,
                                                     double sampled_cache_radiance_sum,
                                                     double sampled_cache_radiance_max) {
    if (!RuntimeCausticTransportDebug3D_IsEnabled() || !g_has_last_snapshot) {
        return false;
    }
    g_has_camera_stats = true;
    g_camera_contributing_sample_count = contributing_sample_count;
    g_camera_contributing_pixel_count = contributing_pixel_count;
    g_camera_total_pixel_x = total_pixel_x;
    g_camera_total_pixel_y = total_pixel_y;
    g_camera_pixel_min_x = pixel_min_x;
    g_camera_pixel_min_y = pixel_min_y;
    g_camera_pixel_max_x = pixel_max_x;
    g_camera_pixel_max_y = pixel_max_y;
    g_camera_sampled_cache_radiance_sum = sampled_cache_radiance_sum;
    g_camera_sampled_cache_radiance_max = sampled_cache_radiance_max;
    return RuntimeCausticTransportDebug3D_WriteArtifacts(&g_last_request_state,
                                                        &g_last_diagnostics);
}

RuntimeCausticTransportDebug3DState RuntimeCausticTransportDebug3D_State(void) {
    return g_debug_state;
}

size_t RuntimeCausticTransportDebug3D_RecordCount(void) {
    return g_debug_path_count;
}

const RuntimeCausticTransportDebugPath3D* RuntimeCausticTransportDebug3D_RecordAt(
    size_t index) {
    if (index >= g_debug_path_count) return NULL;
    return &g_debug_paths[index];
}
