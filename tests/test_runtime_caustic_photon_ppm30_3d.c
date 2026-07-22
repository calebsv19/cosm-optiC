#include "test_runtime_caustic_photon_ppm30_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_integration_3d.h"
#include "render/runtime_caustic_photon_volume_beam_estimator_3d.h"
#include "test_support.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static RuntimeCausticPhotonVolumeBeamSegment3D ppm30_segment(void) {
    RuntimeCausticPhotonVolumeBeamSegment3D segment;
    memset(&segment, 0, sizeof(segment));
    segment.photonId = 30u;
    segment.start = vec3(0.0, 0.0, -1.0);
    segment.end = vec3(0.0, 0.0, 1.0);
    segment.direction = vec3(0.0, 0.0, 1.0);
    segment.flux = vec3(2.0, 1.0, 0.5);
    segment.radiusStart = 0.001;
    segment.radiusEnd = 0.002;
    segment.transmittance = 1.0;
    segment.densityWeight = 0.01;
    segment.mediumId = 0;
    segment.provenance.originalMediumId = 0;
    segment.provenance.segmentStage =
        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    segment.provenance.priorSpecularOrTransmission = true;
    segment.provenance.dielectricEntryCount = 1u;
    segment.provenance.dielectricExitCount = 1u;
    return segment;
}

static int test_ppm30_kernel_normalization(void) {
    const double radius = 0.37;
    const int steps = 20000;
    const double dr = radius / (double)steps;
    double integral = 0.0;
    for (int i = 0; i < steps; ++i) {
        const double r = ((double)i + 0.5) * dr;
        integral += RuntimeCausticPhotonVolumeBeamEstimator3D_CompactKernel(
                        r, radius) * 2.0 * M_PI * r * dr;
    }
    assert_close("ppm30_compact_kernel_normalized", integral, 1.0, 1.0e-8);
    assert_true("ppm30_compact_kernel_support",
                RuntimeCausticPhotonVolumeBeamEstimator3D_CompactKernel(
                    0.0, radius) > 0.0 &&
                    RuntimeCausticPhotonVolumeBeamEstimator3D_CompactKernel(
                        radius, radius) == 0.0 &&
                    RuntimeCausticPhotonVolumeBeamEstimator3D_CompactKernel(
                        radius * 2.0, radius) == 0.0);
    return 0;
}

static int test_ppm30_phase_normalization_and_camera_order(void) {
    const int steps = 40000;
    const double dmu = 2.0 / (double)steps;
    const double g = 0.55;
    double integral = 0.0;
    for (int i = 0; i < steps; ++i) {
        const double mu = -1.0 + ((double)i + 0.5) * dmu;
        integral += 2.0 * M_PI *
            RuntimeCausticPhotonVolumeBeamEstimator3D_HenyeyGreensteinPhase(
                mu, g) * dmu;
    }
    assert_close("ppm30_hg_phase_normalized", integral, 1.0, 1.0e-8);
    assert_true("ppm30_hg_forward_side_reverse_order",
                RuntimeCausticPhotonVolumeBeamEstimator3D_HenyeyGreensteinPhase(
                    1.0, g) >
                    RuntimeCausticPhotonVolumeBeamEstimator3D_HenyeyGreensteinPhase(
                        0.0, g) &&
                    RuntimeCausticPhotonVolumeBeamEstimator3D_HenyeyGreensteinPhase(
                        0.0, g) >
                    RuntimeCausticPhotonVolumeBeamEstimator3D_HenyeyGreensteinPhase(
                        -1.0, g));
    return 0;
}

static int test_ppm30_exact_once_radiance_and_vacuum(void) {
    RuntimeCausticPhotonVolumeBeamEstimatorSettings3D settings;
    RuntimeCausticPhotonVolumeBeamEstimatorInput3D input;
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D readback;
    const double density = 0.4;
    const double beam_distance = 2.0;
    const double step = 0.1;
    const double camera_t = 0.7;
    double expected_weight;

    RuntimeCausticPhotonVolumeBeamEstimator3D_DefaultSettings(&settings);
    settings.scatteringCoefficient = 0.8;
    settings.extinctionCoefficient = 1.1;
    settings.phaseAnisotropy = 0.3;
    memset(&input, 0, sizeof(input));
    input.beamFluxDensity = vec3(2.0, 1.0, 0.5);
    input.beamDirection = vec3(0.0, 0.0, 4.0);
    input.viewToCameraDirection = vec3(0.0, 0.0, 2.0);
    input.beamDistance = beam_distance;
    input.mediumDensity = density;
    input.cameraTransmittance = camera_t;
    input.stepLength = step;
    expected_weight = exp(-1.1 * density * beam_distance) *
        (1.0 - exp(-0.8 * density * step)) *
        RuntimeCausticPhotonVolumeBeamEstimator3D_HenyeyGreensteinPhase(
            1.0, 0.3) * camera_t;
    assert_true("ppm30_exact_once_evaluate",
                RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
                    &settings, &input, &readback));
    assert_close("ppm30_exact_once_weight", readback.integrationWeight,
                 expected_weight, 1.0e-14);
    assert_close("ppm30_exact_once_flux", readback.radiance.x,
                 2.0 * expected_weight, 1.0e-14);
    input.mediumDensity = 0.0;
    assert_true("ppm30_vacuum_is_zero",
                !RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
                    &settings, &input, &readback) &&
                    !readback.contributed && readback.radiance.x == 0.0);
    return 0;
}

static double ppm30_step_sum(int steps) {
    const double density = 0.4;
    const double sigma_s = 0.8;
    const double length = 2.0;
    const double ds = length / (double)steps;
    double result = 0.0;
    for (int i = 0; i < steps; ++i) {
        result += 1.0 - exp(-sigma_s * density * ds);
    }
    return result;
}

static int test_ppm30_step_refinement_converges(void) {
    const double limit = 0.8 * 0.4 * 2.0;
    const double coarse_error = fabs(ppm30_step_sum(16) - limit);
    const double medium_error = fabs(ppm30_step_sum(64) - limit);
    const double fine_error = fabs(ppm30_step_sum(256) - limit);
    assert_true("ppm30_step_refinement_monotonic",
                fine_error < medium_error && medium_error < coarse_error);
    assert_true("ppm30_step_refinement_fine_bounded", fine_error < 0.001);
    return 0;
}

static int test_ppm30_stage_medium_and_clip_contract(void) {
    RuntimeCausticPhotonVolumeBeamEstimatorSettings3D settings;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = ppm30_segment();
    RuntimeCausticPhotonVolumeBeamSegment3D clipped;
    RuntimeCausticPhotonVolumeBeamEligibility3D eligibility;

    RuntimeCausticPhotonVolumeBeamEstimator3D_DefaultSettings(&settings);
    eligibility = RuntimeCausticPhotonVolumeBeamEstimator3D_SegmentEligibility(
        &segment, &settings);
    assert_true("ppm30_post_lens_medium_zero_eligible",
                eligibility == RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_ELIGIBLE);
    segment.provenance.segmentStage =
        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_LENS_INTERIOR;
    assert_true("ppm30_lens_interior_reason_coded",
                RuntimeCausticPhotonVolumeBeamEstimator3D_SegmentEligibility(
                    &segment, &settings) ==
                    RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_STAGE);
    segment.provenance.segmentStage =
        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    segment.mediumId = 1;
    assert_true("ppm30_medium_mismatch_reason_coded",
                RuntimeCausticPhotonVolumeBeamEstimator3D_SegmentEligibility(
                    &segment, &settings) ==
                    RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_MEDIUM);
    segment.mediumId = 0;
    assert_true("ppm30_clip_to_participating_bounds",
                RuntimeCausticPhotonVolumeBeamEstimator3D_ClipSegmentToBounds(
                    &segment,
                    vec3(-0.5, -0.5, -0.25),
                    vec3(0.5, 0.5, 0.50),
                    &clipped));
    assert_close("ppm30_clip_start", clipped.start.z, -0.25, 1.0e-12);
    assert_close("ppm30_clip_end", clipped.end.z, 0.50, 1.0e-12);
    assert_true("ppm30_clip_preserves_identity",
                clipped.mediumId == 0 &&
                    clipped.provenance.originalMediumId == 0 &&
                    clipped.provenance.segmentStage ==
                        RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS &&
                    clipped.provenance.dielectricEntryCount == 1u &&
                    clipped.provenance.dielectricExitCount == 1u);
    return 0;
}

static bool ppm30_query(RuntimeCausticBeamMap3D* map,
                        Vec3 direction,
                        double radius,
                        RuntimeCausticBeamMapQueryResult3D* result) {
    RuntimeCausticBeamMapQuery3D query;
    RuntimeCausticBeamMap3D_DefaultQuery(&query);
    query.position = vec3(0.05, 0.0, 0.0);
    query.direction = direction;
    query.radius = radius;
    query.mediumId = 0;
    query.requireMediumId = true;
    query.segmentStage = RUNTIME_CAUSTIC_PHOTON_SEGMENT_STAGE_POST_LENS;
    query.requireSegmentStage = true;
    query.estimator.estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS;
    query.estimator.minimumEffectiveSamples = 1u;
    return RuntimeCausticBeamMap3D_Query(map, &query, result);
}

static int test_ppm30_beam_map_is_camera_independent(void) {
    RuntimeCausticBeamMap3D map;
    RuntimeCausticPhotonVolumeBeamSegment3D segment = ppm30_segment();
    RuntimeCausticBeamMapQueryResult3D forward;
    RuntimeCausticBeamMapQueryResult3D reverse;
    RuntimeCausticBeamMapQueryResult3D side;
    RuntimeCausticBeamMapQueryResult3D narrow;

    RuntimeCausticBeamMap3D_Init(&map);
    assert_true("ppm30_beam_map_allocate",
                RuntimeCausticBeamMap3D_Allocate(&map, 1u));
    segment.radiusStart = 10.0;
    segment.radiusEnd = 20.0;
    segment.densityWeight = 99.0;
    assert_true("ppm30_beam_map_store",
                RuntimeCausticBeamMap3D_StoreSegment(&map, &segment));
    assert_true("ppm30_beam_map_camera_relations_hit",
                ppm30_query(&map, vec3(0.0, 0.0, 1.0), 0.10, &forward) &&
                    ppm30_query(&map, vec3(0.0, 0.0, -1.0), 0.10, &reverse) &&
                    ppm30_query(&map, vec3(1.0, 0.0, 0.0), 0.10, &side));
    assert_close("ppm30_beam_irradiance_forward_reverse",
                 forward.physicalFlux.x, reverse.physicalFlux.x, 1.0e-12);
    assert_close("ppm30_beam_irradiance_forward_side",
                 forward.physicalFlux.x, side.physicalFlux.x, 1.0e-12);
    assert_true("ppm30_physical_query_radius_controls_support",
                !ppm30_query(&map, vec3(0.0, 0.0, 1.0), 0.04, &narrow) &&
                    narrow.radiusRejectCount == 1u);
    RuntimeCausticBeamMap3D_Free(&map);
    return 0;
}

static int test_ppm30_distance_and_phase_are_physical(void) {
    RuntimeCausticPhotonVolumeBeamEstimatorSettings3D settings;
    RuntimeCausticPhotonVolumeBeamEstimatorInput3D input;
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D near_forward;
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D far_forward;
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D side;
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D reverse;

    RuntimeCausticPhotonVolumeBeamEstimator3D_DefaultSettings(&settings);
    memset(&input, 0, sizeof(input));
    input.beamFluxDensity = vec3(1.0, 1.0, 1.0);
    input.beamDirection = vec3(0.0, 0.0, 1.0);
    input.viewToCameraDirection = vec3(0.0, 0.0, 1.0);
    input.mediumDensity = 0.5;
    input.cameraTransmittance = 1.0;
    input.stepLength = 0.1;
    input.beamDistance = 0.25;
    assert_true("ppm30_near_forward",
                RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
                    &settings, &input, &near_forward));
    input.beamDistance = 2.0;
    assert_true("ppm30_far_forward",
                RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
                    &settings, &input, &far_forward));
    input.beamDistance = 0.25;
    input.viewToCameraDirection = vec3(1.0, 0.0, 0.0);
    assert_true("ppm30_side_camera",
                RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
                    &settings, &input, &side));
    input.viewToCameraDirection = vec3(0.0, 0.0, -1.0);
    assert_true("ppm30_reverse_camera",
                RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
                    &settings, &input, &reverse));
    assert_true("ppm30_beam_distance_attenuates",
                near_forward.radiance.x > far_forward.radiance.x);
    assert_true("ppm30_camera_phase_orders_radiance",
                near_forward.radiance.x > side.radiance.x &&
                    side.radiance.x > reverse.radiance.x);
    return 0;
}

static int test_ppm30_settings_normalize(void) {
    RuntimeCausticPhotonIntegrationSettings3D settings;
    RuntimeCausticPhotonIntegration3D_DefaultSettings(&settings);
    settings.volumeMediumId = -3;
    settings.volumeQueryRadius = NAN;
    settings.volumeScatteringCoefficient = 2.0;
    settings.volumeExtinctionCoefficient = 0.5;
    settings.volumePhaseAnisotropy = 2.0;
    RuntimeCausticPhotonIntegration3D_NormalizeSettings(&settings);
    assert_true("ppm30_settings_normalized",
                settings.volumeMediumId == 0 &&
                    settings.volumeQueryRadius == 0.10 &&
                    settings.volumeExtinctionCoefficient == 2.0 &&
                    settings.volumePhaseAnisotropy == 0.95);
    return 0;
}

int run_test_runtime_caustic_photon_ppm30_3d_tests(void) {
    int failures = 0;
    failures += test_ppm30_kernel_normalization();
    failures += test_ppm30_phase_normalization_and_camera_order();
    failures += test_ppm30_exact_once_radiance_and_vacuum();
    failures += test_ppm30_step_refinement_converges();
    failures += test_ppm30_stage_medium_and_clip_contract();
    failures += test_ppm30_beam_map_is_camera_independent();
    failures += test_ppm30_distance_and_phase_are_physical();
    failures += test_ppm30_settings_normalize();
    return failures;
}
