#include "render/forward_light_integrator.h"
#include "config/config_manager.h"
#include "render/fast_rng.h"
#include "render/ray_types.h"
#include "render/material_bsdf.h"
#include "camera/camera.h"
#include <SDL2/SDL.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#define MAX_BOUNCES 3
#define MIN_ENERGY 0.003
#define FORWARD_TONEMAP_GAMMA 0.55f
#define FORWARD_STEP 0.75
#define FORWARD_MAX_TRAVEL 600.0
#define FORWARD_PRIMARY_SCALE 4.0
#define FORWARD_SECONDARY_SCALE 0.4
#define FORWARD_DEBUG_REFLECTIONS 0

typedef struct {
    double sourceX, sourceY;
    int rayStart, rayEnd;
    const IntegratorContext* ctx;
    FastRNG rng;
} ThreadData;

typedef struct {
    double sum;
    double maxValue;
    size_t samples;
} EnergyStats;

static double ForwardFalloffDistance(const IntegratorContext* ctx);
static double ForwardDistanceAttenuation(double distance, double scale, int mode);
static void EnergyStatsAccumulate(EnergyStats* stats, float value);
static float EnergyStatsExposure(const EnergyStats* stats);
static float ComputeTileExposure(const TileGrid* grid);
static float ComputeBufferExposure(const IntegratorContext* ctx);
static Uint8 ForwardEnergyToPixel(float energy, float exposure);
static const MaterialBSDF* GetMaterial(const IntegratorContext* ctx, int objectIndex) {
    if (!ctx || !ctx->materials) return NULL;
    if (objectIndex < 0 || objectIndex >= ctx->materialCount) return NULL;
    return &ctx->materials[objectIndex];
}

static bool TraceRayToSurface(const IntegratorContext* ctx,
                              double originX,
                              double originY,
                              double dirX,
                              double dirY,
                              HitInfo2D* hit,
                              const SceneObject** outObj,
                              double maxDistance) {
    if (!ctx || !ctx->uniformGrid) return false;
    double dx = dirX;
    double dy = dirY;
    double len = sqrt(dx * dx + dy * dy);
    if (len <= GRID_EPSILON) return false;
    dx /= len;
    dy /= len;
    Ray2D ray = {
        .ox = originX,
        .oy = originY,
        .dx = dx,
        .dy = dy
    };
    HitInfo2D localHit;
    double tMin = PATH_EPSILON;
    double tMax = (maxDistance > 0.0) ? maxDistance : DBL_MAX;
    if (!UniformGridTraceRay(ctx->uniformGrid, &ray, tMin, tMax, &localHit)) {
        return false;
    }
    if (hit) {
        *hit = localHit;
    }
    if (outObj) {
        *outObj = &ctx->objects[localHit.objectIndex];
    }
    return true;
}

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

static bool DepositEnergy(const IntegratorContext* ctx,
                          double worldX,
                          double worldY,
                          double energy,
                          bool clampValue,
                          bool isDirect) {
    if (energy <= 0.0) return true;
    int pixelIndex;
    int screenX = 0;
    int screenY = 0;
    if (!WorldToPixel(worldX, worldY, ctx->width, ctx->height, &pixelIndex, &screenX, &screenY)) {
        return false;
    }

    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        int tileSize = ctx->tileGrid->tileSize;
        int tileX = screenX / tileSize;
        int tileY = screenY / tileSize;
        if (tileX >= ctx->tileGrid->tilesX || tileY >= ctx->tileGrid->tilesY || tileX < 0 || tileY < 0) {
            return false;
        }
        size_t tileIndex = (size_t)tileY * (size_t)ctx->tileGrid->tilesX + (size_t)tileX;
        IntegratorTile* tile = &ctx->tileGrid->tiles[tileIndex];
        if (!tile->energy) return false;
        int localX = screenX - tile->originX;
        int localY = screenY - tile->originY;
        if (localX < 0 || localY < 0 || localX >= tile->width || localY >= tile->height) return false;
        size_t localIndex = (size_t)localY * (size_t)tile->width + (size_t)localX;
        float* slot = &tile->energy[localIndex];
        if (clampValue) {
            if (energy > *slot) {
                *slot = (float)energy;
            }
        } else {
            *slot += (float)energy;
        }
    } else if (ctx->energyBuffer) {
        float* slot = &ctx->energyBuffer[pixelIndex];
        float* directSlot = (ctx->directEnergyBuffer ? &ctx->directEnergyBuffer[pixelIndex] : NULL);
        if (directSlot) {
            if (isDirect) {
                if (energy > *directSlot) {
                    *directSlot = (float)energy;
                }
            } else if (*directSlot > 0.0f) {
                if (energy > *directSlot) {
                    energy = *directSlot;
                }
            }
        }
        if (clampValue) {
            if (energy > *slot) {
                *slot = (float)energy;
            }
        } else {
            *slot += (float)energy;
        }
        if (directSlot && isDirect == false && *directSlot > 0.0f && *slot > *directSlot) {
            *slot = *directSlot;
        }
    }
    return true;
}

static double DepositSegmentEnergy(const IntegratorContext* ctx,
                                   double startX,
                                   double startY,
                                   double dirX,
                                   double dirY,
                                   double length,
                                   double energy,
                                   double scale,
                                   double step,
                                   double clampValue,
                                   bool clampDeposits,
                                   bool isDirect) {
    if (!ctx || length <= 0.0 || energy <= 0.0) {
        return energy;
    }
    double dx = dirX;
    double dy = dirY;
    double len = sqrt(dx * dx + dy * dy);
    if (len <= GRID_EPSILON) return energy;
    dx /= len;
    dy /= len;
    double traveled = length;
    bool exitedEarly = false;
    double t;
    double falloffDistance = ForwardFalloffDistance(ctx);
    int falloffMode = animSettings.forwardFalloffMode;
    bool useFalloff = (falloffMode != FORWARD_FALLOFF_MODE_NONE) && (falloffDistance > 0.0);
    for (t = 0.0; t < length; t += step) {
        double px = startX + dx * t;
        double py = startY + dy * t;
        double attenuation = useFalloff ? ForwardDistanceAttenuation(t, falloffDistance, falloffMode) : 1.0;
        double deposit = energy * scale * attenuation;
        if (clampValue > 0.0 && deposit > clampValue) {
            deposit = clampValue;
        }
        if (!DepositEnergy(ctx, px, py, deposit, clampDeposits, isDirect)) {
            traveled = t;
            exitedEarly = true;
            break;
        }
        traveled = t + step;
    }
    if (!exitedEarly) {
        traveled = length;
    }
    double effectiveLength = fmin(traveled, length);
    double remainingFactor = useFalloff ? ForwardDistanceAttenuation(effectiveLength, falloffDistance, falloffMode) : 1.0;
    return energy * remainingFactor;
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

static void TonemapTile(const IntegratorContext* ctx, const IntegratorTile* tile, float exposure) {
    if (!tile || !tile->energy) return;
    for (int ly = 0; ly < tile->height; ly++) {
        for (int lx = 0; lx < tile->width; lx++) {
            size_t localIndex = (size_t)ly * (size_t)tile->width + (size_t)lx;
            Uint8 pixel = ForwardEnergyToPixel(tile->energy[localIndex], exposure);
            int globalX = tile->originX + lx;
            int globalY = tile->originY + ly;
            size_t globalIndex = (size_t)globalY * (size_t)ctx->width + (size_t)globalX;
            ctx->pixelBuffer[globalIndex] = pixel;
        }
    }
}

typedef struct {
    IntegratorContext* ctx;
    float exposure;
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
        TonemapTile(payload->ctx, &grid->tiles[idx], payload->exposure);
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
        size_t totalPixels = (size_t)ctx->width * (size_t)ctx->height;
        float exposure = ComputeTileExposure(ctx->tileGrid);
        if (exposure <= 0.0f) {
            memset(ctx->pixelBuffer, 0, totalPixels * sizeof(Uint8));
            return;
        }

        TileJobPayload payload = {
            .ctx = ctx,
            .exposure = exposure
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
        float exposure = ComputeBufferExposure(ctx);
        if (exposure <= 0.0f) {
            memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));
            return;
        }
        for (int i = 0; i < total; i++) {
            ctx->pixelBuffer[i] = ForwardEnergyToPixel(ctx->energyBuffer[i], exposure);
        }
    }
}

static inline void EnergyStatsAccumulate(EnergyStats* stats, float value) {
    if (!stats || value <= 0.0f) return;
    stats->sum += value;
    stats->samples++;
    if (value > stats->maxValue) {
        stats->maxValue = value;
    }
}

static float EnergyStatsExposure(const EnergyStats* stats) {
    if (!stats || stats->samples == 0) {
        return 1.0f;
    }
    double mean = stats->sum / (double)stats->samples;
    double scale = mean * 6.0;
    if (stats->maxValue > scale * 4.0) {
        scale = (scale * 0.5) + (stats->maxValue * 0.1);
    }
    if (scale <= 1e-4) {
        scale = stats->maxValue > 0.0 ? stats->maxValue : 1e-4;
    }
    return (float)(1.0 / scale);
}

static float ComputeBufferExposure(const IntegratorContext* ctx) {
    if (!ctx || !ctx->energyBuffer) return 1.0f;
    size_t total = (size_t)ctx->width * (size_t)ctx->height;
    EnergyStats stats = {0};
    for (size_t i = 0; i < total; i++) {
        EnergyStatsAccumulate(&stats, ctx->energyBuffer[i]);
    }
    return EnergyStatsExposure(&stats);
}

static float ComputeTileExposure(const TileGrid* grid) {
    if (!grid || !grid->tiles) return 1.0f;
    EnergyStats stats = {0};
    for (size_t i = 0; i < grid->count; i++) {
        const IntegratorTile* tile = &grid->tiles[i];
        if (!tile->energy) continue;
        size_t total = (size_t)tile->width * (size_t)tile->height;
        for (size_t j = 0; j < total; j++) {
            EnergyStatsAccumulate(&stats, tile->energy[j]);
        }
    }
    return EnergyStatsExposure(&stats);
}

static Uint8 ForwardEnergyToPixel(float energy, float exposure) {
    float e = fmaxf(energy, 0.0f);
    float mapped = 1.0f - expf(-e * exposure);
    float tone = powf(Clamp01(mapped), FORWARD_TONEMAP_GAMMA);
    return (Uint8)Clamp(tone * 255.0f, 0, 255);
}

static double ForwardClampForBounce(int bounce) {
    double base = animSettings.lightIntensity * FORWARD_PRIMARY_SCALE * 1.5;
    if (bounce <= 0) return base;
    return base / (double)(bounce + 1);
}

static void TracePhotonPath(const IntegratorContext* ctx,
                            FastRNG* rng,
                            double originX,
                            double originY,
                            double dirX,
                            double dirY,
                            double initialThroughput,
                            int bounceOffset) {
    if (!ctx || !rng) return;
    double throughput = initialThroughput;
    double ox = originX;
    double oy = originY;
    double dx = dirX;
    double dy = dirY;

    for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
        HitInfo2D hit;
        const SceneObject* obj = NULL;
        bool haveHit = TraceRayToSurface(ctx, ox, oy, dx, dy, &hit, &obj, 0.0);
        double travel = haveHit ? fmax(hit.t, FORWARD_STEP) : hypot((dx != 0 ? ctx->width : 0), (dy != 0 ? ctx->height : 0));
        if (haveHit) {
            travel = fmax(hit.t, FORWARD_STEP);
        }
        int effectiveBounce = bounce + bounceOffset;
        bool isPrimaryBounce = (effectiveBounce == 0);
        double depositScale = isPrimaryBounce ? FORWARD_PRIMARY_SCALE : FORWARD_SECONDARY_SCALE;
#if FORWARD_DEBUG_REFLECTIONS
        if (isPrimaryBounce) depositScale = 0.0;
#endif
        bool clampDeposits = (effectiveBounce > 0);
        bool isDirectBounce = (effectiveBounce == 0);
        throughput = DepositSegmentEnergy(ctx,
                                          ox,
                                          oy,
                                          dx,
                                          dy,
                                          travel,
                                          throughput,
                                          depositScale,
                                          FORWARD_STEP,
                                          ForwardClampForBounce(effectiveBounce),
                                          clampDeposits,
                                          isDirectBounce);
        if (!haveHit) {
            break;
        }
        double hitContribution = throughput * (isPrimaryBounce ? 1.0 : (0.3 / (effectiveBounce + 1)));
#if FORWARD_DEBUG_REFLECTIONS
        if (!isPrimaryBounce) {
            DepositEnergy(ctx, hit.px, hit.py, hitContribution, true, false);
        }
#else
        DepositEnergy(ctx, hit.px, hit.py, hitContribution, !isPrimaryBounce, isDirectBounce);
#endif
        const MaterialBSDF* material = GetMaterial(ctx, hit.objectIndex);
        MaterialBSDF fallback = {0};
        if (!material && obj) {
            MaterialBSDFInitFromSceneObject(obj, &fallback);
            material = &fallback;
        }
        if (!material) {
            break;
        }

        int secondaryCount = (bounce == 0) ? 1 : 3;
        bool spawned = false;
        for (int sampleIdx = 0; sampleIdx < secondaryCount; sampleIdx++) {
            BSDFSample sample;
            if (!MaterialBSDFSample(material,
                                    hit.nx,
                                    hit.ny,
                                    dx,
                                    dy,
                                    rng,
                                    &sample)) {
                continue;
            }
            if (sample.pdf <= 1e-8 || sample.weight <= 0.0) {
                continue;
            }

            double nextThroughput = throughput * (sample.weight / sample.pdf);
            nextThroughput = ClampThroughput(nextThroughput, 0.0, 1e6);
            if (nextThroughput < MIN_ENERGY) {
                continue;
            }

            if (animSettings.pathRussianRoulette && bounce >= 1) {
                if (ShouldTerminatePath(nextThroughput, animSettings.rouletteThreshold, rng)) {
                    continue;
                }
            }

            if (!spawned) {
                ox = hit.px + hit.nx * PATH_EPSILON;
                oy = hit.py + hit.ny * PATH_EPSILON;
                dx = sample.dirX;
                dy = sample.dirY;
                throughput = nextThroughput;
                spawned = true;
            } else {
                TracePhotonPath(ctx,
                                rng,
                                hit.px + hit.nx * PATH_EPSILON,
                                hit.py + hit.ny * PATH_EPSILON,
                                sample.dirX,
                                sample.dirY,
                                nextThroughput,
                                effectiveBounce + 1);
            }
        }
        if (!spawned) {
            break;
        }
    }
}

static void EmitRayRange(const IntegratorContext* ctx,
                         FastRNG* rng,
                         double sourceX,
                         double sourceY,
                         int rayStart,
                         int rayEnd) {
    for (int ray = rayStart; ray < rayEnd; ray++) {
        double angle = (2.0 * M_PI * ray) / sceneSettings.rays;
        double dirX = cos(angle);
        double dirY = sin(angle);
        TracePhotonPath(ctx,
                        rng,
                        sourceX,
                        sourceY,
                        dirX,
                        dirY,
                        animSettings.lightIntensity,
                        0);
    }
}


static void FillRays(const IntegratorContext* ctx, const LightSource* light) {
    FastRNG rng;
    FastRNGSeed(&rng, ctx->frameSeed ^ 0x4d595df4d0f33173ULL, 0x631c8ULL);
    EmitRayRange(ctx, &rng, light->x, light->y, 0, sceneSettings.rays);
}

static int RayCalculationWorker(void* data) {
    ThreadData* threadData = (ThreadData*)data;
    EmitRayRange(threadData->ctx,
                 &threadData->rng,
                 threadData->sourceX,
                 threadData->sourceY,
                 threadData->rayStart,
                 threadData->rayEnd);
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
                .ctx = ctx
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
static double ForwardFalloffDistance(const IntegratorContext* ctx) {
    double distance = animSettings.forwardDecay;
    if (distance > 0.0) {
        return distance;
    }
    double w = (ctx && ctx->width > 0) ? (double)ctx->width : (double)sceneSettings.windowWidth;
    double h = (ctx && ctx->height > 0) ? (double)ctx->height : (double)sceneSettings.windowHeight;
    if (w <= 0.0) w = 1200.0;
    if (h <= 0.0) h = 800.0;
    return hypot(w, h);
}

static double ForwardDistanceAttenuation(double distance, double scale, int mode) {
    if (mode == FORWARD_FALLOFF_MODE_NONE || scale <= 0.0) {
        return 1.0;
    }
    double safeScale = fmax(scale, 1.0);
    double normalized = fmax(distance, 0.0) / safeScale;
    if (mode == FORWARD_FALLOFF_MODE_LINEAR) {
        return 1.0 / (1.0 + normalized);
    }
    double denom = 1.0 + normalized * normalized;
    return 1.0 / denom;
}
