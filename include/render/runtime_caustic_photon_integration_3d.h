#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_INTEGRATION_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_INTEGRATION_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_emit_3d.h"
#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_settings_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_caustic_surface_cache_3d.h"
#include "render/runtime_caustic_volume_cache_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_PRODUCT_MODE_OFF = 0,
    RUNTIME_CAUSTIC_PRODUCT_MODE_REFERENCE = 1,
    RUNTIME_CAUSTIC_PRODUCT_MODE_PRODUCTION = 2
} RuntimeCausticProductMode3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_EXPLORATORY_REFERENCE = 1,
    RUNTIME_CAUSTIC_PHOTON_INTEGRATION_ROUTE_PHOTON_QUERY_READY = 2
} RuntimeCausticPhotonIntegrationRoute3D;

typedef enum {
    RUNTIME_CAUSTIC_PHOTON_POPULATION_SOURCE_NONE = 0,
    RUNTIME_CAUSTIC_PHOTON_POPULATION_SOURCE_SURFACE_PROXY = 1,
    RUNTIME_CAUSTIC_PHOTON_POPULATION_SOURCE_TRACE_RECORDS = 2
} RuntimeCausticPhotonPopulationSource3D;

typedef struct {
    RuntimeCausticProductMode3D productMode;
    bool surfaceQueryEnabled;
    bool volumeQueryEnabled;
    bool renderContributionEnabled;
    int sampleBudget;
    int maxPathDepth;
    double surfaceRadianceScale;
    double surfaceQueryRadius;
    double volumeQueryRadius;
} RuntimeCausticPhotonIntegrationSettings3D;

typedef struct {
    bool querySurface;
    RuntimeCausticPhotonMapQuery3D surface;
    bool queryVolume;
    RuntimeCausticBeamMapQuery3D volume;
} RuntimeCausticPhotonIntegrationQuery3D;

typedef struct {
    bool valid;
    Vec3 position;
    Vec3 normal;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
    double footprintRadius;
    Vec3 receiverCentroid;
    double receiverMeanDistance;
    double receiverMaxDistance;
    uint64_t receiverCount;
} RuntimeCausticPhotonReceiverSelection3D;

typedef struct {
    RuntimeCausticProductMode3D productMode;
    RuntimeCausticPhotonIntegrationRoute3D route;
    bool surfaceQueryEnabled;
    bool volumeQueryEnabled;
    bool renderContributionEnabled;
    bool renderContributionSuppressed;
    bool surfaceHit;
    bool volumeHit;
    Vec3 surfaceFlux;
    Vec3 volumeFlux;
    Vec3 combinedFlux;
    uint64_t surfaceCandidateCount;
    uint64_t surfaceContributingCount;
    uint64_t volumeCandidateCount;
    uint64_t volumeContributingCount;
} RuntimeCausticPhotonIntegrationResult3D;

typedef struct {
    bool eligible;
    bool suppressed;
    bool hasSurfaceContribution;
    bool hasVolumeContribution;
    Vec3 surfacePosition;
    Vec3 surfaceNormal;
    double surfaceRadius;
    Vec3 surfaceRadiance;
    int surfaceSceneObjectIndex;
    int surfacePrimitiveIndex;
    int surfaceTriangleIndex;
    Vec3 volumePosition;
    Vec3 volumeDirection;
    double volumeRadius;
    Vec3 volumeRadiance;
    Vec3 combinedRadiance;
    uint64_t surfaceContributingCount;
    uint64_t volumeContributingCount;
} RuntimeCausticPhotonContribution3D;

enum {
    RUNTIME_CAUSTIC_PHOTON_RECEIVER_CONTRIBUTION_MAX_RECORDS = 64
};

typedef struct {
    bool valid;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
    Vec3 position;
    Vec3 normal;
    double radius;
    uint64_t storedRecordCount;
    bool queryHit;
    bool contributionEligible;
    bool surfaceDeposited;
    uint64_t candidateCount;
    uint64_t contributingCount;
    Vec3 radiance;
} RuntimeCausticPhotonReceiverContributionRecord3D;

typedef struct {
    bool attempted;
    bool eligible;
    bool suppressed;
    uint64_t receiverBucketCount;
    uint64_t receiverQueryAttemptCount;
    uint64_t receiverQueryHitCount;
    uint64_t receiverContributionEligibleCount;
    uint64_t receiverSurfaceDepositAttemptCount;
    uint64_t receiverSurfaceDepositAcceptedCount;
    uint64_t receiverSurfaceCandidateCount;
    uint64_t receiverSurfaceContributingCount;
    Vec3 receiverSurfaceRadiance;
    uint64_t recordCount;
    RuntimeCausticPhotonReceiverContributionRecord3D records[
        RUNTIME_CAUSTIC_PHOTON_RECEIVER_CONTRIBUTION_MAX_RECORDS];
} RuntimeCausticPhotonReceiverContributionReadback3D;

typedef struct {
    bool attempted;
    bool surfaceMapAllocated;
    bool emissionAttempted;
    bool emissionSucceeded;
    bool surfaceMapPopulationAttempted;
    bool surfaceMapPopulated;
    bool tracePopulationAttempted;
    bool tracePopulationSucceeded;
    bool preparedSceneMeshDielectricAttempted;
    bool preparedSceneMeshDielectricSucceeded;
    bool fixtureMeshDielectricFallbackUsed;
    bool volumeBeamMapAllocated;
    bool volumeBeamPopulationAttempted;
    bool volumeBeamPopulated;
    RuntimeCausticPhotonPopulationSource3D populationSource;
    uint64_t requestedSampleBudget;
    uint64_t emittedPhotonCount;
    uint64_t rejectedPhotonCount;
    uint64_t traceInputCount;
    uint64_t traceSolvedPathCount;
    uint64_t traceRecordCount;
    uint64_t receiverLookupAttemptCount;
    uint64_t receiverDirectHitCount;
    uint64_t receiverCrossingProbeAttemptCount;
    uint64_t receiverCrossingProbeHitCount;
    uint64_t receiverCandidateCount;
    uint64_t receiverAcceptedHitCount;
    uint64_t receiverMissRejectCount;
    uint64_t receiverSelfHitRejectCount;
    uint64_t receiverInvalidRejectCount;
    uint64_t receiverMaterialFilterRejectCount;
    uint64_t receiverObjectFilterRejectCount;
    uint64_t receiverCompetingRejectCount;
    uint64_t receiverSelectedHitCount;
    uint64_t receiverSelectedBucketCount;
    double receiverFootprintRadius;
    double receiverMeanDistance;
    double receiverMaxDistance;
    uint64_t preparedSceneMeshDielectricCandidateCount;
    int preparedSceneMeshDielectricSceneObjectIndex;
    int preparedSceneMeshDielectricPrimitiveIndex;
    int preparedSceneMeshDielectricTriangleIndex;
    int preparedSceneMeshDielectricTriangleCount;
    uint64_t surfaceMapStoreAttemptCount;
    uint64_t surfaceMapStoreAcceptedCount;
    uint64_t surfaceMapStoreRejectedCount;
    uint64_t surfaceMapRecordCount;
    uint64_t surfaceMapAccelerationInsertedCount;
    uint64_t volumeBeamStoreAttemptCount;
    uint64_t volumeBeamStoreAcceptedCount;
    uint64_t volumeBeamStoreRejectedCount;
    uint64_t volumeBeamSegmentCount;
    uint64_t volumeBeamAccelerationInsertedCount;
    Vec3 totalEmittedFlux;
    Vec3 totalStoredSurfaceFlux;
    Vec3 totalStoredVolumeFlux;
} RuntimeCausticPhotonMapPopulationReadback3D;

typedef struct {
    bool attempted;
    bool surfaceAttempted;
    bool surfaceDeposited;
    bool volumeAttempted;
    bool volumeDeposited;
} RuntimeCausticPhotonContributionDepositResult3D;

typedef struct {
    RuntimeCausticProductMode3D productMode;
    RuntimeCausticPhotonIntegrationRoute3D route;
    bool queryAttempted;
    bool queryHit;
    bool contributionAttempted;
    bool contributionEligible;
    bool renderContributionSuppressed;
    bool cacheDepositAttempted;
    bool surfaceDeposited;
    bool volumeDeposited;
    uint64_t surfaceCandidateCount;
    uint64_t surfaceContributingCount;
    uint64_t volumeCandidateCount;
    uint64_t volumeContributingCount;
    RuntimeCausticPhotonReceiverContributionReadback3D receiverContribution;
    uint64_t estimatedCost;
    Vec3 radiance;
    RuntimeCausticPhotonMapPopulationReadback3D mapPopulation;
} RuntimeCausticPhotonRenderCallsiteReadback3D;

void RuntimeCausticPhotonIntegration3D_DefaultSettings(
    RuntimeCausticPhotonIntegrationSettings3D* settings);
void RuntimeCausticPhotonIntegration3D_DefaultQuery(
    RuntimeCausticPhotonIntegrationQuery3D* query);
RuntimeCausticProductMode3D RuntimeCausticProductMode3D_FromLabel(
    const char* label);
const char* RuntimeCausticProductMode3D_Label(RuntimeCausticProductMode3D mode);
const char* RuntimeCausticPhotonIntegrationRoute3D_Label(
    RuntimeCausticPhotonIntegrationRoute3D route);
const char* RuntimeCausticPhotonPopulationSource3D_Label(
    RuntimeCausticPhotonPopulationSource3D source);
RuntimeCausticPhotonIntegrationRoute3D
RuntimeCausticPhotonIntegration3D_RouteForSettings(
    const RuntimeCausticPhotonIntegrationSettings3D* settings);
void RuntimeCausticPhotonIntegration3D_ApplyToCausticSettings(
    const RuntimeCausticPhotonIntegrationSettings3D* integration,
    RuntimeCausticSettings3D* caustic);
bool RuntimeCausticPhotonIntegration3D_Query(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonIntegrationResult3D* out_result);
bool RuntimeCausticPhotonIntegration3D_BuildContribution(
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    const RuntimeCausticPhotonIntegrationResult3D* query_result,
    RuntimeCausticPhotonContribution3D* out_contribution);
bool RuntimeCausticPhotonIntegration3D_DepositContributionToCaches(
    RuntimeCausticSurfaceCache3D* surface_cache,
    RuntimeCausticVolumeCache3D* volume_cache,
    const RuntimeCausticPhotonContribution3D* contribution,
    RuntimeCausticPhotonContributionDepositResult3D* out_result);
bool RuntimeCausticPhotonIntegration3D_DepositSurfaceContributionsForReceiverBuckets(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticSurfaceCache3D* surface_cache,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    RuntimeCausticPhotonReceiverContributionReadback3D* out_readback);
bool RuntimeCausticPhotonIntegration3D_PopulateSurfaceMapFromLightSet(
    RuntimeCausticPhotonMap3D* surface_map,
    const RuntimeLightSet3D* light_set,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonMapPopulationReadback3D* out_readback);
bool RuntimeCausticPhotonIntegration3D_PopulateMapsFromTraceRecords(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticPhotonTrace3D* traces,
    uint64_t trace_count,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonMapPopulationReadback3D* out_readback);
bool RuntimeCausticPhotonIntegration3D_PopulateMapsFromMeshDielectricFixture(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeLightSet3D* light_set,
    const RuntimeCausticLensShape3D* mesh_dielectric,
    const RuntimeTriangle3D* entry_triangle,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonMapPopulationReadback3D* out_readback);
bool RuntimeCausticPhotonIntegration3D_PopulateReceiverSurfaceMapFromMeshDielectricScene(
    RuntimeCausticPhotonMap3D* surface_map,
    const RuntimeScene3D* scene,
    const RuntimeCausticLensShape3D* mesh_dielectric,
    const RuntimeTriangle3D* entry_triangle,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    RuntimeCausticPhotonMapPopulationReadback3D* out_readback,
    RuntimeCausticPhotonReceiverSelection3D* out_receiver);
bool RuntimeCausticPhotonIntegration3D_EvaluateRenderCallsite(
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    RuntimeCausticSurfaceCache3D* surface_cache,
    RuntimeCausticVolumeCache3D* volume_cache,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonRenderCallsiteReadback3D* out_readback);
bool RuntimeCausticPhotonIntegration3D_EvaluatePopulatedRenderCallsite(
    const RuntimeLightSet3D* light_set,
    RuntimeCausticBeamMap3D* beam_map,
    RuntimeCausticSurfaceCache3D* surface_cache,
    RuntimeCausticVolumeCache3D* volume_cache,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    const RuntimeCausticPhotonIntegrationQuery3D* query,
    RuntimeCausticPhotonRenderCallsiteReadback3D* out_readback);

#endif
