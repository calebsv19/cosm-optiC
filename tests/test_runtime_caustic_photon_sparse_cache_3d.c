#include "test_runtime_caustic_photon_sparse_cache_3d.h"

#include <string.h>

#include "render/runtime_caustic_photon_distributed_beam_cache_3d.h"
#include "render/runtime_caustic_photon_sparse_brick_cache_3d.h"
#include "render/runtime_caustic_photon_volume_beam_estimator_3d.h"
#include "test_support.h"

static bool sparse_cache_grid(RuntimeVolumeGrid3D* grid) {
    RuntimeVolumeGrid3D_Reset(grid);
    return RuntimeVolumeGrid3D_Configure(grid,
                                         1u,
                                         8u,
                                         8u,
                                         8u,
                                         1.0,
                                         31u,
                                         0.02,
                                         vec3(-1.0, -1.0, 0.0),
                                         0.25,
                                         vec3(0.0, 0.0, 1.0),
                                         0u);
}

static RuntimeCausticPhotonVolumeBeamSegment3D sparse_cache_segment(
    uint64_t id,
    double x,
    Vec3 flux) {
    RuntimeCausticPhotonVolumeBeamSegment3D segment;
    memset(&segment, 0, sizeof(segment));
    segment.photonId = id;
    segment.start = vec3(x, 0.125, 0.125);
    segment.end = vec3(x, 0.125, 1.875);
    segment.direction = vec3(0.0, 0.0, 1.0);
    segment.flux = flux;
    segment.radiusStart = 0.01;
    segment.radiusEnd = 0.01;
    segment.transmittance = 0.8;
    segment.densityWeight = 1.0;
    segment.mediumId = 0;
    segment.provenance.originalMediumId = 0;
    segment.provenance.segmentStage =
        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    segment.provenance.priorSpecularOrTransmission = true;
    return segment;
}

static bool sparse_cache_maps(RuntimeCausticBeamMap3D* map,
                       RuntimeCausticVolumeCache3D* oracle,
                       RuntimeCausticVolumeCache3D* accelerated,
                       RuntimeVolumeGrid3D* grid) {
    const RuntimeCausticPhotonVolumeBeamSegment3D a =
        sparse_cache_segment(1u, 0.125, vec3(2.0, 1.0, 0.5));
    const RuntimeCausticPhotonVolumeBeamSegment3D b =
        sparse_cache_segment(2u, -0.125, vec3(0.5, 1.0, 2.0));
    RuntimeCausticBeamMap3D_Init(map);
    RuntimeCausticVolumeCache3D_Init(oracle);
    RuntimeCausticVolumeCache3D_Init(accelerated);
    return sparse_cache_grid(grid) &&
        RuntimeCausticBeamMap3D_Allocate(map, 2u) &&
        RuntimeCausticBeamMap3D_StoreSegment(map, &a) &&
        RuntimeCausticBeamMap3D_StoreSegment(map, &b) &&
        RuntimeCausticVolumeCache3D_Allocate(oracle, grid) &&
        RuntimeCausticVolumeCache3D_Allocate(accelerated, grid);
}

static int test_sparse_cache_sparse_brick_storage_contract(void) {
    const int before = test_support_failures();
    RuntimeVolumeGrid3D grid;
    RuntimeCausticPhotonSparseBrickCache3D* first = NULL;
    RuntimeCausticPhotonSparseBrickCache3D* second = NULL;
    RuntimeCausticPhotonSparseBrickCache3D* bounded = NULL;
    RuntimeCausticPhotonSparseBrickCacheStats3D firstStats;
    RuntimeCausticPhotonSparseBrickCacheStats3D secondStats;
    RuntimeCausticPhotonSparseBrickCacheStats3D boundedStats;
    uint32_t brickW = 0u;
    uint32_t brickH = 0u;
    uint32_t brickD = 0u;
    uint64_t entries = 0u;
    const uint64_t corner = 4u + 5u * (5u + 6u * 6u);
    const uint64_t allocationOrder[] = {0u, corner, 4u, 30u};
    RuntimeVolumeGrid3D_Reset(&grid);
    assert_true("sparse_cache_sparse_irregular_grid",
                RuntimeVolumeGrid3D_Configure(
                    &grid, 1u, 5u, 6u, 7u, 0.0, 0u, 0.01,
                    vec3(-1.0, -2.0, -3.0), 0.25,
                    vec3(0.0, 0.0, 1.0), 0u));
    assert_true("sparse_cache_sparse_directory_shape",
                RuntimeCausticPhotonSparseBrickCache3D_DirectoryShape(
                    5u, 6u, 7u, &brickW, &brickH, &brickD, &entries) &&
                    brickW == 2u && brickH == 2u && brickD == 2u &&
                    entries == 8u);
    assert_true("sparse_cache_sparse_directory_overflow_rejected",
                !RuntimeCausticPhotonSparseBrickCache3D_DirectoryShape(
                    UINT32_MAX, UINT32_MAX, UINT32_MAX,
                    &brickW, &brickH, &brickD, &entries));
    first = RuntimeCausticPhotonSparseBrickCache3D_Create(&grid, 2520u, 0u);
    second = RuntimeCausticPhotonSparseBrickCache3D_Create(&grid, 2520u, 0u);
    assert_true("sparse_cache_sparse_create", first != NULL && second != NULL);
    assert_true("sparse_cache_sparse_missing_exact_zero",
                first && RuntimeCausticPhotonSparseBrickCache3D_FindCell(
                    first, 1u) == NULL);
    for (uint64_t i = 0u; first && second &&
         i < sizeof(allocationOrder) / sizeof(allocationOrder[0]); ++i) {
        float* a = RuntimeCausticPhotonSparseBrickCache3D_AcquireCell(
            first, allocationOrder[i]);
        float* b = RuntimeCausticPhotonSparseBrickCache3D_AcquireCell(
            second, allocationOrder[i]);
        assert_true("sparse_cache_sparse_edge_corner_acquire", a != NULL && b != NULL);
        if (a && b) {
            a[0] = (float)(i + 1u);
            b[0] = (float)(i + 1u);
        }
    }
    RuntimeCausticPhotonSparseBrickCache3D_Snapshot(first, &firstStats);
    RuntimeCausticPhotonSparseBrickCache3D_Snapshot(second, &secondStats);
    assert_true("sparse_cache_sparse_deterministic_allocation",
                firstStats.allocatedBrickCount == secondStats.allocatedBrickCount &&
                    firstStats.allocationOrderHash == secondStats.allocationOrderHash);
    assert_true("sparse_cache_sparse_accounting_partition",
                firstStats.directoryBytes > 0u && firstStats.payloadBytes > 0u &&
                    firstStats.metadataBytes > 0u &&
                    firstStats.peakBytes == 2520u + firstStats.directoryBytes +
                        firstStats.payloadBytes + firstStats.metadataBytes);
    bounded = RuntimeCausticPhotonSparseBrickCache3D_Create(&grid, 0u, 1u);
    assert_true("sparse_cache_sparse_bounded_create", bounded != NULL);
    assert_true("sparse_cache_sparse_first_brick_allocates",
                bounded && RuntimeCausticPhotonSparseBrickCache3D_AcquireCell(
                    bounded, 0u) != NULL);
    assert_true("sparse_cache_sparse_allocation_failure_is_closed",
                bounded && RuntimeCausticPhotonSparseBrickCache3D_AcquireCell(
                    bounded, corner) == NULL);
    RuntimeCausticPhotonSparseBrickCache3D_Snapshot(bounded, &boundedStats);
    assert_true("sparse_cache_sparse_failure_accounted",
                boundedStats.allocatedBrickCount == 1u &&
                    boundedStats.allocationFailureCount == 1u);
    RuntimeCausticPhotonSparseBrickCache3D_Clear(bounded);
    RuntimeCausticPhotonSparseBrickCache3D_Snapshot(bounded, &boundedStats);
    assert_true("sparse_cache_sparse_failure_cleanup",
                boundedStats.allocatedBrickCount == 0u &&
                    boundedStats.allocationFailureCount == 0u &&
                    RuntimeCausticPhotonSparseBrickCache3D_FindCell(
                        bounded, 0u) == NULL);
    RuntimeCausticPhotonSparseBrickCache3D_Destroy(bounded);
    RuntimeCausticPhotonSparseBrickCache3D_Destroy(second);
    RuntimeCausticPhotonSparseBrickCache3D_Destroy(first);
    return test_support_failures() - before;
}

static int test_sparse_cache_sparse_dense_field_and_query_equivalence(void) {
    const int before = test_support_failures();
    RuntimeVolumeGrid3D grid;
    RuntimeCausticBeamMap3D map;
    RuntimeCausticVolumeCache3D dense;
    RuntimeCausticVolumeCache3D sparse;
    RuntimeCausticDistributedBeamCacheSettings3D settings;
    RuntimeCausticDistributedBeamCacheReadback3D denseBuild;
    RuntimeCausticDistributedBeamCacheReadback3D sparseBuild;
    assert_true("sparse_cache_sparse_equivalence_setup",
                sparse_cache_maps(&map, &dense, &sparse, &grid));
    RuntimeCausticDistributedBeamCache3D_DefaultSettings(&settings);
    settings.queryRadius = 0.40;
    settings.buildMode = RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_RASTER_ACCELERATED;
    settings.storageBackend = RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_DENSE;
    assert_true("sparse_cache_dense_reference_build",
                RuntimeCausticDistributedBeamCache3D_Build(
                    &dense, &map, &settings, &denseBuild));
    settings.storageBackend = RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_SPARSE_BRICKS;
    assert_true("sparse_cache_sparse_candidate_build",
                RuntimeCausticDistributedBeamCache3D_Build(
                    &sparse, &map, &settings, &sparseBuild));
    assert_true("sparse_cache_sparse_backend_readback",
                denseBuild.storageBackend ==
                    RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_DENSE &&
                    sparseBuild.storageBackend ==
                        RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_SPARSE_BRICKS &&
                    sparseBuild.sparseAllocatedBrickCount > 0u &&
                    sparseBuild.sparseAllocationFailureCount == 0u);
    assert_true("sparse_cache_sparse_conservation_equivalence",
                memcmp(&denseBuild.expectedIntegratedFlux,
                       &sparseBuild.expectedIntegratedFlux,
                       sizeof(denseBuild.expectedIntegratedFlux)) == 0 &&
                    memcmp(&denseBuild.cachedIntegratedFlux,
                           &sparseBuild.cachedIntegratedFlux,
                           sizeof(denseBuild.cachedIntegratedFlux)) == 0 &&
                    denseBuild.integratedFluxRelativeError ==
                        sparseBuild.integratedFluxRelativeError);
    for (uint64_t i = 0u; i < grid.cellCount; ++i) {
        float denseFields[RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_CELL_FIELD_FLOAT_COUNT_3D];
        float sparseFields[RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_CELL_FIELD_FLOAT_COUNT_3D];
        assert_true("sparse_cache_dense_cell_serializes",
                    RuntimeCausticDistributedBeamCache3D_ReadCellFields(
                        &dense, i, denseFields));
        assert_true("sparse_cache_sparse_cell_serializes",
                    RuntimeCausticDistributedBeamCache3D_ReadCellFields(
                        &sparse, i, sparseFields));
        assert_true("sparse_cache_dense_sparse_cell_bitwise_equivalent",
                    memcmp(denseFields, sparseFields, sizeof(denseFields)) == 0);
    }
    for (uint32_t z = 0u; z < grid.gridD; ++z) {
        for (uint32_t y = 0u; y < grid.gridH; ++y) {
            for (uint32_t x = 0u; x < grid.gridW; ++x) {
                RuntimeCausticBeamMapQueryResult3D denseSample;
                RuntimeCausticBeamMapQueryResult3D sparseSample;
                const Vec3 position = vec3(
                    grid.origin.x + ((double)x + 0.25) * grid.voxelSize,
                    grid.origin.y + ((double)y + 0.75) * grid.voxelSize,
                    grid.origin.z + ((double)z + 0.25) * grid.voxelSize);
                const bool denseHit = RuntimeCausticDistributedBeamCache3D_Sample(
                    &dense, position, &denseSample);
                const bool sparseHit = RuntimeCausticDistributedBeamCache3D_Sample(
                    &sparse, position, &sparseSample);
                assert_true("sparse_cache_dense_sparse_query_hit_equivalent",
                            denseHit == sparseHit);
                assert_true("sparse_cache_dense_sparse_query_field_equivalent",
                            memcmp(&denseSample.physicalFlux,
                                   &sparseSample.physicalFlux,
                                   sizeof(denseSample.physicalFlux)) == 0 &&
                                memcmp(&denseSample.meanBeamDirection,
                                       &sparseSample.meanBeamDirection,
                                       sizeof(denseSample.meanBeamDirection)) == 0 &&
                                denseSample.meanBeamDistance ==
                                    sparseSample.meanBeamDistance &&
                                denseSample.beamDirectionWeightSum ==
                                    sparseSample.beamDirectionWeightSum);
            }
        }
    }
    settings.mediumId = 7;
    settings.requireMediumId = true;
    assert_true("sparse_cache_sparse_medium_isolation",
                !RuntimeCausticDistributedBeamCache3D_Build(
                    &sparse, &map, &settings, &sparseBuild) &&
                    sparseBuild.segmentRasterizedCount == 0u &&
                    sparseBuild.cachedIntegratedFlux.x == 0.0 &&
                    sparseBuild.cachedIntegratedFlux.y == 0.0 &&
                    sparseBuild.cachedIntegratedFlux.z == 0.0);
    RuntimeCausticVolumeCache3D_Free(&sparse);
    RuntimeCausticVolumeCache3D_Free(&dense);
    RuntimeCausticBeamMap3D_Free(&map);
    return test_support_failures() - before;
}

static int test_sparse_cache_oracle_acceleration_equivalence(void) {
    const int before = test_support_failures();
    RuntimeVolumeGrid3D grid;
    RuntimeCausticBeamMap3D map;
    RuntimeCausticVolumeCache3D oracle;
    RuntimeCausticVolumeCache3D accelerated;
    RuntimeCausticDistributedBeamCacheSettings3D settings;
    RuntimeCausticDistributedBeamCacheReadback3D oracle_readback;
    RuntimeCausticDistributedBeamCacheReadback3D accelerated_readback;
    assert_true("sparse_cache_setup", sparse_cache_maps(&map, &oracle, &accelerated, &grid));
    RuntimeCausticDistributedBeamCache3D_DefaultSettings(&settings);
    settings.queryRadius = 0.40;
    settings.buildMode = RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_LINEAR_ORACLE;
    assert_true("sparse_cache_linear_oracle_build",
                RuntimeCausticDistributedBeamCache3D_Build(
                    &oracle, &map, &settings, &oracle_readback));
    settings.buildMode =
        RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_RASTER_ACCELERATED;
    assert_true("sparse_cache_accelerated_build",
                RuntimeCausticDistributedBeamCache3D_Build(
                    &accelerated, &map, &settings, &accelerated_readback));
    assert_true("sparse_cache_every_beam_rasterized",
                oracle_readback.segmentRasterizedCount == 2u &&
                    accelerated_readback.segmentRasterizedCount == 2u);
    assert_true("sparse_cache_conservative_subvoxel_fields",
                oracle_readback.conservativeSubvoxelField &&
                    accelerated_readback.conservativeSubvoxelField &&
                    oracle_readback.subvoxelCountPerCell == 8u &&
                    accelerated_readback.subvoxelCountPerCell == 8u &&
                    oracle.beamSubvoxelField &&
                    accelerated.beamSubvoxelField);
    assert_true("sparse_cache_half_voxel_spacing",
                accelerated_readback.maximumAxialSpacing <=
                    0.5 * grid.voxelSize + 1.0e-12);
    assert_true("sparse_cache_acceleration_reduces_tests",
                accelerated_readback.cellTestCount < oracle_readback.cellTestCount);
    for (uint64_t i = 0u; i < grid.cellCount; ++i) {
        assert_close("sparse_cache_oracle_accel_r", oracle.radianceR[i],
                     accelerated.radianceR[i], 1.0e-6);
        assert_close("sparse_cache_oracle_accel_g", oracle.radianceG[i],
                     accelerated.radianceG[i], 1.0e-6);
        assert_close("sparse_cache_oracle_accel_b", oracle.radianceB[i],
                     accelerated.radianceB[i], 1.0e-6);
        assert_close("sparse_cache_oracle_accel_direction_weight",
                     oracle.beamDirectionWeight[i],
                     accelerated.beamDirectionWeight[i], 1.0e-6);
        {
            Vec3 oracle_subvoxel_average = vec3(0.0, 0.0, 0.0);
            Vec3 accelerated_subvoxel_average = vec3(0.0, 0.0, 0.0);
            for (uint64_t subvoxel = 0u; subvoxel < 8u; ++subvoxel) {
                const uint64_t index = i * 8u + subvoxel;
                assert_close("sparse_cache_oracle_accel_subvoxel_r",
                             oracle.beamSubvoxelRadianceR[index],
                             accelerated.beamSubvoxelRadianceR[index], 1.0e-6);
                assert_close("sparse_cache_oracle_accel_subvoxel_g",
                             oracle.beamSubvoxelRadianceG[index],
                             accelerated.beamSubvoxelRadianceG[index], 1.0e-6);
                assert_close("sparse_cache_oracle_accel_subvoxel_b",
                             oracle.beamSubvoxelRadianceB[index],
                             accelerated.beamSubvoxelRadianceB[index], 1.0e-6);
                oracle_subvoxel_average = vec3_add(
                    oracle_subvoxel_average,
                    vec3(oracle.beamSubvoxelRadianceR[index],
                         oracle.beamSubvoxelRadianceG[index],
                         oracle.beamSubvoxelRadianceB[index]));
                accelerated_subvoxel_average = vec3_add(
                    accelerated_subvoxel_average,
                    vec3(accelerated.beamSubvoxelRadianceR[index],
                         accelerated.beamSubvoxelRadianceG[index],
                         accelerated.beamSubvoxelRadianceB[index]));
            }
            oracle_subvoxel_average = vec3_scale(
                oracle_subvoxel_average, 1.0 / 8.0);
            accelerated_subvoxel_average = vec3_scale(
                accelerated_subvoxel_average, 1.0 / 8.0);
            assert_close("sparse_cache_oracle_subvoxel_conserves_coarse_r",
                         oracle_subvoxel_average.x, oracle.radianceR[i], 1.0e-6);
            assert_close("sparse_cache_accel_subvoxel_conserves_coarse_r",
                         accelerated_subvoxel_average.x,
                         accelerated.radianceR[i], 1.0e-6);
        }
    }
    assert_close("sparse_cache_integrated_energy_bounded",
                 accelerated_readback.integratedFluxRelativeError, 0.0, 0.15);
    assert_true("sparse_cache_segment_normalization_ledger",
                accelerated_readback.segmentNormalizationCount == 2u &&
                    accelerated_readback.segmentNormalizationDenominatorSum > 0.0 &&
                    accelerated_readback.segmentNormalizationScaleMinimum > 0.0 &&
                    accelerated_readback.segmentNormalizationScaleMaximum >=
                        accelerated_readback.segmentNormalizationScaleMinimum &&
                    accelerated_readback.segmentNormalizationScaleMean >=
                        accelerated_readback.segmentNormalizationScaleMinimum &&
                    accelerated_readback.segmentNormalizationScaleMean <=
                        accelerated_readback.segmentNormalizationScaleMaximum);
    RuntimeCausticVolumeCache3D_Free(&accelerated);
    RuntimeCausticVolumeCache3D_Free(&oracle);
    RuntimeCausticBeamMap3D_Free(&map);
    return test_support_failures() - before;
}

static int test_sparse_cache_direct_cache_sample_and_phase_equivalence(void) {
    const int before = test_support_failures();
    RuntimeVolumeGrid3D grid;
    RuntimeCausticBeamMap3D map;
    RuntimeCausticVolumeCache3D oracle;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticDistributedBeamCacheSettings3D settings;
    RuntimeCausticDistributedBeamCacheReadback3D build;
    RuntimeCausticBeamMapQuery3D query;
    RuntimeCausticBeamMapQueryResult3D direct;
    RuntimeCausticBeamMapQueryResult3D cached;
    RuntimeCausticPhotonVolumeBeamEstimatorSettings3D estimator_settings;
    RuntimeCausticPhotonVolumeBeamEstimatorInput3D direct_input;
    RuntimeCausticPhotonVolumeBeamEstimatorInput3D cache_input;
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D direct_radiance;
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D cache_radiance;
    const Vec3 position = vec3(0.4375, 0.1875, 0.9375);
    assert_true("sparse_cache_sample_setup", sparse_cache_maps(&map, &oracle, &cache, &grid));
    RuntimeCausticDistributedBeamCache3D_DefaultSettings(&settings);
    settings.queryRadius = 0.40;
    assert_true("sparse_cache_cache_build",
                RuntimeCausticDistributedBeamCache3D_Build(
                    &cache, &map, &settings, &build));
    RuntimeCausticBeamMap3D_DefaultQuery(&query);
    query.position = position;
    query.direction = vec3(1.0, 0.0, 0.0);
    query.radius = settings.queryRadius;
    query.mediumId = 0;
    query.requireMediumId = true;
    query.segmentStage = RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    query.requireSegmentStage = true;
    query.estimator.estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS;
    query.estimator.minimumEffectiveSamples = 1u;
    assert_true("sparse_cache_direct_query",
                RuntimeCausticBeamMap3D_Query(&map, &query, &direct));
    assert_true("sparse_cache_cache_query",
                RuntimeCausticDistributedBeamCache3D_Sample(
                    &cache, position, &cached));
    assert_close("sparse_cache_direct_cache_flux_r", direct.physicalFlux.x,
                 cached.physicalFlux.x, direct.physicalFlux.x * 0.30);
    assert_close("sparse_cache_direct_cache_flux_g", direct.physicalFlux.y,
                 cached.physicalFlux.y, direct.physicalFlux.y * 0.30);
    assert_close("sparse_cache_direct_cache_flux_b", direct.physicalFlux.z,
                 cached.physicalFlux.z, direct.physicalFlux.z * 0.30);
    assert_close("sparse_cache_direct_cache_direction_z", direct.meanBeamDirection.z,
                 cached.meanBeamDirection.z, 1.0e-6);
    assert_close("sparse_cache_direct_cache_distance", direct.meanBeamDistance,
                 cached.meanBeamDistance,
                 build.subvoxelSize * 0.5 + 1.0e-6);
    RuntimeCausticPhotonVolumeBeamEstimator3D_DefaultSettings(&estimator_settings);
    memset(&direct_input, 0, sizeof(direct_input));
    direct_input.beamFluxDensity = direct.physicalFlux;
    direct_input.beamDirection = direct.meanBeamDirection;
    direct_input.viewToCameraDirection = vec3(0.0, 0.0, 1.0);
    direct_input.beamDistance = direct.meanBeamDistance;
    direct_input.mediumDensity = 0.2;
    direct_input.cameraTransmittance = 0.75;
    direct_input.stepLength = 0.1;
    cache_input = direct_input;
    cache_input.beamFluxDensity = cached.physicalFlux;
    cache_input.beamDirection = cached.meanBeamDirection;
    cache_input.beamDistance = cached.meanBeamDistance;
    assert_true("sparse_cache_direct_phase_evaluate",
                RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
                    &estimator_settings, &direct_input, &direct_radiance));
    assert_true("sparse_cache_cache_phase_evaluate",
                RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
                    &estimator_settings, &cache_input, &cache_radiance));
    assert_close("sparse_cache_direct_cache_phase_radiance",
                 direct_radiance.radiance.x,
                 cache_radiance.radiance.x,
                 direct_radiance.radiance.x * 0.30);
    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeCausticVolumeCache3D_Free(&oracle);
    RuntimeCausticBeamMap3D_Free(&map);
    return test_support_failures() - before;
}

static int test_sparse_cache_tier_bounds_fail_closed(void) {
    const int before = test_support_failures();
    RuntimeVolumeGrid3D grid;
    RuntimeCausticBeamMap3D map;
    RuntimeCausticVolumeCache3D oracle;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticDistributedBeamCacheSettings3D settings;
    RuntimeCausticDistributedBeamCacheReadback3D readback;
    assert_true("sparse_cache_bounds_setup", sparse_cache_maps(&map, &oracle, &cache, &grid));
    RuntimeCausticDistributedBeamCache3D_DefaultSettings(&settings);
    settings.memoryCeilingBytes = 1u;
    assert_true("sparse_cache_memory_bound_fail_closed",
                !RuntimeCausticDistributedBeamCache3D_Build(
                    &cache, &map, &settings, &readback) &&
                    !readback.memoryBoundSatisfied && !readback.built);
    RuntimeCausticDistributedBeamCache3D_DefaultSettings(&settings);
    settings.maxSegments = 1u;
    assert_true("sparse_cache_segment_bound_fail_closed",
                !RuntimeCausticDistributedBeamCache3D_Build(
                    &cache, &map, &settings, &readback) &&
                    readback.segmentLimitReached && !readback.workBoundSatisfied &&
                    !cache.physicalBeamField);
    RuntimeCausticDistributedBeamCache3D_DefaultSettings(&settings);
    settings.maxCellTests = 1u;
    assert_true("sparse_cache_work_bound_fail_closed",
                !RuntimeCausticDistributedBeamCache3D_Build(
                    &cache, &map, &settings, &readback) &&
                    readback.cellTestLimitReached && !readback.built);
    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeCausticVolumeCache3D_Free(&oracle);
    RuntimeCausticBeamMap3D_Free(&map);
    return test_support_failures() - before;
}

static int test_sparse_cache_tier_monotonicity(void) {
    const int before = test_support_failures();
    RuntimeCausticDistributedBeamCacheSettings3D preview;
    RuntimeCausticDistributedBeamCacheSettings3D inspection;
    RuntimeCausticDistributedBeamCacheSettings3D final;
    RuntimeCausticDistributedBeamCache3D_DefaultSettings(&preview);
    inspection = preview;
    final = preview;
    RuntimeCausticDistributedBeamCache3D_ApplyQualityTier(
        &inspection, RUNTIME_CAUSTIC_PHOTON_BUDGET_INSPECTION);
    RuntimeCausticDistributedBeamCache3D_ApplyQualityTier(
        &final, RUNTIME_CAUSTIC_PHOTON_BUDGET_FINAL);
    assert_true("sparse_cache_tier_segment_monotonic",
                preview.maxSegments < inspection.maxSegments &&
                    inspection.maxSegments < final.maxSegments);
    assert_true("sparse_cache_tier_memory_monotonic",
                preview.memoryCeilingBytes < inspection.memoryCeilingBytes &&
                    inspection.memoryCeilingBytes < final.memoryCeilingBytes);
    assert_true("sparse_cache_tier_work_monotonic",
                preview.maxCellTests < inspection.maxCellTests &&
                    inspection.maxCellTests < final.maxCellTests);
    return test_support_failures() - before;
}


int run_test_runtime_caustic_photon_sparse_cache_3d_tests(void) {
    int failures = 0;
    failures += test_sparse_cache_sparse_brick_storage_contract();
    failures += test_sparse_cache_sparse_dense_field_and_query_equivalence();
    failures += test_sparse_cache_oracle_acceleration_equivalence();
    failures += test_sparse_cache_direct_cache_sample_and_phase_equivalence();
    failures += test_sparse_cache_tier_bounds_fail_closed();
    failures += test_sparse_cache_tier_monotonicity();
    return failures;
}
