#include "render/ray_tracing2.h"
#include "config/config_manager.h"  // Include animation.h to access `sceneObjects[]` and `objectCount`
#include "render/render_helper.h"
#include "editor/scene_editor.h"
#include "app/animation.h"
#include "camera/camera.h"
#include <stdio.h>   
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_atomic.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

typedef struct {
    Uint8* pixelBuffer;
    float* energyBuffer;
    int width;
    int height;
    SceneObject* objects;
    int objectCount;
    struct TileGrid* tileGrid;
    bool useTiles;
} TraceContext;

typedef struct {
    double sourceX, sourceY;
    int rayStart, rayEnd;
    const TraceContext* ctx;
} ThreadData;

typedef struct {
    int originX;
    int originY;
    int width;
    int height;
    float* energy;
} Tile;

typedef struct TileGrid {
    Tile* tiles;
    int tileSize;
    int tilesX;
    int tilesY;
    size_t count;
    int width;
    int height;
} TileGrid;


// Global light source.
static Circle light = {100, 100, 10};  // Default position (updated dynamically)
const int TOTAL_RAYS = 1800;
Uint8* pixelBuffer = NULL;  // Global but uninitialized
float* energyBuffer = NULL;
static TileGrid tileGrid = {0};
static bool rngSeeded = false;

static bool WorldToPixel(double worldX, double worldY,
                         int width, int height, int* pixelIndex,
                         int* screenX, int* screenY) {
    CameraPoint screen = CameraWorldToScreen(&sceneSettings.camera,
                                             worldX,
                                             worldY,
                                             width,
                                             height);
    int sx = (int)lround(screen.x);
    int sy = (int)lround(screen.y);
    if (screenX) *screenX = sx;
    if (screenY) *screenY = sy;
    if (sx < 0 || sx >= width || sy < 0 || sy >= height) {
        return false;
    }
    if (pixelIndex) {
        *pixelIndex = sy * width + sx;
    }
    return true;
}

static int ClampTileSize(int requested) {
    if (requested < 4) return 4;
    if (requested % 4 != 0) {
        requested += 4 - (requested % 4);
    }
    return requested;
}

static void FreeTileGrid(void) {
    if (!tileGrid.tiles) return;
    for (size_t i = 0; i < tileGrid.count; i++) {
        free(tileGrid.tiles[i].energy);
        tileGrid.tiles[i].energy = NULL;
    }
    free(tileGrid.tiles);
    tileGrid.tiles = NULL;
    tileGrid.count = 0;
    tileGrid.tilesX = tileGrid.tilesY = 0;
    tileGrid.width = tileGrid.height = 0;
}

static void EnsureTileGrid(int width, int height, int tileSize) {
    tileSize = ClampTileSize(tileSize);
    if (tileGrid.tiles &&
        tileGrid.width == width &&
        tileGrid.height == height &&
        tileGrid.tileSize == tileSize) {
        return;
    }

    FreeTileGrid();

    tileGrid.tileSize = tileSize;
    tileGrid.width = width;
    tileGrid.height = height;
    tileGrid.tilesX = (width + tileSize - 1) / tileSize;
    tileGrid.tilesY = (height + tileSize - 1) / tileSize;
    tileGrid.count = (size_t)tileGrid.tilesX * (size_t)tileGrid.tilesY;
    tileGrid.tiles = (Tile*)calloc(tileGrid.count, sizeof(Tile));
    if (!tileGrid.tiles) {
        printf("ERROR: Failed to allocate tile grid.\n");
        tileGrid.count = 0;
        return;
    }

    for (int ty = 0; ty < tileGrid.tilesY; ty++) {
        for (int tx = 0; tx < tileGrid.tilesX; tx++) {
            size_t idx = (size_t)ty * (size_t)tileGrid.tilesX + (size_t)tx;
            Tile* tile = &tileGrid.tiles[idx];
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

static void ClearTileEnergies(TileGrid* grid) {
    if (!grid || !grid->tiles) return;
    for (size_t i = 0; i < grid->count; i++) {
        Tile* tile = &grid->tiles[i];
        if (tile->energy) {
            memset(tile->energy, 0, (size_t)tile->width * (size_t)tile->height * sizeof(float));
        }
    }
}

#define MAX_BOUNCES 3
#define MIN_ENERGY 0.003
#define ENERGY_BOOST 4.5

static double RandomUnit(void) {
    if (!rngSeeded) {
        srand((unsigned int)time(NULL));
        rngSeeded = true;
    }
    return rand() / (double)RAND_MAX;
}

static bool ShouldTerminate(double energy) {
    double threshold = animSettings.rouletteThreshold;
    if (threshold <= 0.0) return false;
    if (energy >= threshold) return false;
    double survival = energy / threshold;
    if (survival <= 0.0) return true;
    double roll = RandomUnit();
    return roll > survival;
}

static double Clamp(double value, double minValue, double maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static double Clamp01(double value) {
    return Clamp(value, 0.0, 1.0);
}

static void Normalize(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 0.0001) {
        *x /= len;
        *y /= len;
    }
}

static double Noise2D(double x, double y) {
    double s = sin(x * 12.9898 + y * 78.233);
    double frac = s - floor(s);
    return frac;
}

static void DepositEnergy(const TraceContext* ctx, double worldX, double worldY, double energy) {
    if (energy <= 0.0) return;
    int pixelIndex;
    int screenX = 0;
    int screenY = 0;
    if (!WorldToPixel(worldX, worldY, ctx->width, ctx->height, &pixelIndex, &screenX, &screenY)) {
        return;
    }

    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        int tileSize = ctx->tileGrid->tileSize;
        int tileX = screenX / tileSize;
        int tileY = screenY / tileSize;
        if (tileX >= ctx->tileGrid->tilesX || tileY >= ctx->tileGrid->tilesY || tileX < 0 || tileY < 0) {
            return;
        }
        size_t tileIndex = (size_t)tileY * (size_t)ctx->tileGrid->tilesX + (size_t)tileX;
        Tile* tile = &ctx->tileGrid->tiles[tileIndex];
        if (!tile->energy) return;
        int localX = screenX - tile->originX;
        int localY = screenY - tile->originY;
        if (localX < 0 || localY < 0 || localX >= tile->width || localY >= tile->height) return;
        size_t localIndex = (size_t)localY * (size_t)tile->width + (size_t)localX;
        tile->energy[localIndex] += (float)energy;
    } else if (ctx->energyBuffer) {
        ctx->energyBuffer[pixelIndex] += (float)energy;
    }
}

static void ApplyEnergyDiffusion(TraceContext* ctx, int radius, double strength) {
    if (!ctx->energyBuffer || radius <= 0 || strength <= 0.0) return;
    int width = ctx->width;
    int height = ctx->height;
    size_t total = (size_t)width * (size_t)height;
    float* temp = (float*)malloc(total * sizeof(float));
    if (!temp) return;

    int clampedRadius = radius > 20 ? 20 : radius;
    if (clampedRadius < 1) {
        free(temp);
        return;
    }

    float sigma = (float)clampedRadius * 0.5f + 0.5f;
    float twoSigmaSq = 2.0f * sigma * sigma;
    float blend = (float)Clamp01(strength);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f;
            float weightSum = 0.0f;
            for (int dy = -clampedRadius; dy <= clampedRadius; dy++) {
                int sy = y + dy;
                if (sy < 0 || sy >= height) continue;
                for (int dx = -clampedRadius; dx <= clampedRadius; dx++) {
                    int sx = x + dx;
                    if (sx < 0 || sx >= width) continue;
                    float dist2 = (float)(dx * dx + dy * dy);
                    float weight = expf(-dist2 / twoSigmaSq);
                    accum += ctx->energyBuffer[sy * width + sx] * weight;
                    weightSum += weight;
                }
            }
            float blurred = (weightSum > 0.0f) ? (accum / weightSum) : ctx->energyBuffer[y * width + x];
            float original = ctx->energyBuffer[y * width + x];
            temp[y * width + x] = (1.0f - blend) * original + blend * blurred;
        }
    }

    memcpy(ctx->energyBuffer, temp, total * sizeof(float));
    free(temp);
}

static float FindMaxTileEnergy(const TileGrid* grid) {
    if (!grid || !grid->tiles) return 0.0f;
    float maxEnergy = 0.0f;
    for (size_t i = 0; i < grid->count; i++) {
        const Tile* tile = &grid->tiles[i];
        if (!tile->energy) continue;
        size_t total = (size_t)tile->width * (size_t)tile->height;
        for (size_t j = 0; j < total; j++) {
            if (tile->energy[j] > maxEnergy) {
                maxEnergy = tile->energy[j];
            }
        }
    }
    return maxEnergy;
}

static float FindMaxEnergyBuffer(const TraceContext* ctx) {
    if (!ctx->energyBuffer) return 0.0f;
    int total = ctx->width * ctx->height;
    float maxEnergy = 0.0f;
    for (int i = 0; i < total; i++) {
        if (ctx->energyBuffer[i] > maxEnergy) {
            maxEnergy = ctx->energyBuffer[i];
        }
    }
    return maxEnergy;
}

static void TonemapTile(const TraceContext* ctx, const Tile* tile, float invMaxEnergy) {
    if (!tile || !tile->energy) return;
    for (int ly = 0; ly < tile->height; ly++) {
        for (int lx = 0; lx < tile->width; lx++) {
            size_t localIndex = (size_t)ly * (size_t)tile->width + (size_t)lx;
            float normalized = tile->energy[localIndex] * invMaxEnergy * ENERGY_BOOST;
            float tone = powf(Clamp01(normalized), 0.55f);
            int globalX = tile->originX + lx;
            int globalY = tile->originY + ly;
            size_t globalIndex = (size_t)globalY * (size_t)ctx->width + (size_t)globalX;
            ctx->pixelBuffer[globalIndex] = (Uint8)Clamp(tone * 255.0f, 0, 255);
        }
    }
}

typedef struct {
    TraceContext* ctx;
    float invMaxEnergy;
    SDL_atomic_t cursor;
} TileJobPayload;

static int TileWorker(void* data) {
    TileJobPayload* payload = (TileJobPayload*)data;
    TileGrid* grid = payload->ctx->tileGrid;
    if (!grid) return 0;
    while (true) {
        int idx = SDL_AtomicAdd(&payload->cursor, 1);
        if (idx >= (int)grid->count) {
            break;
        }
        TonemapTile(payload->ctx, &grid->tiles[idx], payload->invMaxEnergy);
    }
    return 0;
}

static void ConvertEnergyToPixels(TraceContext* ctx) {
    if (!ctx->pixelBuffer) return;

    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        if (ctx->tileGrid->count == 0) {
            memset(ctx->pixelBuffer, 0, (size_t)ctx->width * (size_t)ctx->height * sizeof(Uint8));
            return;
        }
        float maxEnergy = FindMaxTileEnergy(ctx->tileGrid);
        size_t totalPixels = (size_t)ctx->width * (size_t)ctx->height;
        if (maxEnergy <= 0.0f) {
            memset(ctx->pixelBuffer, 0, totalPixels * sizeof(Uint8));
            return;
        }
        float invMax = 1.0f / maxEnergy;

        TileJobPayload payload = {
            .ctx = ctx,
            .invMaxEnergy = invMax
        };
        SDL_AtomicSet(&payload.cursor, 0);

        int workerCount = SDL_GetCPUCount();
        if (workerCount <= 0) workerCount = 4;
        if (workerCount > (int)ctx->tileGrid->count) {
            workerCount = (int)ctx->tileGrid->count;
        }
        if (workerCount < 1) workerCount = 1;

        SDL_Thread** workers = (SDL_Thread**)malloc((size_t)workerCount * sizeof(SDL_Thread*));
        for (int i = 0; i < workerCount; i++) {
            workers[i] = SDL_CreateThread(TileWorker, "TileWorker", &payload);
        }
        for (int i = 0; i < workerCount; i++) {
            if (workers[i]) {
                SDL_WaitThread(workers[i], NULL);
            }
        }
        free(workers);
    } else if (ctx->energyBuffer) {
        int total = ctx->width * ctx->height;
        float maxEnergy = FindMaxEnergyBuffer(ctx);
        if (maxEnergy <= 0.0f) {
            memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));
            return;
        }
        float invMax = 1.0f / maxEnergy;
        for (int i = 0; i < total; i++) {
            float normalized = ctx->energyBuffer[i] * invMax * ENERGY_BOOST;
            float tone = powf(Clamp01(normalized), 0.55f);
            ctx->pixelBuffer[i] = (Uint8)Clamp(tone * 255.0f, 0, 255);
        }
    }
}

static void ReflectDirection(double dx, double dy, double nx, double ny, double* rx, double* ry) {
    double dot = dx * nx + dy * ny;
    *rx = dx - 2.0 * dot * nx;
    *ry = dy - 2.0 * dot * ny;
    Normalize(rx, ry);
}

static void ApplyRoughness(double* dx, double* dy, double roughness, double jitter) {
    if (roughness <= 0.0) return;
    double angle = atan2(*dy, *dx);
    double spread = roughness * 0.5 * M_PI;
    double offset = (jitter - floor(jitter)) - 0.5;
    angle += offset * spread;
    *dx = cos(angle);
    *dy = sin(angle);
}

static bool ComputeSurfaceNormal(const SceneObject* obj, double px, double py, double* nx, double* ny) {
    if (strcmp(obj->type, "circle") == 0) {
        *nx = px - obj->x;
        *ny = py - obj->y;
        Normalize(nx, ny);
        return true;
    }

    if (obj->numPoints < 2) {
        return false;
    }

    double minDist = 1e9;
    double bestNx = 0.0, bestNy = 0.0;
    for (int i = 0; i < obj->numPoints; i++) {
        int next = (i + 1) % obj->numPoints;
        double x1 = obj->shapePoints[i][0] + obj->x;
        double y1 = obj->shapePoints[i][1] + obj->y;
        double x2 = obj->shapePoints[next][0] + obj->x;
        double y2 = obj->shapePoints[next][1] + obj->y;

        double edgeX = x2 - x1;
        double edgeY = y2 - y1;
        double edgeLen = edgeX * edgeX + edgeY * edgeY;
        if (edgeLen < 1e-6) continue;

        double t = ((px - x1) * edgeX + (py - y1) * edgeY) / edgeLen;
        t = Clamp01(t);
        double closestX = x1 + edgeX * t;
        double closestY = y1 + edgeY * t;
        double dx = px - closestX;
        double dy = py - closestY;
        double dist = dx * dx + dy * dy;

        if (dist < minDist) {
            minDist = dist;
            bestNx = dy;
            bestNy = -dx;
        }
    }

    if (minDist < 1e8) {
        double centerDX = px - obj->x;
        double centerDY = py - obj->y;
        double dot = bestNx * centerDX + bestNy * centerDY;
        if (dot < 0) {
            bestNx = -bestNx;
            bestNy = -bestNy;
        }
        Normalize(&bestNx, &bestNy);
        *nx = bestNx;
        *ny = bestNy;
        return true;
    }
    return false;
}

static double SampleTexture(const SceneObject* obj, double px, double py) {
    (void)px;
    (void)py;
    double r = (double)((obj->color >> 16) & 0xFF);
    double g = (double)((obj->color >> 8) & 0xFF);
    double b = (double)(obj->color & 0xFF);
    double colorLuma = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0;
    double base = Clamp01(colorLuma);
    if (obj->opacity < 1.0) {
        base *= obj->opacity;
    }

    switch (obj->textureId) {
        case 1: {
            int checker = (((int)floor(px / 25.0) + (int)floor(py / 25.0)) & 1);
            return base * (checker ? 0.8 : 0.4);
        }
        case 2: {
            double stripes = fabs(sin(px * 0.1));
            return base * (0.5 + 0.5 * stripes);
        }
        case 3: {
            double ring = fmod(sqrt((px - obj->x) * (px - obj->x) + (py - obj->y) * (py - obj->y)), 30.0);
            return base * (ring < 15.0 ? 0.9 : 0.3);
        }
        default:
            return base;
    }
}

static void TraceRayRecursive(const TraceContext* ctx,
                              double originX,
                              double originY,
                              double dirX,
                              double dirY,
                              int depth,
                              double energy) {
    if (depth > MAX_BOUNCES || energy < MIN_ENERGY) {
        return;
    }

    double x = originX;
    double y = originY;
    double step = 1.5;
    double decay = 0.9985;
    int maxSteps = ctx->width + ctx->height;

    for (int i = 0; i < maxSteps; i++) {
        x += dirX * step;
        y += dirY * step;

        if (!WorldToPixel(x, y, ctx->width, ctx->height, NULL, NULL, NULL)) {
            break;
        }

        DepositEnergy(ctx, x, y, energy * 0.8);

        SceneObject* hitObj = NULL;
        for (int j = 0; j < ctx->objectCount; j++) {
            if (IsInsideObject((int)x, (int)y, &ctx->objects[j])) {
                hitObj = &ctx->objects[j];
                break;
            }
        }

        if (hitObj) {
            double nx = 0.0, ny = 0.0;
            if (!ComputeSurfaceNormal(hitObj, x, y, &nx, &ny)) {
                nx = -dirX;
                ny = -dirY;
            }

            double surfaceSample = SampleTexture(hitObj, x, y);
            double surfaceEnergy = energy * surfaceSample * 1.25;
            DepositEnergy(ctx, x, y, surfaceEnergy);

            double reflectivity = Clamp01(hitObj->reflectivity);
            double roughness = Clamp01(hitObj->roughness);
            double specEnergy = surfaceEnergy * reflectivity;
            double diffuseEnergy = surfaceEnergy * (1.0 - reflectivity);

            if (specEnergy > MIN_ENERGY) {
                double rx, ry;
                ReflectDirection(dirX, dirY, nx, ny, &rx, &ry);
                double jitter = Noise2D(x + depth * 13.37, y - depth * 7.31);
                ApplyRoughness(&rx, &ry, roughness, jitter);
                TraceRayRecursive(ctx, x + nx * 0.5, y + ny * 0.5, rx, ry, depth + 1, specEnergy * 0.9);
            }

            if (diffuseEnergy > MIN_ENERGY) {
                double jitter = Noise2D(x * 0.5 + depth * 2.0, y * 0.5 - depth * 3.0);
                double baseAngle = atan2(ny, nx);
                double spread = (0.5 + roughness * 0.5) * M_PI_2;
                double newAngle = baseAngle + (jitter - 0.5) * spread;
                double dx = cos(newAngle);
                double dy = sin(newAngle);
                TraceRayRecursive(ctx, x + nx * 0.2, y + ny * 0.2, dx, dy, depth + 1, diffuseEnergy * 0.7);
            }
            return;
        }

        energy *= decay;
        if (energy < MIN_ENERGY) {
            break;
        }
        if (ShouldTerminate(energy)) {
            break;
        }
    }
}

static void EmitRayRange(const TraceContext* ctx,
                         double sourceX,
                         double sourceY,
                         int rayStart,
                         int rayEnd) {
    for (int ray = rayStart; ray < rayEnd; ray++) {
        double angle = (2.0 * M_PI * ray) / sceneSettings.rays;
        double dirX = cos(angle);
        double dirY = sin(angle);
        TraceRayRecursive(ctx, sourceX, sourceY, dirX, dirY, 0, 1.0);
    }
}

void InitRayTracingScene(void) {
    printf("DEBUG: Initializing Ray Tracing Scene\n");

    if (sceneSettings.objectCount == 0) {
        printf("WARNING: No scene objects detected! Rendering empty scene.\n");
    } else {
        printf("INFO: Loaded %d scene objects for ray tracing.\n", sceneSettings.objectCount);
        for (int i = 0; i < sceneSettings.objectCount; i++) {
            printf("  → Object %d: Type: %s, X: %.2f, Y: %.2f\n",
                   i, sceneSettings.sceneObjects[i].type, 
                   (double)sceneSettings.sceneObjects[i].x, 
                   (double)sceneSettings.sceneObjects[i].y);
        }
    }

    int WIDTH = sceneSettings.windowWidth;
    int HEIGHT = sceneSettings.windowHeight;

    size_t pixelCount = (size_t)WIDTH * (size_t)HEIGHT;

    if (pixelBuffer == NULL) {
        pixelBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!pixelBuffer) {
            printf("ERROR: Failed to allocate memory for pixel buffer!\n");
            exit(1);
        }
    }

    if (energyBuffer == NULL) {
        energyBuffer = (float*)malloc(pixelCount * sizeof(float));
        if (!energyBuffer) {
            printf("ERROR: Failed to allocate memory for energy buffer!\n");
            exit(1);
        }
    }

    memset(pixelBuffer, 0, pixelCount * sizeof(Uint8)); // Clear buffer
    memset(energyBuffer, 0, pixelCount * sizeof(float));

    // Use the first Bézier path point as the default light position
    if (sceneSettings.bezierPath.numPoints > 0) {
        light.x = sceneSettings.bezierPath.points[0].x;
        light.y = sceneSettings.bezierPath.points[0].y;
        printf("INFO: Light source initialized from Bézier Path at (%.2f, %.2f)\n", light.x, light.y);
    } else {
        // Fallback to hardcoded default if Bézier path is invalid
        light.x = 100;
        light.y = 100;
        printf("WARNING: Bézier Path is uninitialized! Using default light position (100, 100)\n");
    }
}

void CleanupRayTracing(void) {
    if (pixelBuffer != NULL) {
        free(pixelBuffer);
        pixelBuffer = NULL;
    }
    if (energyBuffer != NULL) {
        free(energyBuffer);
        energyBuffer = NULL;
    }
    FreeTileGrid();
}

void SetLightPosition(double x, double y) {
    light.x = x;
    light.y = y;
}


static void BuildGaussianKernel(float* kernel, int radius) {
    float sigma = (float)radius * 0.5f + 0.5f;
    float sum = 0.0f;
    for (int i = -radius; i <= radius; i++) {
        float value = expf(-(i * i) / (2.0f * sigma * sigma));
        kernel[i + radius] = value;
        sum += value;
    }
    if (sum > 0.0f) {
        for (int i = 0; i < (2 * radius + 1); i++) {
            kernel[i] /= sum;
        }
    }
}

static void ApplySeparableBlur(Uint8* pixelBuffer, int width, int height, int radius) {
    if (radius <= 0 || !pixelBuffer) return;
    int kernelSize = radius * 2 + 1;
    float* kernel = (float*)malloc((size_t)kernelSize * sizeof(float));
    if (!kernel) return;
    BuildGaussianKernel(kernel, radius);

    size_t total = (size_t)width * (size_t)height;
    float* temp = (float*)malloc(total * sizeof(float));
    float* output = (float*)malloc(total * sizeof(float));
    if (!temp || !output) {
        free(kernel);
        free(temp);
        free(output);
        return;
    }

    // Horizontal pass
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f;
            for (int k = -radius; k <= radius; k++) {
                int sx = x + k;
                if (sx < 0) sx = 0;
                if (sx >= width) sx = width - 1;
                accum += kernel[k + radius] * pixelBuffer[y * width + sx];
            }
            temp[y * width + x] = accum;
        }
    }

    // Vertical pass
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f;
            for (int k = -radius; k <= radius; k++) {
                int sy = y + k;
                if (sy < 0) sy = 0;
                if (sy >= height) sy = height - 1;
                accum += kernel[k + radius] * temp[sy * width + x];
            }
            output[y * width + x] = accum;
        }
    }

    for (size_t i = 0; i < total; i++) {
        pixelBuffer[i] = (Uint8)Clamp(output[i], 0, 255);
    }

    free(kernel);
    free(temp);
    free(output);
}


void FillRays(const TraceContext* ctx, double x, double y) {
    EmitRayRange(ctx, x, y, 0, sceneSettings.rays);
}

int RayCalculationWorker(void* data) {
    ThreadData* threadData = (ThreadData*)data;
    EmitRayRange(threadData->ctx, threadData->sourceX, threadData->sourceY,
                 threadData->rayStart, threadData->rayEnd);
    return 0;
}

void RenderRayTracingScene(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    int WIDTH = sceneSettings.windowWidth;
    int HEIGHT = sceneSettings.windowHeight;

    size_t pixelCount = (size_t)WIDTH * (size_t)HEIGHT;

    bool useTiles = animSettings.useTiledRenderer;
    int tileSize = useTiles ? ClampTileSize(animSettings.tileSize) : 0;

    // Ensure pixelBuffer is allocated
    if (pixelBuffer == NULL) {
        pixelBuffer = (Uint8*)malloc(pixelCount * sizeof(Uint8));
        if (!pixelBuffer) {
            printf("ERROR: Failed to allocate pixel buffer during render.\n");
            return;
        }
    }

    if (!useTiles) {
        if (energyBuffer == NULL) {
            energyBuffer = (float*)malloc(pixelCount * sizeof(float));
            if (!energyBuffer) {
                printf("ERROR: Failed to allocate energy buffer during render.\n");
                return;
            }
        }
        memset(energyBuffer, 0, pixelCount * sizeof(float));
    } else {
        EnsureTileGrid(WIDTH, HEIGHT, tileSize);
        ClearTileEnergies(&tileGrid);
    }
    
    memset(pixelBuffer, 0, pixelCount * sizeof(Uint8)); // Clear buffer

    TraceContext context = {
        .pixelBuffer = pixelBuffer,
        .energyBuffer = useTiles ? NULL : energyBuffer,
        .width = WIDTH,
        .height = HEIGHT,
        .objects = sceneSettings.sceneObjects,
        .objectCount = sceneSettings.objectCount,
        .tileGrid = useTiles ? &tileGrid : NULL,
        .useTiles = useTiles
    };

    if (animSettings.lightMode == 0) {
        // Original long-ray mode
        FillRays(&context, light.x, light.y);
    } else if (animSettings.lightMode == 1) {
        const int NUM_THREADS = 4;
        SDL_Thread* threads[NUM_THREADS];
        ThreadData threadData[NUM_THREADS];
        int raysPerThread = sceneSettings.rays / NUM_THREADS;

        for (int i = 0; i < NUM_THREADS; i++) {
            threadData[i] = (ThreadData){
                .sourceX = light.x,
                .sourceY = light.y,
                .rayStart = i * raysPerThread,
                .rayEnd = (i == NUM_THREADS - 1) ? sceneSettings.rays : (i + 1) * raysPerThread,
                .ctx = &context
            };
            threads[i] = SDL_CreateThread(RayCalculationWorker, "RayWorker", &threadData[i]);
        }
        for (int i = 0; i < NUM_THREADS; i++)
            SDL_WaitThread(threads[i], NULL);
    }

    if (!useTiles && animSettings.lightDiffusionEnabled && animSettings.lightDiffusionRadius > 0) {
        ApplyEnergyDiffusion(&context,
                             animSettings.lightDiffusionRadius,
                             animSettings.lightDiffusionStrength);
    }

    ConvertEnergyToPixels(&context);

    int blurRadius = 0;
    if (animSettings.blurMode == 1) blurRadius = 1;
    else if (animSettings.blurMode == 2) blurRadius = 2;
    else if (animSettings.blurMode == 3) blurRadius = 3;
    if (blurRadius > 0) {
        ApplySeparableBlur(pixelBuffer, WIDTH, HEIGHT, blurRadius);
    }

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Uint8 brightness = pixelBuffer[y * WIDTH + x];
            if (brightness > 0) {
                SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    CameraPoint lightScreen = CameraWorldToScreen(&sceneSettings.camera,
                                                  light.x,
                                                  light.y,
                                                  WIDTH,
                                                  HEIGHT);
    int lightRadius = (int)lround(light.r * sceneSettings.camera.zoom);
    RenderCircle(renderer, (int)lround(lightScreen.x), (int)lround(lightScreen.y), lightRadius, true);


    // ✅ Draw objects using the new method
    for (int i = 0; i < sceneSettings.objectCount; i++) {
        int brightness = CalculateObjectBrightness(&sceneSettings.sceneObjects[i], light.x, light.y);
	SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
        RenderSceneObject(renderer, &sceneSettings.sceneObjects[i], true);
    }

    SDL_RenderPresent(renderer);
}


void ProcessRayTracingEvent(SDL_Event* event) {
    if (event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONDOWN) {
        CameraPoint world = CameraScreenToWorld(&sceneSettings.camera,
                                                event->motion.x,
                                                event->motion.y,
                                                sceneSettings.windowWidth,
                                                sceneSettings.windowHeight);
        light.x = world.x;
        light.y = world.y;
    }
    else if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_b) {  // Press "B" to switch blur mode
            animSettings.blurMode = (animSettings.blurMode == 2) ? 0 : animSettings.blurMode + 1;
            printf("Blur Mode: %s\n", (animSettings.blurMode == 0) ? "None" :
                                        (animSettings.blurMode == 1) ? "Light Blur" :
                                        "Heavy Blur");
    }
}


void GetCurrentLightPosition(double* x, double* y) {
    *x = light.x;
    *y = light.y;
}
