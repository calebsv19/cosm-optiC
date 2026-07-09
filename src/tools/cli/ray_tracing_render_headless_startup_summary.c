#include "tools/ray_tracing_render_headless_internal.h"

#include "app/ray_tracing_request_utils.h"

#include <stdio.h>

static double ray_tracing_headless_startup_nonnegative(double value) {
    return value > 0.0 ? value : 0.0;
}

static double ray_tracing_headless_startup_percent_of(double value, double total) {
    return total > 0.0 ? (ray_tracing_headless_startup_nonnegative(value) * 100.0) / total : 0.0;
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
            ray_tracing_headless_startup_nonnegative(ms),
            ray_tracing_headless_startup_percent_of(ms, total_ms),
            comma ? "," : "");
}

void ray_tracing_headless_write_startup_load_timing_matrix(
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
            ray_tracing_headless_startup_nonnegative(total_reference_ms));
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
