#include "tools/ray_tracing_render_headless_internal.h"

#include "app/ray_tracing_request_utils.h"

#include <stdio.h>

#include "render/runtime_dynamic_geometry_accel_3d.h"

void ray_tracing_headless_write_dynamic_geometry_acceleration_summary(
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

void ray_tracing_headless_write_dynamic_water_acceleration_cache_summary(
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
