#include "render/integrator_common.h"
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
