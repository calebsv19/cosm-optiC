#include "render/integrator_common.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ClampTileSize(int requested) {
    if (requested < 4) return 4;
    if (requested % 4 != 0) {
        requested += 4 - (requested % 4);
    }
    return requested;
}

void TileGridFree(TileGrid* grid) {
    if (!grid || !grid->tiles) return;
    for (size_t i = 0; i < grid->count; i++) {
        free(grid->tiles[i].energy);
        grid->tiles[i].energy = NULL;
    }
    free(grid->tiles);
    grid->tiles = NULL;
    grid->count = 0;
    grid->tilesX = grid->tilesY = 0;
    grid->width = grid->height = 0;
}

void TileGridEnsure(TileGrid* grid, int width, int height, int tileSize) {
    if (!grid) return;
    tileSize = ClampTileSize(tileSize);
    if (grid->tiles &&
        grid->width == width &&
        grid->height == height &&
        grid->tileSize == tileSize) {
        return;
    }

    TileGridFree(grid);

    grid->tileSize = tileSize;
    grid->width = width;
    grid->height = height;
    grid->tilesX = (width + tileSize - 1) / tileSize;
    grid->tilesY = (height + tileSize - 1) / tileSize;
    grid->count = (size_t)grid->tilesX * (size_t)grid->tilesY;
    grid->tiles = (IntegratorTile*)calloc(grid->count, sizeof(IntegratorTile));
    if (!grid->tiles) {
        printf("ERROR: Failed to allocate tile grid.\n");
        grid->count = 0;
        return;
    }

    for (int ty = 0; ty < grid->tilesY; ty++) {
        for (int tx = 0; tx < grid->tilesX; tx++) {
            size_t idx = (size_t)ty * (size_t)grid->tilesX + (size_t)tx;
            IntegratorTile* tile = &grid->tiles[idx];
            tile->originX = tx * tileSize;
            tile->originY = ty * tileSize;
            tile->width = (tile->originX + tileSize > width) ? (width - tile->originX) : tileSize;
            tile->height = (tile->originY + tileSize > height) ? (height - tile->originY) : tileSize;
            tile->energy = (float*)calloc((size_t)tile->width * (size_t)tile->height, sizeof(float));
            if (!tile->energy) {
                printf("ERROR: Failed to allocate tile energy buffer.\n");
            }
        }
    }
}

void TileGridClear(TileGrid* grid) {
    if (!grid || !grid->tiles) return;
    for (size_t i = 0; i < grid->count; i++) {
        IntegratorTile* tile = &grid->tiles[i];
        if (tile->energy) {
            memset(tile->energy, 0, (size_t)tile->width * (size_t)tile->height * sizeof(float));
        }
    }
}

double Clamp(double value, double minValue, double maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

double Clamp01(double value) {
    return Clamp(value, 0.0, 1.0);
}

bool IrradianceCacheEnsure(IrradianceCache* cache, int objectCount, int samplesPerObject) {
    if (!cache || objectCount <= 0 || samplesPerObject <= 0) return false;
    int totalSamples = objectCount * samplesPerObject;
    if (cache->data && cache->objectCount == objectCount && cache->samplesPerObject == samplesPerObject) {
        memset(cache->data, 0, (size_t)totalSamples * sizeof(SurfaceIrradiance));
        return true;
    }
    free(cache->data);
    cache->data = (SurfaceIrradiance*)calloc((size_t)totalSamples, sizeof(SurfaceIrradiance));
    if (!cache->data) {
        cache->objectCount = 0;
        cache->samplesPerObject = 0;
        return false;
    }
    cache->objectCount = objectCount;
    cache->samplesPerObject = samplesPerObject;
    return true;
}

void IrradianceCacheClear(IrradianceCache* cache) {
    if (!cache) return;
    free(cache->data);
    cache->data = NULL;
    cache->objectCount = 0;
    cache->samplesPerObject = 0;
}

SurfaceIrradiance* IrradianceCacheGet(const IrradianceCache* cache, int objectIndex, int sampleIndex) {
    if (!cache || !cache->data) return NULL;
    if (objectIndex < 0 || objectIndex >= cache->objectCount) return NULL;
    if (sampleIndex < 0 || sampleIndex >= cache->samplesPerObject) return NULL;
    size_t idx = (size_t)objectIndex * (size_t)cache->samplesPerObject + (size_t)sampleIndex;
    return &cache->data[idx];
}

double PathLuminance(double throughput) {
    return fabs(throughput);
}

double ClampThroughput(double throughput, double minValue, double maxValue) {
    if (throughput < minValue) return minValue;
    if (throughput > maxValue) return maxValue;
    return throughput;
}

bool ShouldTerminatePath(double luminance, double threshold, FastRNG* rng) {
    if (threshold <= 0.0 || !rng) return false;
    if (luminance >= threshold) return false;
    double survival = luminance / threshold;
    if (survival <= 0.0) {
        return true;
    }
    double roll = FastRNGNextDouble(rng);
    return roll > survival;
}

void SeedPixelRNG(FastRNG* rng, uint64_t frameSeed, int pixelX, int pixelY, uint32_t salt) {
    if (!rng) return;
    uint64_t seed = frameSeed ^ 0x9E3779B97F4A7C15ULL;
    seed ^= ((uint64_t)(pixelX + 1) * 0xC2B2AE3D27D4EB4FULL);
    seed ^= ((uint64_t)(pixelY + 1) * 0x165667B19E3779F9ULL);
    uint64_t seq = ((uint64_t)salt + 1ULL) * 0x94D049BB133111EBULL;
    FastRNGSeed(rng, seed, seq);
}
