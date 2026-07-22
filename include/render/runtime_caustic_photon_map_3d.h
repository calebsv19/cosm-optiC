#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_MAP_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_MAP_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_estimator_3d.h"
#include "render/runtime_caustic_photon_receiver_patch_3d.h"
#include "render/runtime_caustic_photon_sample_support_3d.h"
#include "render/runtime_caustic_photon_trace_3d.h"

typedef struct {
    uint64_t defaultCapacity;
    double defaultQueryRadius;
    double minNormalDot;
    uint64_t defaultQueryCandidateLimit;
    double physicalEnergyScale;
    double displayGain;
    RuntimeCausticPhotonEstimatorSettings3D estimator;
} RuntimeCausticPhotonMapSettings3D;

typedef struct {
    Vec3 position;
    Vec3 normal;
    double radius;
    int sceneObjectIndex;
    int primitiveIndex;
    int triangleIndex;
    int materialId;
    bool requireReceiverIdentity;
    RuntimeCausticPhotonReceiverDomain3D receiverDomain;
    double minNormalDot;
    uint64_t candidateLimit;
    double physicalEnergyScale;
    double displayGain;
    RuntimeCausticPhotonEstimatorSettings3D estimator;
} RuntimeCausticPhotonMapQuery3D;

typedef struct {
    bool hit;
    Vec3 flux;
    Vec3 physicalFlux;
    Vec3 displayFlux;
    Vec3 directTwoInterfacePhysicalFlux;
    Vec3 multipathPhysicalFlux;
    Vec3 unclassifiedPhysicalFlux;
    uint64_t directTwoInterfaceContributingCount;
    uint64_t multipathContributingCount;
    uint64_t unclassifiedContributingCount;
    double weightSum;
    uint64_t testedCount;
    uint64_t candidateCount;
    uint64_t contributingCount;
    uint64_t candidateLimit;
    bool candidateLimitReached;
    double physicalEnergyScale;
    double displayGain;
    double nearestDistance;
    double nearestContributionDistance;
    double farthestContributionDistance;
    double nearestNormalDot;
    double meanContributionDistance;
    double varianceProxy;
    uint64_t effectiveSampleCount;
    uint64_t radiusRejectCount;
    uint64_t normalRejectCount;
    uint64_t incidentHemisphereRejectCount;
    uint64_t receiverRejectCount;
    uint64_t receiverObjectRejectCount;
    uint64_t receiverMaterialRejectCount;
    uint64_t receiverExactTriangleRejectCount;
    double supportRadius;
    bool supportAdaptive;
    double kernelBoundaryWeight;
    double densityEstimate;
    double meanIncidentCosine;
    bool storedFluxAlreadyPdfCompensated;
    bool undersampled;
    bool fallbackUsed;
    Vec3 rejectedPhysicalFlux;
    RuntimeCausticPhotonEstimatorSettings3D estimator;
    const char* estimatorLabel;
    bool estimatorImplemented;
} RuntimeCausticPhotonMapQueryResult3D;

typedef struct {
    bool allocated;
    uint64_t recordCapacity;
    uint64_t recordCount;
    uint64_t storeAttemptCount;
    uint64_t storeAcceptedCount;
    uint64_t storeRejectedCount;
    uint64_t queryCount;
    uint64_t queryHitCount;
    uint64_t lastQueryTestedCount;
    uint64_t lastQueryCandidateCount;
    uint64_t lastQueryContributingCount;
    uint64_t lastQueryCandidateLimit;
    bool lastQueryCandidateLimitReached;
    bool lastQueryAccelerationUsed;
    uint64_t lastQueryGridCellVisitCount;
    double lastQueryNearestDistance;
    double lastQueryNearestContributionDistance;
    double lastQueryFarthestContributionDistance;
    double lastQueryNearestNormalDot;
    double lastQueryMeanContributionDistance;
    double lastQueryVarianceProxy;
    uint64_t lastQueryEffectiveSampleCount;
    uint64_t lastQueryRadiusRejectCount;
    uint64_t lastQueryNormalRejectCount;
    uint64_t lastQueryIncidentHemisphereRejectCount;
    uint64_t lastQueryReceiverRejectCount;
    uint64_t lastQueryReceiverObjectRejectCount;
    uint64_t lastQueryReceiverMaterialRejectCount;
    uint64_t lastQueryReceiverExactTriangleRejectCount;
    double lastQuerySupportRadius;
    bool lastQuerySupportAdaptive;
    double lastQueryKernelBoundaryWeight;
    double lastQueryDensityEstimate;
    double lastQueryMeanIncidentCosine;
    bool lastQueryStoredFluxAlreadyPdfCompensated;
    bool lastQueryUndersampled;
    bool lastQueryFallbackUsed;
    Vec3 lastQueryRejectedPhysicalFlux;
    RuntimeCausticPhotonEstimatorSettings3D lastQueryEstimator;
    const char* lastQueryEstimatorLabel;
    bool lastQueryEstimatorImplemented;
    Vec3 totalStoredFlux;
    Vec3 totalQueriedPhysicalFlux;
    Vec3 totalQueriedDisplayFlux;
    Vec3 lastQueryFlux;
    Vec3 lastQueryPhysicalFlux;
    Vec3 lastQueryDisplayFlux;
    double lastQueryPhysicalEnergyScale;
    double lastQueryDisplayGain;
    bool accelerationAllocated;
    double accelerationCellSize;
    uint64_t accelerationBucketCount;
    uint64_t accelerationInsertedCount;
    uint64_t accelerationFallbackLinearQueryCount;
    RuntimeCausticPhotonSampleSupportReadback3D sampleSupport;
} RuntimeCausticPhotonMapDiagnostics3D;

typedef struct {
    RuntimeCausticPhotonMapRecord3D* records;
    int64_t* accelerationBucketHeads;
    int64_t* accelerationRecordNext;
    int64_t* accelerationRecordCellX;
    int64_t* accelerationRecordCellY;
    int64_t* accelerationRecordCellZ;
    uint64_t recordCapacity;
    uint64_t recordCount;
    uint64_t storeAttemptCount;
    uint64_t storeAcceptedCount;
    uint64_t storeRejectedCount;
    uint64_t queryCount;
    uint64_t queryHitCount;
    double accelerationCellSize;
    double accelerationMaxRecordQueryRadius;
    uint64_t accelerationBucketCount;
    uint64_t accelerationInsertedCount;
    uint64_t accelerationFallbackLinearQueryCount;
    bool lastQueryAccelerationUsed;
    uint64_t lastQueryGridCellVisitCount;
    Vec3 totalQueriedPhysicalFlux;
    Vec3 totalQueriedDisplayFlux;
    RuntimeCausticPhotonMapQueryResult3D lastQuery;
    RuntimeCausticPhotonSampleSupportReadback3D sampleSupport;
    bool ownsRecords;
} RuntimeCausticPhotonMap3D;

void RuntimeCausticPhotonMap3D_DefaultSettings(
    RuntimeCausticPhotonMapSettings3D* settings);
void RuntimeCausticPhotonMap3D_DefaultQuery(RuntimeCausticPhotonMapQuery3D* query);
void RuntimeCausticPhotonMap3D_Init(RuntimeCausticPhotonMap3D* map);
bool RuntimeCausticPhotonMap3D_IsAllocated(const RuntimeCausticPhotonMap3D* map);
bool RuntimeCausticPhotonMap3D_Allocate(RuntimeCausticPhotonMap3D* map,
                                        uint64_t record_capacity);
void RuntimeCausticPhotonMap3D_Clear(RuntimeCausticPhotonMap3D* map);
void RuntimeCausticPhotonMap3D_Free(RuntimeCausticPhotonMap3D* map);
bool RuntimeCausticPhotonMap3D_StoreRecord(
    RuntimeCausticPhotonMap3D* map,
    const RuntimeCausticPhotonMapRecord3D* record);
bool RuntimeCausticPhotonMap3D_StoreSurfaceHit(
    RuntimeCausticPhotonMap3D* map,
    const RuntimeCausticPhotonSurfaceHit3D* hit,
    double path_pdf,
    double query_radius);
bool RuntimeCausticPhotonMap3D_StoreTraceReceiver(
    RuntimeCausticPhotonMap3D* map,
    const RuntimeCausticPhotonTrace3D* trace,
    Vec3 receiver_normal,
    double query_radius,
    int receiver_scene_object_index,
    int receiver_primitive_index,
    int receiver_triangle_index);
bool RuntimeCausticPhotonMap3D_PrepareSampleCenteredSupports(
    RuntimeCausticPhotonMap3D* map,
    uint64_t neighbor_limit);
bool RuntimeCausticPhotonMap3D_Query(
    RuntimeCausticPhotonMap3D* map,
    const RuntimeCausticPhotonMapQuery3D* query,
    RuntimeCausticPhotonMapQueryResult3D* out_result);
void RuntimeCausticPhotonMap3D_SnapshotDiagnostics(
    const RuntimeCausticPhotonMap3D* map,
    RuntimeCausticPhotonMapDiagnostics3D* out_diagnostics);

#endif
