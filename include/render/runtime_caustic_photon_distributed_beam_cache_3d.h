#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_DISTRIBUTED_BEAM_CACHE_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_DISTRIBUTED_BEAM_CACHE_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_lifecycle_3d.h"
#include "render/runtime_caustic_volume_cache_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_LINEAR_ORACLE = 0,
    RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_BUILD_RASTER_ACCELERATED = 1
} RuntimeCausticDistributedBeamBuildMode3D;

typedef enum {
    RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_DENSE = 0,
    RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_STORAGE_SPARSE_BRICKS = 1
} RuntimeCausticDistributedBeamStorageBackend3D;

enum { RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_CELL_FIELD_FLOAT_COUNT_3D = 32 };

typedef struct {
    RuntimeCausticPhotonBudgetTier3D qualityTier;
    RuntimeCausticDistributedBeamBuildMode3D buildMode;
    RuntimeCausticDistributedBeamStorageBackend3D storageBackend;
    double queryRadius;
    int mediumId;
    bool requireMediumId;
    RuntimeCausticPhotonSegmentStage3D segmentStage;
    bool requireSegmentStage;
    uint64_t maxSegments;
    uint64_t maxAxialSamples;
    uint64_t maxCellTests;
    uint64_t memoryCeilingBytes;
    /* Test/report-only spatial convergence override; zero preserves the tier. */
    double diagnosticVoxelScale;
    bool configurationAssertionEnabled;
    bool configurationLifecycleAssertionEnabled;
    bool configurationLifecycleExpectedBuilt;
    bool configurationLifecycleExpectedActiveEmpty;
    uint32_t expectedGridW;
    uint32_t expectedGridH;
    uint32_t expectedGridD;
    double expectedVoxelSize;
    uint64_t expectedMaximumPeakBytes;
} RuntimeCausticDistributedBeamCacheSettings3D;

typedef struct {
    bool attempted;
    bool built;
    bool accelerated;
    bool memoryBoundSatisfied;
    bool workBoundSatisfied;
    bool segmentLimitReached;
    bool axialSampleLimitReached;
    bool cellTestLimitReached;
    RuntimeCausticPhotonBudgetTier3D qualityTier;
    RuntimeCausticDistributedBeamBuildMode3D buildMode;
    uint64_t gridCellCount;
    uint32_t gridW;
    uint32_t gridH;
    uint32_t gridD;
    uint64_t storageBytes;
    RuntimeCausticDistributedBeamStorageBackend3D storageBackend;
    uint64_t sparseDirectoryBytes;
    uint64_t sparseAllocatedBrickCount;
    uint64_t sparsePayloadBytes;
    uint64_t sparseMetadataBytes;
    uint64_t sparsePeakBytes;
    uint64_t sparseAllocationOrderHash;
    uint64_t sparseAllocationFailureCount;
    uint64_t memoryCeilingBytes;
    bool configurationAssertionEnabled;
    bool configurationAssertionMatched;
    bool configurationLifecycleAssertionEnabled;
    bool configurationLifecycleExpectedBuilt;
    bool configurationLifecycleExpectedActiveEmpty;
    bool configurationLifecycleMatched;
    RuntimeCausticDistributedBeamStorageBackend3D requestedStorageBackend;
    uint32_t requestedGridW;
    uint32_t requestedGridH;
    uint32_t requestedGridD;
    double requestedVoxelSize;
    double requestedDiagnosticVoxelScale;
    uint64_t expectedMaximumPeakBytes;
    uint64_t segmentExaminedCount;
    uint64_t segmentEligibleCount;
    uint64_t segmentRasterizedCount;
    uint64_t segmentRejectedStageCount;
    uint64_t segmentRejectedMediumCount;
    uint64_t segmentRejectedInvalidCount;
    uint64_t segmentRejectedOutsideCount;
    uint64_t axialSampleCount;
    uint64_t maxSegments;
    uint64_t maxAxialSamples;
    uint64_t cellTestCount;
    uint64_t cellContributionCount;
    uint64_t maxCellTests;
    uint64_t nonZeroCellCount;
    bool conservativeSubvoxelField;
    uint64_t subvoxelCountPerCell;
    uint64_t nonZeroSubvoxelCount;
    double subvoxelSize;
    double maximumAxialSpacing;
    double voxelSize;
    double queryRadius;
    Vec3 expectedIntegratedFlux;
    Vec3 cachedIntegratedFlux;
    double integratedFluxRelativeError;
    uint64_t segmentNormalizationCount;
    double segmentNormalizationDenominatorSum;
    double segmentNormalizationScaleMinimum;
    double segmentNormalizationScaleMaximum;
    double segmentNormalizationScaleMean;
    bool reconstructionLatticeAttempted;
    bool reconstructionLatticeValid;
    uint64_t reconstructionLatticeCellCount;
    uint64_t rasterOracleSampleCount;
    uint64_t reconstructionSampleCount;
    uint64_t reconstructionPositiveOverlapCount;
    uint64_t reconstructionDirectOnlyCount;
    uint64_t reconstructionCacheOnlyCount;
    Vec3 rasterOracleDirectFluxDensitySum;
    Vec3 rasterOracleCachedFluxDensitySum;
    double rasterOracleRelativeError;
    Vec3 reconstructionDirectFluxDensitySum;
    Vec3 reconstructionCachedFluxDensitySum;
    double reconstructionRelativeError;
    Vec3 reconstructionDirectCentroid;
    Vec3 reconstructionCachedCentroid;
    double reconstructionCentroidDistance;
    bool pairedTraceAttempted;
    bool pairedTraceValid;
    bool pairedCachePhotonIdentityAvailable;
    uint64_t pairedSampleCount;
    Vec3 pairedQueryCoordinateSum;
    double pairedQueryCoordinateMismatchMaximum;
    uint64_t pairedDirectTestedCount;
    uint64_t pairedDirectCandidateCount;
    uint64_t pairedDirectContributingCount;
    uint64_t pairedDirectRadiusRejectCount;
    uint64_t pairedDirectMediumRejectCount;
    uint64_t pairedDirectStageRejectCount;
    uint64_t pairedDirectPhotonIdXor;
    uint64_t pairedDirectPhotonIdSum;
    double pairedDirectKernelWeightSum;
    double pairedDirectContributionDistanceSum;
    uint64_t pairedCacheHitCount;
    uint64_t pairedCacheAggregateCount;
    double allocationClearSeconds;
    double populationSeconds;
    double reconstructionAnalysisSeconds;
    double buildTotalSeconds;
} RuntimeCausticDistributedBeamCacheReadback3D;

void RuntimeCausticDistributedBeamCache3D_DefaultSettings(
    RuntimeCausticDistributedBeamCacheSettings3D* settings);
void RuntimeCausticDistributedBeamCache3D_ApplyQualityTier(
    RuntimeCausticDistributedBeamCacheSettings3D* settings,
    RuntimeCausticPhotonBudgetTier3D tier);
const char* RuntimeCausticDistributedBeamBuildMode3D_Label(
    RuntimeCausticDistributedBeamBuildMode3D mode);
const char* RuntimeCausticDistributedBeamStorageBackend3D_Label(
    RuntimeCausticDistributedBeamStorageBackend3D backend);
RuntimeCausticDistributedBeamStorageBackend3D
RuntimeCausticDistributedBeamStorageBackend3D_FromLabel(const char* label);
bool RuntimeCausticDistributedBeamCache3D_Build(
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticDistributedBeamCacheSettings3D* settings,
    RuntimeCausticDistributedBeamCacheReadback3D* out_readback);
bool RuntimeCausticDistributedBeamCache3D_Sample(
    const RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    RuntimeCausticBeamMapQueryResult3D* out_result);
bool RuntimeCausticDistributedBeamCache3D_ReadCellFields(
    const RuntimeCausticVolumeCache3D* cache,
    uint64_t linearCellIndex,
    float outFields[RUNTIME_CAUSTIC_DISTRIBUTED_BEAM_CELL_FIELD_FLOAT_COUNT_3D]);

#endif
