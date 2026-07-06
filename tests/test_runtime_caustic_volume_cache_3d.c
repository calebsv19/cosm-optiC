#include "test_runtime_caustic_volume_cache_3d.h"

#include "render/runtime_caustic_volume_cache_3d.h"
#include "test_support.h"

static bool test_caustic_cache_make_grid(RuntimeVolumeGrid3D* grid) {
    RuntimeVolumeGrid3D_Reset(grid);
    return RuntimeVolumeGrid3D_Configure(grid,
                                         1u,
                                         2u,
                                         2u,
                                         2u,
                                         1.0,
                                         7u,
                                         0.02,
                                         vec3(-1.0, -1.0, 0.0),
                                         0.5,
                                         vec3(0.0, 0.0, 1.0),
                                         0u);
}

static int test_runtime_caustic_volume_cache_empty_allocation(void) {
    RuntimeVolumeGrid3D grid;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticVolumeCacheDiagnostics3D diagnostics;
    bool ok = false;

    RuntimeCausticVolumeCache3D_Init(&cache);
    ok = test_caustic_cache_make_grid(&grid);
    assert_true("runtime_caustic_volume_cache_empty_grid_ok", ok);
    ok = RuntimeCausticVolumeCache3D_Allocate(&cache, &grid);
    assert_true("runtime_caustic_volume_cache_empty_allocate", ok);
    assert_true("runtime_caustic_volume_cache_empty_allocated",
                RuntimeCausticVolumeCache3D_IsAllocated(&cache));

    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_volume_cache_empty_state",
                diagnostics.state == RUNTIME_CAUSTIC_CACHE_STATE_ALLOCATED_EMPTY);
    assert_true("runtime_caustic_volume_cache_empty_cell_count",
                diagnostics.cellCount == 8u);
    assert_true("runtime_caustic_volume_cache_empty_nonzero",
                diagnostics.nonZeroCellCount == 0u);
    assert_close("runtime_caustic_volume_cache_empty_total_r",
                 diagnostics.totalRadianceR,
                 0.0,
                 1e-9);

    RuntimeCausticVolumeCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_volume_cache_deposit_one_cell(void) {
    RuntimeVolumeGrid3D grid;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticVolumeCacheDiagnostics3D diagnostics;
    Vec3 sample = {0};
    bool ok = false;

    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_volume_cache_deposit_grid",
                test_caustic_cache_make_grid(&grid));
    assert_true("runtime_caustic_volume_cache_deposit_allocate",
                RuntimeCausticVolumeCache3D_Allocate(&cache, &grid));
    ok = RuntimeCausticVolumeCache3D_DepositAtPosition(
        &cache, vec3(-0.75, -0.75, 0.25), 2.0, 0.5, 0.25);
    assert_true("runtime_caustic_volume_cache_deposit_ok", ok);
    ok = RuntimeCausticVolumeCache3D_SampleAtPosition(&cache,
                                                      vec3(-0.75, -0.75, 0.25),
                                                      &sample);
    assert_true("runtime_caustic_volume_cache_deposit_sample_ok", ok);
    assert_true("runtime_caustic_volume_cache_deposit_sample_positive",
                sample.x > 0.0 && sample.y > 0.0 && sample.z > 0.0);

    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_volume_cache_deposit_state",
                diagnostics.state == RUNTIME_CAUSTIC_CACHE_STATE_SAMPLED);
    assert_true("runtime_caustic_volume_cache_deposit_nonzero",
                diagnostics.nonZeroCellCount == 1u);
    assert_true("runtime_caustic_volume_cache_deposit_accepted",
                diagnostics.depositAcceptedCount == 1u);
    assert_true("runtime_caustic_volume_cache_deposit_samples",
                diagnostics.sampleContributingCount == 1u);
    assert_close("runtime_caustic_volume_cache_deposit_total_r",
                 diagnostics.totalRadianceR,
                 2.0,
                 1e-6);
    assert_true("runtime_caustic_volume_cache_deposit_has_bounds",
                diagnostics.hasNonZeroBounds);
    assert_close("runtime_caustic_volume_cache_deposit_centroid_x",
                 diagnostics.radianceCentroid.x,
                 -0.75,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_deposit_centroid_y",
                 diagnostics.radianceCentroid.y,
                 -0.75,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_deposit_centroid_z",
                 diagnostics.radianceCentroid.z,
                 0.25,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_deposit_bounds_min_x",
                 diagnostics.nonZeroBoundsMin.x,
                 -0.75,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_deposit_bounds_max_z",
                 diagnostics.nonZeroBoundsMax.z,
                 0.25,
                 1e-6);

    RuntimeCausticVolumeCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_volume_cache_rgb_accumulates(void) {
    RuntimeVolumeGrid3D grid;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticVolumeCacheDiagnostics3D diagnostics;

    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_volume_cache_rgb_grid",
                test_caustic_cache_make_grid(&grid));
    assert_true("runtime_caustic_volume_cache_rgb_allocate",
                RuntimeCausticVolumeCache3D_Allocate(&cache, &grid));
    assert_true("runtime_caustic_volume_cache_rgb_deposit_a",
                RuntimeCausticVolumeCache3D_DepositAtPosition(
                    &cache, vec3(-0.75, -0.75, 0.25), 1.0, 2.0, 3.0));
    assert_true("runtime_caustic_volume_cache_rgb_deposit_b",
                RuntimeCausticVolumeCache3D_DepositAtPosition(
                    &cache, vec3(-0.75, -0.75, 0.25), 0.5, 0.25, 0.125));

    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_volume_cache_rgb_nonzero",
                diagnostics.nonZeroCellCount == 1u);
    assert_close("runtime_caustic_volume_cache_rgb_total_r",
                 diagnostics.totalRadianceR,
                 1.5,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_rgb_total_g",
                 diagnostics.totalRadianceG,
                 2.25,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_rgb_total_b",
                 diagnostics.totalRadianceB,
                 3.125,
                 1e-6);

    RuntimeCausticVolumeCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_volume_cache_footprint_normalizes_energy(void) {
    RuntimeVolumeGrid3D grid;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticVolumeCacheDiagnostics3D diagnostics;

    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_volume_cache_footprint_grid",
                test_caustic_cache_make_grid(&grid));
    assert_true("runtime_caustic_volume_cache_footprint_allocate",
                RuntimeCausticVolumeCache3D_Allocate(&cache, &grid));
    assert_true("runtime_caustic_volume_cache_footprint_deposit",
                RuntimeCausticVolumeCache3D_DepositFootprintAtPosition(
                    &cache, vec3(-0.5, -0.5, 0.5), 0.75, 8.0, 4.0, 2.0));

    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_volume_cache_footprint_multi_cell",
                diagnostics.nonZeroCellCount > 1u);
    assert_true("runtime_caustic_volume_cache_footprint_count",
                diagnostics.footprintDepositCount == 1u);
    assert_true("runtime_caustic_volume_cache_footprint_cell_count",
                diagnostics.footprintCellContributionCount > 1u);
    assert_true("runtime_caustic_volume_cache_footprint_radius",
                diagnostics.averageFootprintRadiusVoxels > 1.0);
    assert_close("runtime_caustic_volume_cache_footprint_input_r",
                 diagnostics.footprintInputRadianceR,
                 8.0,
                 1e-9);
    assert_close("runtime_caustic_volume_cache_footprint_deposited_r",
                 diagnostics.footprintDepositedRadianceR,
                 8.0,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_footprint_total_r",
                 diagnostics.totalRadianceR,
                 diagnostics.footprintDepositedRadianceR,
                 1e-5);
    assert_close("runtime_caustic_volume_cache_footprint_deposited_g",
                 diagnostics.footprintDepositedRadianceG,
                 4.0,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_footprint_deposited_b",
                 diagnostics.footprintDepositedRadianceB,
                 2.0,
                 1e-6);

    RuntimeCausticVolumeCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_volume_cache_directional_footprint_normalizes_energy(void) {
    RuntimeVolumeGrid3D grid;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticVolumeCacheDiagnostics3D diagnostics;

    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_volume_cache_directional_grid",
                test_caustic_cache_make_grid(&grid));
    assert_true("runtime_caustic_volume_cache_directional_allocate",
                RuntimeCausticVolumeCache3D_Allocate(&cache, &grid));
    assert_true("runtime_caustic_volume_cache_directional_deposit",
                RuntimeCausticVolumeCache3D_DepositDirectionalFootprintAtPosition(
                    &cache,
                    vec3(-0.5, -0.5, 0.5),
                    vec3(0.0, 0.0, 1.0),
                    0.5,
                    1.0,
                    6.0,
                    3.0,
                    1.5));

    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_volume_cache_directional_multi_cell",
                diagnostics.nonZeroCellCount > 1u);
    assert_true("runtime_caustic_volume_cache_directional_count",
                diagnostics.footprintDepositCount == 1u);
    assert_true("runtime_caustic_volume_cache_directional_cell_count",
                diagnostics.footprintCellContributionCount > 1u);
    assert_close("runtime_caustic_volume_cache_directional_avg_radius",
                 diagnostics.averageFootprintRadiusVoxels,
                 4.0 / 3.0,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_directional_input_r",
                 diagnostics.footprintInputRadianceR,
                 6.0,
                 1e-9);
    assert_close("runtime_caustic_volume_cache_directional_deposited_r",
                 diagnostics.footprintDepositedRadianceR,
                 6.0,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_directional_deposited_g",
                 diagnostics.footprintDepositedRadianceG,
                 3.0,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_directional_deposited_b",
                 diagnostics.footprintDepositedRadianceB,
                 1.5,
                 1e-6);
    assert_close("runtime_caustic_volume_cache_directional_total_r",
                 diagnostics.totalRadianceR,
                 diagnostics.footprintDepositedRadianceR,
                 1e-5);

    RuntimeCausticVolumeCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_volume_cache_rejects_out_of_bounds(void) {
    RuntimeVolumeGrid3D grid;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticVolumeCacheDiagnostics3D diagnostics;
    Vec3 sample = {0};

    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_volume_cache_oob_grid",
                test_caustic_cache_make_grid(&grid));
    assert_true("runtime_caustic_volume_cache_oob_allocate",
                RuntimeCausticVolumeCache3D_Allocate(&cache, &grid));
    assert_true("runtime_caustic_volume_cache_oob_rejected",
                !RuntimeCausticVolumeCache3D_DepositAtPosition(
                    &cache, vec3(9.0, 0.0, 0.0), 5.0, 5.0, 5.0));
    assert_true("runtime_caustic_volume_cache_oob_sample_rejected",
                !RuntimeCausticVolumeCache3D_SampleAtPosition(
                    &cache, vec3(9.0, 0.0, 0.0), &sample));

    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_volume_cache_oob_deposit_rejected_count",
                diagnostics.depositRejectedCount == 1u);
    assert_true("runtime_caustic_volume_cache_oob_lookup_count",
                diagnostics.sampleLookupCount == 1u);
    assert_true("runtime_caustic_volume_cache_oob_no_contrib",
                diagnostics.sampleContributingCount == 0u);
    assert_true("runtime_caustic_volume_cache_oob_state_empty",
                diagnostics.state == RUNTIME_CAUSTIC_CACHE_STATE_ALLOCATED_EMPTY);

    RuntimeCausticVolumeCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_volume_cache_world_position_sampling(void) {
    RuntimeVolumeGrid3D grid;
    RuntimeCausticVolumeCache3D cache;
    Vec3 sample = {0};

    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_volume_cache_sample_grid",
                test_caustic_cache_make_grid(&grid));
    assert_true("runtime_caustic_volume_cache_sample_allocate",
                RuntimeCausticVolumeCache3D_Allocate(&cache, &grid));
    assert_true("runtime_caustic_volume_cache_sample_deposit",
                RuntimeCausticVolumeCache3D_DepositAtPosition(
                    &cache, vec3(-1.0, -1.0, 0.0), 8.0, 4.0, 2.0));
    assert_true("runtime_caustic_volume_cache_sample_lookup",
                RuntimeCausticVolumeCache3D_SampleAtPosition(
                    &cache, vec3(-0.75, -0.75, 0.25), &sample));
    assert_close("runtime_caustic_volume_cache_sample_r", sample.x, 1.0, 1e-6);
    assert_close("runtime_caustic_volume_cache_sample_g", sample.y, 0.5, 1e-6);
    assert_close("runtime_caustic_volume_cache_sample_b", sample.z, 0.25, 1e-6);

    RuntimeCausticVolumeCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_volume_cache_filtered_sampling_expands_coverage(void) {
    RuntimeVolumeGrid3D grid;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticVolumeCacheDiagnostics3D diagnostics;
    Vec3 point_sample = {0};
    Vec3 filtered_sample = {0};

    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_volume_cache_filtered_grid",
                test_caustic_cache_make_grid(&grid));
    assert_true("runtime_caustic_volume_cache_filtered_allocate",
                RuntimeCausticVolumeCache3D_Allocate(&cache, &grid));
    assert_true("runtime_caustic_volume_cache_filtered_deposit",
                RuntimeCausticVolumeCache3D_DepositAtPosition(
                    &cache, vec3(-0.75, -0.75, 0.25), 8.0, 4.0, 2.0));
    assert_true("runtime_caustic_volume_cache_filtered_point_lookup",
                RuntimeCausticVolumeCache3D_SampleAtPosition(
                    &cache, vec3(-0.25, -0.25, 0.75), &point_sample));
    assert_close("runtime_caustic_volume_cache_filtered_point_miss",
                 point_sample.x,
                 0.0,
                 1e-9);
    assert_true("runtime_caustic_volume_cache_filtered_lookup",
                RuntimeCausticVolumeCache3D_SampleFilteredAtPosition(
                    &cache, vec3(-0.25, -0.25, 0.75), 1.0, &filtered_sample));
    assert_true("runtime_caustic_volume_cache_filtered_positive",
                filtered_sample.x > 0.0 && filtered_sample.y > 0.0 &&
                    filtered_sample.z > 0.0);

    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_volume_cache_filtered_contrib_count",
                diagnostics.sampleContributingCount == 1u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_volume_cache_clear_reset(void) {
    RuntimeVolumeGrid3D grid;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticVolumeCacheDiagnostics3D diagnostics;

    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_volume_cache_clear_grid",
                test_caustic_cache_make_grid(&grid));
    assert_true("runtime_caustic_volume_cache_clear_allocate",
                RuntimeCausticVolumeCache3D_Allocate(&cache, &grid));
    assert_true("runtime_caustic_volume_cache_clear_deposit",
                RuntimeCausticVolumeCache3D_DepositAtPosition(
                    &cache, vec3(-0.75, -0.75, 0.25), 3.0, 3.0, 3.0));
    RuntimeCausticVolumeCache3D_Clear(&cache);
    RuntimeCausticVolumeCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_volume_cache_clear_still_allocated",
                diagnostics.allocated);
    assert_true("runtime_caustic_volume_cache_clear_state_empty",
                diagnostics.state == RUNTIME_CAUSTIC_CACHE_STATE_ALLOCATED_EMPTY);
    assert_true("runtime_caustic_volume_cache_clear_nonzero",
                diagnostics.nonZeroCellCount == 0u);
    assert_true("runtime_caustic_volume_cache_clear_attempts",
                diagnostics.depositAttemptCount == 0u);
    assert_close("runtime_caustic_volume_cache_clear_total",
                 diagnostics.totalRadianceR,
                 0.0,
                 1e-9);

    RuntimeCausticVolumeCache3D_Free(&cache);
    return 0;
}

int run_test_runtime_caustic_volume_cache_3d_tests(void) {
    int before = test_support_failures();

    test_runtime_caustic_volume_cache_empty_allocation();
    test_runtime_caustic_volume_cache_deposit_one_cell();
    test_runtime_caustic_volume_cache_rgb_accumulates();
    test_runtime_caustic_volume_cache_footprint_normalizes_energy();
    test_runtime_caustic_volume_cache_directional_footprint_normalizes_energy();
    test_runtime_caustic_volume_cache_rejects_out_of_bounds();
    test_runtime_caustic_volume_cache_world_position_sampling();
    test_runtime_caustic_volume_cache_filtered_sampling_expands_coverage();
    test_runtime_caustic_volume_cache_clear_reset();

    return test_support_failures() - before;
}
