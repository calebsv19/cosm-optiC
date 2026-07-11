#include "tools/ray_tracing_render_headless_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app/ray_tracing_request_utils.h"
#include "render/runtime_ray_3d.h"

static bool ray_tracing_headless_motion_track_is_mesh_instance(
    const RayTracingRuntimeMeshAssetSet *mesh_assets,
    const RuntimeMotionTrack3D *track) {
    if (!mesh_assets || !track || !track->object_id[0]) return false;
    for (int i = 0; i < mesh_assets->instance_count; ++i) {
        if (strcmp(mesh_assets->instances[i].object_id, track->object_id) == 0) {
            return true;
        }
    }
    return false;
}

void ray_tracing_headless_write_object_motion_acceleration_summary(
    FILE *file,
    const RayTracingAgentRenderRequest *request,
    const RayTracingHeadlessPreflight *preflight) {
    const RayTracingRuntimeMeshAssetSet *mesh_assets = NULL;
    int executable_tracks = 0;
    int moving_mesh_tracks = 0;
    int moving_primitive_tracks = 0;
    int total_mesh_instances = 0;
    int static_mesh_instances = 0;
    int frame_count = 0;
    double first_frame_t = 0.0;
    double last_frame_t = 0.0;
    bool normalized_t_changes = false;
    bool water_surface_present = false;
    bool generated_or_deforming_geometry_present = false;
    bool has_motion = false;
    bool has_moving_mesh = false;
    bool route_parity_checked = false;
    bool route_parity_clean = false;

    if (!file || !preflight) return;

    mesh_assets = ray_tracing_runtime_mesh_assets_last();
    total_mesh_instances = mesh_assets ? mesh_assets->instance_count
                                       : preflight->mesh_asset_timing_stats.loaded_instances;

    for (int i = 0; i < preflight->object_motion_summary.stored_tracks; ++i) {
        const RuntimeMotionTrack3D *track = &preflight->object_motion_summary.tracks[i];
        if (!RuntimeMotionTrack3DHasExecutableMotion(track)) continue;
        executable_tracks += 1;
        if (ray_tracing_headless_motion_track_is_mesh_instance(mesh_assets, track)) {
            moving_mesh_tracks += 1;
        } else {
            moving_primitive_tracks += 1;
        }
    }

    if (total_mesh_instances < moving_mesh_tracks) {
        total_mesh_instances = moving_mesh_tracks;
    }
    static_mesh_instances = total_mesh_instances - moving_mesh_tracks;
    has_motion = executable_tracks > 0;
    has_moving_mesh = moving_mesh_tracks > 0;
    frame_count = request && request->frame_count > 0 ? request->frame_count
                                                      : preflight->frames_rendered;
    if (request && frame_count > 0) {
        first_frame_t = ray_tracing_headless_frame_normalized_t(request, 0);
        last_frame_t = ray_tracing_headless_frame_normalized_t(request, frame_count - 1);
        normalized_t_changes = fabs(last_frame_t - first_frame_t) > 1e-9;
    }

    water_surface_present = preflight->water_surface_source_found ||
                            preflight->water_surface_loaded ||
                            preflight->water_surface_mesh_attached;
    generated_or_deforming_geometry_present =
        preflight->volume_attached || preflight->volume_frame_selection_dynamic;
    route_parity_checked = preflight->ray_trace_route_stats.parityCheckedRays > 0u;
    route_parity_clean =
        route_parity_checked && preflight->ray_trace_route_stats.parityMismatches == 0u;

    fprintf(file, "  \"object_motion_acceleration\": {\n");
    fprintf(file, "    \"valid\": true,\n");
    fprintf(file, "    \"authored_rigid_motion_tracks\": %d,\n", executable_tracks);
    fprintf(file, "    \"authored_rigid_primitive_motion_tracks\": %d,\n",
            moving_primitive_tracks);
    fprintf(file, "    \"authored_rigid_mesh_motion_tracks\": %d,\n",
            moving_mesh_tracks);
    fprintf(file, "    \"total_mesh_asset_instances\": %d,\n", total_mesh_instances);
    fprintf(file, "    \"static_mesh_asset_instances\": %d,\n", static_mesh_instances);
    fprintf(file, "    \"moving_mesh_asset_instances\": %d,\n", moving_mesh_tracks);
    fprintf(file, "    \"mesh_local_blas_identity_reusable\": %s,\n",
            has_moving_mesh ? "true" : "false");
    fprintf(file, "    \"mesh_local_blas_policy\": ");
    RayTracingJsonWriteString(
        file,
        has_moving_mesh ? "reuse_mesh_asset_local_blas_for_rigid_instance_motion"
                        : "static_mesh_asset_blas_cache");
    fprintf(file, ",\n");
    fprintf(file, "    \"moving_object_tlas_policy\": ");
    RayTracingJsonWriteString(
        file,
        has_motion ? "rebuild_tlas_per_sample_until_refit_contract_lands"
                   : "static_scene_or_prepared_cache_reuse");
    fprintf(file, ",\n");
    fprintf(file, "    \"prepared_scene_time_dependent\": %s,\n",
            preflight->prepared_scene_cache_stats.staticGeometryReuseEnabled ? "false"
                                                                             : "true");
    fprintf(file, "    \"prepared_scene_time_dependent_required\": %s,\n",
            has_motion && normalized_t_changes ? "true" : "false");
    fprintf(file, "    \"tlas_rebuild_required\": %s,\n",
            has_motion ? "true" : "false");
    fprintf(file, "    \"tlas_refit_available\": false,\n");
    fprintf(file, "    \"tlas_rebuilds\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.tlasRebuilds);
    fprintf(file, "    \"tlas_refits\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.tlasRefits);
    fprintf(file, "    \"blas_cached_asset_count\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasCachedAssetCount);
    fprintf(file, "    \"blas_full_rebuilds\": %llu,\n",
            (unsigned long long)preflight->scene_acceleration_stats.blasFullRebuilds);
    fprintf(file, "    \"generated_or_deforming_geometry_present\": %s,\n",
            generated_or_deforming_geometry_present ? "true" : "false");
    fprintf(file, "    \"water_surface_present\": %s,\n",
            water_surface_present ? "true" : "false");
    fprintf(file, "    \"water_surface_dynamic\": %s,\n",
            preflight->water_surface_frame_selection_dynamic ? "true" : "false");
    fprintf(file, "    \"frame_count\": %d,\n", frame_count);
    fprintf(file, "    \"first_frame_normalized_t\": %.9f,\n", first_frame_t);
    fprintf(file, "    \"last_frame_normalized_t\": %.9f,\n", last_frame_t);
    fprintf(file, "    \"normalized_t_changes_across_frames\": %s,\n",
            normalized_t_changes ? "true" : "false");
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
    fprintf(file, "    \"flattened_fallback_available\": true,\n");
    fprintf(file, "    \"route_parity_checkable\": true,\n");
    fprintf(file, "    \"route_parity_checked\": %s,\n",
            route_parity_checked ? "true" : "false");
    fprintf(file, "    \"route_parity_clean\": %s,\n",
            route_parity_clean ? "true" : "false");
    fprintf(file, "    \"route_parity_checked_rays\": %llu,\n",
            (unsigned long long)preflight->ray_trace_route_stats.parityCheckedRays);
    fprintf(file, "    \"route_parity_mismatches\": %llu\n",
            (unsigned long long)preflight->ray_trace_route_stats.parityMismatches);
    fprintf(file, "  },\n");
}
