#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_ESTIMATOR_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_ESTIMATOR_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_photon_settings_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_ELIGIBLE = 0,
    RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_STAGE,
    RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_MEDIUM,
    RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_OUTSIDE_VOLUME,
    RUNTIME_CAUSTIC_PHOTON_VOLUME_BEAM_REJECT_INVALID
} RuntimeCausticPhotonVolumeBeamEligibility3D;

typedef struct {
    double queryRadius;
    double scatteringCoefficient;
    double extinctionCoefficient;
    double phaseAnisotropy;
    bool requireMediumId;
    int mediumId;
    bool requireSegmentStage;
    RuntimeCausticPhotonSegmentStage3D segmentStage;
} RuntimeCausticPhotonVolumeBeamEstimatorSettings3D;

typedef struct {
    Vec3 beamFluxDensity;
    Vec3 beamDirection;
    Vec3 viewToCameraDirection;
    double beamDistance;
    double mediumDensity;
    double cameraTransmittance;
    double stepLength;
} RuntimeCausticPhotonVolumeBeamEstimatorInput3D;

typedef struct {
    bool evaluated;
    bool contributed;
    double phaseCosine;
    double phaseValue;
    double beamTransmittance;
    double scatterProbability;
    double cameraTransmittance;
    double integrationWeight;
    Vec3 radiance;
} RuntimeCausticPhotonVolumeBeamEstimatorReadback3D;

typedef struct {
    bool attempted;
    bool valid;
    uint64_t segmentExaminedCount;
    uint64_t segmentEligibleCount;
    uint64_t segmentClippedCount;
    uint64_t segmentRejectedStageCount;
    uint64_t segmentRejectedMediumCount;
    uint64_t segmentRejectedOutsideCount;
    uint64_t segmentRejectedInvalidCount;
    double clippedLengthSum;
    Vec3 eligiblePowerSum;
    Vec3 clippedPowerLength;
    Vec3 transmittedPowerLength;
    Vec3 spatialBoundsMin;
    Vec3 spatialBoundsMax;
    Vec3 powerLengthCentroid;
    Vec3 powerLengthVariance;
    Vec3 powerLengthStdDev;
    Vec3 powerLengthMeanDirection;
    double scalarPowerLength;
    double scalarTransmittedPowerLength;
} RuntimeCausticPhotonVolumeBeamOracleReadback3D;

void RuntimeCausticPhotonVolumeBeamEstimator3D_DefaultSettings(
    RuntimeCausticPhotonVolumeBeamEstimatorSettings3D* settings);
const char* RuntimeCausticPhotonVolumeBeamEligibility3D_Label(
    RuntimeCausticPhotonVolumeBeamEligibility3D eligibility);
double RuntimeCausticPhotonVolumeBeamEstimator3D_CompactKernel(
    double distance,
    double radius);
double RuntimeCausticPhotonVolumeBeamEstimator3D_HenyeyGreensteinPhase(
    double cosine,
    double anisotropy);
RuntimeCausticPhotonVolumeBeamEligibility3D
RuntimeCausticPhotonVolumeBeamEstimator3D_SegmentEligibility(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    const RuntimeCausticPhotonVolumeBeamEstimatorSettings3D* settings);
bool RuntimeCausticPhotonVolumeBeamEstimator3D_ClipSegmentToBounds(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    Vec3 bounds_min,
    Vec3 bounds_max,
    RuntimeCausticPhotonVolumeBeamSegment3D* out_segment);
bool RuntimeCausticPhotonVolumeBeamEstimator3D_Evaluate(
    const RuntimeCausticPhotonVolumeBeamEstimatorSettings3D* settings,
    const RuntimeCausticPhotonVolumeBeamEstimatorInput3D* input,
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D* out_readback);
bool RuntimeCausticPhotonVolumeBeamEstimator3D_AnalyzePopulation(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segments,
    uint64_t segment_count,
    const RuntimeCausticPhotonVolumeBeamEstimatorSettings3D* settings,
    Vec3 bounds_min,
    Vec3 bounds_max,
    RuntimeCausticPhotonVolumeBeamOracleReadback3D* out_readback);

#endif
