#ifndef RENDER_INTEGRATOR_COMMON_H
#define RENDER_INTEGRATOR_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <SDL2/SDL_stdinc.h>
#include "scene/object_manager.h"
#include "render/uniform_grid.h"
#include "render/fast_rng.h"
#include "render/surface_mesh.h"

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

#define IRRADIANCE_BIN_COUNT 256

typedef struct {
    double dirX;
    double dirY;
    double mean;
    double variance;
    double distance;
    int samples;
    bool valid;
} IrradianceBin;

typedef struct {
    double px;
    double py;
    double nx;
    double ny;
    IrradianceBin bins[IRRADIANCE_BIN_COUNT];
} SurfaceIrradiance;

typedef struct {
    SurfaceIrradiance* data;
    int samplesPerObject;
    int objectCount;
} IrradianceCache;

struct MaterialBSDF;

typedef struct {
    Uint8* pixelBuffer;
    float* energyBuffer;
    float* directEnergyBuffer;
    int width;
    int height;
    SceneObject* objects;
    int objectCount;
    TileGrid* tileGrid;
    bool useTiles;
    uint64_t frameSeed;
    const UniformGrid* uniformGrid;
    int integratorMode;
    IrradianceCache* cache;
    const struct MaterialBSDF* materials;
    int materialCount;
    SurfaceMesh* mesh;
    TriangleMesh* triangleMesh;
    uint64_t feelerSeed;
} IntegratorContext;

typedef struct {
    double x;
    double y;
    double radius;
} LightSource;

typedef struct {
    double throughput;
    double eta;
} RayPayload;

int ClampTileSize(int requested);
void TileGridFree(TileGrid* grid);
void TileGridEnsure(TileGrid* grid, int width, int height, int tileSize);
void TileGridClear(TileGrid* grid);
double Clamp(double value, double minValue, double maxValue);
double Clamp01(double value);
bool IrradianceCacheEnsure(IrradianceCache* cache, int objectCount, int samplesPerObject);
void IrradianceCacheClear(IrradianceCache* cache);
SurfaceIrradiance* IrradianceCacheGet(const IrradianceCache* cache, int objectIndex, int sampleIndex);
double PathLuminance(double throughput);
double ClampThroughput(double throughput, double minValue, double maxValue);
bool ShouldTerminatePath(double luminance, double threshold, FastRNG* rng);
void SeedPixelRNG(FastRNG* rng, uint64_t frameSeed, int pixelX, int pixelY, uint32_t salt);

#endif
