#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_DIRECT_CONSUMER_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_DIRECT_CONSUMER_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_integration_3d.h"
#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_photon_receiver_bsdf_3d.h"
#include "render/runtime_caustic_photon_volume_beam_estimator_3d.h"
#include "render/runtime_ray_3d.h"

typedef struct {
    Vec3 position;
    double supportRadius;
    double densityEstimate;
    double nearestDistance;
    double nearestContributionDistance;
    double farthestContributionDistance;
    uint64_t candidateCount;
    uint64_t effectiveSampleCount;
    uint64_t neighborLimit;
    uint64_t radiusRejectCount;
    uint64_t normalRejectCount;
    uint64_t incidentHemisphereRejectCount;
    uint64_t receiverRejectCount;
    uint64_t receiverObjectRejectCount;
    uint64_t receiverMaterialRejectCount;
    uint64_t receiverExactTriangleRejectCount;
    Vec3 physicalFlux;
    bool queryHit;
    bool undersampled;
} RuntimeCausticPhotonSurfaceDiagnosticSample3D;

typedef struct {
    uint64_t queryCount;
    uint64_t positiveQueryCount;
    uint64_t undersampledPositiveQueryCount;
    uint64_t effectiveSampleCountSum;
    uint64_t effectiveSampleHistogram[5];
} RuntimeCausticPhotonSurfaceDiagnosticAggregate3D;

typedef struct {
    Vec3 position;
    double queryRadius;
    int mediumId;
    RuntimeCausticPhotonSegmentStage3D segmentStage;
    Vec3 beamDirection;
    Vec3 viewToCameraDirection;
    double beamDistance;
    double mediumDensity;
    double stepLength;
    double phaseCosine;
    double phaseValue;
    double beamTransmittance;
    double scatterProbability;
    double cameraTransmittance;
    double integrationWeight;
    Vec3 radiance;
    bool contributed;
} RuntimeCausticPhotonVolumeDiagnosticSample3D;

const char* RuntimeCausticPhotonConsumer3D_Label(RuntimeCausticPhotonConsumer3D consumer);
void RuntimeCausticPhotonDirectConsumer3D_Bind(
    RuntimeCausticPhotonConsumer3D consumer,
    const RuntimeCausticPhotonMap3D* surface_map,
    const RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticPhotonIntegrationSettings3D* settings);
void RuntimeCausticPhotonDirectConsumer3D_Reset(void);
bool RuntimeCausticPhotonDirectConsumer3D_Active(void);
double RuntimeCausticPhotonDirectConsumer3D_VolumeQueryRadius(void);
void RuntimeCausticPhotonDirectConsumer3D_SnapshotReceiverBsdf(
    RuntimeCausticPhotonReceiverBsdfReadback3D* out_readback);
void RuntimeCausticPhotonDirectConsumer3D_ResetSurfaceDiagnostics(void);
uint64_t RuntimeCausticPhotonDirectConsumer3D_SurfaceDiagnosticCount(void);
void RuntimeCausticPhotonDirectConsumer3D_SnapshotSurfaceDiagnosticAggregate(
    RuntimeCausticPhotonSurfaceDiagnosticAggregate3D* out_aggregate);
bool RuntimeCausticPhotonDirectConsumer3D_SurfaceDiagnosticAt(
    uint64_t index,
    RuntimeCausticPhotonSurfaceDiagnosticSample3D* out_sample);
uint64_t RuntimeCausticPhotonDirectConsumer3D_VolumeDiagnosticCount(void);
bool RuntimeCausticPhotonDirectConsumer3D_VolumeDiagnosticAt(
    uint64_t index,
    RuntimeCausticPhotonVolumeDiagnosticSample3D* out_sample);
bool RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
    const HitInfo3D* hit,
    Vec3* out_radiance,
    RuntimeCausticPhotonMapQueryResult3D* out_readback);
bool RuntimeCausticPhotonDirectConsumer3D_SampleSurfaceWithReceiverArea(
    const HitInfo3D* hit,
    double receiver_area_m2,
    Vec3* out_radiance,
    RuntimeCausticPhotonMapQueryResult3D* out_readback);
bool RuntimeCausticPhotonDirectConsumer3D_SampleBeam(
    Vec3 position,
    Vec3 direction,
    double radius,
    int medium_id,
    Vec3* out_radiance,
    RuntimeCausticBeamMapQueryResult3D* out_readback);
bool RuntimeCausticPhotonDirectConsumer3D_EvaluateBeamRadiance(
    const RuntimeCausticBeamMapQueryResult3D* beam_query,
    Vec3 view_ray_direction,
    double medium_density,
    double camera_transmittance,
    double step_length,
    Vec3* out_radiance,
    RuntimeCausticPhotonVolumeBeamEstimatorReadback3D* out_readback);

#endif
