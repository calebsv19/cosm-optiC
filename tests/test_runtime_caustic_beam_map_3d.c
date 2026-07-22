#include "test_runtime_caustic_beam_map_3d.h"

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_sphere_lens_3d.h"
#include "test_support.h"

#include <string.h>

static RuntimeCausticBeamMapQuery3D test_beam_query(Vec3 position, int medium_id) {
    RuntimeCausticBeamMapQuery3D query;
    RuntimeCausticBeamMap3D_DefaultQuery(&query);
    query.position = position;
    query.direction = vec3(0.0, 0.0, 1.0);
    query.radius = 0.25;
    query.mediumId = medium_id;
    query.requireMediumId = true;
    return query;
}

static int test_runtime_caustic_beam_map_allocate_store_query(void) {
    RuntimeCausticBeamMap3D map;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = {0};
    RuntimeCausticBeamMapQuery3D query = test_beam_query(vec3(0.05, 0.0, 1.0), 3);
    RuntimeCausticBeamMapQueryResult3D result;
    RuntimeCausticBeamMapDiagnostics3D diagnostics;

    RuntimeCausticBeamMap3D_Init(&map);
    assert_true("runtime_caustic_beam_map_allocate",
                RuntimeCausticBeamMap3D_Allocate(&map, 4u));
    segment.photonId = 1u;
    segment.depth = 3u;
    segment.start = vec3(0.0, 0.0, 0.0);
    segment.end = vec3(0.0, 0.0, 2.0);
    segment.direction = vec3(0.0, 0.0, 1.0);
    segment.flux = vec3(2.0, 1.0, 0.5);
    segment.radiusStart = 0.20;
    segment.radiusEnd = 0.30;
    segment.transmittance = 0.80;
    segment.densityWeight = 0.50;
    segment.mediumId = 3;

    assert_true("runtime_caustic_beam_map_store_segment",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    assert_true("runtime_caustic_beam_map_query_hit",
                RuntimeCausticBeamMap3D_Query(&map, &query, &result));
    assert_true("runtime_caustic_beam_map_query_positive", result.flux.x > 0.0);
    assert_true("runtime_caustic_beam_map_query_counts",
                result.candidateCount == 1u && result.contributingCount == 1u);
    RuntimeCausticBeamMap3D_SnapshotDiagnostics(&map, &diagnostics);
    assert_true("runtime_caustic_beam_map_diag_counts",
                diagnostics.segmentCount == 1u &&
                    diagnostics.queryHitCount == 1u &&
                    diagnostics.lastQueryContributingCount == 1u);
    assert_close("runtime_caustic_beam_map_total_flux",
                 diagnostics.totalStoredFlux.x,
                 2.0,
                 1e-9);
    RuntimeCausticBeamMap3D_Free(&map);
    return 0;
}

static int test_runtime_caustic_beam_map_trace_segment_store(void) {
    RuntimeCausticSphereLens3DDescriptor sphere;
    RuntimeCausticSphereLens3DLight light;
    RuntimeCausticSphereLens3DSample sample;
    RuntimeCausticPhotonTraceSettings3D trace_settings;
    RuntimeCausticPhotonTrace3D trace;
    RuntimeCausticBeamMap3D map;
    RuntimeCausticBeamMapQuery3D query;
    RuntimeCausticBeamMapQueryResult3D result;
    Vec3 midpoint;

    RuntimeCausticSphereLens3D_DefaultDescriptor(&sphere);
    RuntimeCausticSphereLens3D_DefaultLight(&light);
    RuntimeCausticSphereLens3D_DefaultSample(&sample);
    RuntimeCausticPhotonTrace3D_DefaultSettings(&trace_settings);
    RuntimeCausticBeamMap3D_Init(&map);

    sphere.ior = 1.2;
    light.radius = 0.05;
    light.intensity = 2.0;
    sample.lensU = 0.10;
    sample.receiverPlaneZ = -2.0;

    assert_true("runtime_caustic_beam_map_trace_sphere",
                RuntimeCausticPhotonTrace3D_TraceSphereLens(&sphere,
                                                            &light,
                                                            &sample,
                                                            &trace_settings,
                                                            77u,
                                                            12u,
                                                            2,
                                                            3,
                                                            &trace));
    assert_true("runtime_caustic_beam_map_trace_allocate",
                RuntimeCausticBeamMap3D_Allocate(&map, 4u));
    assert_true("runtime_caustic_beam_map_store_trace_segment",
                RuntimeCausticBeamMap3D_StoreTraceSegment(&map,
                                                          &trace,
                                                          0.05,
                                                          0.25,
                                                          0.70,
                                                          0.60,
                                                          4));
    midpoint = vec3_lerp(trace.postExitOrigin, trace.receiverCrossing, 0.50);
    query = test_beam_query(midpoint, 4);
    query.direction = trace.postExitDirection;
    assert_true("runtime_caustic_beam_map_trace_query",
                RuntimeCausticBeamMap3D_Query(&map, &query, &result));
    assert_true("runtime_caustic_beam_map_trace_query_flux",
                result.flux.x > 0.0 && result.contributingCount == 1u);
    assert_true("runtime_caustic_beam_map_trace_no_render",
                trace.debug.storedSurfaceFlux.x == 0.0 &&
                    trace.debug.storedVolumeFlux.x == 0.0);
    RuntimeCausticBeamMap3D_Free(&map);
    return 0;
}

static int test_runtime_caustic_beam_map_rejects_capacity_and_medium(void) {
    RuntimeCausticBeamMap3D map;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = {0};
    RuntimeCausticBeamMapQuery3D query = test_beam_query(vec3(0.0, 0.0, 0.5), 9);
    RuntimeCausticBeamMapQueryResult3D result;
    RuntimeCausticBeamMapDiagnostics3D diagnostics;

    RuntimeCausticBeamMap3D_Init(&map);
    assert_true("runtime_caustic_beam_map_capacity_allocate",
                RuntimeCausticBeamMap3D_Allocate(&map, 1u));
    segment.photonId = 1u;
    segment.start = vec3(0.0, 0.0, 0.0);
    segment.end = vec3(0.0, 0.0, 1.0);
    segment.direction = vec3(0.0, 0.0, 1.0);
    segment.flux = vec3(1.0, 1.0, 1.0);
    segment.radiusStart = 0.10;
    segment.radiusEnd = 0.10;
    segment.transmittance = 1.0;
    segment.densityWeight = 1.0;
    segment.mediumId = 3;

    assert_true("runtime_caustic_beam_map_capacity_first",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    segment.photonId = 2u;
    assert_true("runtime_caustic_beam_map_capacity_second_reject",
                !RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    assert_true("runtime_caustic_beam_map_medium_reject_query",
                !RuntimeCausticBeamMap3D_Query(&map, &query, &result));
    RuntimeCausticBeamMap3D_SnapshotDiagnostics(&map, &diagnostics);
    assert_true("runtime_caustic_beam_map_capacity_diag",
                diagnostics.segmentCount == 1u &&
                    diagnostics.storeRejectedCount == 1u &&
                    diagnostics.lastQueryCandidateCount == 1u &&
                    diagnostics.lastQueryContributingCount == 0u);
    RuntimeCausticBeamMap3D_Free(&map);
    return 0;
}

static int test_runtime_caustic_beam_map_query_cost_and_energy_ledgers(void) {
    RuntimeCausticBeamMap3D map;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = {0};
    RuntimeCausticBeamMapQuery3D query = test_beam_query(vec3(0.0, 0.0, 0.5), 3);
    RuntimeCausticBeamMapQueryResult3D result;
    RuntimeCausticBeamMapDiagnostics3D diagnostics;

    RuntimeCausticBeamMap3D_Init(&map);
    assert_true("runtime_caustic_beam_map_ppm9_allocate",
                RuntimeCausticBeamMap3D_Allocate(&map, 4u));

    segment.depth = 2u;
    segment.start = vec3(0.0, 0.0, 0.0);
    segment.end = vec3(0.0, 0.0, 1.0);
    segment.direction = vec3(0.0, 0.0, 1.0);
    segment.flux = vec3(1.0, 0.5, 0.25);
    segment.radiusStart = 0.20;
    segment.radiusEnd = 0.20;
    segment.transmittance = 1.0;
    segment.densityWeight = 1.0;
    segment.mediumId = 3;

    segment.photonId = 1u;
    assert_true("runtime_caustic_beam_map_ppm9_store_first",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    segment.photonId = 2u;
    segment.start = vec3(0.02, 0.0, 0.0);
    segment.end = vec3(0.02, 0.0, 1.0);
    assert_true("runtime_caustic_beam_map_ppm9_store_second",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    segment.photonId = 3u;
    segment.start = vec3(0.04, 0.0, 0.0);
    segment.end = vec3(0.04, 0.0, 1.0);
    assert_true("runtime_caustic_beam_map_ppm9_store_third",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));

    query.candidateLimit = 2u;
    query.physicalEnergyScale = 0.50;
    query.displayGain = 2.0;
    assert_true("runtime_caustic_beam_map_ppm9_query_hit",
                RuntimeCausticBeamMap3D_Query(&map, &query, &result));
    assert_true("runtime_caustic_beam_map_ppm9_cost_bounded",
                result.testedCount == 2u &&
                    result.candidateCount == 2u &&
                    result.contributingCount == 2u &&
                    result.candidateLimit == 2u &&
                    result.candidateLimitReached);
    assert_close("runtime_caustic_beam_map_ppm9_flux_alias",
                 result.flux.x,
                 result.physicalFlux.x,
                 1e-9);
    assert_close("runtime_caustic_beam_map_ppm9_display_gain",
                 result.displayFlux.x,
                 result.physicalFlux.x * 2.0,
                 1e-9);

    RuntimeCausticBeamMap3D_SnapshotDiagnostics(&map, &diagnostics);
    assert_true("runtime_caustic_beam_map_ppm9_diag_cost",
                diagnostics.lastQueryTestedCount == 2u &&
                    diagnostics.lastQueryCandidateLimit == 2u &&
                    diagnostics.lastQueryCandidateLimitReached);
    assert_close("runtime_caustic_beam_map_ppm9_diag_stored_flux",
                 diagnostics.totalStoredFlux.x,
                 3.0,
                 1e-9);
    assert_close("runtime_caustic_beam_map_ppm9_diag_queried_physical",
                 diagnostics.totalQueriedPhysicalFlux.x,
                 result.physicalFlux.x,
                 1e-9);
    assert_close("runtime_caustic_beam_map_ppm9_diag_queried_display",
                 diagnostics.totalQueriedDisplayFlux.x,
                 result.displayFlux.x,
                 1e-9);
    RuntimeCausticBeamMap3D_Free(&map);
    return 0;
}

static int test_runtime_caustic_beam_map_grid_acceleration_prunes_sparse_segments(void) {
    RuntimeCausticBeamMap3D map;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = {0};
    RuntimeCausticBeamMapQuery3D query = test_beam_query(vec3(0.0, 0.0, 0.05), 3);
    RuntimeCausticBeamMapQueryResult3D result;
    RuntimeCausticBeamMapDiagnostics3D diagnostics;

    RuntimeCausticBeamMap3D_Init(&map);
    assert_true("runtime_caustic_beam_map_grid_allocate",
                RuntimeCausticBeamMap3D_Allocate(&map, 16u));

    segment.depth = 2u;
    segment.direction = vec3(0.0, 0.0, 1.0);
    segment.flux = vec3(1.0, 1.0, 1.0);
    segment.radiusStart = 0.05;
    segment.radiusEnd = 0.05;
    segment.transmittance = 1.0;
    segment.densityWeight = 1.0;
    segment.mediumId = 3;

    for (uint64_t i = 0u; i < 12u; ++i) {
        const double x = (double)i * 5.0;
        segment.photonId = i + 1u;
        segment.start = vec3(x, 0.0, 0.0);
        segment.end = vec3(x, 0.0, 0.10);
        assert_true("runtime_caustic_beam_map_grid_store",
                    RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    }

    query.radius = 0.10;
    assert_true("runtime_caustic_beam_map_grid_query",
                RuntimeCausticBeamMap3D_Query(&map, &query, &result));
    RuntimeCausticBeamMap3D_SnapshotDiagnostics(&map, &diagnostics);
    assert_true("runtime_caustic_beam_map_grid_pruned",
                result.contributingCount == 1u &&
                    result.testedCount < map.segmentCount &&
                    diagnostics.lastQueryAccelerationUsed &&
                    diagnostics.lastQueryGridCellVisitCount > 0u &&
                    diagnostics.accelerationAllocated &&
                    diagnostics.accelerationInsertedCount == map.segmentCount &&
                    diagnostics.accelerationFallbackLinearQueryCount == 0u);
    RuntimeCausticBeamMap3D_Free(&map);
    return 0;
}

static int test_runtime_caustic_beam_map_ppm24a_estimator_contract(void) {
    RuntimeCausticBeamMap3D map;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = {0};
    RuntimeCausticBeamMapQuery3D query =
        test_beam_query(vec3(0.0, 0.0, 0.5), 3);
    RuntimeCausticBeamMapQueryResult3D compatibility;
    RuntimeCausticBeamMapQueryResult3D explicit_radius;
    RuntimeCausticBeamMapQueryResult3D knn;
    RuntimeCausticBeamMapDiagnostics3D diagnostics;

    RuntimeCausticBeamMap3D_Init(&map);
    assert_true("runtime_caustic_beam_map_ppm24a_allocate",
                RuntimeCausticBeamMap3D_Allocate(&map, 4u));
    segment.direction = vec3(0.0, 0.0, 1.0);
    segment.flux = vec3(1.0, 0.5, 0.25);
    segment.radiusStart = 0.20;
    segment.radiusEnd = 0.20;
    segment.transmittance = 1.0;
    segment.densityWeight = 1.0;
    segment.mediumId = 3;
    segment.photonId = 1u;
    segment.start = vec3(0.01, 0.0, 0.0);
    segment.end = vec3(0.01, 0.0, 1.0);
    assert_true("runtime_caustic_beam_map_ppm24a_store_first",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    segment.photonId = 2u;
    segment.start = vec3(0.08, 0.0, 0.0);
    segment.end = vec3(0.08, 0.0, 1.0);
    assert_true("runtime_caustic_beam_map_ppm24a_store_second",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    segment.photonId = 3u;
    segment.mediumId = 9;
    segment.start = vec3(0.12, 0.0, 0.0);
    segment.end = vec3(0.12, 0.0, 1.0);
    assert_true("runtime_caustic_beam_map_ppm24a_store_rejected_candidate",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));

    assert_true("runtime_caustic_beam_map_ppm24a_compat_query",
                RuntimeCausticBeamMap3D_Query(&map, &query, &compatibility));
    query.estimator.estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS;
    assert_true("runtime_caustic_beam_map_ppm24a_explicit_radius_query",
                RuntimeCausticBeamMap3D_Query(&map, &query, &explicit_radius));
    assert_close("runtime_caustic_beam_map_ppm24a_radius_compat_x",
                 explicit_radius.flux.x, compatibility.flux.x, 1e-12);
    assert_true("runtime_caustic_beam_map_ppm24a_readback",
                explicit_radius.estimatorImplemented &&
                    strcmp(explicit_radius.estimatorLabel, "radius") == 0 &&
                    explicit_radius.effectiveSampleCount == 2u &&
                    explicit_radius.mediumRejectCount == 1u &&
                    explicit_radius.rejectedPhysicalFlux.x > 0.0 &&
                    explicit_radius.nearestContributionDistance <=
                        explicit_radius.farthestContributionDistance &&
                    explicit_radius.varianceProxy >= 0.0);

    query.estimator.estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST;
    query.estimator.neighborLimit = 1u;
    query.estimator.minimumEffectiveSamples = 1u;
    assert_true("runtime_caustic_beam_map_ppm24c_knn",
                RuntimeCausticBeamMap3D_Query(&map, &query, &knn) &&
                    knn.estimatorImplemented &&
                    strcmp(knn.estimatorLabel, "k_nearest") == 0 &&
                    knn.testedCount == 3u && knn.effectiveSampleCount == 1u &&
                    knn.nearestContributionDistance == 0.01 &&
                    knn.farthestContributionDistance == 0.01 &&
                    knn.mediumRejectCount == 1u);
    RuntimeCausticBeamMap3D_SnapshotDiagnostics(&map, &diagnostics);
    assert_true("runtime_caustic_beam_map_ppm24c_diag",
                diagnostics.lastQueryEstimatorImplemented &&
                    diagnostics.lastQueryEffectiveSampleCount == 1u &&
                    strcmp(diagnostics.lastQueryEstimatorLabel, "k_nearest") == 0);
    RuntimeCausticBeamMap3D_Free(&map);
    return 0;
}

int run_test_runtime_caustic_beam_map_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_beam_map_allocate_store_query();
    failures += test_runtime_caustic_beam_map_trace_segment_store();
    failures += test_runtime_caustic_beam_map_rejects_capacity_and_medium();
    failures += test_runtime_caustic_beam_map_query_cost_and_energy_ledgers();
    failures += test_runtime_caustic_beam_map_grid_acceleration_prunes_sparse_segments();
    failures += test_runtime_caustic_beam_map_ppm24a_estimator_contract();
    return failures;
}
