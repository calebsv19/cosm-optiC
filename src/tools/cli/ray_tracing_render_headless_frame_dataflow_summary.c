#include "tools/ray_tracing_render_headless_internal.h"

#include "app/ray_tracing_request_utils.h"
#include "render/runtime_frame_dataflow_ledger_3d.h"
#include "render/runtime_native_3d_render_request_snapshot.h"

#include <stdint.h>
#include <stdio.h>

static uint64_t ray_tracing_frame_dataflow_u64_product(uint64_t a, uint64_t b) {
    if (a == 0u || b == 0u) return 0u;
    if (a > UINT64_MAX / b) return UINT64_MAX;
    return a * b;
}

static uint64_t ray_tracing_frame_dataflow_pixel_count(
    const RayTracingAgentRenderRequest* request) {
    if (!request || request->width <= 0 || request->height <= 0) return 0u;
    return ray_tracing_frame_dataflow_u64_product((uint64_t)request->width,
                                                  (uint64_t)request->height);
}

static uint64_t ray_tracing_frame_dataflow_estimated_scene_bytes(
    int primitive_count,
    int triangle_count) {
    uint64_t primitive_bytes = 0u;
    uint64_t triangle_bytes = 0u;
    if (primitive_count > 0) {
        primitive_bytes = ray_tracing_frame_dataflow_u64_product(
            (uint64_t)primitive_count,
            (uint64_t)sizeof(RuntimePrimitive3D));
    }
    if (triangle_count > 0) {
        triangle_bytes = ray_tracing_frame_dataflow_u64_product(
            (uint64_t)triangle_count,
            (uint64_t)sizeof(RuntimeTriangle3D));
    }
    if (UINT64_MAX - primitive_bytes < triangle_bytes) return UINT64_MAX;
    return primitive_bytes + triangle_bytes;
}

static const char* ray_tracing_frame_dataflow_bvh_skip_decision_label(
    RuntimeNative3DFrameBVHSkipDecision decision) {
    switch (decision) {
        case RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_ROUTE_REQUIRES_FLATTENED_BVH:
            return "route_requires_flattened_bvh";
        case RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_TLAS_BIND_NOT_READY:
            return "tlas_bind_not_ready";
        case RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_SKIPPED_TLAS_READY:
            return "skipped_tlas_ready";
        case RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_DISABLED_BY_ENV:
            return "disabled_by_env";
        case RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_NOT_REQUESTED:
        default:
            return "not_requested";
    }
}

void ray_tracing_headless_write_frame_dataflow_state_ledger(
    FILE* file,
    const RayTracingAgentRenderRequest* request,
    const RayTracingHeadlessPreflight* preflight) {
    const bool enabled = RuntimeFrameDataflowLedger3D_IsEnabled();
    const uint64_t pixel_count = ray_tracing_frame_dataflow_pixel_count(request);
    const uint64_t frame_count =
        request && request->frame_count > 0 ? (uint64_t)request->frame_count : 0u;
    const uint64_t render_pixel_buffer_bytes =
        ray_tracing_frame_dataflow_u64_product(
            pixel_count,
            (uint64_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
    const uint64_t radiance_buffer_bytes =
        ray_tracing_frame_dataflow_u64_product(
            ray_tracing_frame_dataflow_u64_product(
                pixel_count,
                (uint64_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS),
            (uint64_t)sizeof(float));
    const uint64_t estimated_prepared_scene_bytes =
        preflight
            ? ray_tracing_frame_dataflow_estimated_scene_bytes(
                  preflight->prepared_scene_cache_stats.cachedPrimitiveCount,
                  preflight->bvh_build_stats.triangleCount)
            : 0u;
    const uint64_t estimated_cached_scene_bytes =
        preflight
            ? ray_tracing_frame_dataflow_estimated_scene_bytes(
                  preflight->prepared_scene_cache_stats.cachedPrimitiveCount,
                  preflight->prepared_scene_cache_stats.cachedTriangleCount)
            : 0u;
    const uint64_t total_output_pixel_bytes =
        ray_tracing_frame_dataflow_u64_product(render_pixel_buffer_bytes, frame_count);
    const uint64_t total_radiance_buffer_bytes =
        ray_tracing_frame_dataflow_u64_product(radiance_buffer_bytes, frame_count);
    const uint64_t frames_rendered =
        preflight && preflight->frames_rendered > 0 ? (uint64_t)preflight->frames_rendered : 0u;
    const uint64_t measured_output_pixel_bytes =
        ray_tracing_frame_dataflow_u64_product(render_pixel_buffer_bytes, frames_rendered);
    const uint64_t preview_host_movement_bytes =
        preflight
            ? preflight->stats.temporalDirtyPreviewHostBytes +
                  preflight->stats.temporalFinalResolveHostBytes +
                  preflight->stats.temporalHistorySeedHostBytes +
                  preflight->stats.temporalHistoryPromoteHostBytes +
                  preflight->stats.temporalFinalPreviewPresentHostBytes
            : 0u;
    RuntimeNative3DResourceBudget resource_budget = {0};
    const RuntimeNative3DResourceBudget* resource_budget_ptr =
        request ? ray_tracing_headless_request_resource_budget(request, &resource_budget)
                : NULL;
    const RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = request && request->sampling_frame_offset > 0
                              ? (uint32_t)request->sampling_frame_offset
                              : 0u,
        .temporalSubpassIndex = 0u,
        .temporalSubpassCount = request && request->temporal_frames > 0
                                    ? (uint16_t)request->temporal_frames
                                    : 0u,
    };
    RuntimeNative3DRenderRequestSnapshot snapshot;
    RuntimeNative3DRenderRequestSnapshotDesc snapshot_desc = {
        .generationBound = false,
        .generation = 0u,
        .outputWidth = request ? request->width : 0,
        .outputHeight = request ? request->height : 0,
        .renderWidth = request ? request->width : 0,
        .renderHeight = request ? request->height : 0,
        .hostWidth = request ? request->width : 0,
        .hostHeight = request ? request->height : 0,
        .frameIndex = request ? request->start_frame : 0,
        .frameCount = request ? request->frame_count : 0,
        .temporalFrames = request ? request->temporal_frames : 0,
        .tileSize = request ? RuntimeNative3DTileSchedulerResolveTileSize(
                                  request->has_tile_size_override
                                      ? request->tile_size_override
                                      : 0)
                            : 0,
        .integratorId = request ? request->integrator_3d
                                : RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        .sampling = &sampling,
        .resourceBudget = resource_budget_ptr,
        .preparedFrameBound = preflight && preflight->prepared_frame,
        .preparedFrameValid = preflight && preflight->prepared_frame,
        .preparedFrameWidth = request ? request->width : 0,
        .preparedFrameHeight = request ? request->height : 0,
        .preparedPrimitiveCount =
            preflight && preflight->prepared_scene_cache_stats.cachedPrimitiveCount > 0
                ? (uint64_t)preflight->prepared_scene_cache_stats.cachedPrimitiveCount
                : 0u,
        .preparedTriangleCount =
            preflight && preflight->bvh_build_stats.triangleCount > 0
                ? (uint64_t)preflight->bvh_build_stats.triangleCount
                : 0u,
        .materialSnapshotBound =
            preflight && preflight->scene_applied &&
            preflight->scene_summary.valid_contract,
        .materialCount =
            preflight && preflight->scene_summary.material_count > 0
                ? (uint64_t)preflight->scene_summary.material_count
                : 0u,
        .materialObjectBindingCount =
            preflight && preflight->scene_summary.object_count > 0
                ? (uint64_t)preflight->scene_summary.object_count
                : 0u,
        .lightSnapshotBound = preflight && preflight->environment_summary_built,
        .enabledLightCount =
            preflight && preflight->registered_enabled_light_count > 0
                ? (uint64_t)preflight->registered_enabled_light_count
                : 0u,
        .materialEmitterLightCount =
            preflight && preflight->registered_light_material_emitter_enabled_count > 0
                ? (uint64_t)preflight->registered_light_material_emitter_enabled_count
                : 0u,
        .sceneAccelerationBound =
            preflight && preflight->scene_acceleration_stats.enabled,
        .traceRoute = preflight ? preflight->ray_trace_route_stats.activeRoute
                                : RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH,
        .tlasInstanceCount =
            preflight ? preflight->scene_acceleration_stats.tlasInstanceCount : 0u,
        .tlasNodeCount =
            preflight ? preflight->scene_acceleration_stats.tlasNodeCount : 0u,
        .traceContextCallbackBound =
            preflight &&
            preflight->ray_trace_route_stats.sceneAccelerationTraceCallbackBound,
        .volumeEnabled = request && request->volume_enabled,
        .volumeAttached = preflight && preflight->volume_attached,
        .volumeFrameSelectionDynamic =
            preflight && preflight->volume_frame_selection_dynamic,
        .waterSurfaceSourceFound =
            preflight && preflight->water_surface_source_found,
        .waterSurfaceLoaded = preflight && preflight->water_surface_loaded,
        .waterSurfaceMeshAttached =
            preflight && preflight->water_surface_mesh_attached,
        .waterSurfaceFrameSelectionDynamic =
            preflight && preflight->water_surface_frame_selection_dynamic,
        .waterSurfaceSampleCount =
            preflight ? preflight->water_surface_sample_count : 0u,
        .waterSurfaceTriangleCount =
            preflight ? preflight->water_surface_triangle_count : 0,
        .frameDataflowLedgerEnabled = enabled,
        .outputRoot = request ? request->output_root : NULL,
        .summaryPath = request ? request->summary_path : NULL,
        .progressPath = request ? request->progress_path : NULL,
        .cancelToken = NULL,
    };
    RuntimeNative3DRenderRequestSnapshot_Build(&snapshot, &snapshot_desc);

    fprintf(file, "  \"frame_dataflow_state_ledger\": {\n");
    fprintf(file, "    \"enabled\": %s,\n", enabled ? "true" : "false");
    fprintf(file, "    \"activation_env\": ");
    RayTracingJsonWriteString(file, "RAY_TRACING_FRAME_DATAFLOW_STATE_LEDGER");
    fprintf(file, ",\n");
    fprintf(file, "    \"flattened_bvh_skip_on_tlas_force_env\": ");
    RayTracingJsonWriteString(file, "RAY_TRACING_NATIVE3D_SKIP_FLATTENED_BVH_ON_TLAS");
    fprintf(file, ",\n");
    fprintf(file, "    \"flattened_bvh_skip_on_tlas_default_disable_env\": ");
    RayTracingJsonWriteString(file, "RAY_TRACING_NATIVE3D_DISABLE_DEFAULT_TLAS_BVH_SKIP");
    fprintf(file, ",\n");
    fprintf(file, "    \"schema_version\": ");
    RayTracingJsonWriteString(file, "frame_dataflow_state_ledger_v1");
    fprintf(file, ",\n");
    fprintf(file, "    \"source\": ");
    RayTracingJsonWriteString(file, "derived_from_headless_preflight_v1");
    fprintf(file, ",\n");
    fprintf(file, "    \"estimates_are_allocator_exact\": false,\n");
    fprintf(file, "    \"render_request\": {\n");
    fprintf(file, "      \"width\": %d,\n", request ? request->width : 0);
    fprintf(file, "      \"height\": %d,\n", request ? request->height : 0);
    fprintf(file, "      \"frame_count\": %d,\n", request ? request->frame_count : 0);
    fprintf(file, "      \"temporal_frames\": %d,\n",
            request ? request->temporal_frames : 0);
    fprintf(file, "      \"pixel_count_per_frame\": %llu,\n",
            (unsigned long long)pixel_count);
    fprintf(file, "      \"integrator_3d\": ");
    RayTracingJsonWriteString(
        file,
        request ? ray_tracing_agent_render_request_integrator_label(
                      request->integrator_3d)
                : "");
    fprintf(file, "\n");
    fprintf(file, "    },\n");
    fprintf(file, "    \"render_request_snapshot_state\": {\n");
    fprintf(file, "      \"snapshot_type\": ");
    RayTracingJsonWriteString(file, "RuntimeNative3DRenderRequestSnapshot");
    fprintf(file, ",\n");
    fprintf(file, "      \"valid\": %s,\n", snapshot.valid ? "true" : "false");
    fprintf(file, "      \"generation_bound\": %s,\n",
            snapshot.generationBound ? "true" : "false");
    fprintf(file, "      \"generation\": %llu,\n",
            (unsigned long long)snapshot.generation);
    fprintf(file, "      \"dimensions\": {\n");
    fprintf(file, "        \"output_width\": %d,\n", snapshot.outputWidth);
    fprintf(file, "        \"output_height\": %d,\n", snapshot.outputHeight);
    fprintf(file, "        \"render_width\": %d,\n", snapshot.renderWidth);
    fprintf(file, "        \"render_height\": %d,\n", snapshot.renderHeight);
    fprintf(file, "        \"host_width\": %d,\n", snapshot.hostWidth);
    fprintf(file, "        \"host_height\": %d\n", snapshot.hostHeight);
    fprintf(file, "      },\n");
    fprintf(file, "      \"sampling\": {\n");
    fprintf(file, "        \"sampling_bound\": %s,\n",
            snapshot.samplingBound ? "true" : "false");
    fprintf(file, "        \"frame_index\": %d,\n", snapshot.frameIndex);
    fprintf(file, "        \"frame_count\": %d,\n", snapshot.frameCount);
    fprintf(file, "        \"temporal_frames\": %d,\n", snapshot.temporalFrames);
    fprintf(file, "        \"tile_size\": %d,\n", snapshot.tileSize);
    fprintf(file, "        \"sample_sequence\": %u,\n",
            (unsigned)snapshot.sampling.sampleSequence);
    fprintf(file, "        \"temporal_subpass_count\": %u,\n",
            (unsigned)snapshot.sampling.temporalSubpassCount);
    fprintf(file, "        \"integrator_3d\": ");
    RayTracingJsonWriteString(
        file,
        ray_tracing_agent_render_request_integrator_label(snapshot.integratorId));
    fprintf(file, "\n");
    fprintf(file, "      },\n");
    fprintf(file, "      \"resource_budget\": {\n");
    fprintf(file, "        \"bound\": %s,\n",
            snapshot.resourceBudgetBound ? "true" : "false");
    fprintf(file, "        \"cpu_percent\": %d,\n", snapshot.resourceBudget.cpuPercent);
    fprintf(file, "        \"max_worker_threads\": %d,\n",
            snapshot.resourceBudget.maxWorkerThreads);
    fprintf(file, "        \"reserve_cpu_count\": %d\n",
            snapshot.resourceBudget.reserveCpuCount);
    fprintf(file, "      },\n");
    fprintf(file, "      \"scene_snapshot\": {\n");
    fprintf(file, "        \"prepared_frame_bound\": %s,\n",
            snapshot.preparedFrameBound ? "true" : "false");
    fprintf(file, "        \"prepared_frame_valid\": %s,\n",
            snapshot.preparedFrameValid ? "true" : "false");
    fprintf(file, "        \"prepared_frame_width\": %d,\n",
            snapshot.preparedFrameWidth);
    fprintf(file, "        \"prepared_frame_height\": %d,\n",
            snapshot.preparedFrameHeight);
    fprintf(file, "        \"prepared_primitive_count\": %llu,\n",
            (unsigned long long)snapshot.preparedPrimitiveCount);
    fprintf(file, "        \"prepared_triangle_count\": %llu,\n",
            (unsigned long long)snapshot.preparedTriangleCount);
    fprintf(file, "        \"material_snapshot_bound\": %s,\n",
            snapshot.materialSnapshotBound ? "true" : "false");
    fprintf(file, "        \"material_count\": %llu,\n",
            (unsigned long long)snapshot.materialCount);
    fprintf(file, "        \"material_object_binding_count\": %llu,\n",
            (unsigned long long)snapshot.materialObjectBindingCount);
    fprintf(file, "        \"light_snapshot_bound\": %s,\n",
            snapshot.lightSnapshotBound ? "true" : "false");
    fprintf(file, "        \"enabled_light_count\": %llu,\n",
            (unsigned long long)snapshot.enabledLightCount);
    fprintf(file, "        \"material_emitter_light_count\": %llu\n",
            (unsigned long long)snapshot.materialEmitterLightCount);
    fprintf(file, "      },\n");
    fprintf(file, "      \"acceleration_snapshot\": {\n");
    fprintf(file, "        \"scene_acceleration_bound\": %s,\n",
            snapshot.sceneAccelerationBound ? "true" : "false");
    fprintf(file, "        \"trace_route\": ");
    RayTracingJsonWriteString(file, RuntimeRay3DTraceRouteLabel(snapshot.traceRoute));
    fprintf(file, ",\n");
    fprintf(file, "        \"tlas_instance_count\": %llu,\n",
            (unsigned long long)snapshot.tlasInstanceCount);
    fprintf(file, "        \"tlas_node_count\": %llu,\n",
            (unsigned long long)snapshot.tlasNodeCount);
    fprintf(file, "        \"trace_context_callback_bound\": %s\n",
            snapshot.traceContextCallbackBound ? "true" : "false");
    fprintf(file, "      },\n");
    fprintf(file, "      \"volume_water_snapshot\": {\n");
    fprintf(file, "        \"volume_enabled\": %s,\n",
            snapshot.volumeEnabled ? "true" : "false");
    fprintf(file, "        \"volume_attached\": %s,\n",
            snapshot.volumeAttached ? "true" : "false");
    fprintf(file, "        \"volume_frame_selection_dynamic\": %s,\n",
            snapshot.volumeFrameSelectionDynamic ? "true" : "false");
    fprintf(file, "        \"water_surface_source_found\": %s,\n",
            snapshot.waterSurfaceSourceFound ? "true" : "false");
    fprintf(file, "        \"water_surface_loaded\": %s,\n",
            snapshot.waterSurfaceLoaded ? "true" : "false");
    fprintf(file, "        \"water_surface_mesh_attached\": %s,\n",
            snapshot.waterSurfaceMeshAttached ? "true" : "false");
    fprintf(file, "        \"water_surface_frame_selection_dynamic\": %s,\n",
            snapshot.waterSurfaceFrameSelectionDynamic ? "true" : "false");
    fprintf(file, "        \"water_surface_sample_count\": %llu,\n",
            (unsigned long long)snapshot.waterSurfaceSampleCount);
    fprintf(file, "        \"water_surface_triangle_count\": %d\n",
            snapshot.waterSurfaceTriangleCount);
    fprintf(file, "      },\n");
    fprintf(file, "      \"diagnostics_and_output_snapshot\": {\n");
    fprintf(file, "        \"frame_dataflow_ledger_enabled\": %s,\n",
            snapshot.frameDataflowLedgerEnabled ? "true" : "false");
    fprintf(file, "        \"output_root_bound\": %s,\n",
            snapshot.outputRootBound ? "true" : "false");
    fprintf(file, "        \"summary_destination_bound\": %s,\n",
            snapshot.summaryDestinationBound ? "true" : "false");
    fprintf(file, "        \"progress_destination_bound\": %s\n",
            snapshot.progressDestinationBound ? "true" : "false");
    fprintf(file, "      },\n");
    fprintf(file, "      \"cancellation_snapshot\": {\n");
    fprintf(file, "        \"cancel_token_type\": ");
    RayTracingJsonWriteString(file, "RuntimeNative3DTileSchedulerCancelToken");
    fprintf(file, ",\n");
    fprintf(file, "        \"cancel_token_bound\": %s,\n",
            snapshot.cancelTokenBound ? "true" : "false");
    fprintf(file, "        \"cancel_generation\": %llu\n",
            (unsigned long long)snapshot.cancelGeneration);
    fprintf(file, "      }\n");
    fprintf(file, "    },\n");
    fprintf(file, "    \"prepare_and_scene_state\": {\n");
    fprintf(file, "      \"prepared_frame\": %s,\n",
            preflight && preflight->prepared_frame ? "true" : "false");
    fprintf(file, "      \"runtime_scene_apply_ms\": %.6f,\n",
            preflight ? preflight->runtime_scene_apply_ms : 0.0);
    fprintf(file, "      \"runtime_scene_preflight_ms\": %.6f,\n",
            preflight ? preflight->runtime_scene_preflight_ms : 0.0);
    fprintf(file, "      \"native_prepare_frame_ms\": %.6f,\n",
            preflight ? preflight->native_prepare_frame_ms : 0.0);
    fprintf(file, "      \"caustic_cache_prep_ms\": %.6f,\n",
            preflight ? preflight->caustic_cache_prep_ms : 0.0);
    fprintf(file, "      \"prepared_scene_cache_valid\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.valid ? "true" : "false");
    fprintf(file, "      \"prepared_scene_cache_hits\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.hits : 0u));
    fprintf(file, "      \"prepared_scene_cache_misses\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.misses : 0u));
    fprintf(file, "      \"cached_primitive_count\": %d,\n",
            preflight ? preflight->prepared_scene_cache_stats.cachedPrimitiveCount : 0);
    fprintf(file, "      \"cached_triangle_count\": %d,\n",
            preflight ? preflight->prepared_scene_cache_stats.cachedTriangleCount : 0);
    fprintf(file, "      \"prepared_frame_triangle_count\": %d,\n",
            preflight ? preflight->bvh_build_stats.triangleCount : 0);
    fprintf(file, "      \"estimated_cached_scene_bytes\": %llu,\n",
            (unsigned long long)estimated_cached_scene_bytes);
    fprintf(file, "      \"estimated_prepared_frame_scene_bytes\": %llu,\n",
            (unsigned long long)estimated_prepared_scene_bytes);
    fprintf(file, "      \"prepared_scene_copy_bind_dataflow\": {\n");
    fprintf(file, "        \"stats_enabled\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.dataflowStatsEnabled ? "true" : "false");
    fprintf(file, "        \"prepare_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.prepareCalls : 0u));
    fprintf(file, "        \"cache_hit_prepare_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.cacheHitPrepareCalls : 0u));
    fprintf(file, "        \"cache_miss_prepare_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.cacheMissPrepareCalls : 0u));
    fprintf(file, "        \"copy_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.copyCalls : 0u));
    fprintf(file, "        \"cache_hit_copy_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.cacheHitCopyCalls : 0u));
    fprintf(file, "        \"cache_miss_copy_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.cacheMissCopyCalls : 0u));
    fprintf(file, "        \"bind_after_copy_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.bindAfterCopyCalls : 0u));
    fprintf(file, "        \"final_frame_bind_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.finalFrameBindCalls : 0u));
    fprintf(file, "        \"frame_bvh_ensure_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.frameBVHEnsureCalls : 0u));
    fprintf(file, "        \"frame_bvh_build_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.frameBVHBuildCalls : 0u));
    fprintf(file, "        \"frame_bvh_already_ready_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.frameBVHAlreadyReadyCalls : 0u));
    fprintf(file, "        \"frame_bvh_skip_for_tlas_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.frameBVHSkipForTLASCalls : 0u));
    fprintf(file, "        \"frame_bvh_tlas_readiness_checks\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.frameBVHTLASReadinessChecks : 0u));
    fprintf(file, "        \"scene_build_ms_total\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.sceneBuildMsTotal : 0.0);
    fprintf(file, "        \"cache_store_ms_total\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.cacheStoreMsTotal : 0.0);
    fprintf(file, "        \"copy_ms_total\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.copyMsTotal : 0.0);
    fprintf(file, "        \"cache_hit_copy_ms_total\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.cacheHitCopyMsTotal : 0.0);
    fprintf(file, "        \"cache_miss_copy_ms_total\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.cacheMissCopyMsTotal : 0.0);
    fprintf(file, "        \"bind_after_copy_ms_total\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.bindAfterCopyMsTotal : 0.0);
    fprintf(file, "        \"final_frame_bind_ms_total\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.finalFrameBindMsTotal : 0.0);
    fprintf(file, "        \"frame_bvh_ensure_ms_total\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.frameBVHEnsureMsTotal : 0.0);
    fprintf(file, "        \"frame_bvh_tlas_readiness_bind_ms_total\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.frameBVHTLASReadinessBindMsTotal : 0.0);
    fprintf(file, "        \"last_prepare_valid\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.lastPrepareValid ? "true" : "false");
    fprintf(file, "        \"last_prepare_cache_hit\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.lastPrepareCacheHit ? "true" : "false");
    fprintf(file, "        \"last_prepare_cache_miss\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.lastPrepareCacheMiss ? "true" : "false");
    fprintf(file, "        \"last_prepare_copied_scene\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.lastPrepareCopiedScene ? "true" : "false");
        fprintf(file, "        \"flattened_bvh_skip_on_tlas_enabled\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.flattenedBVHSkipOnTLASEnabled ? "true" : "false");
    fprintf(file, "        \"flattened_bvh_skip_on_tlas_default_enabled\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.flattenedBVHSkipOnTLASDefaultEnabled ? "true" : "false");
    fprintf(file, "        \"flattened_bvh_skip_on_tlas_force_enabled\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.flattenedBVHSkipOnTLASForceEnabled ? "true" : "false");
    fprintf(file, "        \"flattened_bvh_skip_on_tlas_default_disabled\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.flattenedBVHSkipOnTLASDefaultDisabled ? "true" : "false");
    fprintf(file, "        \"last_frame_bvh_required\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.lastFrameBVHRequired ? "true" : "false");
    fprintf(file, "        \"last_frame_bvh_skipped_for_tlas\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.lastFrameBVHSkippedForTLAS ? "true" : "false");
    fprintf(file, "        \"last_frame_bvh_ready\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.lastFrameBVHReady ? "true" : "false");
    fprintf(file, "        \"last_tlas_ready_for_frame_bvh_skip\": %s,\n",
            preflight && preflight->prepared_scene_cache_stats.lastTLASReadyForFrameBVHSkip ? "true" : "false");
    fprintf(file, "        \"last_frame_bvh_skip_decision\": ");
    RayTracingJsonWriteString(
        file,
        ray_tracing_frame_dataflow_bvh_skip_decision_label(
            preflight
                ? preflight->prepared_scene_cache_stats.lastFrameBVHSkipDecision
                : RUNTIME_NATIVE_3D_FRAME_BVH_SKIP_DECISION_NOT_REQUESTED));
    fprintf(file, ",\n");
    fprintf(file, "        \"last_scene_build_ms\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastSceneBuildMs : 0.0);
    fprintf(file, "        \"last_cache_store_ms\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastCacheStoreMs : 0.0);
    fprintf(file, "        \"last_copy_ms\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastCopyMs : 0.0);
    fprintf(file, "        \"last_bind_after_copy_ms\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastBindAfterCopyMs : 0.0);
    fprintf(file, "        \"last_final_frame_bind_ms\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastFinalFrameBindMs : 0.0);
    fprintf(file, "        \"last_frame_bvh_ensure_ms\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastFrameBVHEnsureMs : 0.0);
    fprintf(file, "        \"last_frame_bvh_tlas_readiness_bind_ms\": %.6f,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastFrameBVHTLASReadinessBindMs : 0.0);
    fprintf(file, "        \"last_copied_primitive_count\": %d,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastCopiedPrimitiveCount : 0);
    fprintf(file, "        \"last_copied_triangle_count\": %d,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastCopiedTriangleCount : 0);
    fprintf(file, "        \"last_copied_bvh_node_count\": %d,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastCopiedBVHNodeCount : 0);
    fprintf(file, "        \"last_copied_light_count\": %d,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastCopiedLightCount : 0);
    fprintf(file, "        \"last_copied_emissive_candidate_count\": %d,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastCopiedEmissiveCandidateCount : 0);
    fprintf(file, "        \"last_frame_bvh_triangle_count\": %d,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastFrameBVHTriangleCount : 0);
    fprintf(file, "        \"last_frame_bvh_node_count\": %d,\n",
            preflight ? preflight->prepared_scene_cache_stats.lastFrameBVHNodeCount : 0);
    fprintf(file, "        \"last_frame_bvh_total_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.lastFrameBVHTotalBytes : 0u));
    fprintf(file, "        \"last_copied_primitive_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.lastCopiedPrimitiveBytes : 0u));
    fprintf(file, "        \"last_copied_triangle_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.lastCopiedTriangleBytes : 0u));
    fprintf(file, "        \"last_copied_bvh_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.lastCopiedBVHBytes : 0u));
    fprintf(file, "        \"last_copied_light_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.lastCopiedLightBytes : 0u));
    fprintf(file, "        \"last_copied_emissive_candidate_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.lastCopiedEmissiveCandidateBytes : 0u));
    fprintf(file, "        \"last_copied_estimated_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.lastCopiedEstimatedBytes : 0u));
    fprintf(file, "        \"total_copied_estimated_bytes\": %llu\n",
            (unsigned long long)(preflight ? preflight->prepared_scene_cache_stats.totalCopiedEstimatedBytes : 0u));
    fprintf(file, "      }\n");
    fprintf(file, "    },\n");
    fprintf(file, "    \"acceleration_state\": {\n");
    fprintf(file, "      \"prepared_accel_enabled\": %s,\n",
            preflight && preflight->scene_acceleration_stats.enabled ? "true" : "false");
    fprintf(file, "      \"blas_prepare_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->scene_acceleration_stats.blasPrepareCalls : 0u));
    fprintf(file, "      \"blas_cache_hits\": %llu,\n",
            (unsigned long long)(preflight ? preflight->scene_acceleration_stats.blasCacheHits : 0u));
    fprintf(file, "      \"blas_cache_misses\": %llu,\n",
            (unsigned long long)(preflight ? preflight->scene_acceleration_stats.blasCacheMisses : 0u));
    fprintf(file, "      \"blas_full_rebuilds\": %llu,\n",
            (unsigned long long)(preflight ? preflight->scene_acceleration_stats.blasFullRebuilds : 0u));
    fprintf(file, "      \"tlas_instance_count\": %llu,\n",
            (unsigned long long)(preflight ? preflight->scene_acceleration_stats.tlasInstanceCount : 0u));
    fprintf(file, "      \"tlas_node_count\": %llu,\n",
            (unsigned long long)(preflight ? preflight->scene_acceleration_stats.tlasNodeCount : 0u));
    fprintf(file, "      \"tlas_rebuilds\": %llu,\n",
            (unsigned long long)(preflight ? preflight->scene_acceleration_stats.tlasRebuilds : 0u));
    fprintf(file, "      \"tlas_refits\": %llu,\n",
            (unsigned long long)(preflight ? preflight->scene_acceleration_stats.tlasRefits : 0u));
    fprintf(file, "      \"tlas_build_ms\": %.6f,\n",
            preflight ? preflight->scene_acceleration_stats.tlasBuildMs : 0.0);
    fprintf(file, "      \"tlas_bind_ms\": %.6f,\n",
            preflight ? preflight->scene_acceleration_stats.tlasBindMs : 0.0);
    fprintf(file, "      \"trace_route\": ");
    RayTracingJsonWriteString(
        file,
        preflight ? RuntimeRay3DTraceRouteLabel(
                        preflight->ray_trace_route_stats.activeRoute)
                  : "");
    fprintf(file, ",\n");
    fprintf(file, "      \"trace_context_stats_owned\": %s,\n",
            preflight && preflight->ray_trace_route_stats.traceContextStatsOwned ? "true" : "false");
    fprintf(file, "      \"trace_context_callback_bound\": %s,\n",
            preflight && preflight->ray_trace_route_stats.sceneAccelerationTraceCallbackBound ? "true" : "false");
    fprintf(file, "      \"route_trace_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->ray_trace_route_stats.traceCalls : 0u));
    fprintf(file, "      \"route_tlas_trace_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->ray_trace_route_stats.tlasTraceCalls : 0u));
    fprintf(file, "      \"route_flattened_trace_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->ray_trace_route_stats.flattenedTraceCalls : 0u));
    fprintf(file, "      \"route_flattened_fallback_calls\": %llu\n",
            (unsigned long long)(preflight ? preflight->ray_trace_route_stats.flattenedFallbackCalls : 0u));
    fprintf(file, "    },\n");
    fprintf(file, "    \"scratch_and_output_estimates\": {\n");
    fprintf(file, "      \"render_pixel_buffer_bytes_per_frame\": %llu,\n",
            (unsigned long long)render_pixel_buffer_bytes);
    fprintf(file, "      \"radiance_buffer_bytes_per_full_frame\": %llu,\n",
            (unsigned long long)radiance_buffer_bytes);
    fprintf(file, "      \"estimated_output_pixel_bytes_all_frames\": %llu,\n",
            (unsigned long long)total_output_pixel_bytes);
    fprintf(file, "      \"estimated_radiance_buffer_bytes_all_frames\": %llu,\n",
            (unsigned long long)total_radiance_buffer_bytes);
    fprintf(file, "      \"bvh_total_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->bvh_build_stats.totalBytes : 0u));
    fprintf(file, "      \"bvh_node_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->bvh_build_stats.nodeBytes : 0u));
    fprintf(file, "      \"bvh_index_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->bvh_build_stats.indexBytes : 0u));
    fprintf(file, "      \"render_unit_scratch_state\": {\n");
    fprintf(file, "        \"reuse_disable_env\": ");
    RayTracingJsonWriteString(file, "RAY_TRACING_NATIVE3D_DISABLE_RENDER_UNIT_SCRATCH_REUSE");
    fprintf(file, ",\n");
    fprintf(file, "        \"owner_count\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitScratchOwnerCount : 0u));
    fprintf(file, "        \"setup_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitScratchSetupCalls : 0u));
    fprintf(file, "        \"cache_acquire_hits\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitScratchCacheAcquireHits : 0u));
    fprintf(file, "        \"cache_acquire_misses\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitScratchCacheAcquireMisses : 0u));
    fprintf(file, "        \"radiance_resize_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitRadianceScratchResizeCalls : 0u));
    fprintf(file, "        \"radiance_reuse_calls\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitRadianceScratchReuseCalls : 0u));
    fprintf(file, "        \"radiance_clear_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitRadianceScratchClearBytes : 0u));
    fprintf(file, "        \"radiance_requested_bytes_max\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitRadianceScratchRequestedBytesMax : 0u));
    fprintf(file, "        \"radiance_capacity_bytes_max\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitRadianceScratchCapacityBytesMax : 0u));
    fprintf(file, "        \"temporal_capacity_bytes_max\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitTemporalScratchCapacityBytesMax : 0u));
    fprintf(file, "        \"adaptive_mask_capacity_bytes_max\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitAdaptiveMaskScratchCapacityBytesMax : 0u));
    fprintf(file, "        \"adaptive_state_capacity_bytes_max\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitAdaptiveStateScratchCapacityBytesMax : 0u));
    fprintf(file, "        \"feature_capacity_bytes_max\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitFeatureScratchCapacityBytesMax : 0u));
    fprintf(file, "        \"owned_bytes_observed_total\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitScratchOwnedBytes : 0u));
    fprintf(file, "        \"owned_bytes_max_per_unit\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitScratchMaxOwnerBytes : 0u));
    fprintf(file, "        \"owned_bytes_max_frame\": %llu\n",
            (unsigned long long)(preflight ? preflight->stats.renderUnitScratchMaxFrameOwnedBytes : 0u));
    fprintf(file, "      }\n");
    fprintf(file, "    },\n");
    fprintf(file, "    \"tile_and_temporal_state\": {\n");
    fprintf(file, "      \"planned_parent_tile_count\": %d,\n",
            preflight ? preflight->stats.temporalPlannedParentTileCount : 0);
    fprintf(file, "      \"emitted_tile_job_count\": %d,\n",
            preflight ? preflight->stats.temporalEmittedTileJobCount : 0);
    fprintf(file, "      \"dispatched_tile_job_count\": %d,\n",
            preflight ? preflight->stats.temporalDispatchedTileJobCount : 0);
    fprintf(file, "      \"completed_tile_job_count\": %d,\n",
            preflight ? preflight->stats.temporalCompletedTileJobCount : 0);
    fprintf(file, "      \"occupancy_skipped_tile_count\": %d,\n",
            preflight ? preflight->stats.temporalOccupancySkippedTileCount : 0);
    fprintf(file, "      \"committed_subpasses\": %d,\n",
            preflight ? preflight->stats.temporalCommittedSubpasses : 0);
    fprintf(file, "      \"pixels_rendered\": %d,\n",
            preflight ? preflight->stats.temporalPixelsRendered : 0);
    fprintf(file, "      \"pixels_skipped\": %d,\n",
            preflight ? preflight->stats.temporalPixelsSkipped : 0);
    fprintf(file, "      \"total_tile_ms\": %.6f,\n",
            preflight ? preflight->stats.temporalTotalTileMs : 0.0);
    fprintf(file, "      \"average_tile_ms\": %.6f,\n",
            preflight ? preflight->stats.temporalAverageTileMs : 0.0);
    fprintf(file, "      \"max_tile_ms\": %.6f,\n",
            preflight ? preflight->stats.temporalMaxTileMs : 0.0);
    fprintf(file, "      \"scheduler_lifetime_and_cancellation\": {\n");
    fprintf(file, "        \"cancel_token_type\": ");
    RayTracingJsonWriteString(file, "RuntimeNative3DTileSchedulerCancelToken");
    fprintf(file, ",\n");
    fprintf(file, "        \"job_array_owners\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerJobArrayOwnerCount : 0);
    fprintf(file, "        \"parent_metric_array_owners\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerParentMetricArrayOwnerCount : 0);
    fprintf(file, "        \"progress_tile_array_owners\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerProgressTileArrayOwnerCount : 0);
    fprintf(file, "        \"completion_queue_owners\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerCompletionQueueOwnerCount : 0);
    fprintf(file, "        \"worker_pool_owners\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerWorkerPoolOwnerCount : 0);
    fprintf(file, "        \"cancel_token_bound\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerCancelTokenBound : 0);
    fprintf(file, "        \"cancel_checks\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerCancelCheckCount : 0);
    fprintf(file, "        \"cancel_requested\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerCancelRequestedCount : 0);
    fprintf(file, "        \"cancel_before_dispatch\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerCancelBeforeDispatchCount : 0);
    fprintf(file, "        \"cancel_during_wait\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerCancelDuringWaitCount : 0);
    fprintf(file, "        \"cancel_before_final_resolve\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerCancelBeforeFinalResolveCount : 0);
    fprintf(file, "        \"final_resolve_blocked_by_cancel\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerFinalResolveBlockedByCancelCount : 0);
    fprintf(file, "        \"worker_drain_shutdowns\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerWorkerDrainShutdownCount : 0);
    fprintf(file, "        \"worker_cancel_shutdowns\": %d,\n",
            preflight ? preflight->stats.temporalTileSchedulerWorkerCancelShutdownCount : 0);
    fprintf(file, "        \"cancel_generation\": %llu\n",
            (unsigned long long)(preflight ? preflight->stats.temporalTileSchedulerCancelGeneration : 0u));
    fprintf(file, "      }\n");
    fprintf(file, "    },\n");
    fprintf(file, "    \"presentation_and_output_state\": {\n");
    fprintf(file, "      \"pixel_stride_bytes\": %d,\n",
            RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
    fprintf(file, "      \"requested_output_frames\": %llu,\n",
            (unsigned long long)frame_count);
    fprintf(file, "      \"written_output_frames\": %llu,\n",
            (unsigned long long)frames_rendered);
    fprintf(file, "      \"render_pixel_buffer_bytes_per_frame\": %llu,\n",
            (unsigned long long)render_pixel_buffer_bytes);
    fprintf(file, "      \"requested_output_pixel_bytes_total\": %llu,\n",
            (unsigned long long)total_output_pixel_bytes);
    fprintf(file, "      \"measured_output_pixel_bytes_written\": %llu,\n",
            (unsigned long long)measured_output_pixel_bytes);
    fprintf(file, "      \"final_full_resolve_count\": %d,\n",
            preflight ? preflight->stats.temporalFinalFullResolveCount : 0);
    fprintf(file, "      \"host_full_resolve_count\": %d,\n",
            preflight ? preflight->stats.temporalHostFullResolveCount : 0);
    fprintf(file, "      \"dirty_preview_present_count\": %d,\n",
            preflight ? preflight->stats.temporalDirtyPreviewPresentCount : 0);
    fprintf(file, "      \"final_preview_present_count\": %d,\n",
            preflight ? preflight->stats.temporalFinalPreviewPresentCount : 0);
    fprintf(file, "      \"dirty_preview_host_pixels\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.temporalDirtyPreviewHostPixels : 0u));
    fprintf(file, "      \"dirty_preview_host_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.temporalDirtyPreviewHostBytes : 0u));
    fprintf(file, "      \"final_resolve_host_pixels\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.temporalFinalResolveHostPixels : 0u));
    fprintf(file, "      \"final_resolve_host_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.temporalFinalResolveHostBytes : 0u));
    fprintf(file, "      \"history_seed_host_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.temporalHistorySeedHostBytes : 0u));
    fprintf(file, "      \"history_promote_host_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.temporalHistoryPromoteHostBytes : 0u));
    fprintf(file, "      \"final_preview_present_host_bytes\": %llu,\n",
            (unsigned long long)(preflight ? preflight->stats.temporalFinalPreviewPresentHostBytes : 0u));
    fprintf(file, "      \"measured_preview_host_movement_bytes\": %llu,\n",
            (unsigned long long)preview_host_movement_bytes);
    fprintf(file, "      \"frame_write_ms\": %.6f,\n",
            preflight ? preflight->frame_write_ms : 0.0);
    fprintf(file, "      \"nonzero_pixels_last_frame\": %llu,\n",
            (unsigned long long)(preflight ? preflight->nonzero_pixels : 0u));
    fprintf(file, "      \"max_rgb_last_frame\": [%u, %u, %u]\n",
            preflight ? (unsigned)preflight->max_r : 0u,
            preflight ? (unsigned)preflight->max_g : 0u,
            preflight ? (unsigned)preflight->max_b : 0u);
    fprintf(file, "    },\n");
    fprintf(file, "    \"known_global_state_sources\": {\n");
    fprintf(file, "      \"prepared_scene_cache\": true,\n");
    fprintf(file, "      \"scene_acceleration_tlas\": true,\n");
    fprintf(file, "      \"ray_trace_route_stats\": true,\n");
    fprintf(file, "      \"render_trace_cost_ledger\": true\n");
    fprintf(file, "    }\n");
    fprintf(file, "  },\n");
}
