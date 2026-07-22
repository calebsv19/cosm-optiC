#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_SPARSE_BRICK_CACHE_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_SPARSE_BRICK_CACHE_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_volume_3d.h"

enum {
    RUNTIME_CAUSTIC_SPARSE_BRICK_EDGE_3D = 4,
    RUNTIME_CAUSTIC_SPARSE_BRICK_CELL_COUNT_3D = 64,
    RUNTIME_CAUSTIC_SPARSE_BRICK_FIELD_FLOAT_COUNT_3D = 29,
    RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_X_3D = 0,
    RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_Y_3D = 1,
    RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_Z_3D = 2,
    RUNTIME_CAUSTIC_SPARSE_FIELD_DISTANCE_WEIGHTED_3D = 3,
    RUNTIME_CAUSTIC_SPARSE_FIELD_DIRECTION_WEIGHT_3D = 4,
    RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_R_3D = 5,
    RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_G_3D = 13,
    RUNTIME_CAUSTIC_SPARSE_FIELD_SUBVOXEL_B_3D = 21
};

typedef struct RuntimeCausticPhotonSparseBrickCache3D
    RuntimeCausticPhotonSparseBrickCache3D;

typedef struct {
    uint32_t brickGridW;
    uint32_t brickGridH;
    uint32_t brickGridD;
    uint64_t directoryEntryCount;
    uint64_t allocatedBrickCount;
    uint64_t directoryBytes;
    uint64_t payloadBytes;
    uint64_t metadataBytes;
    uint64_t peakBytes;
    uint64_t allocationOrderHash;
    uint64_t allocationFailureCount;
} RuntimeCausticPhotonSparseBrickCacheStats3D;

bool RuntimeCausticPhotonSparseBrickCache3D_DirectoryShape(
    uint32_t gridW,
    uint32_t gridH,
    uint32_t gridD,
    uint32_t* outBrickW,
    uint32_t* outBrickH,
    uint32_t* outBrickD,
    uint64_t* outEntryCount);

RuntimeCausticPhotonSparseBrickCache3D*
RuntimeCausticPhotonSparseBrickCache3D_Create(
    const RuntimeVolumeGrid3D* grid,
    uint64_t denseBaseBytes,
    uint64_t maximumBrickCount);
void RuntimeCausticPhotonSparseBrickCache3D_Clear(
    RuntimeCausticPhotonSparseBrickCache3D* cache);
void RuntimeCausticPhotonSparseBrickCache3D_Destroy(
    RuntimeCausticPhotonSparseBrickCache3D* cache);
float* RuntimeCausticPhotonSparseBrickCache3D_AcquireCell(
    RuntimeCausticPhotonSparseBrickCache3D* cache,
    uint64_t linearCellIndex);
const float* RuntimeCausticPhotonSparseBrickCache3D_FindCell(
    const RuntimeCausticPhotonSparseBrickCache3D* cache,
    uint64_t linearCellIndex);
void RuntimeCausticPhotonSparseBrickCache3D_Snapshot(
    const RuntimeCausticPhotonSparseBrickCache3D* cache,
    RuntimeCausticPhotonSparseBrickCacheStats3D* outStats);

#endif
