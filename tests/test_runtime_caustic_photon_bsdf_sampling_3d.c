#include "test_runtime_caustic_photon_bsdf_sampling_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_bsdf_policy_3d.h"
#include "render/runtime_caustic_photon_bsdf_sampling_3d.h"
#include "test_support.h"

static RuntimeCausticPhotonSample3D photon_bsdf_sampling_photon(uint64_t index) {
    RuntimeCausticPhotonSample3D photon;
    memset(&photon, 0, sizeof(photon));
    photon.photonId = 900000u + index;
    photon.sampleIndex = index;
    photon.rngSeed = 0x71c53a9du;
    photon.direction = vec3(0.0, 0.0, -1.0);
    photon.flux = vec3(2.0, 1.0, 0.5);
    photon.emissionPdf = 0.5;
    return photon;
}

static RuntimeMaterialPayload3D photon_bsdf_sampling_material(void) {
    RuntimeMaterialPayload3D material;
    RuntimeMaterialPayload3D_Reset(&material);
    material.valid = true;
    material.baseColorR = 0.8;
    material.baseColorG = 0.6;
    material.baseColorB = 0.4;
    material.transparency = 0.4;
    material.opticalIor = 1.5;
    material.bsdf.ior = 1.5;
    material.bsdf.diffuseWeight = 0.5;
    material.bsdf.specWeight = 0.5;
    material.bsdf.roughness = 0.2;
    return material;
}

static int test_runtime_caustic_photon_bsdf_sampling_replay_and_identity(void) {
    RuntimeCausticPhotonSample3D photon = photon_bsdf_sampling_photon(17u);
    RuntimeCausticPhotonSample3D changed = photon;
    RuntimeCausticPhotonBsdfSampleStream3D a;
    RuntimeCausticPhotonBsdfSampleStream3D repeat;
    RuntimeCausticPhotonBsdfSampleStream3D different;

    assert_true("runtime_caustic_photon_bsdf_sampling_generate",
                RuntimeCausticPhotonBsdfSampling3D_Generate(&photon, 3u, &a));
    assert_true("runtime_caustic_photon_bsdf_sampling_replay",
                RuntimeCausticPhotonBsdfSampling3D_Generate(&photon, 3u, &repeat) &&
                    memcmp(&a, &repeat, sizeof(a)) == 0);
    changed.photonId++;
    assert_true("runtime_caustic_photon_bsdf_sampling_changed_identity",
                RuntimeCausticPhotonBsdfSampling3D_Generate(&changed, 3u, &different) &&
                    different.baseSeed != a.baseSeed &&
                    (different.bsdfSample.lobeUnitSample !=
                         a.bsdfSample.lobeUnitSample ||
                     different.bsdfSample.directionSample.unitU !=
                         a.bsdfSample.directionSample.unitU));
    assert_true("runtime_caustic_photon_bsdf_sampling_changed_depth",
                RuntimeCausticPhotonBsdfSampling3D_Generate(&photon, 4u, &different) &&
                    different.baseSeed != a.baseSeed);
    assert_true("runtime_caustic_photon_bsdf_sampling_invalid_depth",
                !RuntimeCausticPhotonBsdfSampling3D_Generate(&photon, 0u, &different));
    return 0;
}

static int test_runtime_caustic_photon_bsdf_sampling_statistics(void) {
    const uint64_t sample_count = 32768u;
    RuntimeMaterialPayload3D material = photon_bsdf_sampling_material();
    RuntimeCausticPhotonBsdfPolicy3D policy;
    uint64_t lobe_counts[RUNTIME_CAUSTIC_PHOTON_BSDF_LOBE_EMISSIVE + 1u] = {0};
    Vec3 selected_sum = vec3(0.0, 0.0, 0.0);

    assert_true("runtime_caustic_photon_bsdf_sampling_statistics_policy",
                RuntimeCausticPhotonBsdfPolicy3D_Build(
                    &material, 1.0, vec3(2.0, 1.0, 0.5), &policy));
    for (uint64_t i = 0u; i < sample_count; ++i) {
        RuntimeCausticPhotonSample3D photon = photon_bsdf_sampling_photon(i);
        RuntimeCausticPhotonBsdfSampleStream3D stream;
        RuntimeCausticPhotonBsdfSelection3D selection;
        if (!RuntimeCausticPhotonBsdfSampling3D_Generate(&photon, 1u, &stream) ||
            !RuntimeCausticPhotonBsdfPolicy3D_Select(
                &policy, stream.bsdfSample.lobeUnitSample, &selection)) {
            assert_true("runtime_caustic_photon_bsdf_sampling_statistics_select", false);
            return 0;
        }
        lobe_counts[selection.lobe]++;
        selected_sum = vec3_add(selected_sum, selection.throughputAfter);
    }

    for (uint32_t i = 0u; i < policy.candidateCount; ++i) {
        const RuntimeCausticPhotonBsdfCandidate3D* candidate = &policy.candidates[i];
        double observed = (double)lobe_counts[candidate->lobe] / (double)sample_count;
        assert_close("runtime_caustic_photon_bsdf_sampling_lobe_frequency",
                     observed,
                     candidate->selectionPdf,
                     0.0125);
    }
    selected_sum = vec3_scale(selected_sum, 1.0 / (double)sample_count);
    assert_close("runtime_caustic_photon_bsdf_sampling_expected_r",
                 selected_sum.x,
                 policy.expectedScatteredThroughput.x,
                 0.03);
    assert_close("runtime_caustic_photon_bsdf_sampling_expected_g",
                 selected_sum.y,
                 policy.expectedScatteredThroughput.y,
                 0.02);
    assert_close("runtime_caustic_photon_bsdf_sampling_expected_b",
                 selected_sum.z,
                 policy.expectedScatteredThroughput.z,
                 0.01);
    return 0;
}

static int test_runtime_caustic_photon_bsdf_roulette_ledger(void) {
    RuntimePathDepthPolicy3D policy = {0};
    RuntimeCausticPhotonRoulette3D roulette;
    const Vec3 throughput = vec3(0.25, 0.25, 0.25);

    policy.minDepthBeforeRoulette = 2;
    policy.rouletteThreshold = 1.0;
    assert_true("runtime_caustic_photon_bsdf_roulette_before_min_depth",
                RuntimeCausticPhotonBsdfSampling3D_EvaluateRoulette(
                    &policy, 1u, throughput, 0.9, &roulette) &&
                    roulette.valid && !roulette.evaluated && !roulette.terminated &&
                    roulette.branchPdf == 1.0 && roulette.throughputAfter.x == 0.25);
    assert_true("runtime_caustic_photon_bsdf_roulette_survives",
                RuntimeCausticPhotonBsdfSampling3D_EvaluateRoulette(
                    &policy, 2u, throughput, 0.1, &roulette) &&
                    roulette.evaluated && !roulette.terminated);
    assert_close("runtime_caustic_photon_bsdf_roulette_survival_probability",
                 roulette.survivalProbability,
                 0.25,
                 1.0e-12);
    assert_close("runtime_caustic_photon_bsdf_roulette_survival_weight",
                 roulette.throughputAfter.x,
                 1.0,
                 1.0e-12);
    assert_true("runtime_caustic_photon_bsdf_roulette_terminates",
                RuntimeCausticPhotonBsdfSampling3D_EvaluateRoulette(
                    &policy, 2u, throughput, 0.9, &roulette) &&
                    roulette.evaluated && roulette.terminated &&
                    roulette.branchPdf == 0.75 && roulette.throughputAfter.x == 0.0 &&
                    roulette.terminatedThroughput.x == 0.25);
    assert_true("runtime_caustic_photon_bsdf_roulette_rejects_unit_endpoint",
                !RuntimeCausticPhotonBsdfSampling3D_EvaluateRoulette(
                    &policy, 2u, throughput, 1.0, &roulette));
    return 0;
}

static int test_runtime_caustic_photon_bsdf_roulette_statistics(void) {
    const uint64_t sample_count = 32768u;
    RuntimePathDepthPolicy3D policy = {0};
    const Vec3 throughput = vec3(0.25, 0.125, 0.0625);
    Vec3 weighted_sum = vec3(0.0, 0.0, 0.0);
    double expected_survival;
    uint64_t survived = 0u;

    policy.minDepthBeforeRoulette = 2;
    policy.rouletteThreshold = 1.0;
    expected_survival = RuntimePathDepthPolicy3D_SurvivalProbability(
        &policy,
        2,
        0.2126 * throughput.x + 0.7152 * throughput.y + 0.0722 * throughput.z);
    for (uint64_t i = 0u; i < sample_count; ++i) {
        RuntimeCausticPhotonSample3D photon = photon_bsdf_sampling_photon(i);
        RuntimeCausticPhotonBsdfSampleStream3D stream;
        RuntimeCausticPhotonRoulette3D roulette;
        if (!RuntimeCausticPhotonBsdfSampling3D_Generate(&photon, 2u, &stream) ||
            !RuntimeCausticPhotonBsdfSampling3D_EvaluateRoulette(
                &policy, 2u, throughput, stream.rouletteUnitSample, &roulette)) {
            assert_true("runtime_caustic_photon_bsdf_roulette_statistics_evaluate", false);
            return 0;
        }
        if (!roulette.terminated) survived++;
        weighted_sum = vec3_add(weighted_sum, roulette.throughputAfter);
    }
    assert_close("runtime_caustic_photon_bsdf_roulette_survival_frequency",
                 (double)survived / (double)sample_count,
                 expected_survival,
                 0.0125);
    weighted_sum = vec3_scale(weighted_sum, 1.0 / (double)sample_count);
    assert_close("runtime_caustic_photon_bsdf_roulette_unbiased_r",
                 weighted_sum.x,
                 throughput.x,
                 0.0125);
    assert_close("runtime_caustic_photon_bsdf_roulette_unbiased_g",
                 weighted_sum.y,
                 throughput.y,
                 0.00625);
    assert_close("runtime_caustic_photon_bsdf_roulette_unbiased_b",
                 weighted_sum.z,
                 throughput.z,
                 0.003125);
    return 0;
}

int run_test_runtime_caustic_photon_bsdf_sampling_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_bsdf_sampling_replay_and_identity();
    failures += test_runtime_caustic_photon_bsdf_sampling_statistics();
    failures += test_runtime_caustic_photon_bsdf_roulette_ledger();
    failures += test_runtime_caustic_photon_bsdf_roulette_statistics();
    return failures;
}
