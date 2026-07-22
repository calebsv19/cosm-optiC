#include "test_runtime_caustic_photon_volume_segment_normalization_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_distributed_beam_cache_3d.h"
#include "render/runtime_caustic_photon_volume_segment_normalization_3d.h"
#include "test_support.h"

static bool normalization_test_grid(RuntimeVolumeGrid3D* grid) {
    return RuntimeVolumeGrid3D_Configure(
        grid, 1u, 20u, 20u, 20u, 0.0, 0u, 0.0,
        vec3(-1.0, -1.0, 0.0), 0.1, vec3(0.0, 1.0, 0.0), 0u);
}

static RuntimeCausticPhotonVolumeBeamSegment3D normalization_test_segment(void) {
    RuntimeCausticPhotonVolumeBeamSegment3D segment;
    memset(&segment, 0, sizeof(segment));
    segment.photonId = 1u;
    segment.start = vec3(0.0, 0.0, 0.5);
    segment.end = vec3(0.0, 0.0, 1.5);
    segment.direction = vec3(0.0, 0.0, 1.0);
    segment.flux = vec3(1.0, 0.5, 0.25);
    segment.radiusStart = 0.1;
    segment.radiusEnd = 0.1;
    segment.transmittance = 0.8;
    segment.densityWeight = 1.0;
    segment.mediumId = 0;
    segment.provenance.segmentStage =
        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    return segment;
}

static RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D
normalization_test_settings(void) {
    RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D settings;
    RuntimeCausticPhotonVolumeSegmentNormalization3D_DefaultSettings(&settings);
    settings.queryRadius = 0.1;
    settings.targetVoxelSize = 0.1;
    settings.maxSegments = 8u;
    settings.maxAxialSamples = 100000u;
    settings.maxCellTests = 1000000u;
    return settings;
}

static int test_volume_segment_normalization_numeric_contract(void) {
    const int before = test_support_failures();
    RuntimeVolumeGrid3D grid;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = normalization_test_segment();
    RuntimeCausticPhotonVolumeBeamSegment3D reverse = segment;
    RuntimeCausticPhotonVolumeBeamSegment3D degenerate = segment;
    RuntimeCausticPhotonVolumeSegmentNormalizationResult3D forward_result;
    RuntimeCausticPhotonVolumeSegmentNormalizationResult3D reverse_result;
    RuntimeCausticPhotonVolumeSegmentNormalizationResult3D degenerate_result;
    assert_true("volume_segment_normalization_grid", normalization_test_grid(&grid));
    assert_true("volume_segment_normalization_forward",
                RuntimeCausticPhotonVolumeSegmentNormalization3D_EvaluateClipped(
                    &segment, &grid, 0.1, true, 100000u, 1000000u,
                    &forward_result));
    reverse.start = segment.end;
    reverse.end = segment.start;
    reverse.direction = vec3_scale(segment.direction, -1.0);
    assert_true("volume_segment_normalization_reverse",
                RuntimeCausticPhotonVolumeSegmentNormalization3D_EvaluateClipped(
                    &reverse, &grid, 0.1, true, 100000u, 1000000u,
                    &reverse_result));
    degenerate.end = degenerate.start;
    assert_true("volume_segment_normalization_degenerate_zero",
                !RuntimeCausticPhotonVolumeSegmentNormalization3D_EvaluateClipped(
                    &degenerate, &grid, 0.1, true, 100000u, 1000000u,
                    &degenerate_result) &&
                    degenerate_result.degenerate &&
                    degenerate_result.scale == 0.0);
    assert_true("volume_segment_normalization_finite_nonnegative",
                isfinite(forward_result.discreteIntegral) &&
                    isfinite(forward_result.scale) &&
                    forward_result.discreteIntegral > 0.0 &&
                    forward_result.scale >= 0.0);
    assert_close("volume_segment_normalization_reversal_integral",
                 reverse_result.discreteIntegral,
                 forward_result.discreteIntegral,
                 1.0e-12);
    assert_close("volume_segment_normalization_reversal_scale",
                 reverse_result.scale, forward_result.scale, 1.0e-12);
    assert_close("volume_segment_normalization_conserves_length",
                 forward_result.discreteIntegral * forward_result.scale,
                 forward_result.segmentLength,
                 1.0e-12);
    return test_support_failures() - before;
}

static int test_volume_segment_normalization_query_contract(void) {
    const int before = test_support_failures();
    RuntimeVolumeGrid3D grid;
    RuntimeCausticBeamMap3D map;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = normalization_test_segment();
    RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D settings =
        normalization_test_settings();
    RuntimeCausticPhotonVolumeSegmentNormalizationReadback3D readback;
    RuntimeCausticBeamMapQuery3D query;
    RuntimeCausticBeamMapQueryResult3D endpoint;
    RuntimeCausticBeamMapQueryResult3D interior;
    RuntimeCausticBeamMapQueryResult3D beyond_endpoint;
    RuntimeCausticBeamMapQueryResult3D doubled;
    RuntimeCausticBeamMap3D_Init(&map);
    assert_true("volume_segment_normalization_query_grid", normalization_test_grid(&grid));
    assert_true("volume_segment_normalization_query_allocate",
                RuntimeCausticBeamMap3D_Allocate(&map, 2u));
    assert_true("volume_segment_normalization_query_store",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    assert_true("volume_segment_normalization_query_prepare",
                RuntimeCausticPhotonVolumeSegmentNormalization3D_PrepareMap(
                    &map, &grid, &settings, &readback));
    RuntimeCausticBeamMap3D_DefaultQuery(&query);
    query.radius = 0.1;
    query.mediumId = 0;
    query.requireMediumId = true;
    query.segmentStage = RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    query.requireSegmentStage = true;
    query.estimator.minimumEffectiveSamples = 1u;
    query.position = segment.start;
    assert_true("volume_segment_normalization_endpoint",
                RuntimeCausticBeamMap3D_Query(&map, &query, &endpoint));
    query.position = vec3_lerp(segment.start, segment.end, 0.5);
    assert_true("volume_segment_normalization_interior",
                RuntimeCausticBeamMap3D_Query(&map, &query, &interior));
    query.position = vec3(0.0, 0.0, segment.end.z + 0.05);
    assert_true("volume_segment_normalization_beyond_endpoint",
                RuntimeCausticBeamMap3D_Query(&map, &query, &beyond_endpoint));
    assert_close("volume_segment_normalization_endpoint_interior",
                 endpoint.physicalFlux.x, interior.physicalFlux.x, 1.0e-12);
    assert_true("volume_segment_normalization_endpoint_falloff",
                beyond_endpoint.physicalFlux.x > 0.0 &&
                    beyond_endpoint.physicalFlux.x < endpoint.physicalFlux.x);
    map.segments[0].flux = vec3_scale(map.segments[0].flux, 2.0);
    query.position = vec3_lerp(segment.start, segment.end, 0.5);
    assert_true("volume_segment_normalization_linearity_query",
                RuntimeCausticBeamMap3D_Query(&map, &query, &doubled));
    assert_close("volume_segment_normalization_constant_radiance_linearity",
                 doubled.physicalFlux.x, interior.physicalFlux.x * 2.0,
                 1.0e-12);
    RuntimeCausticBeamMap3D_Free(&map);
    return test_support_failures() - before;
}

static int test_volume_segment_normalization_direct_cache_identity(void) {
    const int before = test_support_failures();
    RuntimeVolumeGrid3D grid;
    RuntimeCausticBeamMap3D map;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = normalization_test_segment();
    RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D normalization =
        normalization_test_settings();
    RuntimeCausticPhotonVolumeSegmentNormalizationReadback3D prepared;
    RuntimeCausticDistributedBeamCacheSettings3D cache_settings;
    RuntimeCausticDistributedBeamCacheReadback3D cache_readback;
    double direct_scale;
    RuntimeCausticBeamMap3D_Init(&map);
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("volume_segment_normalization_identity_grid", normalization_test_grid(&grid));
    assert_true("volume_segment_normalization_identity_allocate_map",
                RuntimeCausticBeamMap3D_Allocate(&map, 2u));
    assert_true("volume_segment_normalization_identity_store",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    assert_true("volume_segment_normalization_identity_prepare",
                RuntimeCausticPhotonVolumeSegmentNormalization3D_PrepareMap(
                    &map, &grid, &normalization, &prepared));
    direct_scale = map.segmentFiniteNormalization[0];
    assert_true("volume_segment_normalization_identity_allocate_cache",
                RuntimeCausticVolumeCache3D_Allocate(&cache, &grid));
    RuntimeCausticDistributedBeamCache3D_DefaultSettings(&cache_settings);
    cache_settings.queryRadius = 0.1;
    cache_settings.maxSegments = 8u;
    cache_settings.maxAxialSamples = 100000u;
    cache_settings.maxCellTests = 2000000u;
    assert_true("volume_segment_normalization_identity_cache_build",
                RuntimeCausticDistributedBeamCache3D_Build(
                    &cache, &map, &cache_settings, &cache_readback));
    assert_close("volume_segment_normalization_direct_cache_same_scale",
                 map.segmentFiniteNormalization[0], direct_scale, 0.0);
    assert_close("volume_segment_normalization_cache_readback_same_scale",
                 cache_readback.segmentNormalizationScaleMean,
                 direct_scale,
                 0.0);
    assert_close("volume_segment_normalization_cache_conservation",
                 cache_readback.integratedFluxRelativeError, 0.0, 1.0e-12);
    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeCausticBeamMap3D_Free(&map);
    return test_support_failures() - before;
}

int run_test_runtime_caustic_photon_volume_segment_normalization_3d_tests(void) {
    int failures = 0;
    failures += test_volume_segment_normalization_numeric_contract();
    failures += test_volume_segment_normalization_query_contract();
    failures += test_volume_segment_normalization_direct_cache_identity();
    return failures;
}
