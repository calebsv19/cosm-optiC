#include "test_runtime_caustic_photon_estimator_3d.h"

#include "render/runtime_caustic_photon_estimator_3d.h"
#include "test_support.h"

#include <string.h>

static int test_runtime_caustic_photon_estimator_labels(void) {
    RuntimeCausticPhotonEstimator3D estimator =
        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS;

    assert_true("runtime_caustic_photon_estimator_radius_label",
                RuntimeCausticPhotonEstimator3D_FromLabel("radius", &estimator) &&
                    estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS);
    assert_true("runtime_caustic_photon_estimator_knn_label",
                RuntimeCausticPhotonEstimator3D_FromLabel("k_nearest", &estimator) &&
                    estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST);
    assert_true("runtime_caustic_photon_estimator_knn_alias",
                RuntimeCausticPhotonEstimator3D_FromLabel("knn", &estimator) &&
                    estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST);
    assert_true("runtime_caustic_photon_estimator_neighbor_gather_label",
                RuntimeCausticPhotonEstimator3D_FromLabel(
                    "neighbor_gather", &estimator) &&
                    estimator ==
                        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_NEIGHBOR_GATHER);
    assert_true("runtime_caustic_photon_estimator_budget_scaled_gather_label",
                RuntimeCausticPhotonEstimator3D_FromLabel(
                    "budget_scaled_gather", &estimator) &&
                    estimator ==
                        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_BUDGET_SCALED_GATHER);
    assert_true("runtime_caustic_photon_estimator_population_scaled_gather_label",
                RuntimeCausticPhotonEstimator3D_FromLabel(
                    "population_scaled_gather", &estimator) &&
                    estimator ==
                        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER);
    assert_true("runtime_caustic_photon_estimator_invalid_label",
                !RuntimeCausticPhotonEstimator3D_FromLabel("adaptive", &estimator));
    assert_true("runtime_caustic_photon_estimator_canonical_labels",
                strcmp(RuntimeCausticPhotonEstimator3D_Label(
                           RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS),
                       "radius") == 0 &&
                    strcmp(RuntimeCausticPhotonEstimator3D_Label(
                               RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST),
                           "k_nearest") == 0 &&
                    strcmp(RuntimeCausticPhotonEstimator3D_Label(
                               RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_NEIGHBOR_GATHER),
                           "neighbor_gather") == 0 &&
                    strcmp(RuntimeCausticPhotonEstimator3D_Label(
                               RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_BUDGET_SCALED_GATHER),
                           "budget_scaled_gather") == 0 &&
                    strcmp(RuntimeCausticPhotonEstimator3D_Label(
                               RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER),
                           "population_scaled_gather") == 0);
    return 0;
}

static int test_runtime_caustic_photon_estimator_bounded_settings(void) {
    RuntimeCausticPhotonEstimatorSettings3D settings;

    RuntimeCausticPhotonEstimator3D_DefaultSettings(&settings);
    assert_true("runtime_caustic_photon_estimator_radius_default",
                settings.estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS &&
                    settings.neighborLimit == 32u &&
                    settings.minimumEffectiveSamples == 1u);
    settings.estimator = (RuntimeCausticPhotonEstimator3D)99;
    settings.neighborLimit = 99999u;
    settings.minimumEffectiveSamples = 99999u;
    RuntimeCausticPhotonEstimator3D_NormalizeSettings(&settings);
    assert_true("runtime_caustic_photon_estimator_bounds",
                settings.estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS &&
                    settings.neighborLimit == 4096u &&
                    settings.minimumEffectiveSamples == 4096u);
    assert_true("runtime_caustic_photon_estimator_ppm24a_implementation",
                RuntimeCausticPhotonEstimator3D_IsImplemented(
                    RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS) &&
                    RuntimeCausticPhotonEstimator3D_IsImplemented(
                        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST) &&
                    RuntimeCausticPhotonEstimator3D_IsImplemented(
                        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_NEIGHBOR_GATHER) &&
                    RuntimeCausticPhotonEstimator3D_IsImplemented(
                        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_BUDGET_SCALED_GATHER) &&
                    RuntimeCausticPhotonEstimator3D_IsImplemented(
                        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER));
    return 0;
}

static int test_runtime_caustic_photon_estimator_deterministic_top_k(void) {
    RuntimeCausticPhotonEstimatorCandidate3D candidates[2] = {0};
    RuntimeCausticPhotonEstimatorCandidate3D candidate = {0};
    uint64_t count = 0u;

    candidate.distance = 0.2;
    candidate.photonId = 20u;
    candidate.storageIndex = 0u;
    assert_true("runtime_caustic_photon_estimator_insert_first",
                RuntimeCausticPhotonEstimator3D_InsertCandidate(
                    candidates, &count, 2u, candidate));
    candidate.distance = 0.1;
    candidate.photonId = 30u;
    candidate.storageIndex = 1u;
    assert_true("runtime_caustic_photon_estimator_insert_nearest",
                RuntimeCausticPhotonEstimator3D_InsertCandidate(
                    candidates, &count, 2u, candidate));
    candidate.distance = 0.1;
    candidate.photonId = 10u;
    candidate.storageIndex = 2u;
    assert_true("runtime_caustic_photon_estimator_insert_tie",
                RuntimeCausticPhotonEstimator3D_InsertCandidate(
                    candidates, &count, 2u, candidate));
    assert_true("runtime_caustic_photon_estimator_order",
                count == 2u && candidates[0].photonId == 10u &&
                    candidates[1].photonId == 30u);
    return 0;
}

static int test_runtime_caustic_photon_estimator_convergent_neighbor_growth(void) {
    const uint64_t k4 = RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
        4096u, 64u, 256u);
    const uint64_t k16 = RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
        16384u, 64u, 256u);
    const uint64_t k64 = RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
        65536u, 64u, 256u);
    assert_true("runtime_caustic_photon_estimator_convergent_k_growth",
                k4 == 64u && k16 == 128u && k64 == 256u &&
                    k4 < k16 && k16 < k64);
    assert_true("runtime_caustic_photon_estimator_convergent_fraction_shrinks",
                (double)k16 / 16384.0 < (double)k4 / 4096.0 &&
                    (double)k64 / 65536.0 < (double)k16 / 16384.0);
    assert_true("runtime_caustic_photon_estimator_convergent_bounds",
                RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
                    9u, 4u, 256u) == 4u &&
                    RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
                        1000000u, 64u, 256u) == 256u);
    return 0;
}

static int test_runtime_caustic_photon_estimator_budget_scaled_growth(void) {
    assert_true(
        "runtime_caustic_photon_estimator_budget_scaled_reference",
        RuntimeCausticPhotonEstimator3D_BudgetScaledNeighborLimit(
            4096u, 4096u, 8u, 4u, 256u) == 8u);
    assert_true(
        "runtime_caustic_photon_estimator_budget_scaled_16k",
        RuntimeCausticPhotonEstimator3D_BudgetScaledNeighborLimit(
            16384u, 4096u, 8u, 4u, 256u) == 32u);
    assert_true(
        "runtime_caustic_photon_estimator_budget_scaled_64k",
        RuntimeCausticPhotonEstimator3D_BudgetScaledNeighborLimit(
            65536u, 4096u, 8u, 4u, 256u) == 128u);
    assert_true(
        "runtime_caustic_photon_estimator_budget_scaled_bounds",
        RuntimeCausticPhotonEstimator3D_BudgetScaledNeighborLimit(
            1u, 4096u, 8u, 4u, 256u) == 4u &&
            RuntimeCausticPhotonEstimator3D_BudgetScaledNeighborLimit(
                1000000u, 4096u, 8u, 4u, 256u) == 256u);
    return 0;
}

int run_test_runtime_caustic_photon_estimator_3d_tests(void) {
    int failures = 0;
    failures += test_runtime_caustic_photon_estimator_labels();
    failures += test_runtime_caustic_photon_estimator_bounded_settings();
    failures += test_runtime_caustic_photon_estimator_deterministic_top_k();
    failures += test_runtime_caustic_photon_estimator_convergent_neighbor_growth();
    failures += test_runtime_caustic_photon_estimator_budget_scaled_growth();
    return failures;
}
