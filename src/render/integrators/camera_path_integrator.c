#include "render/camera_path_integrator.h"
#include "config/config_manager.h"
#include "render/fast_rng.h"
#include "render/ray_types.h"
#include "camera/camera.h"
#include "render/timer_hud_api.h"
#include <SDL2/SDL.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define PATH_LIGHT_INTENSITY 2.0
#define PATH_FALLOFF_SCALE 0.8
#define CAMERA_ENERGY_BOOST 6.0f
#define DIRECT_SHADOW_SAMPLES 4
#define REFLECTION_PROBES 6
#define REFLECTION_RECURSE_WEIGHT 0.35

typedef struct {
    IntegratorContext* ctx;
    const LightSource* light;
    int yStart;
    int yEnd;
    int spp;
    int maxDepth;
} CameraSamplingJob;

static double RandomUnit(FastRNG* rng) {
    return FastRNGNextDouble(rng);
}

static double Halton(int index, int base) {
    double result = 0.0;
    double f = 1.0;
    while (index > 0) {
        f /= base;
        result += f * (index % base);
        index /= base;
    }
    return result;
}

static void NormalizeVector(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > GRID_EPSILON) {
        *x /= len;
        *y /= len;
    }
}

static double ObjectAlbedo(const SceneObject* obj) {
    double r = (double)((obj->color >> 16) & 0xFF) / 255.0;
    double g = (double)((obj->color >> 8) & 0xFF) / 255.0;
    double b = (double)(obj->color & 0xFF) / 255.0;
    double luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    if (luma < 0.05) luma = 0.05;
    return Clamp01(luma);
}

static bool ComputeSurfaceNormal(const SceneObject* obj, double px, double py, double* nx, double* ny) {
    if (strcmp(obj->type, "circle") == 0) {
        *nx = px - obj->x;
        *ny = py - obj->y;
        NormalizeVector(nx, ny);
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
        NormalizeVector(&bestNx, &bestNy);
        *nx = bestNx;
        *ny = bestNy;
        return true;
    }
    return false;
}

static void SampleHemisphereDirectionDeterministic(double nx,
                                                   double ny,
                                                   int sampleIndex,
                                                   int sampleCount,
                                                   double* dirX,
                                                   double* dirY) {
    double base = atan2(ny, nx);
    double t = ((double)sampleIndex + 0.5) / (double)sampleCount;
    double angle = base - M_PI_2 + t * M_PI;
    double jitter = (double)sampleIndex * 0.1732050807;
    angle += (Halton(sampleIndex + 1, 5) - 0.5) * 0.15 + jitter * 0.01;
    *dirX = cos(angle);
    *dirY = sin(angle);
    NormalizeVector(dirX, dirY);
}

static bool SampleSurfaceAtPoint(const IntegratorContext* ctx,
                                 double px,
                                 double py,
                                 HitInfo2D* hit,
                                 const SceneObject** outObj) {
    if (!ctx || !ctx->objects || ctx->objectCount == 0) return false;

    const SceneObject* obj = NULL;
    int index = -1;
    int* indices = NULL;
    int count = 0;

    if (ctx->uniformGrid && UniformGridPointTest(ctx->uniformGrid, px, py, &indices, &count)) {
        for (int i = 0; i < count; i++) {
            int objIndex = indices[i];
            if (objIndex < 0 || objIndex >= ctx->objectCount) continue;
            if (IsInsideObject((int)px, (int)py, &ctx->objects[objIndex])) {
                obj = &ctx->objects[objIndex];
                index = objIndex;
                break;
            }
        }
    }

    if (!obj) {
        for (int i = 0; i < ctx->objectCount; i++) {
            if (IsInsideObject((int)px, (int)py, &ctx->objects[i])) {
                obj = &ctx->objects[i];
                index = i;
                break;
            }
        }
    }

    if (!obj) {
        return false;
    }

    double nx = 0.0;
    double ny = -1.0;
    if (!ComputeSurfaceNormal(obj, px, py, &nx, &ny)) {
        nx = 0.0;
        ny = -1.0;
    }

    if (hit) {
        hit->px = px;
        hit->py = py;
        hit->nx = nx;
        hit->ny = ny;
        hit->objectIndex = index;
        hit->t = 0.0;
    }
    if (outObj) {
        *outObj = obj;
    }
    return true;
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
    NormalizeVector(&dirX, &dirY);
    Ray2D ray = {
        .ox = originX,
        .oy = originY,
        .dx = dirX,
        .dy = dirY
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

static bool IsOccluded(const IntegratorContext* ctx,
                       double originX,
                       double originY,
                       double dirX,
                       double dirY,
                       double maxDistance) {
    if (!ctx || !ctx->uniformGrid) return false;
    NormalizeVector(&dirX, &dirY);
    Ray2D ray = {
        .ox = originX,
        .oy = originY,
        .dx = dirX,
        .dy = dirY
    };
    HitInfo2D tmp;
    return UniformGridTraceRay(ctx->uniformGrid, &ray, PATH_EPSILON, maxDistance, &tmp);
}

static double ComputeBackgroundLighting(const IntegratorContext* ctx,
                                        const LightSource* light,
                                        double px,
                                        double py) {
    if (!light) return animSettings.environmentBrightness;
    double lx = light->x - px;
    double ly = light->y - py;
    double dist2 = lx * lx + ly * ly;
    if (dist2 < GRID_EPSILON) return PATH_LIGHT_INTENSITY;
    double dist = sqrt(dist2);
    double dirX = lx / dist;
    double dirY = ly / dist;
    if (IsOccluded(ctx, px + dirX * PATH_EPSILON, py + dirY * PATH_EPSILON, dirX, dirY, dist - PATH_EPSILON)) {
        return animSettings.environmentBrightness;
    }
    double falloff = PATH_LIGHT_INTENSITY / (dist * PATH_FALLOFF_SCALE + 1.0);
    return falloff + animSettings.environmentBrightness;
}

static double SampleDirectLightingMulti(const IntegratorContext* ctx,
                                        const LightSource* light,
                                        const HitInfo2D* hit,
                                        double albedo,
                                        FastRNG* rng) {
    if (!ctx || !light || !hit) return 0.0;
    double accum = 0.0;
    for (int i = 0; i < DIRECT_SHADOW_SAMPLES; i++) {
        double lx = light->x - hit->px;
        double ly = light->y - hit->py;
        double dist2 = lx * lx + ly * ly;
        if (dist2 < GRID_EPSILON) {
            accum += PATH_LIGHT_INTENSITY;
            continue;
        }
        double dist = sqrt(dist2);
        double dirX = lx / dist;
        double dirY = ly / dist;

        double jitter1 = (RandomUnit(rng) - 0.5) * 0.4;
        double jitter2 = (RandomUnit(rng) - 0.5) * 0.2;
        dirX += hit->nx * jitter1 + hit->ny * jitter2;
        dirY += hit->ny * jitter1 - hit->nx * jitter2;
        NormalizeVector(&dirX, &dirY);

        double cosTerm = fmax(hit->nx * dirX + hit->ny * dirY, 0.0);
        if (cosTerm <= 0.0) continue;

        double originX = hit->px + hit->nx * PATH_EPSILON;
        double originY = hit->py + hit->ny * PATH_EPSILON;
        if (IsOccluded(ctx, originX, originY, dirX, dirY, dist - PATH_EPSILON)) {
            continue;
        }
        double falloff = PATH_LIGHT_INTENSITY / (dist * PATH_FALLOFF_SCALE + 1.0);
        double brdf = albedo * INV_PI;
        accum += falloff * brdf * cosTerm;
    }
    return accum / DIRECT_SHADOW_SAMPLES;
}

static double SampleReflectionProbes(const IntegratorContext* ctx,
                                     const LightSource* light,
                                     const HitInfo2D* hit,
                                     double albedo,
                                     FastRNG* rng) {
    if (!ctx || !light || !hit || !ctx->uniformGrid) return 0.0;
    double total = 0.0;
    double lx = light->x - hit->px;
    double ly = light->y - hit->py;
    NormalizeVector(&lx, &ly);
    for (int i = 0; i < REFLECTION_PROBES; i++) {
        double mix = (double)i / (double)REFLECTION_PROBES;
        double jitter = (RandomUnit(rng) - 0.5) * 0.8;
        double dirX = hit->nx * (1.0 - mix) + lx * mix + hit->ny * jitter * 0.2;
        double dirY = hit->ny * (1.0 - mix) + ly * mix - hit->nx * jitter * 0.2;
        NormalizeVector(&dirX, &dirY);
        double originX = hit->px + hit->nx * PATH_EPSILON;
        double originY = hit->py + hit->ny * PATH_EPSILON;
        HitInfo2D bounceHit;
        const SceneObject* bounceObj = NULL;
        if (!TraceRayToSurface(ctx, originX, originY, dirX, dirY, &bounceHit, &bounceObj, 0.0)) {
            total += animSettings.environmentBrightness * 0.15;
            continue;
        }
        double bounceAlbedo = ObjectAlbedo(bounceObj);
        double bounceLight = SampleDirectLightingMulti(ctx, light, &bounceHit, bounceAlbedo, rng);
        if (ctx->cache) {
            int objIndex = (int)(bounceObj - ctx->objects);
            if (objIndex >= 0 && objIndex < ctx->objectCount) {
                int sampleIdx = (int)(fabs(bounceHit.nx) * (ctx->cache->samplesPerObject - 1));
                SurfaceIrradiance* sample = IrradianceCacheGet(ctx->cache, objIndex, sampleIdx);
                if (sample) {
                    bounceLight += sample->intensity;
                }
            }
        }
        total += bounceLight * albedo * 0.85;
    }
    return total / REFLECTION_PROBES;
}

static double SampleCachedIrradiance(const IntegratorContext* ctx,
                                     const SceneObject* obj,
                                     const HitInfo2D* hit) {
    if (!ctx || !ctx->cache || !ctx->cache->data) return 0.0;
    int objIndex = (int)(obj - ctx->objects);
    if (objIndex < 0 || objIndex >= ctx->objectCount) return 0.0;
    double angle = atan2(hit->py - obj->y, hit->px - obj->x);
    double u = (angle + M_PI) * (0.5 / M_PI);
    int sampleIdx = (int)(Clamp01(u) * (ctx->cache->samplesPerObject - 1));
    SurfaceIrradiance* sample = IrradianceCacheGet(ctx->cache, objIndex, sampleIdx);
    if (!sample) return 0.0;
    return sample->intensity;
}

static double TraceSurfaceRadiance(const IntegratorContext* ctx,
                                   const LightSource* light,
                                   const HitInfo2D* hit,
                                   const SceneObject* obj,
                                   FastRNG* rng,
                                   int depthRemaining) {
    if (!hit || !obj) return animSettings.environmentBrightness;
    double albedo = ObjectAlbedo(obj);
    double radiance = animSettings.environmentBrightness;

    if (animSettings.pathDirectLighting) {
        radiance += SampleDirectLightingMulti(ctx, light, hit, albedo, rng);
    }

    radiance += SampleReflectionProbes(ctx, light, hit, albedo, rng);
    radiance += SampleCachedIrradiance(ctx, obj, hit) * 0.9 * albedo;

    if (depthRemaining <= 1 || !ctx->uniformGrid) {
        return radiance;
    }

    double originX = hit->px + hit->nx * PATH_EPSILON;
    double originY = hit->py + hit->ny * PATH_EPSILON;

    double bounceAccum = 0.0;
    int bounceSamples = REFLECTION_PROBES * 2;
    for (int i = 0; i < bounceSamples; i++) {
        double dirX, dirY;
        SampleHemisphereDirectionDeterministic(hit->nx, hit->ny, i, bounceSamples, &dirX, &dirY);
        HitInfo2D bounceHit;
        const SceneObject* bounceObj = NULL;
        if (TraceRayToSurface(ctx, originX, originY, dirX, dirY, &bounceHit, &bounceObj, 0.0)) {
            double bounceAlbedo = ObjectAlbedo(bounceObj);
            double bounceLight = SampleDirectLightingMulti(ctx, light, &bounceHit, bounceAlbedo, rng);
            bounceAccum += bounceLight;
            if (depthRemaining > 1) {
                bounceAccum += REFLECTION_RECURSE_WEIGHT *
                    TraceSurfaceRadiance(ctx, light, &bounceHit, bounceObj, rng, depthRemaining - 1);
            }
        } else {
            bounceAccum += animSettings.environmentBrightness * 0.2;
        }
    }
    bounceAccum /= (double)bounceSamples;
    radiance += albedo * bounceAccum;
    return radiance;
}

static double ShadeSample(const IntegratorContext* ctx,
                          const LightSource* light,
                          double worldX,
                          double worldY,
                          FastRNG* rng,
                          int maxDepth) {
    HitInfo2D hit = {0};
    const SceneObject* obj = NULL;
    if (SampleSurfaceAtPoint(ctx, worldX, worldY, &hit, &obj)) {
        return TraceSurfaceRadiance(ctx, light, &hit, obj, rng, maxDepth);
    }
    return ComputeBackgroundLighting(ctx, light, worldX, worldY);
}

static void WriteEnergySample(const IntegratorContext* ctx,
                              int pixelX,
                              int pixelY,
                              float value) {
    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        TileGrid* grid = ctx->tileGrid;
        int tileSize = grid->tileSize;
        int tileX = pixelX / tileSize;
        int tileY = pixelY / tileSize;
        if (tileX < 0 || tileY < 0 || tileX >= grid->tilesX || tileY >= grid->tilesY) {
            return;
        }
        size_t tileIndex = (size_t)tileY * (size_t)grid->tilesX + (size_t)tileX;
        IntegratorTile* tile = &grid->tiles[tileIndex];
        if (!tile->energy) return;
        int localX = pixelX - tile->originX;
        int localY = pixelY - tile->originY;
        if (localX < 0 || localY < 0 || localX >= tile->width || localY >= tile->height) {
            return;
        }
        size_t localIndex = (size_t)localY * (size_t)tile->width + (size_t)localX;
        tile->energy[localIndex] = value;
        return;
    }

    if (ctx->energyBuffer) {
        size_t idx = (size_t)pixelY * (size_t)ctx->width + (size_t)pixelX;
        ctx->energyBuffer[idx] = value;
    }
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

static void TonemapTiles(const IntegratorContext* ctx, float invMaxEnergy) {
    if (!ctx->tileGrid || !ctx->tileGrid->tiles || !ctx->pixelBuffer) return;
    for (size_t tileIndex = 0; tileIndex < ctx->tileGrid->count; tileIndex++) {
        const IntegratorTile* tile = &ctx->tileGrid->tiles[tileIndex];
        if (!tile->energy) continue;
        for (int ly = 0; ly < tile->height; ly++) {
            for (int lx = 0; lx < tile->width; lx++) {
                size_t localIndex = (size_t)ly * (size_t)tile->width + (size_t)lx;
                float normalized = tile->energy[localIndex] * invMaxEnergy * CAMERA_ENERGY_BOOST;
                float tone = powf(Clamp01(normalized), 0.55f);
                int globalX = tile->originX + lx;
                int globalY = tile->originY + ly;
                size_t globalIndex = (size_t)globalY * (size_t)ctx->width + (size_t)globalX;
                ctx->pixelBuffer[globalIndex] = (Uint8)Clamp(tone * 255.0f, 0, 255);
            }
        }
    }
}

static int CameraSamplingWorker(void* data) {
    CameraSamplingJob* job = (CameraSamplingJob*)data;
    IntegratorContext* ctx = job->ctx;
    int width = ctx->width;
    int spp = job->spp;

    for (int y = job->yStart; y < job->yEnd; y++) {
        for (int x = 0; x < width; x++) {
            FastRNG rng;
            FastRNGSeed(&rng,
                        ctx->frameSeed + ((uint64_t)y * 1315423911ULL) + ((uint64_t)x * 2654435761ULL),
                        animSettings.pathSeed + x + y * width);
            double sum = 0.0;
            int haltonBase = ((y * width) + x) * spp;
            for (int s = 0; s < spp; s++) {
                int haltonIndex = haltonBase + s + 1;
                double jitterX = Halton(haltonIndex, 2);
                double jitterY = Halton(haltonIndex, 3);
                CameraPoint sample = CameraScreenToWorld(&sceneSettings.camera,
                                                         x + jitterX,
                                                         y + jitterY,
                                                         ctx->width,
                                                         ctx->height);
                sum += ShadeSample(ctx, job->light, sample.x, sample.y, &rng, job->maxDepth);
            }
            WriteEnergySample(ctx, x, y, (float)(sum / spp));
        }
    }
    return 0;
}

void CameraPathIntegratorRender(IntegratorContext* ctx,
                                const LightSource* light) {
    if (!ctx) return;
    int width = ctx->width;
    int height = ctx->height;
    size_t total = (size_t)width * (size_t)height;

    bool usingTiles = (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles);
    bool usingBuffer = (!usingTiles && ctx->energyBuffer);
    if (!usingTiles && !usingBuffer) {
        return;
    }

    if (usingBuffer) {
        memset(ctx->energyBuffer, 0, total * sizeof(float));
    }

    int spp = animSettings.pathSamplesPerPixel > 0 ? animSettings.pathSamplesPerPixel : 1;
    int maxDepth = animSettings.pathMaxDepth > 0 ? animSettings.pathMaxDepth : 1;

    ts_start_timer("CameraPath Sampling");
    int workerCount = SDL_GetCPUCount();
    if (workerCount < 1) workerCount = 1;
    if (workerCount > height) workerCount = height;
    int rowsPerWorker = height / workerCount;
    if (rowsPerWorker == 0) rowsPerWorker = 1;

    CameraSamplingJob* jobs = (CameraSamplingJob*)malloc((size_t)workerCount * sizeof(CameraSamplingJob));
    SDL_Thread** workers = (SDL_Thread**)malloc((size_t)workerCount * sizeof(SDL_Thread*));
    if (!jobs || !workers) {
        workerCount = 1;
    }

    int yStart = 0;
    for (int i = 0; i < workerCount; i++) {
        int yEnd = (i == workerCount - 1) ? height : (yStart + rowsPerWorker);
        if (yEnd > height) yEnd = height;
        if (yStart >= yEnd) {
            break;
        }
        jobs[i] = (CameraSamplingJob){
            .ctx = ctx,
            .light = light,
            .yStart = yStart,
            .yEnd = yEnd,
            .spp = spp,
            .maxDepth = maxDepth
        };
        workers[i] = SDL_CreateThread(CameraSamplingWorker, "CamPathWorker", &jobs[i]);
        if (!workers[i]) {
            CameraSamplingWorker(&jobs[i]);
        }
        yStart = yEnd;
    }
    for (int i = 0; i < workerCount; i++) {
        if (workers && workers[i]) {
            SDL_WaitThread(workers[i], NULL);
        }
    }
    free(workers);
    free(jobs);
    ts_stop_timer("CameraPath Sampling");

    if (!ctx->pixelBuffer) return;
    ts_start_timer("CameraPath Tonemap");
    bool tonemapComplete = false;
    if (usingTiles) {
        float maxEnergy = FindMaxTileEnergy(ctx->tileGrid);
        if (maxEnergy <= 0.0f) {
            memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));
        } else {
            TonemapTiles(ctx, 1.0f / maxEnergy);
        }
        tonemapComplete = true;
    }

    if (!tonemapComplete) {
        if (!ctx->energyBuffer) {
            memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));
            ts_stop_timer("CameraPath Tonemap");
            return;
        }

        float maxEnergy = 0.0f;
        for (size_t i = 0; i < total; i++) {
            if (ctx->energyBuffer[i] > maxEnergy) {
                maxEnergy = ctx->energyBuffer[i];
            }
        }
        if (maxEnergy <= 0.0f) {
            memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));
            ts_stop_timer("CameraPath Tonemap");
            return;
        }
        float invMax = 1.0f / maxEnergy;
        for (size_t i = 0; i < total; i++) {
            float normalized = ctx->energyBuffer[i] * invMax * CAMERA_ENERGY_BOOST;
            float tone = powf(Clamp01(normalized), 0.55f);
            ctx->pixelBuffer[i] = (Uint8)Clamp(tone * 255.0f, 0, 255);
        }
    }
    ts_stop_timer("CameraPath Tonemap");
}
