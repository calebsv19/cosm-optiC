#ifndef RENDER_INTEGRATOR_COMMON_H
#define RENDER_INTEGRATOR_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <SDL2/SDL_stdinc.h>
#include "scene/object_manager.h"
#include "render/uniform_grid.h"

#ifndef GRID_EPSILON
#define GRID_EPSILON 1e-6
#endif

#ifndef PATH_EPSILON
#define PATH_EPSILON 1e-3
#endif

#ifndef INV_PI
#define INV_PI (1.0 / M_PI)
#endif

typedef struct {
    int originX;
    int originY;
    int width;
    int height;
    float* energy;
} IntegratorTile;

typedef struct {
    IntegratorTile* tiles;
    int tileSize;
    int tilesX;
    int tilesY;
    size_t count;
    int width;
    int height;
} TileGrid;

typedef struct {
    Uint8* pixelBuffer;
    float* energyBuffer;
    int width;
    int height;
    SceneObject* objects;
    int objectCount;
    TileGrid* tileGrid;
    bool useTiles;
    uint64_t frameSeed;
    const UniformGrid* uniformGrid;
    int integratorMode;
} IntegratorContext;

typedef struct {
    double x;
    double y;
    double radius;
} LightSource;

int ClampTileSize(int requested);
void TileGridFree(TileGrid* grid);
void TileGridEnsure(TileGrid* grid, int width, int height, int tileSize);
void TileGridClear(TileGrid* grid);
double Clamp(double value, double minValue, double maxValue);
double Clamp01(double value);

#endif
