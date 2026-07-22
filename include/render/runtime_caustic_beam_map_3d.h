#ifndef RENDER_RUNTIME_CAUSTIC_BEAM_MAP_3D_H
#define RENDER_RUNTIME_CAUSTIC_BEAM_MAP_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_estimator_3d.h"
#include "render/runtime_caustic_photon_trace_3d.h"

typedef struct {
    uint64_t defaultCapacity;
    double defaultQueryRadius;
    double minDirectionDot;
    uint64_t defaultQueryCandidateLimit;
    double physicalEnergyScale;
    double displayGain;
    RuntimeCausticPhotonEstimatorSettings3D estimator;
} RuntimeCausticBeamMapSettings3D;

typedef struct {
    Vec3 position;
    Vec3 direction;
    double radius;
    int mediumId;
    bool requireMediumId;
    RuntimeCausticPhotonSegmentStage3D segmentStage;
    bool requireSegmentStage;
    double minDirectionDot;
    uint64_t candidateLimit;
    double physicalEnergyScale;
    double displayGain;
    RuntimeCausticPhotonEstimatorSettings3D estimator;
} RuntimeCausticBeamMapQuery3D;

typedef struct {
    bool hit;
    Vec3 queryPosition;
    Vec3 queryDirection;
    double queryRadius;
    int queryMediumId;
    RuntimeCausticPhotonSegmentStage3D querySegmentStage;
    Vec3 flux;
    Vec3 physicalFlux;
    Vec3 displayFlux;
    double weightSum;
    uint64_t testedCount;
    uint64_t candidateCount;
    uint64_t contributingCount;
    uint64_t contributingPhotonIdXor;
    uint64_t contributingPhotonIdSum;
    uint64_t radiusRejectCount;
    uint64_t directionRejectCount;
    uint64_t mediumRejectCount;
    uint64_t stageRejectCount;
    uint64_t candidateLimit;
    bool candidateLimitReached;
    double physicalEnergyScale;
    double displayGain;
    double nearestDistance;
    double nearestContributionDistance;
    double farthestContributionDistance;
    double nearestT;
    double nearestDirectionDot;
    Vec3 meanBeamDirection;
    double beamDirectionWeightSum;
    double meanBeamDistance;
    double kernelRadius;
    double kernelBoundaryWeight;
    double meanContributionDistance;
    double varianceProxy;
    uint64_t effectiveSampleCount;
    Vec3 rejectedPhysicalFlux;
    RuntimeCausticPhotonEstimatorSettings3D estimator;
    const char* estimatorLabel;
    bool estimatorImplemented;
} RuntimeCausticBeamMapQueryResult3D;

typedef struct {
    bool allocated;
    uint64_t segmentCapacity;
    uint64_t segmentCount;
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
    double lastQueryNearestT;
    double lastQueryNearestDirectionDot;
    double lastQueryMeanContributionDistance;
    double lastQueryVarianceProxy;
    uint64_t lastQueryEffectiveSampleCount;
    uint64_t lastQueryRadiusRejectCount;
    uint64_t lastQueryDirectionRejectCount;
    uint64_t lastQueryMediumRejectCount;
    uint64_t lastQueryStageRejectCount;
    Vec3 lastQueryMeanBeamDirection;
    double lastQueryMeanBeamDistance;
    double lastQueryKernelRadius;
    double lastQueryKernelBoundaryWeight;
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
} RuntimeCausticBeamMapDiagnostics3D;

typedef struct {
    RuntimeCausticPhotonVolumeBeamSegment3D* segments;
    double* segmentFiniteNormalization;
    int64_t* accelerationBucketHeads;
    int64_t* accelerationSegmentNext;
    int64_t* accelerationSegmentCellX;
    int64_t* accelerationSegmentCellY;
    int64_t* accelerationSegmentCellZ;
    uint64_t segmentCapacity;
    uint64_t segmentCount;
    bool finiteSegmentNormalizationPrepared;
    uint64_t finiteSegmentNormalizationCount;
    double finiteSegmentNormalizationScaleMinimum;
    double finiteSegmentNormalizationScaleMaximum;
    double finiteSegmentNormalizationScaleMean;
    uint64_t storeAttemptCount;
    uint64_t storeAcceptedCount;
    uint64_t storeRejectedCount;
    uint64_t queryCount;
    uint64_t queryHitCount;
    double accelerationCellSize;
    double accelerationMaxSegmentRadius;
    double accelerationMaxSegmentHalfLength;
    uint64_t accelerationBucketCount;
    uint64_t accelerationInsertedCount;
    uint64_t accelerationFallbackLinearQueryCount;
    bool lastQueryAccelerationUsed;
    uint64_t lastQueryGridCellVisitCount;
    Vec3 totalQueriedPhysicalFlux;
    Vec3 totalQueriedDisplayFlux;
    RuntimeCausticBeamMapQueryResult3D lastQuery;
    bool ownsSegments;
} RuntimeCausticBeamMap3D;

void RuntimeCausticBeamMap3D_DefaultSettings(
    RuntimeCausticBeamMapSettings3D* settings);
void RuntimeCausticBeamMap3D_DefaultQuery(RuntimeCausticBeamMapQuery3D* query);
void RuntimeCausticBeamMap3D_Init(RuntimeCausticBeamMap3D* map);
bool RuntimeCausticBeamMap3D_IsAllocated(const RuntimeCausticBeamMap3D* map);
bool RuntimeCausticBeamMap3D_Allocate(RuntimeCausticBeamMap3D* map,
                                      uint64_t segment_capacity);
void RuntimeCausticBeamMap3D_Clear(RuntimeCausticBeamMap3D* map);
void RuntimeCausticBeamMap3D_Free(RuntimeCausticBeamMap3D* map);
bool RuntimeCausticBeamMap3D_StoreSegment(
    RuntimeCausticBeamMap3D* map,
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment);
bool RuntimeCausticBeamMap3D_StoreTraceSegment(
    RuntimeCausticBeamMap3D* map,
    const RuntimeCausticPhotonTrace3D* trace,
    double radius_start,
    double radius_end,
    double transmittance,
    double density_weight,
    int medium_id);
bool RuntimeCausticBeamMap3D_Query(
    RuntimeCausticBeamMap3D* map,
    const RuntimeCausticBeamMapQuery3D* query,
    RuntimeCausticBeamMapQueryResult3D* out_result);
void RuntimeCausticBeamMap3D_SnapshotDiagnostics(
    const RuntimeCausticBeamMap3D* map,
    RuntimeCausticBeamMapDiagnostics3D* out_diagnostics);

#endif
