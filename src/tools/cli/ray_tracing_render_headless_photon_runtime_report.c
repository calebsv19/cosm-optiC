#include "tools/ray_tracing_render_headless_internal.h"
#include "render/runtime_native_3d_render_photon_prepare.h"
#include "render/runtime_caustic_photon_direct_consumer_3d.h"

#include <inttypes.h>
#include <stdio.h>

bool ray_tracing_headless_write_photon_runtime_report(
    const RayTracingAgentRenderRequest* request, int frame_index,
    double cumulative_frame_wall_ms) {
    if (!request->caustic_photon_render_prep_population_enabled) return true;
    RuntimeCausticPhotonPreparationStats3D stats;
    RuntimeCausticPhotonSurfaceDiagnosticAggregate3D queries;
    RuntimeNative3DRender_SnapshotPhotonPreparation(&stats);
    RuntimeCausticPhotonDirectConsumer3D_SnapshotSurfaceDiagnosticAggregate(&queries);
    char path[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/photon_runtime_%04d.json",
                 request->output_root, frame_index) >= (int)sizeof(path)) return false;
    FILE* file = fopen(path, "w");
    if (!file) return false;
    const RuntimeCausticPhotonMapLifecycleReadback3D* life = &stats.latest.mapLifecycle;
    const RuntimeCausticPhotonMapPopulationReadback3D* pop = &stats.latest.mapPopulation;
    bool ok = fprintf(file,
        "{\n  \"schema\":\"ray_tracing_photon_runtime_v1\",\n"
        "  \"capture_stage\":\"after_render_frame_before_diagnostic_probes\",\n"
        "  \"frame_index\":%d,\n  \"available\":%s,\n"
        "  \"process_scope\":\"includes_preflight_and_runtime_preparations\",\n"
        "  \"preparation_count\":%" PRIu64 ",\n"
        "  \"generation\":%" PRIu64 ",\n  \"rebuild_count\":%" PRIu64 ",\n"
        "  \"reuse_count\":%" PRIu64 ",\n"
        "  \"latest_prepare_rebuilt\":%s,\n  \"latest_prepare_reused\":%s,\n"
        "  \"latest_rebuild_reason\":\"%s\",\n"
        "  \"cumulative_wall_ms\":{\"fingerprint\":%.9f,\"map_build\":%.9f,"
        "\"emission\":%.9f,\"path_transport\":%.9f,\"support_preparation\":%.9f,"
        "\"prep_probe_and_deposit\":%.9f,\"frame_render_including_runtime_prep\":%.9f},\n"
        "  \"latest_map_build_wall_ms\":%.9f,\n"
        "  \"camera_gather_timing_available\":false,\n"
        "  \"surface_query_scope\":\"calls_since_last_consumer_bind_or_reset\",\n"
        "  \"surface_query_count\":%" PRIu64 ",\n"
        "  \"positive_surface_query_count\":%" PRIu64 ",\n"
        "  \"stored_surface_records\":%" PRIu64 ",\n"
        "  \"stored_beam_segments\":%" PRIu64 ",\n"
        "  \"surface_acceleration_inserted_records\":%" PRIu64 ",\n"
        "  \"support_spatial_index_used\":%s,\n"
        "  \"support_search_candidates_tested\":%" PRIu64 ",\n"
        "  \"support_preparation_count_scope\":\"last_successful_map_build\",\n"
        "  \"emitted_flux\":[%.17g,%.17g,%.17g],\n"
        "  \"stored_surface_flux\":[%.17g,%.17g,%.17g],\n"
        "  \"stored_volume_flux\":[%.17g,%.17g,%.17g],\n",
        frame_index, stats.available ? "true" : "false", stats.preparationCount,
        life->generation, life->rebuildCount, life->reuseCount,
        life->rebuilt ? "true" : "false", life->reused ? "true" : "false",
        RuntimeCausticPhotonMapRebuildReason3D_Label(life->rebuildReason),
        stats.fingerprintWallMs, stats.mapBuildWallMs, stats.emissionWallMs,
        stats.pathTransportWallMs, stats.supportPreparationWallMs,
        stats.probeAndDepositWallMs, cumulative_frame_wall_ms, life->mapBuildCpuMs,
        queries.queryCount, queries.positiveQueryCount, pop->surfaceMapRecordCount,
        pop->volumeBeamSegmentCount, pop->surfaceMapAccelerationInsertedCount,
        stats.support.spatialIndexUsed ? "true" : "false", stats.support.searchCandidateCount,
        pop->totalEmittedFlux.x, pop->totalEmittedFlux.y, pop->totalEmittedFlux.z,
        pop->totalStoredSurfaceFlux.x, pop->totalStoredSurfaceFlux.y, pop->totalStoredSurfaceFlux.z,
        pop->totalStoredVolumeFlux.x, pop->totalStoredVolumeFlux.y, pop->totalStoredVolumeFlux.z) >= 0;
    if (fprintf(file,
        "  \"budget\":{\"requested\":%llu,\"effective\":%llu,\"limit\":%llu,"
        "\"limited\":%s,\"emitted\":%llu,\"surface_capacity\":%llu,"
        "\"beam_capacity\":%llu,\"record_storage_ceiling_bytes\":%llu,"
        "\"surface_store_rejects\":%llu,\"beam_store_rejects\":%llu,"
        "\"path_capacity_rejects\":%llu,\"support_neighbor_limit\":%llu}\n}\n",
        (unsigned long long)pop->requestedSampleBudget,
        (unsigned long long)pop->effectiveSampleBudget,
        (unsigned long long)pop->sampleBudgetLimit,
        pop->sampleBudgetLimited ? "true" : "false",
        (unsigned long long)pop->emittedPhotonCount,
        (unsigned long long)pop->surfaceMapCapacity,
        (unsigned long long)pop->volumeBeamCapacity,
        (unsigned long long)pop->recordStorageCeilingBytes,
        (unsigned long long)pop->surfaceMapStoreRejectedCount,
        (unsigned long long)pop->volumeBeamStoreRejectedCount,
        (unsigned long long)pop->pathPopulationCapacityRejectCount,
        (unsigned long long)stats.support.neighborLimit) < 0) ok=false;
    if (fclose(file) != 0) ok = false;
    return ok;
}
