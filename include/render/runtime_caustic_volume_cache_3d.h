#ifndef RENDER_RUNTIME_CAUSTIC_VOLUME_CACHE_3D_H
#define RENDER_RUNTIME_CAUSTIC_VOLUME_CACHE_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "math/vec3.h"
#include "render/runtime_caustic_settings_3d.h"
#include "render/runtime_volume_3d.h"

typedef struct RuntimeCausticPhotonSparseBrickCache3D
    RuntimeCausticPhotonSparseBrickCache3D;

typedef struct {
    bool allocated;
    uint32_t gridW;
    uint32_t gridH;
    uint32_t gridD;
    uint64_t cellCount;
    Vec3 origin;
    Vec3 boundsMin;
    Vec3 boundsMax;
    double voxelSize;
    uint64_t allocatedCellCount;
    uint64_t nonZeroCellCount;
    uint64_t depositAttemptCount;
    uint64_t depositAcceptedCount;
    uint64_t depositRejectedCount;
    uint64_t footprintDepositCount;
    uint64_t footprintCellContributionCount;
    uint64_t sampleLookupCount;
    uint64_t sampleContributingCount;
    bool hasNonZeroBounds;
    Vec3 nonZeroBoundsMin;
    Vec3 nonZeroBoundsMax;
    Vec3 radianceCentroid;
    double totalRadianceR;
    double totalRadianceG;
    double totalRadianceB;
    double footprintInputRadianceR;
    double footprintInputRadianceG;
    double footprintInputRadianceB;
    double footprintDepositedRadianceR;
    double footprintDepositedRadianceG;
    double footprintDepositedRadianceB;
    double averageFootprintRadiusVoxels;
    double maxCellRadiance;
    RuntimeCausticCacheState3D state;
} RuntimeCausticVolumeCacheDiagnostics3D;

typedef struct {
    RuntimeVolumeGrid3D grid;
    float* radianceR;
    float* radianceG;
    float* radianceB;
    float* beamDirectionX;
    float* beamDirectionY;
    float* beamDirectionZ;
    float* beamDistanceWeighted;
    float* beamDirectionWeight;
    float* beamSubvoxelRadianceR;
    float* beamSubvoxelRadianceG;
    float* beamSubvoxelRadianceB;
    uint64_t depositAttemptCount;
    uint64_t depositAcceptedCount;
    uint64_t depositRejectedCount;
    uint64_t footprintDepositCount;
    uint64_t footprintCellContributionCount;
    uint64_t sampleLookupCount;
    uint64_t sampleContributingCount;
    double footprintRadiusVoxelSum;
    double footprintInputRadianceR;
    double footprintInputRadianceG;
    double footprintInputRadianceB;
    double footprintDepositedRadianceR;
    double footprintDepositedRadianceG;
    double footprintDepositedRadianceB;
    bool physicalBeamField;
    bool beamSubvoxelField;
    bool ownsBuffers;
    RuntimeCausticPhotonSparseBrickCache3D* sparseBeamField;
} RuntimeCausticVolumeCache3D;

void RuntimeCausticVolumeCache3D_Init(RuntimeCausticVolumeCache3D* cache);
bool RuntimeCausticVolumeCache3D_IsAllocated(const RuntimeCausticVolumeCache3D* cache);
bool RuntimeCausticVolumeCache3D_Allocate(RuntimeCausticVolumeCache3D* cache,
                                          const RuntimeVolumeGrid3D* grid);
bool RuntimeCausticVolumeCache3D_AllocateFromVolume(
    RuntimeCausticVolumeCache3D* cache,
    const RuntimeVolumeAttachment3D* volume);
void RuntimeCausticVolumeCache3D_Clear(RuntimeCausticVolumeCache3D* cache);
void RuntimeCausticVolumeCache3D_Free(RuntimeCausticVolumeCache3D* cache);
bool RuntimeCausticVolumeCache3D_DepositAtPosition(RuntimeCausticVolumeCache3D* cache,
                                                   Vec3 position,
                                                   double radiance_r,
                                                   double radiance_g,
                                                   double radiance_b);
bool RuntimeCausticVolumeCache3D_DepositFootprintAtPosition(
    RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    double radius_world,
    double radiance_r,
    double radiance_g,
    double radiance_b);
bool RuntimeCausticVolumeCache3D_DepositDirectionalFootprintAtPosition(
    RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    Vec3 direction,
    double perpendicular_radius_world,
    double axial_radius_world,
    double radiance_r,
    double radiance_g,
    double radiance_b);
bool RuntimeCausticVolumeCache3D_SampleAtPosition(RuntimeCausticVolumeCache3D* cache,
                                                  Vec3 position,
                                                  Vec3* out_radiance);
bool RuntimeCausticVolumeCache3D_SampleFilteredAtPosition(
    RuntimeCausticVolumeCache3D* cache,
    Vec3 position,
    double radius_world,
    Vec3* out_radiance);
void RuntimeCausticVolumeCache3D_SnapshotDiagnostics(
    const RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticVolumeCacheDiagnostics3D* out_diagnostics);

#endif
