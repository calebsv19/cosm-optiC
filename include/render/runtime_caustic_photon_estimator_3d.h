#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_3D_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_RADIUS = 0,
    RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST = 1,
    RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_NEIGHBOR_GATHER = 2,
    RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_BUDGET_SCALED_GATHER = 3,
    RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_POPULATION_SCALED_GATHER = 4
} RuntimeCausticPhotonEstimator3D;

typedef struct {
    RuntimeCausticPhotonEstimator3D estimator;
    uint64_t neighborLimit;
    uint64_t minimumEffectiveSamples;
} RuntimeCausticPhotonEstimatorSettings3D;

typedef struct {
    double distance;
    uint64_t photonId;
    uint64_t storageIndex;
} RuntimeCausticPhotonEstimatorCandidate3D;

void RuntimeCausticPhotonEstimator3D_DefaultSettings(
    RuntimeCausticPhotonEstimatorSettings3D* settings);
void RuntimeCausticPhotonEstimator3D_NormalizeSettings(
    RuntimeCausticPhotonEstimatorSettings3D* settings);
uint64_t RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
    uint64_t record_count,
    uint64_t minimum_neighbors,
    uint64_t maximum_neighbors);
uint64_t RuntimeCausticPhotonEstimator3D_BudgetScaledNeighborLimit(
    uint64_t sample_budget,
    uint64_t reference_sample_budget,
    uint64_t reference_neighbors,
    uint64_t minimum_neighbors,
    uint64_t maximum_neighbors);
const char* RuntimeCausticPhotonEstimator3D_Label(
    RuntimeCausticPhotonEstimator3D estimator);
bool RuntimeCausticPhotonEstimator3D_FromLabel(
    const char* label,
    RuntimeCausticPhotonEstimator3D* out_estimator);
bool RuntimeCausticPhotonEstimator3D_IsImplemented(
    RuntimeCausticPhotonEstimator3D estimator);
bool RuntimeCausticPhotonEstimator3D_InsertCandidate(
    RuntimeCausticPhotonEstimatorCandidate3D* candidates,
    uint64_t* candidate_count,
    uint64_t capacity,
    RuntimeCausticPhotonEstimatorCandidate3D candidate);

#endif
