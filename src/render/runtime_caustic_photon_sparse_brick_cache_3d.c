#include "render/runtime_caustic_photon_sparse_brick_cache_3d.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t directoryIndex;
    uint64_t allocationOrdinal;
    float cells[RUNTIME_CAUSTIC_SPARSE_BRICK_CELL_COUNT_3D]
               [RUNTIME_CAUSTIC_SPARSE_BRICK_FIELD_FLOAT_COUNT_3D];
} RuntimeCausticPhotonSparseBrick3D;

struct RuntimeCausticPhotonSparseBrickCache3D {
    RuntimeVolumeGrid3D grid;
    uint32_t brickGridW;
    uint32_t brickGridH;
    uint32_t brickGridD;
    uint64_t directoryEntryCount;
    RuntimeCausticPhotonSparseBrick3D** directory;
    uint64_t allocatedBrickCount;
    uint64_t denseBaseBytes;
    uint64_t maximumBrickCount;
    uint64_t allocationOrderHash;
    uint64_t allocationFailureCount;
};

bool RuntimeCausticPhotonSparseBrickCache3D_DirectoryShape(
    uint32_t gridW,
    uint32_t gridH,
    uint32_t gridD,
    uint32_t* outBrickW,
    uint32_t* outBrickH,
    uint32_t* outBrickD,
    uint64_t* outEntryCount) {
    uint32_t brickW;
    uint32_t brickH;
    uint32_t brickD;
    uint64_t count;
    if (gridW == 0u || gridH == 0u || gridD == 0u ||
        !outBrickW || !outBrickH || !outBrickD || !outEntryCount) return false;
    brickW = gridW / 4u + (gridW % 4u != 0u);
    brickH = gridH / 4u + (gridH % 4u != 0u);
    brickD = gridD / 4u + (gridD % 4u != 0u);
    count = (uint64_t)brickW * (uint64_t)brickH;
    if (count > UINT64_MAX / brickD ||
        count * brickD > (uint64_t)(SIZE_MAX / sizeof(void*))) return false;
    *outBrickW = brickW;
    *outBrickH = brickH;
    *outBrickD = brickD;
    *outEntryCount = count * brickD;
    return true;
}

static bool sparse_brick_location(
    const RuntimeCausticPhotonSparseBrickCache3D* cache,
    uint64_t index,
    uint64_t* outDirectoryIndex,
    uint32_t* outLocalIndex) {
    uint64_t plane;
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t bx;
    uint32_t by;
    uint32_t bz;
    if (!cache || index >= cache->grid.cellCount ||
        !outDirectoryIndex || !outLocalIndex) return false;
    plane = (uint64_t)cache->grid.gridW * (uint64_t)cache->grid.gridH;
    z = (uint32_t)(index / plane);
    index -= (uint64_t)z * plane;
    y = (uint32_t)(index / cache->grid.gridW);
    x = (uint32_t)(index - (uint64_t)y * cache->grid.gridW);
    bx = x / RUNTIME_CAUSTIC_SPARSE_BRICK_EDGE_3D;
    by = y / RUNTIME_CAUSTIC_SPARSE_BRICK_EDGE_3D;
    bz = z / RUNTIME_CAUSTIC_SPARSE_BRICK_EDGE_3D;
    *outDirectoryIndex = (uint64_t)bx + (uint64_t)cache->brickGridW *
        ((uint64_t)by + (uint64_t)cache->brickGridH * (uint64_t)bz);
    *outLocalIndex = (x & 3u) + 4u * ((y & 3u) + 4u * (z & 3u));
    return *outDirectoryIndex < cache->directoryEntryCount;
}

RuntimeCausticPhotonSparseBrickCache3D*
RuntimeCausticPhotonSparseBrickCache3D_Create(
    const RuntimeVolumeGrid3D* grid,
    uint64_t denseBaseBytes,
    uint64_t maximumBrickCount) {
    RuntimeCausticPhotonSparseBrickCache3D* cache;
    uint64_t count;
    if (!RuntimeVolumeGrid3D_IsConfigured(grid)) return NULL;
    cache = (RuntimeCausticPhotonSparseBrickCache3D*)calloc(1u, sizeof(*cache));
    if (!cache) return NULL;
    cache->grid = *grid;
    if (!RuntimeCausticPhotonSparseBrickCache3D_DirectoryShape(
            grid->gridW, grid->gridH, grid->gridD,
            &cache->brickGridW, &cache->brickGridH, &cache->brickGridD,
            &count)) {
        free(cache);
        return NULL;
    }
    cache->directory = (RuntimeCausticPhotonSparseBrick3D**)calloc(
        (size_t)count, sizeof(*cache->directory));
    if (!cache->directory) {
        free(cache);
        return NULL;
    }
    cache->directoryEntryCount = count;
    cache->denseBaseBytes = denseBaseBytes;
    cache->maximumBrickCount = maximumBrickCount;
    return cache;
}

void RuntimeCausticPhotonSparseBrickCache3D_Clear(
    RuntimeCausticPhotonSparseBrickCache3D* cache) {
    if (!cache) return;
    for (uint64_t i = 0u; i < cache->directoryEntryCount; ++i) {
        free(cache->directory[i]);
        cache->directory[i] = NULL;
    }
    cache->allocatedBrickCount = 0u;
    cache->allocationOrderHash = 0u;
    cache->allocationFailureCount = 0u;
}

void RuntimeCausticPhotonSparseBrickCache3D_Destroy(
    RuntimeCausticPhotonSparseBrickCache3D* cache) {
    if (!cache) return;
    RuntimeCausticPhotonSparseBrickCache3D_Clear(cache);
    free(cache->directory);
    free(cache);
}

float* RuntimeCausticPhotonSparseBrickCache3D_AcquireCell(
    RuntimeCausticPhotonSparseBrickCache3D* cache,
    uint64_t linearCellIndex) {
    RuntimeCausticPhotonSparseBrick3D* brick;
    uint64_t directoryIndex;
    uint32_t localIndex;
    if (!sparse_brick_location(
            cache, linearCellIndex, &directoryIndex, &localIndex)) return NULL;
    brick = cache->directory[directoryIndex];
    if (!brick) {
        if (cache->maximumBrickCount > 0u &&
            cache->allocatedBrickCount >= cache->maximumBrickCount) {
            cache->allocationFailureCount++;
            return NULL;
        }
        brick = (RuntimeCausticPhotonSparseBrick3D*)calloc(1u, sizeof(*brick));
        if (!brick) {
            cache->allocationFailureCount++;
            return NULL;
        }
        brick->directoryIndex = directoryIndex;
        brick->allocationOrdinal = cache->allocatedBrickCount;
        cache->directory[directoryIndex] = brick;
        cache->allocatedBrickCount++;
        cache->allocationOrderHash =
            (cache->allocationOrderHash * UINT64_C(1099511628211)) ^
            (directoryIndex + UINT64_C(1469598103934665603));
    }
    return brick->cells[localIndex];
}

const float* RuntimeCausticPhotonSparseBrickCache3D_FindCell(
    const RuntimeCausticPhotonSparseBrickCache3D* cache,
    uint64_t linearCellIndex) {
    uint64_t directoryIndex;
    uint32_t localIndex;
    if (!sparse_brick_location(
            cache, linearCellIndex, &directoryIndex, &localIndex) ||
        !cache->directory[directoryIndex]) return NULL;
    return cache->directory[directoryIndex]->cells[localIndex];
}

void RuntimeCausticPhotonSparseBrickCache3D_Snapshot(
    const RuntimeCausticPhotonSparseBrickCache3D* cache,
    RuntimeCausticPhotonSparseBrickCacheStats3D* outStats) {
    const uint64_t payloadPerBrick = sizeof(((RuntimeCausticPhotonSparseBrick3D*)0)->cells);
    RuntimeCausticPhotonSparseBrickCacheStats3D stats;
    memset(&stats, 0, sizeof(stats));
    if (!outStats) return;
    if (cache) {
        stats.brickGridW = cache->brickGridW;
        stats.brickGridH = cache->brickGridH;
        stats.brickGridD = cache->brickGridD;
        stats.directoryEntryCount = cache->directoryEntryCount;
        stats.allocatedBrickCount = cache->allocatedBrickCount;
        stats.directoryBytes = cache->directoryEntryCount *
            sizeof(*cache->directory);
        stats.payloadBytes = cache->allocatedBrickCount * payloadPerBrick;
        stats.metadataBytes = sizeof(*cache) + cache->allocatedBrickCount *
            (sizeof(RuntimeCausticPhotonSparseBrick3D) - payloadPerBrick);
        stats.peakBytes = cache->denseBaseBytes + stats.directoryBytes +
            stats.payloadBytes + stats.metadataBytes;
        stats.allocationOrderHash = cache->allocationOrderHash;
        stats.allocationFailureCount = cache->allocationFailureCount;
    }
    *outStats = stats;
}
