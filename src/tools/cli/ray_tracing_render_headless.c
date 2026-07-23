#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app/agent_render_request.h"
#include "config/config_file_io.h"
#include "app/animation.h"
#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"
#include "render/integrators/integrator_common.h"
#include "render/pipeline/ray_tracing2_native3d_overlay.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"
#include "render/runtime_caustic_bootstrap_3d.h"
#include "render/runtime_caustic_transport_3d.h"
#include "render/runtime_caustic_transport_debug_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"
#include "render/runtime_light_radiometry_3d.h"
#include "render/runtime_volume_3d_debug.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_scatter.h"
#include "tools/ray_tracing_render_headless_internal.h"

static void ray_tracing_headless_note_registered_lights(
    RayTracingHeadlessPreflight *preflight,
    const RuntimeNative3DPreparedFrame *frame) {
    if (!preflight || !frame) return;
    preflight->registered_light_count = frame->scene.lightSet.lightCount;
    preflight->registered_enabled_light_count = frame->scene.lightSet.enabledCount;
    for (int i = 0; i < frame->scene.lightSet.lightCount; ++i) {
        const RuntimeLightSource3D* source = &frame->scene.lightSet.lights[i];
        RuntimeLightRadiometry3DEvaluation radiometry = {0};
        const bool physical_radiometry =
            RuntimeLightRadiometry3D_Evaluate(source, &radiometry);
        if (i == 0) {
            preflight->registered_light_first_position_x = source->position.x;
            preflight->registered_light_first_position_y = source->position.y;
            preflight->registered_light_first_position_z = source->position.z;
            preflight->registered_light_first_radius = source->radius;
            preflight->registered_light_first_intensity = source->intensity;
            preflight->registered_light_first_radiance = source->radiance;
            preflight->registered_light_first_total_power_r =
                radiometry.totalEmittedPower.x;
            preflight->registered_light_first_total_power_g =
                radiometry.totalEmittedPower.y;
            preflight->registered_light_first_total_power_b =
                radiometry.totalEmittedPower.z;
            preflight->registered_light_first_color_r = source->color.x;
            preflight->registered_light_first_color_g = source->color.y;
            preflight->registered_light_first_color_b = source->color.z;
        }
        switch (source->kind) {
            case RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE:
                preflight->registered_light_sphere_count += 1;
                break;
            case RUNTIME_LIGHT_SOURCE_3D_KIND_DISK:
                preflight->registered_light_disk_count += 1;
                break;
            case RUNTIME_LIGHT_SOURCE_3D_KIND_RECT:
                preflight->registered_light_rect_count += 1;
                break;
            case RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE:
                preflight->registered_light_mesh_emissive_count += 1;
                break;
            case RUNTIME_LIGHT_SOURCE_3D_KIND_POINT:
            default:
                preflight->registered_light_point_count += 1;
                break;
        }
        switch (source->origin) {
            case RUNTIME_LIGHT_SOURCE_3D_ORIGIN_COMPAT_SCENE_LIGHT:
                preflight->registered_light_compatibility_count += 1;
                break;
            case RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER:
                preflight->registered_light_material_emitter_count += 1;
                if (source->enabled) {
                    preflight->registered_light_material_emitter_enabled_count += 1;
                }
                break;
            case RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT:
            default:
                preflight->registered_light_authored_count += 1;
                break;
        }
        switch (source->emissionProfile) {
            case RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED:
                preflight->registered_light_emission_one_sided_count += 1;
                break;
            case RUNTIME_LIGHT_SOURCE_3D_EMISSION_TWO_SIDED:
                preflight->registered_light_emission_two_sided_count += 1;
                break;
            case RUNTIME_LIGHT_SOURCE_3D_EMISSION_OMNI:
            default:
                preflight->registered_light_emission_omni_count += 1;
                break;
        }
        if (physical_radiometry) {
            preflight->registered_light_radiometry_lambertian_count += 1;
        } else {
            preflight->registered_light_radiometry_legacy_count += 1;
        }
        if (source->meshAreaSamplerOnly) {
            preflight->registered_light_mesh_area_sampler_only_count += 1;
        }
        preflight->registered_light_emissive_candidate_count +=
            source->emissiveCandidateCount;
        preflight->registered_light_emissive_area += source->emissiveArea;
        preflight->registered_light_emissive_weight += source->emissiveWeight;
        if (source->emissiveProxyRadius >
            preflight->registered_light_emissive_proxy_radius_max) {
            preflight->registered_light_emissive_proxy_radius_max =
                source->emissiveProxyRadius;
        }
    }
}

static int run_preflight(const RayTracingAgentRenderRequest *request,
                         RayTracingHeadlessPreflight *out_preflight,
                         const char *job_status_path,
                         const char *job_id,
                         const char *request_path) {
    RuntimeNative3DPreparedFrame frame = {0};
    Point light_point = {0.0, 0.0};
    RayTracingHeadlessPreflight preflight = {0};
    struct timespec preflight_started_at = {0};
    struct timespec stage_started_at = {0};
    char progress_diag[256] = {0};

    if (!request || !out_preflight) return 2;
    (void)clock_gettime(CLOCK_MONOTONIC, &preflight_started_at);
    ray_tracing_runtime_mesh_assets_timing_reset();
    RuntimeScene3DBuilder_TimingReset();
    RuntimeDynamicGeometryAcceleration3D_ResetWaterCacheLifecycle();
    preflight.scene_acceleration_stats =
        RuntimeSceneAcceleration3DDiagnostics_Disabled();
    preflight.request_loaded = true;
    preflight.object_audit_enabled = request->object_audit_enabled;
    snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "ok");
    ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                  request,
                                  "loading_settings",
                                  request->start_frame,
                                  0,
                                  0,
                                  0,
                                  request->temporal_frames,
                                  0u,
                                  0u,
                                  0.0,
                                  -1.0,
                                  "running",
                                  "loading runtime settings",
                                  job_status_path,
                                  job_id,
                                  request_path,
                                  -1);

    LoadAllSettings();
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.interactiveMode = false;
    animSettings.integratorMode3D = (int)request->integrator_3d;
    animSettings.temporalFrames3D = request->temporal_frames;
    if (request->has_tiled_renderer_override) {
        animSettings.useTiledRenderer = request->tiled_renderer_override;
    }
    if (request->has_tile_size_override) {
        animSettings.tileSize = ClampTileSize(request->tile_size_override);
    }
    RuntimeNative3DAdaptiveSampling_SetRuntimeOverride(request->has_adaptive_sampling_override,
                                                       request->adaptive_sampling_enabled_override);
    if (request->has_denoise_enabled_override) {
        animSettings.disneyDenoiseEnabled = request->denoise_enabled_override;
    }
    preflight.denoise_enabled = animSettings.disneyDenoiseEnabled;
    sceneSettings.windowWidth = request->width;
    sceneSettings.windowHeight = request->height;

    ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                  request,
                                  "applying_runtime_scene",
                                  request->start_frame,
                                  0,
                                  0,
                                  0,
                                  request->temporal_frames,
                                  0u,
                                  0u,
                                  ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                  -1.0,
                                  "running",
                                  "applying runtime scene",
                                  job_status_path,
                                  job_id,
                                  request_path,
                                  -1);
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_started_at);
    preflight.scene_applied =
        AnimationSelectSceneSource(SCENE_SOURCE_RUNTIME_SCENE,
                                   request->runtime_scene_path,
                                   true);
    runtime_scene_motion_bridge_get_last_summary(&preflight.object_motion_summary);
    preflight.runtime_scene_apply_ms = ray_tracing_elapsed_ms_since(&stage_started_at);
    ray_tracing_runtime_mesh_assets_timing_snapshot(&preflight.mesh_asset_timing_stats);
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_started_at);
    runtime_scene_bridge_preflight_file(request->runtime_scene_path, &preflight.scene_summary);
    preflight.runtime_scene_preflight_ms = ray_tracing_elapsed_ms_since(&stage_started_at);
    if (!preflight.scene_applied) {
        snprintf(preflight.diagnostics,
                 sizeof(preflight.diagnostics),
                 "failed to apply runtime scene");
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "failed",
                                      request->start_frame,
                                      0,
                                      0,
                                      0,
                                      request->temporal_frames,
                                      0u,
                                      0u,
                                      0.0,
                                      -1.0,
                                      "failed",
                                      preflight.diagnostics,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      3);
        *out_preflight = preflight;
        return 3;
    }
    snprintf(progress_diag,
             sizeof(progress_diag),
             "runtime scene applied (%d objects, %d materials)",
             preflight.scene_summary.object_count,
             preflight.scene_summary.material_count);
    ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                  request,
                                  "runtime_scene_applied",
                                  request->start_frame,
                                  0,
                                  0,
                                  0,
                                  request->temporal_frames,
                                  0u,
                                  0u,
                                  ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                  -1.0,
                                  "running",
                                  progress_diag,
                                  job_status_path,
                                  job_id,
                                  request_path,
                                  -1);
    ray_tracing_headless_apply_inspection_overrides(request);

    if (request->volume_enabled) {
        animSettings.volumeAffectsLighting = request->volume_affects_lighting;
        animSettings.volumeDebugOverlayEnabled = request->volume_debug_overlay;
        if (request->volume_visible) {
            ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                          request,
                                          "attaching_volume",
                                          request->start_frame,
                                          0,
                                          0,
                                          0,
                                          request->temporal_frames,
                                          0u,
                                          0u,
                                          ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                          -1.0,
                                          "running",
                                          "attaching volume source",
                                          job_status_path,
                                          job_id,
                                          request_path,
                                          -1);
            preflight.volume_attached =
                AnimationSelectVolumeSource(request->volume_source_kind,
                                            request->volume_source_path,
                                            false);
            if (!preflight.volume_attached) {
                snprintf(preflight.diagnostics,
                         sizeof(preflight.diagnostics),
                         "failed to attach volume source");
                ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                              request,
                                              "failed",
                                              request->start_frame,
                                              0,
                                              0,
                                              0,
                                              request->temporal_frames,
                                              0u,
                                              0u,
                                              0.0,
                                              -1.0,
                                              "failed",
                                              preflight.diagnostics,
                                              job_status_path,
                                              job_id,
                                              request_path,
                                              4);
                *out_preflight = preflight;
                return 4;
            }
            if (!ray_tracing_headless_populate_volume_frame_selection(&preflight, request)) {
                ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                              request,
                                              "failed",
                                              request->start_frame,
                                              0,
                                              0,
                                              0,
                                              request->temporal_frames,
                                              0u,
                                              0u,
                                              0.0,
                                              -1.0,
                                              "failed",
                                              preflight.diagnostics,
                                              job_status_path,
                                              job_id,
                                              request_path,
                                              4);
                *out_preflight = preflight;
                return 4;
            }
        } else if (ray_tracing_headless_request_has_volume_source(request)) {
            animSettings.volumeInteractionEnabled = false;
            animSettings.volumeSourceKind =
                animation_config_volume_source_kind_clamp(request->volume_source_kind);
            snprintf(animSettings.volumeSourcePath,
                     sizeof(animSettings.volumeSourcePath),
                     "%s",
                     request->volume_source_path);
            preflight.volume_attached = false;
        }
        if (!ray_tracing_headless_populate_water_surface_frame_selection(&preflight, request)) {
            ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                          request,
                                          "failed",
                                          request->start_frame,
                                          0,
                                          0,
                                          0,
                                          request->temporal_frames,
                                          0u,
                                          0u,
                                          0.0,
                                          -1.0,
                                          "failed",
                                          preflight.diagnostics,
                                          job_status_path,
                                          job_id,
                                          request_path,
                                          4);
            *out_preflight = preflight;
            return 4;
        }
    } else {
        AnimationClearVolumeSource();
        preflight.volume_attached = false;
    }

    ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                  request,
                                  "resolving_native_route",
                                  request->start_frame,
                                  0,
                                  0,
                                  0,
                                  request->temporal_frames,
                                  0u,
                                  0u,
                                  ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                  -1.0,
                                  "running",
                                  "resolving native 3D route",
                                  job_status_path,
                                  job_id,
                                  request_path,
                                  -1);
    preflight.route = RayTracingModeBackend_ResolveRoute();
    preflight.route_native_3d = RayTracingModeBackend_IsNative3D(&preflight.route);
    if (!preflight.route_native_3d) {
        snprintf(preflight.diagnostics,
                 sizeof(preflight.diagnostics),
                 "native 3D route not ready");
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "failed",
                                      request->start_frame,
                                      0,
                                      0,
                                      0,
                                      request->temporal_frames,
                                      0u,
                                      0u,
                                      0.0,
                                      -1.0,
                                      "failed",
                                      preflight.diagnostics,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      5);
        *out_preflight = preflight;
        return 5;
    }

    if (sceneSettings.bezierPath.numPoints >= 1) {
        light_point = sceneSettings.bezierPath.points[0];
    }
    ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                  request,
                                  "preparing_native_frame",
                                  request->start_frame,
                                  0,
                                  0,
                                  0,
                                  request->temporal_frames,
                                  0u,
                                  0u,
                                  ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                  -1.0,
                                  "running",
                                  "preparing native frame and acceleration structures",
                                  job_status_path,
                                  job_id,
                                  request_path,
                                  -1);
    RuntimeScene3DBuilder_TimingReset();
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_started_at);
    preflight.prepared_frame =
        RuntimeNative3DPrepareFrameAtFrameIndex(&frame,
                                                request->width,
                                                request->height,
                                                request->normalized_t,
                                                request->start_frame,
                                                light_point.x,
                                                light_point.y);
    preflight.native_prepare_frame_ms = ray_tracing_elapsed_ms_since(&stage_started_at);
    RuntimeScene3DBuilder_TimingSnapshot(&preflight.scene_builder_timing_stats);
    RuntimeMeshBLASCache3D_SnapshotDiagnostics(&preflight.scene_acceleration_stats);
    RuntimeSceneAcceleration3D_AppendTLASDiagnostics(&preflight.scene_acceleration_stats);
    RuntimeRay3D_SnapshotRouteStats(&preflight.ray_trace_route_stats);
    RuntimeDynamicGeometryAcceleration3D_SnapshotWaterCacheDiagnostics(
        &preflight.dynamic_water_cache_stats);
    preflight.caustic_cache_prep_ms = frame.causticCachePrepMs;
    if (preflight.prepared_frame) {
        bool flattened_bvh_required =
            preflight.ray_trace_route_stats.requestedRoute !=
            RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS;
        bool prepared_tlas_ready =
            preflight.scene_acceleration_stats.enabled &&
            preflight.scene_acceleration_stats.tlasInstanceCount > 0u &&
            preflight.scene_acceleration_stats.tlasNodeCount > 0u;
        preflight.environment_summary = frame.scene.environment;
        preflight.environment_summary_built = true;
        ray_tracing_headless_note_registered_lights(&preflight, &frame);
        if (frame.causticPhotonRenderPrepReadbackBuilt) {
            preflight.causticPhotonCallsiteReadback =
                frame.causticPhotonRenderPrepReadback;
            preflight.causticPhotonCallsiteReadbackBuilt = true;
        }
        ray_tracing_headless_probe_caustic_photon_callsite(&preflight,
                                                          &frame,
                                                          request);
        ray_tracing_headless_probe_caustic_photon_trace_callsite(&preflight,
                                                                &frame,
                                                                request);
        RuntimeTriangleMesh3D_BVHBuildStats(&frame.scene.triangleMesh,
                                            &preflight.bvh_build_stats);
        if (flattened_bvh_required &&
            (!preflight.bvh_build_stats.ready ||
             preflight.bvh_build_stats.nodeCount <= 0)) {
            snprintf(preflight.diagnostics,
                     sizeof(preflight.diagnostics),
                     "native 3D BVH unavailable after prepare: triangles=%d nodes=%d",
                     preflight.bvh_build_stats.triangleCount,
                     preflight.bvh_build_stats.nodeCount);
            RuntimeNative3DPreparedFrame_Free(&frame);
            ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                          request,
                                          "bvh_failed",
                                          request->start_frame,
                                          0,
                                          0,
                                          0,
                                          request->temporal_frames,
                                          0u,
                                          0u,
                                          ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                          -1.0,
                                          "failed",
                                          preflight.diagnostics,
                                          job_status_path,
                                          job_id,
                                          request_path,
                                          6);
            *out_preflight = preflight;
            return 6;
        }
        if (!flattened_bvh_required && !prepared_tlas_ready) {
            snprintf(preflight.diagnostics,
                     sizeof(preflight.diagnostics),
                     "native 3D TLAS unavailable after prepare: instances=%llu nodes=%llu",
                     (unsigned long long)preflight.scene_acceleration_stats.tlasInstanceCount,
                     (unsigned long long)preflight.scene_acceleration_stats.tlasNodeCount);
            RuntimeNative3DPreparedFrame_Free(&frame);
            ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                          request,
                                          "tlas_failed",
                                          request->start_frame,
                                          0,
                                          0,
                                          0,
                                          request->temporal_frames,
                                          0u,
                                          0u,
                                          ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                          -1.0,
                                          "failed",
                                          preflight.diagnostics,
                                          job_status_path,
                                          job_id,
                                          request_path,
                                          6);
            *out_preflight = preflight;
            return 6;
        }
        ray_tracing_headless_note_water_surface_mesh(&preflight, &frame);
        snprintf(progress_diag,
                 sizeof(progress_diag),
                 "BVH ready (%d triangles, %d nodes, %.3f ms build)",
                 preflight.bvh_build_stats.triangleCount,
                 preflight.bvh_build_stats.nodeCount,
                 preflight.bvh_build_stats.buildCpuMs);
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "bvh_ready",
                                      request->start_frame,
                                      0,
                                      0,
                                      0,
                                      request->temporal_frames,
                                      0u,
                                      0u,
                                      ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                      -1.0,
                                      "running",
                                      progress_diag,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      -1);
        snprintf(progress_diag,
                 sizeof(progress_diag),
                 "auditing objects (max dimension %d)",
                 request->object_audit_enabled ? request->object_audit_max_dimension : 0);
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "object_audit",
                                      request->start_frame,
                                      0,
                                      0,
                                      0,
                                      request->temporal_frames,
                                      0u,
                                      0u,
                                      ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                      -1.0,
                                      "running",
                                      progress_diag,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      -1);
        (void)clock_gettime(CLOCK_MONOTONIC, &stage_started_at);
        ray_tracing_headless_audit_prepared_frame(&preflight, &frame, request);
        preflight.object_audit_ms = ray_tracing_elapsed_ms_since(&stage_started_at);
        snprintf(progress_diag,
                 sizeof(progress_diag),
                 "object audit ready (%d samples, stride %d)",
                 preflight.object_audit_sample_count,
                 preflight.object_audit_scale_factor);
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "object_audit_ready",
                                      request->start_frame,
                                      0,
                                      0,
                                      0,
                                      request->temporal_frames,
                                      0u,
                                      0u,
                                      ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                      -1.0,
                                      "running",
                                      progress_diag,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      -1);
        RuntimeNative3DPreparedFrame_Free(&frame);
        RuntimeNative3DPreparedSceneCacheStatsSnapshot(
            &preflight.prepared_scene_cache_stats);
    } else {
        snprintf(preflight.diagnostics,
                 sizeof(preflight.diagnostics),
                 "failed to prepare native 3D frame: %s",
                 RuntimeNative3DPrepareFrameLastDiagnostics());
        ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                      request,
                                      "failed",
                                      request->start_frame,
                                      0,
                                      0,
                                      0,
                                      request->temporal_frames,
                                      0u,
                                      0u,
                                      0.0,
                                      -1.0,
                                      "failed",
                                      preflight.diagnostics,
                                      job_status_path,
                                      job_id,
                                      request_path,
                                      6);
        *out_preflight = preflight;
        return 6;
    }

    ray_tracing_render_headless_write_progress_and_job_status(request->progress_path,
                                  request,
                                  "preflight_ready",
                                  request->start_frame,
                                  0,
                                  0,
                                  0,
                                  request->temporal_frames,
                                  0u,
                                  0u,
                                  ray_tracing_elapsed_seconds_since(&preflight_started_at),
                                  -1.0,
                                  "running",
                                  "preflight ready",
                                  job_status_path,
                                  job_id,
                                  request_path,
                                  -1);
    preflight.total_run_ms = ray_tracing_elapsed_ms_since(&preflight_started_at);
    *out_preflight = preflight;
    return 0;
}

static int run_render(const RayTracingAgentRenderRequest *request,
                      RayTracingHeadlessPreflight *out_preflight,
                      const char *job_status_path,
                      const char *job_id,
                      const char *request_path) {
    RayTracingHeadlessPreflight preflight = {0};
    uint8_t *pixels = NULL;
    double light_x = 0.0;
    double light_y = 0.0;
    int preflight_code = 0;
    int setup_code = 0;
    struct timespec render_started_at = {0};
    struct timespec stage_started_at = {0};

    if (!request || !out_preflight) return 2;
    (void)clock_gettime(CLOCK_MONOTONIC, &render_started_at);
    setup_code = ray_tracing_headless_validate_render_output_root(request, &preflight);
    if (setup_code != 0) {
        *out_preflight = preflight;
        return setup_code;
    }

    preflight_code = run_preflight(request, &preflight, job_status_path, job_id, request_path);
    if (preflight_code != 0) {
        *out_preflight = preflight;
        return preflight_code;
    }

    setup_code = ray_tracing_headless_prepare_frame_directory_and_buffer(request,
                                                                         &preflight,
                                                                         &pixels);
    if (setup_code != 0) {
        *out_preflight = preflight;
        return setup_code;
    }

    ray_tracing_headless_initial_light_point(&light_x, &light_y);
    ray_tracing_headless_reset_render_trace_state();
    if (request->render_trace_cost_ledger_enabled) {
        RuntimeRenderTraceCostLedger3D_SetEnabled(true);
    }

    for (int i = 0; i < request->frame_count; ++i) {
        char frame_path[PATH_MAX];
        RuntimeNative3DRenderStats stats = {0};
        RayTracingTemporalProgressContext temporal_progress = {0};
        const int frame_index = request->start_frame + i;
        const double t = ray_tracing_headless_frame_normalized_t(request, i);
        int frame_status = 0;

        frame_status = ray_tracing_headless_prepare_frame_output(frame_path,
                                                                 sizeof(frame_path),
                                                                 request,
                                                                 &preflight,
                                                                 frame_index,
                                                                 job_status_path,
                                                                 job_id,
                                                                 request_path);
        if (frame_status != 0) {
            free(pixels);
            *out_preflight = preflight;
            return frame_status;
        }
        ray_tracing_headless_note_rendering_frame_started(request,
                                                          &preflight,
                                                          frame_index,
                                                          job_status_path,
                                                          job_id,
                                                          request_path);

        temporal_progress.request = request;
        temporal_progress.job_status_path = job_status_path;
        temporal_progress.job_id = job_id;
        temporal_progress.request_path = request_path;
        temporal_progress.frame_index = frame_index;
        temporal_progress.frames_completed = preflight.frames_rendered;
        temporal_progress.total_subpasses = request->temporal_frames;
        (void)clock_gettime(CLOCK_MONOTONIC, &temporal_progress.frame_started_at);

        RuntimeNative3DResourceBudget resource_budget = {0};
        const RuntimeNative3DResourceBudget *active_resource_budget =
            ray_tracing_headless_request_resource_budget(request, &resource_budget);

        (void)clock_gettime(CLOCK_MONOTONIC, &stage_started_at);
        if (!RuntimeNative3DRenderToPixelBufferWithSamplingTemporalDetailedProgressBudgetedAtFrameIndex(
                pixels,
                preflight.route.integratorMode3D,
                request->width,
                request->height,
                t,
                frame_index,
                light_x,
                light_y,
                NULL,
                request->temporal_frames,
                ray_tracing_temporal_progress_callback,
                &temporal_progress,
                ray_tracing_tile_progress_callback,
                &temporal_progress,
                active_resource_budget,
                &stats)) {
            frame_status = ray_tracing_headless_note_render_frame_failed(request,
                                                                        &preflight,
                                                                        &temporal_progress,
                                                                        &stage_started_at,
                                                                        frame_index,
                                                                        job_status_path,
                                                                        job_id,
                                                                        request_path);
            free(pixels);
            *out_preflight = preflight;
            return frame_status;
        }
        preflight.render_trace_ms += ray_tracing_elapsed_ms_since(&stage_started_at);
        preflight.render_frames_ms += ray_tracing_elapsed_ms_since(&stage_started_at);
        RuntimeTriangleBVH3D_SnapshotTraceStats(&preflight.bvh_trace_stats);
        RuntimeRenderTraceCostLedger3D_Snapshot(&preflight.render_trace_cost_ledger);
        if (preflight.bvh_trace_stats.flatFallbackCalls > 0u) {
            frame_status = ray_tracing_headless_note_bvh_flat_fallback_failed(request,
                                                                              &preflight,
                                                                              &temporal_progress,
                                                                              frame_index,
                                                                              job_status_path,
                                                                              job_id,
                                                                              request_path);
            free(pixels);
            *out_preflight = preflight;
            return frame_status;
        }
        RuntimeNative3DRenderStats_Accumulate(&preflight.stats, &stats);
        RuntimeRenderTraceCostLedger3D_Snapshot(&preflight.render_trace_cost_ledger);
        frame_status = ray_tracing_headless_write_rendered_frame_output(frame_path,
                                                                        pixels,
                                                                        i,
                                                                        frame_index,
                                                                        request,
                                                                        &preflight,
                                                                        &temporal_progress,
                                                                        job_status_path,
                                                                        job_id,
                                                                        request_path);
        if (frame_status != 0) {
            free(pixels);
            *out_preflight = preflight;
            return frame_status;
        }
        if (!ray_tracing_headless_write_photon_surface_diagnostics(
                request, frame_index)) {
            snprintf(preflight.diagnostics,
                     sizeof(preflight.diagnostics),
                     "failed to write photon surface diagnostics");
            free(pixels);
            *out_preflight = preflight;
            return 12;
        }
    }

    free(pixels);
    ray_tracing_headless_finalize_render_diagnostics(&preflight, request);
    {
        const int video_code = ray_tracing_headless_encode_video_if_requested(
            request,
            &preflight,
            job_status_path,
            job_id,
            request_path);
        if (video_code != 0) {
            *out_preflight = preflight;
            return video_code;
        }
    }
    snprintf(preflight.diagnostics, sizeof(preflight.diagnostics), "ok");
    preflight.total_run_ms = ray_tracing_elapsed_ms_since(&render_started_at);
    ray_tracing_headless_write_completed_progress(request,
                                                  &preflight,
                                                  job_status_path,
                                                  job_id,
                                                  request_path);
    *out_preflight = preflight;
    return preflight.rendered_frames ? 0 : 9;
}

int main(int argc, char **argv) {
    const char *request_path = NULL;
    const char *summary_override = NULL;
    const char *job_id = NULL;
    const char *job_status_path = NULL;
    bool render_mode = false;
    RayTracingAgentRenderRequest request;
    RayTracingHeadlessPreflight preflight = {0};
    char diagnostics[256] = {0};
    int run_code = 0;

    preflight.scene_acceleration_stats =
        RuntimeSceneAcceleration3DDiagnostics_Disabled();
    RuntimeRay3D_ResetTraceRouteForTests();
    RuntimeRay3D_ResetRouteStats();
    RuntimeRay3D_SnapshotRouteStats(&preflight.ray_trace_route_stats);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--request") == 0 && i + 1 < argc) {
            request_path = argv[++i];
        } else if (strcmp(argv[i], "--summary") == 0 && i + 1 < argc) {
            summary_override = argv[++i];
        } else if (strcmp(argv[i], "--job-id") == 0 && i + 1 < argc) {
            job_id = argv[++i];
        } else if (strcmp(argv[i], "--job-status") == 0 && i + 1 < argc) {
            job_status_path = argv[++i];
        } else if (strcmp(argv[i], "--preflight") == 0) {
            render_mode = false;
        } else if (strcmp(argv[i], "--render") == 0) {
            render_mode = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            ray_tracing_render_headless_usage(argv[0]);
            return 0;
        } else {
            ray_tracing_render_headless_usage(argv[0]);
            return 2;
        }
    }

    if (!request_path) {
        ray_tracing_render_headless_usage(argv[0]);
        return 2;
    }
    if (!ray_tracing_agent_render_request_load_file(request_path,
                                                    &request,
                                                    diagnostics,
                                                    sizeof(diagnostics))) {
        fprintf(stderr, "ray_tracing_render_headless: %s\n", diagnostics);
        return 2;
    }
    if (summary_override && summary_override[0]) {
        snprintf(request.summary_path, sizeof(request.summary_path), "%s", summary_override);
    }
    ray_tracing_render_headless_write_process_started_status(job_status_path,
                                                             job_id,
                                                             request_path,
                                                             &request,
                                                             render_mode);

    run_code = render_mode
                   ? run_render(&request,
                                &preflight,
                                job_status_path,
                                job_id,
                                request_path)
                   : run_preflight(&request,
                                   &preflight,
                                   job_status_path,
                                   job_id,
                                   request_path);
    ray_tracing_render_headless_write_summary(stdout, &request, &preflight);
    ray_tracing_render_headless_write_process_finished_status(job_status_path,
                                                              job_id,
                                                              request_path,
                                                              &request,
                                                              &preflight,
                                                              run_code);
    if (!ray_tracing_render_headless_write_summary_file(request.summary_path, &request, &preflight)) {
        fprintf(stderr,
                "ray_tracing_render_headless: failed to write summary %s\n",
                request.summary_path);
        return 7;
    }
    return run_code;
}
