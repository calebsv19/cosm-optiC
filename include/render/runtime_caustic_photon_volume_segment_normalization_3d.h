#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_VOLUME_SEGMENT_NORMALIZATION_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_VOLUME_SEGMENT_NORMALIZATION_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_beam_map_3d.h"
#include "render/runtime_caustic_photon_lifecycle_3d.h"
#include "render/runtime_volume_3d.h"

typedef struct {
    double queryRadius;
    double targetVoxelSize;
    int mediumId;
    bool requireMediumId;
    RuntimeCausticPhotonSegmentStage3D segmentStage;
    bool requireSegmentStage;
    bool accelerated;
    uint64_t maxSegments;
    uint64_t maxAxialSamples;
    uint64_t maxCellTests;
} RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D;

typedef struct {
    bool attempted;
    bool valid;
    bool degenerate;
    bool axialSampleLimitReached;
    bool cellTestLimitReached;
    double segmentLength;
    double discreteIntegral;
    double scale;
    uint64_t axialSampleCount;
    uint64_t cellTestCount;
    double maximumAxialSpacing;
} RuntimeCausticPhotonVolumeSegmentNormalizationResult3D;

typedef struct {
    bool attempted;
    bool prepared;
    bool workBoundSatisfied;
    uint64_t segmentExaminedCount;
    uint64_t segmentEligibleCount;
    uint64_t segmentPreparedCount;
    uint64_t segmentRejectedStageCount;
    uint64_t segmentRejectedMediumCount;
    uint64_t segmentRejectedInvalidCount;
    uint64_t segmentRejectedOutsideCount;
    uint64_t axialSampleCount;
    uint64_t cellTestCount;
    double denominatorSum;
    double scaleMinimum;
    double scaleMaximum;
    double scaleMean;
    double maximumAxialSpacing;
    RuntimeVolumeGrid3D grid;
} RuntimeCausticPhotonVolumeSegmentNormalizationReadback3D;

void RuntimeCausticPhotonVolumeSegmentNormalization3D_DefaultSettings(
    RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D* settings);
double RuntimeCausticPhotonVolumeSegmentNormalization3D_TargetVoxelSize(
    double query_radius,
    RuntimeCausticPhotonBudgetTier3D tier);
bool RuntimeCausticPhotonVolumeSegmentNormalization3D_ResolveGrid(
    const RuntimeVolumeGrid3D* source,
    double target_voxel_size,
    RuntimeVolumeGrid3D* out_grid);
bool RuntimeCausticPhotonVolumeSegmentNormalization3D_EvaluateClipped(
    const RuntimeCausticPhotonVolumeBeamSegment3D* segment,
    const RuntimeVolumeGrid3D* grid,
    double query_radius,
    bool accelerated,
    uint64_t max_axial_samples,
    uint64_t max_cell_tests,
    RuntimeCausticPhotonVolumeSegmentNormalizationResult3D* out_result);
bool RuntimeCausticPhotonVolumeSegmentNormalization3D_PrepareMap(
    RuntimeCausticBeamMap3D* map,
    const RuntimeVolumeGrid3D* source_grid,
    const RuntimeCausticPhotonVolumeSegmentNormalizationSettings3D* settings,
    RuntimeCausticPhotonVolumeSegmentNormalizationReadback3D* out_readback);

#endif
