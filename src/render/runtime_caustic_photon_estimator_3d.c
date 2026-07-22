#include "render/runtime_caustic_photon_estimator_3d.h"

#include <math.h>
#include <string.h>

enum {
    RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_DEFAULT_NEIGHBOR_LIMIT = 32,
    RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_MAX_NEIGHBOR_LIMIT = 4096
};

void RuntimeCausticPhotonEstimator3D_DefaultSettings(
    RuntimeCausticPhotonEstimatorSettings3D* settings) {
    if (!settings) return;
    settings->estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS;
    settings->neighborLimit =
        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_DEFAULT_NEIGHBOR_LIMIT;
    settings->minimumEffectiveSamples = 1u;
}

void RuntimeCausticPhotonEstimator3D_NormalizeSettings(
    RuntimeCausticPhotonEstimatorSettings3D* settings) {
    if (!settings) return;
    if (settings->estimator != RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS &&
        settings->estimator != RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST &&
        settings->estimator != RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_NEIGHBOR_GATHER &&
        settings->estimator !=
            RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_BUDGET_SCALED_GATHER &&
        settings->estimator !=
            RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER) {
        settings->estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS;
    }
    if (settings->neighborLimit == 0u) {
        settings->neighborLimit =
            RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_DEFAULT_NEIGHBOR_LIMIT;
    }
    if (settings->neighborLimit >
        RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_MAX_NEIGHBOR_LIMIT) {
        settings->neighborLimit =
            RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_MAX_NEIGHBOR_LIMIT;
    }
    if (settings->minimumEffectiveSamples == 0u) {
        settings->minimumEffectiveSamples = 1u;
    }
    if (settings->minimumEffectiveSamples > settings->neighborLimit) {
        settings->minimumEffectiveSamples = settings->neighborLimit;
    }
}

uint64_t RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
    uint64_t record_count,
    uint64_t minimum_neighbors,
    uint64_t maximum_neighbors) {
    uint64_t neighbors;
    if (minimum_neighbors == 0u) minimum_neighbors = 1u;
    if (maximum_neighbors < minimum_neighbors) maximum_neighbors = minimum_neighbors;
    neighbors = record_count > 0u ? (uint64_t)ceil(sqrt((double)record_count)) : 0u;
    if (neighbors < minimum_neighbors) neighbors = minimum_neighbors;
    if (neighbors > maximum_neighbors) neighbors = maximum_neighbors;
    if (record_count > 0u && neighbors > record_count) neighbors = record_count;
    return neighbors;
}

uint64_t RuntimeCausticPhotonEstimator3D_BudgetScaledNeighborLimit(
    uint64_t sample_budget,
    uint64_t reference_sample_budget,
    uint64_t reference_neighbors,
    uint64_t minimum_neighbors,
    uint64_t maximum_neighbors) {
    long double scaled;
    uint64_t neighbors;
    if (minimum_neighbors == 0u) minimum_neighbors = 1u;
    if (maximum_neighbors < minimum_neighbors) maximum_neighbors = minimum_neighbors;
    if (reference_sample_budget == 0u) reference_sample_budget = 1u;
    if (reference_neighbors == 0u) reference_neighbors = minimum_neighbors;
    scaled = ceill((long double)reference_neighbors *
                   (long double)sample_budget /
                   (long double)reference_sample_budget);
    if (scaled >= (long double)maximum_neighbors) return maximum_neighbors;
    neighbors = scaled > 0.0L ? (uint64_t)scaled : 0u;
    if (neighbors < minimum_neighbors) neighbors = minimum_neighbors;
    return neighbors;
}

const char* RuntimeCausticPhotonEstimator3D_Label(
    RuntimeCausticPhotonEstimator3D estimator) {
    switch (estimator) {
        case RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER:
            return "population_scaled_gather";
        case RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_BUDGET_SCALED_GATHER:
            return "budget_scaled_gather";
        case RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_NEIGHBOR_GATHER:
            return "neighbor_gather";
        case RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST:
            return "k_nearest";
        case RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS:
        default:
            return "radius";
    }
}

bool RuntimeCausticPhotonEstimator3D_FromLabel(
    const char* label,
    RuntimeCausticPhotonEstimator3D* out_estimator) {
    RuntimeCausticPhotonEstimator3D estimator;

    if (!label || !out_estimator) return false;
    if (strcmp(label, "radius") == 0) {
        estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS;
    } else if (strcmp(label, "k_nearest") == 0 ||
               strcmp(label, "knn") == 0) {
        estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST;
    } else if (strcmp(label, "neighbor_gather") == 0 ||
               strcmp(label, "gather") == 0 ||
               strcmp(label, "true_knn") == 0) {
        estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_NEIGHBOR_GATHER;
    } else if (strcmp(label, "budget_scaled_gather") == 0 ||
               strcmp(label, "density_scaled_gather") == 0 ||
               strcmp(label, "scaled_gather") == 0) {
        estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_BUDGET_SCALED_GATHER;
    } else if (strcmp(label, "population_scaled_gather") == 0 ||
               strcmp(label, "record_scaled_gather") == 0 ||
               strcmp(label, "convergent_gather") == 0) {
        estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER;
    } else {
        return false;
    }
    *out_estimator = estimator;
    return true;
}

bool RuntimeCausticPhotonEstimator3D_IsImplemented(
    RuntimeCausticPhotonEstimator3D estimator) {
    return estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS ||
           estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST ||
           estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_NEIGHBOR_GATHER ||
           estimator == RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_BUDGET_SCALED_GATHER ||
           estimator ==
               RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER;
}

static bool estimator_candidate_precedes(
    RuntimeCausticPhotonEstimatorCandidate3D lhs,
    RuntimeCausticPhotonEstimatorCandidate3D rhs) {
    if (lhs.distance != rhs.distance) return lhs.distance < rhs.distance;
    if (lhs.photonId != rhs.photonId) return lhs.photonId < rhs.photonId;
    return lhs.storageIndex < rhs.storageIndex;
}

bool RuntimeCausticPhotonEstimator3D_InsertCandidate(
    RuntimeCausticPhotonEstimatorCandidate3D* candidates,
    uint64_t* candidate_count,
    uint64_t capacity,
    RuntimeCausticPhotonEstimatorCandidate3D candidate) {
    uint64_t count;
    uint64_t position;

    if (!candidates || !candidate_count || capacity == 0u ||
        !(candidate.distance >= 0.0)) {
        return false;
    }
    count = *candidate_count;
    if (count > capacity) count = capacity;
    if (count == capacity &&
        !estimator_candidate_precedes(candidate, candidates[count - 1u])) {
        *candidate_count = count;
        return false;
    }
    position = count < capacity ? count : capacity - 1u;
    while (position > 0u &&
           estimator_candidate_precedes(candidate, candidates[position - 1u])) {
        if (position < capacity) candidates[position] = candidates[position - 1u];
        position -= 1u;
    }
    candidates[position] = candidate;
    if (count < capacity) count += 1u;
    *candidate_count = count;
    return true;
}
