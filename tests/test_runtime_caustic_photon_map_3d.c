#include "test_runtime_caustic_photon_map_3d.h"

#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_sphere_lens_3d.h"
#include "test_support.h"

static RuntimeCausticPhotonMapQuery3D test_query(Vec3 position, Vec3 normal) {
    RuntimeCausticPhotonMapQuery3D query;
    RuntimeCausticPhotonMap3D_DefaultQuery(&query);
    query.position = position;
    query.normal = normal;
    query.radius = 0.20;
    query.sceneObjectIndex = 9;
    query.primitiveIndex = 8;
    query.triangleIndex = 7;
    return query;
}

static int test_runtime_caustic_photon_map_allocate_store_query(void) {
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonSurfaceHit3D hit = {0};
    RuntimeCausticPhotonMapQueryResult3D result;
    RuntimeCausticPhotonMapDiagnostics3D diagnostics;
    RuntimeCausticPhotonMapQuery3D query = test_query(vec3(0.02, 0.0, 0.0),
                                                      vec3(0.0, 1.0, 0.0));

    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("runtime_caustic_photon_map_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 4u));
    hit.photonId = 1u;
    hit.depth = 3u;
    hit.position = vec3(0.0, 0.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.incidentDirection = vec3(0.0, -1.0, 0.0);
    hit.flux = vec3(2.0, 1.0, 0.5);
    hit.footprintRadius = 0.20;
    hit.sceneObjectIndex = 9;
    hit.primitiveIndex = 8;
    hit.triangleIndex = 7;

    assert_true("runtime_caustic_photon_map_store_hit",
                RuntimeCausticPhotonMap3D_StoreSurfaceHit(&map, &hit, 0.5, 0.20));
    assert_true("runtime_caustic_photon_map_query_hit",
                RuntimeCausticPhotonMap3D_Query(&map, &query, &result));
    assert_true("runtime_caustic_photon_map_query_positive", result.flux.x > 0.0);
    assert_true("runtime_caustic_photon_map_query_pdf_driven",
                result.flux.x > hit.flux.x);
    RuntimeCausticPhotonMap3D_SnapshotDiagnostics(&map, &diagnostics);
    assert_true("runtime_caustic_photon_map_diag_counts",
                diagnostics.recordCount == 1u &&
                    diagnostics.queryHitCount == 1u &&
                    diagnostics.lastQueryContributingCount == 1u);
    assert_close("runtime_caustic_photon_map_total_flux",
                 diagnostics.totalStoredFlux.x,
                 2.0,
                 1e-9);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

static int test_runtime_caustic_photon_map_trace_receiver_store(void) {
    RuntimeCausticSphereLens3DDescriptor sphere;
    RuntimeCausticSphereLens3DLight light;
    RuntimeCausticSphereLens3DSample sample;
    RuntimeCausticPhotonTraceSettings3D trace_settings;
    RuntimeCausticPhotonTrace3D trace;
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapQuery3D query;
    RuntimeCausticPhotonMapQueryResult3D result;

    RuntimeCausticSphereLens3D_DefaultDescriptor(&sphere);
    RuntimeCausticSphereLens3D_DefaultLight(&light);
    RuntimeCausticSphereLens3D_DefaultSample(&sample);
    RuntimeCausticPhotonTrace3D_DefaultSettings(&trace_settings);
    RuntimeCausticPhotonMap3D_Init(&map);

    sphere.ior = 1.2;
    light.radius = 0.05;
    light.intensity = 2.0;
    sample.lensU = 0.10;
    sample.receiverPlaneZ = -2.0;

    assert_true("runtime_caustic_photon_map_trace_sphere",
                RuntimeCausticPhotonTrace3D_TraceSphereLens(&sphere,
                                                            &light,
                                                            &sample,
                                                            &trace_settings,
                                                            77u,
                                                            12u,
                                                            2,
                                                            3,
                                                            &trace));
    assert_true("runtime_caustic_photon_map_trace_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 4u));
    assert_true("runtime_caustic_photon_map_store_trace_receiver",
                RuntimeCausticPhotonMap3D_StoreTraceReceiver(&map,
                                                             &trace,
                                                             vec3(0.0, 0.0, 1.0),
                                                             0.25,
                                                             9,
                                                             8,
                                                             7));
    query = test_query(trace.receiverCrossing, vec3(0.0, 0.0, 1.0));
    query.radius = 0.25;
    assert_true("runtime_caustic_photon_map_trace_query",
                RuntimeCausticPhotonMap3D_Query(&map, &query, &result));
    assert_true("runtime_caustic_photon_map_trace_query_flux",
                result.flux.x > 0.0 && result.contributingCount == 1u);
    assert_true("runtime_caustic_photon_map_trace_query_no_render",
                trace.debug.storedSurfaceFlux.x == 0.0 &&
                    trace.debug.storedVolumeFlux.x == 0.0);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

static int test_runtime_caustic_photon_map_rejects_capacity_and_identity(void) {
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapRecord3D record = {0};
    RuntimeCausticPhotonMapQuery3D query = test_query(vec3(0.0, 0.0, 0.0),
                                                      vec3(0.0, 1.0, 0.0));
    RuntimeCausticPhotonMapQueryResult3D result;
    RuntimeCausticPhotonMapDiagnostics3D diagnostics;

    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("runtime_caustic_photon_map_capacity_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 1u));
    record.photonId = 1u;
    record.position = vec3(0.0, 0.0, 0.0);
    record.normal = vec3(0.0, 1.0, 0.0);
    record.incidentDirection = vec3(0.0, -1.0, 0.0);
    record.flux = vec3(1.0, 1.0, 1.0);
    record.pathPdf = 1.0;
    record.queryRadius = 0.10;
    record.sceneObjectIndex = 9;
    record.primitiveIndex = 8;
    record.triangleIndex = 7;

    assert_true("runtime_caustic_photon_map_capacity_first",
                RuntimeCausticPhotonMap3D_StoreRecord(&map, &record));
    record.photonId = 2u;
    assert_true("runtime_caustic_photon_map_capacity_second_reject",
                !RuntimeCausticPhotonMap3D_StoreRecord(&map, &record));
    query.sceneObjectIndex = 10;
    assert_true("runtime_caustic_photon_map_identity_reject_query",
                !RuntimeCausticPhotonMap3D_Query(&map, &query, &result));
    RuntimeCausticPhotonMap3D_SnapshotDiagnostics(&map, &diagnostics);
    assert_true("runtime_caustic_photon_map_capacity_diag",
                diagnostics.recordCount == 1u &&
                    diagnostics.storeRejectedCount == 1u &&
                    diagnostics.lastQueryCandidateCount == 1u &&
                    diagnostics.lastQueryContributingCount == 0u);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

static int test_runtime_caustic_photon_map_query_cost_and_energy_ledgers(void) {
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapRecord3D record = {0};
    RuntimeCausticPhotonMapQuery3D query = test_query(vec3(0.0, 0.0, 0.0),
                                                      vec3(0.0, 1.0, 0.0));
    RuntimeCausticPhotonMapQueryResult3D result;
    RuntimeCausticPhotonMapDiagnostics3D diagnostics;

    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("runtime_caustic_photon_map_ppm9_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 4u));

    record.normal = vec3(0.0, 1.0, 0.0);
    record.incidentDirection = vec3(0.0, -1.0, 0.0);
    record.flux = vec3(1.0, 0.5, 0.25);
    record.pathPdf = 1.0;
    record.queryRadius = 0.20;
    record.sceneObjectIndex = 9;
    record.primitiveIndex = 8;
    record.triangleIndex = 7;

    record.photonId = 1u;
    record.position = vec3(0.0, 0.0, 0.0);
    assert_true("runtime_caustic_photon_map_ppm9_store_first",
                RuntimeCausticPhotonMap3D_StoreRecord(&map, &record));
    record.photonId = 2u;
    record.position = vec3(0.02, 0.0, 0.0);
    assert_true("runtime_caustic_photon_map_ppm9_store_second",
                RuntimeCausticPhotonMap3D_StoreRecord(&map, &record));
    record.photonId = 3u;
    record.position = vec3(0.04, 0.0, 0.0);
    assert_true("runtime_caustic_photon_map_ppm9_store_third",
                RuntimeCausticPhotonMap3D_StoreRecord(&map, &record));

    query.candidateLimit = 2u;
    query.physicalEnergyScale = 0.50;
    query.displayGain = 2.0;
    assert_true("runtime_caustic_photon_map_ppm9_query_hit",
                RuntimeCausticPhotonMap3D_Query(&map, &query, &result));
    assert_true("runtime_caustic_photon_map_ppm9_cost_bounded",
                result.testedCount == 2u &&
                    result.candidateCount == 2u &&
                    result.contributingCount == 2u &&
                    result.candidateLimit == 2u &&
                    result.candidateLimitReached);
    assert_close("runtime_caustic_photon_map_ppm9_flux_alias",
                 result.flux.x,
                 result.physicalFlux.x,
                 1e-9);
    assert_close("runtime_caustic_photon_map_ppm9_display_gain",
                 result.displayFlux.x,
                 result.physicalFlux.x * 2.0,
                 1e-9);

    RuntimeCausticPhotonMap3D_SnapshotDiagnostics(&map, &diagnostics);
    assert_true("runtime_caustic_photon_map_ppm9_diag_cost",
                diagnostics.lastQueryTestedCount == 2u &&
                    diagnostics.lastQueryCandidateLimit == 2u &&
                    diagnostics.lastQueryCandidateLimitReached);
    assert_close("runtime_caustic_photon_map_ppm9_diag_stored_flux",
                 diagnostics.totalStoredFlux.x,
                 3.0,
                 1e-9);
    assert_close("runtime_caustic_photon_map_ppm9_diag_queried_physical",
                 diagnostics.totalQueriedPhysicalFlux.x,
                 result.physicalFlux.x,
                 1e-9);
    assert_close("runtime_caustic_photon_map_ppm9_diag_queried_display",
                 diagnostics.totalQueriedDisplayFlux.x,
                 result.displayFlux.x,
                 1e-9);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

static int test_runtime_caustic_photon_map_grid_acceleration_prunes_sparse_records(void) {
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapRecord3D record = {0};
    RuntimeCausticPhotonMapQuery3D query = test_query(vec3(0.0, 0.0, 0.0),
                                                      vec3(0.0, 1.0, 0.0));
    RuntimeCausticPhotonMapQueryResult3D result;
    RuntimeCausticPhotonMapDiagnostics3D diagnostics;

    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("runtime_caustic_photon_map_grid_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 16u));

    record.normal = vec3(0.0, 1.0, 0.0);
    record.incidentDirection = vec3(0.0, -1.0, 0.0);
    record.flux = vec3(1.0, 1.0, 1.0);
    record.pathPdf = 1.0;
    record.queryRadius = 0.10;
    record.sceneObjectIndex = 9;
    record.primitiveIndex = 8;
    record.triangleIndex = 7;

    for (uint64_t i = 0u; i < 12u; ++i) {
        record.photonId = i + 1u;
        record.position = vec3((double)i * 5.0, 0.0, 0.0);
        assert_true("runtime_caustic_photon_map_grid_store",
                    RuntimeCausticPhotonMap3D_StoreRecord(&map, &record));
    }

    query.radius = 0.20;
    assert_true("runtime_caustic_photon_map_grid_query",
                RuntimeCausticPhotonMap3D_Query(&map, &query, &result));
    RuntimeCausticPhotonMap3D_SnapshotDiagnostics(&map, &diagnostics);
    assert_true("runtime_caustic_photon_map_grid_pruned",
                result.contributingCount == 1u &&
                    result.testedCount < map.recordCount &&
                    diagnostics.lastQueryAccelerationUsed &&
                    diagnostics.lastQueryGridCellVisitCount > 0u &&
                    diagnostics.accelerationAllocated &&
                    diagnostics.accelerationInsertedCount == map.recordCount &&
                    diagnostics.accelerationFallbackLinearQueryCount == 0u);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

int run_test_runtime_caustic_photon_map_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_map_allocate_store_query();
    failures += test_runtime_caustic_photon_map_trace_receiver_store();
    failures += test_runtime_caustic_photon_map_rejects_capacity_and_identity();
    failures += test_runtime_caustic_photon_map_query_cost_and_energy_ledgers();
    failures += test_runtime_caustic_photon_map_grid_acceleration_prunes_sparse_records();
    return failures;
}
