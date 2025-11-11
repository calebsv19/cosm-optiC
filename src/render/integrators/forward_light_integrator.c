#include "render/forward_light_integrator.h"
#include "config/config_manager.h"
#include "render/fast_rng.h"
#include "render/ray_types.h"
#include "camera/camera.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_atomic.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#define MAX_BOUNCES 3
#define MIN_ENERGY 0.003
#define ENERGY_BOOST 6.0

typedef struct {
    double sourceX, sourceY;
    int rayStart, rayEnd;
    const IntegratorContext* ctx;
    const LightSource* light;
    FastRNG rng;
} ThreadData;

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

static double RandomUnit(FastRNG* rng) {
    return FastRNGNextDouble(rng);
}

static bool ShouldTerminate(double energy, FastRNG* rng) {
    double threshold = animSettings.rouletteThreshold;
    if (threshold <= 0.0) return false;
    if (energy >= threshold) return false;
    double survival = energy / threshold;
    if (survival <= 0.0) return true;
    double roll = RandomUnit(rng);
    return roll > survival;
}

static void Normalize(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 0.0001) {
        *x /= len;
        *y /= len;
    }
}

static void DepositEnergy(const IntegratorContext* ctx, double worldX, double worldY, double energy) {
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
        IntegratorTile* tile = &ctx->tileGrid->tiles[tileIndex];
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

static void ApplyEnergyDiffusion(IntegratorContext* ctx, int radius, double strength) {
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
        const IntegratorTile* tile = &grid->tiles[i];
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

static float FindMaxEnergyBuffer(const IntegratorContext* ctx) {
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

static void TonemapTile(const IntegratorContext* ctx, const IntegratorTile* tile, float invMaxEnergy) {
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
    IntegratorContext* ctx;
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

static void ConvertEnergyToPixels(IntegratorContext* ctx) {
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

static void TraceRayRecursive(const IntegratorContext* ctx,
                              const LightSource* light,
                              FastRNG* rng,
                              double originX,
                              double originY,
                              double dirX,
                              double dirY,
                              int depth,
                              double energy) {
    bool forwardMode = (ctx->integratorMode == 0);
    int bounceLimit = forwardMode ? MAX_BOUNCES : MAX_BOUNCES;
    double minEnergy = forwardMode ? MIN_ENERGY * 0.2 : MIN_ENERGY;

    if (depth > bounceLimit || energy < minEnergy) {
        return;
    }

    double x = originX;
    double y = originY;
    double step = forwardMode ? 1.0 : 1.5;
    double decay = forwardMode ? 0.9993 : 0.9985;
    int maxSteps = ctx->width + ctx->height;

    for (int i = 0; i < maxSteps; i++) {
        x += dirX * step;
        y += dirY * step;

        if (!WorldToPixel(x, y, ctx->width, ctx->height, NULL, NULL, NULL)) {
            break;
        }

        double depositScale = forwardMode ? (depth == 0 ? 1.8 : 1.0) : 0.8;
        DepositEnergy(ctx, x, y, energy * depositScale);

        SceneObject* hitObj = NULL;
        int* cellIndices = NULL;
        int cellCount = 0;
        bool checked = false;
        if (ctx->uniformGrid && UniformGridPointTest(ctx->uniformGrid, x, y, &cellIndices, &cellCount)) {
            checked = true;
            for (int j = 0; j < cellCount; j++) {
                int objIndex = cellIndices[j];
                if (objIndex < 0 || objIndex >= ctx->objectCount) continue;
                if (IsInsideObject((int)x, (int)y, &ctx->objects[objIndex])) {
                    hitObj = &ctx->objects[objIndex];
                    break;
                }
            }
        }
        if (!checked) {
            for (int j = 0; j < ctx->objectCount; j++) {
                if (IsInsideObject((int)x, (int)y, &ctx->objects[j])) {
                    hitObj = &ctx->objects[j];
                    break;
                }
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
                double jitter = RandomUnit(rng);
                ApplyRoughness(&rx, &ry, roughness, jitter);
                TraceRayRecursive(ctx, light, rng, x + nx * 0.5, y + ny * 0.5, rx, ry, depth + 1, specEnergy * 0.9);
            }

            if (diffuseEnergy > MIN_ENERGY) {
                double jitter = RandomUnit(rng);
                double baseAngle = atan2(ny, nx);
                double spread = (0.5 + roughness * 0.5) * M_PI_2;
                double newAngle = baseAngle + (jitter - 0.5) * spread;
                double dx = cos(newAngle);
                double dy = sin(newAngle);
                TraceRayRecursive(ctx, light, rng, x + nx * 0.2, y + ny * 0.2, dx, dy, depth + 1, diffuseEnergy * 0.7);
            }
            return;
        }

        energy *= decay;
        if (energy < minEnergy) {
            break;
        }
        if (!forwardMode && ShouldTerminate(energy, rng)) {
            break;
        }
    }
}

static void EmitRayRange(const IntegratorContext* ctx,
                         const LightSource* light,
                         FastRNG* rng,
                         double sourceX,
                         double sourceY,
                         int rayStart,
                         int rayEnd,
                         double initialEnergy) {
    (void)light;
    for (int ray = rayStart; ray < rayEnd; ray++) {
        double angle = (2.0 * M_PI * ray) / sceneSettings.rays;
        double dirX = cos(angle);
        double dirY = sin(angle);
        TraceRayRecursive(ctx, light, rng, sourceX, sourceY, dirX, dirY, 0, initialEnergy);
    }
}

static void FillRays(const IntegratorContext* ctx, const LightSource* light) {
    FastRNG rng;
    FastRNGSeed(&rng, ctx->frameSeed ^ 0x4d595df4d0f33173ULL, 0x631c8ULL);
    double baseEnergy = (ctx->integratorMode == 0) ? 3.0 : 1.0;
    EmitRayRange(ctx, light, &rng, light->x, light->y, 0, sceneSettings.rays, baseEnergy);
}

static int RayCalculationWorker(void* data) {
    ThreadData* threadData = (ThreadData*)data;
    double baseEnergy = (threadData->ctx->integratorMode == 0) ? 3.0 : 1.0;
    EmitRayRange(threadData->ctx,
                 threadData->light,
                 &threadData->rng,
                 threadData->sourceX,
                 threadData->sourceY,
                 threadData->rayStart,
                 threadData->rayEnd,
                 baseEnergy);
    return 0;
}

void ForwardLightIntegratorRender(IntegratorContext* ctx,
                                  const LightSource* light) {
    if (!ctx || !light) return;
    if (animSettings.lightMode == 0) {
        FillRays(ctx, light);
    } else {
        const int NUM_THREADS = 4;
        SDL_Thread* threads[NUM_THREADS];
        ThreadData threadData[NUM_THREADS];
        int raysPerThread = sceneSettings.rays / NUM_THREADS;
        for (int i = 0; i < NUM_THREADS; i++) {
            threadData[i] = (ThreadData){
                .sourceX = light->x,
                .sourceY = light->y,
                .rayStart = i * raysPerThread,
                .rayEnd = (i == NUM_THREADS - 1) ? sceneSettings.rays : (i + 1) * raysPerThread,
                .ctx = ctx,
                .light = light
            };
            FastRNGSeed(&threadData[i].rng,
                        ctx->frameSeed ^ ((uint64_t)i + 1ULL) * 0x9E3779B185EBCA87ULL,
                        (uint64_t)threadData[i].rayStart + 1ULL);
            threads[i] = SDL_CreateThread(RayCalculationWorker, "RayWorker", &threadData[i]);
        }
        for (int i = 0; i < NUM_THREADS; i++) {
            SDL_WaitThread(threads[i], NULL);
        }
    }

    if (!ctx->useTiles && animSettings.lightDiffusionEnabled && animSettings.lightDiffusionRadius > 0) {
        ApplyEnergyDiffusion(ctx,
                             animSettings.lightDiffusionRadius,
                             animSettings.lightDiffusionStrength);
    }
    ConvertEnergyToPixels(ctx);
}
