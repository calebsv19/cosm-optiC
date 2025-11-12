#include "render/camera_path_integrator.h"
#include "config/config_manager.h"
#include "render/ray_types.h"
#include "camera/camera.h"
#include "render/timer_hud_api.h"
#include <SDL2/SDL.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

#define CAMERA_TONEMAP_GAMMA 0.55f
#define CAMERA_TONEMAP_EXPOSURE 0.6f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define INDIRECT_FEELER_COUNT 48
#define INDIRECT_DISTANCE_FALLOFF 0.0015f
#define INDIRECT_DISTANCE_DECAY 80.0
#define MAX_FEELER_DISTANCE 1500.0

typedef struct {
    size_t rayTests;
    size_t rayHits;
    size_t segmentFound;
    size_t segmentFacing;
    size_t cacheContributions;
} IndirectStats;

static double PixelFeelerJitter(int pixelX, int pixelY, int feelerIndex) {
    uint32_t h = 0x9E3779B9u;
    h ^= (uint32_t)(pixelX * 73856093u);
    h = (h << 13) | (h >> 19);
    h ^= (uint32_t)(pixelY * 19349663u);
    h = h * 0x85EBCA6Bu;
    h ^= (uint32_t)(feelerIndex * 83492791u);
    h *= 0xC2B2AE35u;
    double scaled = (double)(h & 0xFFFFFFu) / (double)0x1000000u; // [0,1)
    return (scaled - 0.5) * 0.25; // small jitter
}

static void FeelerDirection(int index,
                            double jitter,
                            double* dirX,
                            double* dirY) {
    double u = ((double)index + 0.5 + jitter) / (double)INDIRECT_FEELER_COUNT;
    double angle = u * 2.0 * M_PI;
    *dirX = cos(angle);
    *dirY = sin(angle);
}

static void NormalizeVector(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > GRID_EPSILON) {
        *x /= len;
        *y /= len;
    }
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

static const SurfaceIrradiance* FindClosestCacheEntry(const IntegratorContext* ctx,
                                                      int objectIndex,
                                                      double px,
                                                      double py) {
    if (!ctx || !ctx->cache || !ctx->cache->data) return NULL;
    if (objectIndex < 0 || objectIndex >= ctx->objectCount) return NULL;

    const SurfaceIrradiance* best = NULL;
    double bestDist2 = DBL_MAX;
    for (int i = 0; i < ctx->cache->samplesPerObject; i++) {
        SurfaceIrradiance* entry = IrradianceCacheGet(ctx->cache, objectIndex, i);
        if (!entry) continue;
        double dx = px - entry->px;
        double dy = py - entry->py;
        double dist2 = dx * dx + dy * dy;
        if (dist2 < bestDist2) {
            bestDist2 = dist2;
            best = entry;
        }
    }
    return best;
}

static bool FetchTileSample(const IntegratorContext* ctx,
                            int pixelX,
                            int pixelY,
                            IntegratorTile** outTile,
                            int* outLocalX,
                            int* outLocalY) {
    if (!ctx || !ctx->tileGrid || !ctx->tileGrid->tiles) {
        return false;
    }
    TileGrid* grid = ctx->tileGrid;
    if (pixelX < 0 || pixelY < 0 || pixelX >= grid->width || pixelY >= grid->height) {
        return false;
    }
    int tileSize = grid->tileSize;
    int tileX = pixelX / tileSize;
    int tileY = pixelY / tileSize;
    if (tileX < 0 || tileY < 0 || tileX >= grid->tilesX || tileY >= grid->tilesY) {
        return false;
    }
    size_t tileIndex = (size_t)tileY * (size_t)grid->tilesX + (size_t)tileX;
    IntegratorTile* tile = &grid->tiles[tileIndex];
    if (!tile->energy) {
        return false;
    }
    int localX = pixelX - tile->originX;
    int localY = pixelY - tile->originY;
    if (localX < 0 || localY < 0 || localX >= tile->width || localY >= tile->height) {
        return false;
    }
    if (outTile) *outTile = tile;
    if (outLocalX) *outLocalX = localX;
    if (outLocalY) *outLocalY = localY;
    return true;
}

static float ReadEnergySample(const IntegratorContext* ctx,
                              int pixelX,
                              int pixelY) {
    if (!ctx) return 0.0f;
    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        IntegratorTile* tile = NULL;
        int localX = 0;
        int localY = 0;
        if (!FetchTileSample(ctx, pixelX, pixelY, &tile, &localX, &localY)) {
            return 0.0f;
        }
        size_t idx = (size_t)localY * (size_t)tile->width + (size_t)localX;
        return tile->energy[idx];
    }

    if (!ctx->energyBuffer) return 0.0f;
    if (pixelX < 0 || pixelY < 0 || pixelX >= ctx->width || pixelY >= ctx->height) {
        return 0.0f;
    }
    size_t idx = (size_t)pixelY * (size_t)ctx->width + (size_t)pixelX;
    return ctx->energyBuffer[idx];
}

static void WriteEnergySample(const IntegratorContext* ctx,
                              int pixelX,
                              int pixelY,
                              float value) {
    if (!ctx) return;
    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        IntegratorTile* tile = NULL;
        int localX = 0;
        int localY = 0;
        if (!FetchTileSample(ctx, pixelX, pixelY, &tile, &localX, &localY)) {
            return;
        }
        size_t idx = (size_t)localY * (size_t)tile->width + (size_t)localX;
        tile->energy[idx] = value;
        return;
    }

    if (!ctx->energyBuffer) return;
    if (pixelX < 0 || pixelY < 0 || pixelX >= ctx->width || pixelY >= ctx->height) {
        return;
    }
    size_t idx = (size_t)pixelY * (size_t)ctx->width + (size_t)pixelX;
    ctx->energyBuffer[idx] = value;
}

static void AccumulateEnergyAdd(const IntegratorContext* ctx,
                                int pixelX,
                                int pixelY,
                                float value) {
    if (!ctx || value <= 0.0f) return;
    float current = ReadEnergySample(ctx, pixelX, pixelY);
    WriteEnergySample(ctx, pixelX, pixelY, current + value);
}

static float ComputeDirectRadiance(const LightSource* light,
                                   double worldX,
                                   double worldY) {
    if (!light) return 0.0f;
    double dx = light->x - worldX;
    double dy = light->y - worldY;
    double dist2 = dx * dx + dy * dy;
    double radius = fmax(light->radius, 1.0);
    double area = M_PI * radius * radius;
    double radiance = animSettings.lightIntensity * area / (dist2 + radius * radius);
    return (float)radiance;
}

static float ComputeSegmentRadiance(const SurfaceSegment* segment,
                                    const LightSource* light,
                                    double radiance,
                                    double worldX,
                                    double worldY) {
    if (!segment || radiance <= 0.0f) return 0.0f;
    double cx = 0.5 * (segment->x0 + segment->x1);
    double cy = 0.5 * (segment->y0 + segment->y1);
    double dirX = worldX - cx;
    double dirY = worldY - cy;
    double dist2 = dirX * dirX + dirY * dirY;
    double distance = sqrt(dist2);
    if (distance < PATH_EPSILON) {
        distance = PATH_EPSILON;
        dist2 = PATH_EPSILON * PATH_EPSILON;
    }
    dirX /= distance;
    dirY /= distance;
    double facing = fmax(0.0, segment->nx * dirX + segment->ny * dirY);
    double emission = radiance * facing;
    double radius = (light) ? fmax(light->radius, 1.0) : 1.0;
    double base = emission / (dist2 + radius * radius);
    double decay = exp(-distance / INDIRECT_DISTANCE_DECAY);
    double attenuation = decay / (1.0 + dist2 * INDIRECT_DISTANCE_FALLOFF);
    double Lref = 60.0;
    double lenScale = Clamp(segment->length / Lref, 0.5, 2.0);
    return (float)(base * attenuation * lenScale);
}

static bool SegmentSeesPixel(const IntegratorContext* ctx,
                             const SurfaceSegment* segment,
                             double worldX,
                             double worldY) {
    if (!ctx || !segment) return false;
    double cx = 0.5 * (segment->x0 + segment->x1);
    double cy = 0.5 * (segment->y0 + segment->y1);
    double dirX = worldX - cx;
    double dirY = worldY - cy;
    double dist = hypot(dirX, dirY);
    if (dist <= PATH_EPSILON) {
        return true;
    }
    NormalizeVector(&dirX, &dirY);
    double originX = cx + dirX * PATH_EPSILON;
    double originY = cy + dirY * PATH_EPSILON;
    double maxDist = dist - PATH_EPSILON;
    if (maxDist <= PATH_EPSILON) {
        return true;
    }
    return !IsOccluded(ctx, originX, originY, dirX, dirY, maxDist);
}

static bool SegmentLitByLight(const IntegratorContext* ctx,
                              const LightSource* light,
                              const HitInfo2D* hit) {
    if (!ctx || !light || !hit) return false;
    double dirX = light->x - hit->px;
    double dirY = light->y - hit->py;
    double dist = hypot(dirX, dirY);
    if (dist <= PATH_EPSILON) {
        return true;
    }
    NormalizeVector(&dirX, &dirY);
    double originX = hit->px + hit->nx * PATH_EPSILON;
    double originY = hit->py + hit->ny * PATH_EPSILON;
    double maxDistance = fmax(dist - PATH_EPSILON, PATH_EPSILON);
    return !IsOccluded(ctx, originX, originY, dirX, dirY, maxDistance);
}

static bool HasDirectLineOfSight(const IntegratorContext* ctx,
                                 double worldX,
                                 double worldY,
                                 const LightSource* light) {
    if (!light) return false;
    double dirX = light->x - worldX;
    double dirY = light->y - worldY;
    double dist = sqrt(dirX * dirX + dirY * dirY);
    if (dist <= PATH_EPSILON) {
        return true;
    }
    NormalizeVector(&dirX, &dirY);
    double offset = PATH_EPSILON * 32.0;
    double originX = worldX + dirX * offset;
    double originY = worldY + dirY * offset;
    double maxDistance = fmax(dist - offset - PATH_EPSILON, PATH_EPSILON);
    return !IsOccluded(ctx, originX, originY, dirX, dirY, maxDistance);
}

static void PerformDirectLightingPass(IntegratorContext* ctx,
                                      const LightSource* light) {
    if (!ctx || !light) return;
    size_t losTests = 0;
    size_t losHits = 0;
    for (int y = 0; y < ctx->height; y++) {
        for (int x = 0; x < ctx->width; x++) {
            CameraPoint world = CameraScreenToWorld(&sceneSettings.camera,
                                                    x + 0.5,
                                                    y + 0.5,
                                                    ctx->width,
                                                    ctx->height);
            losTests++;
            if (!HasDirectLineOfSight(ctx, world.x, world.y, light)) {
                continue;
            }
            losHits++;
            float radiance = ComputeDirectRadiance(light, world.x, world.y);
            AccumulateEnergyAdd(ctx, x, y, radiance);
        }
    }
    printf("[CameraIntegr] Direct pass: %zu LOS hits out of %zu tests.\n", losHits, losTests);
}

static float SampleCacheRadiance(const IntegratorContext* ctx,
                                 const HitInfo2D* hit,
                                 double targetX,
                                 double targetY) {
    if (!ctx || !ctx->cache || !hit) return 0.0f;
    const SurfaceIrradiance* entry = FindClosestCacheEntry(ctx,
                                                           hit->objectIndex,
                                                           hit->px,
                                                           hit->py);
    if (!entry) {
        return 0.0f;
    }
    double toPixelX = targetX - hit->px;
    double toPixelY = targetY - hit->py;
    NormalizeVector(&toPixelX, &toPixelY);

    double bestDot = -1.0;
    double bestMean = 0.0;
    for (int i = 0; i < IRRADIANCE_BIN_COUNT; i++) {
        const IrradianceBin* bin = &entry->bins[i];
        if (!bin->valid || bin->samples == 0) continue;
        double dot = bin->dirX * toPixelX + bin->dirY * toPixelY;
        if (dot > bestDot) {
            bestDot = dot;
            bestMean = bin->mean;
        }
    }
    return (float)fmax(bestMean, 0.0);
}

static bool SegmentFacesPoint(const SurfaceSegment* segment,
                              double worldX,
                              double worldY) {
    double toPointX = worldX - 0.5 * (segment->x0 + segment->x1);
    double toPointY = worldY - 0.5 * (segment->y0 + segment->y1);
    return (segment->nx * toPointX + segment->ny * toPointY) > 0.0;
}

static double EstimateAverageObjectExtent(const IntegratorContext* ctx) {
    if (!ctx || !ctx->objects || ctx->objectCount <= 0) {
        return 100.0;
    }
    double total = 0.0;
    for (int i = 0; i < ctx->objectCount; i++) {
        const SceneObject* obj = &ctx->objects[i];
        double extent = 0.0;
        if (strcmp(obj->type, "circle") == 0) {
            extent = fmax(obj->radius * obj->scale * 2.0, 1.0);
        } else if (obj->numPoints > 0) {
            double minX = DBL_MAX;
            double minY = DBL_MAX;
            double maxX = -DBL_MAX;
            double maxY = -DBL_MAX;
            for (int p = 0; p < obj->numPoints; p++) {
                double px = obj->shapePoints[p][0] + obj->x;
                double py = obj->shapePoints[p][1] + obj->y;
                if (px < minX) minX = px;
                if (py < minY) minY = py;
                if (px > maxX) maxX = px;
                if (py > maxY) maxY = py;
            }
            double dx = maxX - minX;
            double dy = maxY - minY;
            extent = fmax(hypot(dx, dy), 1.0);
        }
        if (extent <= 0.0) extent = 1.0;
        total += extent;
    }
    double average = total / (double)ctx->objectCount;
    return fmax(average, 25.0);
}

static float SampleIndirectLighting(const IntegratorContext* ctx,
                                    const LightSource* light,
                                    double worldX,
                                    double worldY,
                                    int pixelX,
                                    int pixelY,
                                    double feelerDistanceLimit,
                                    IndirectStats* stats) {
    if (!ctx || !ctx->uniformGrid || !ctx->cache || !ctx->mesh) return 0.0f;
    const SurfaceMesh* mesh = ctx->mesh;
    if (!mesh->segments || mesh->segmentCount == 0) return 0.0f;
    float directLimit = ComputeDirectRadiance(light, worldX, worldY);
    float perSampleClamp = (directLimit > 0.0f)
        ? (directLimit / (float)INDIRECT_FEELER_COUNT)
        : 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < INDIRECT_FEELER_COUNT; i++) {
        double jitter = PixelFeelerJitter(pixelX, pixelY, i);
        double dirX, dirY;
        FeelerDirection(i, jitter, &dirX, &dirY);
        NormalizeVector(&dirX, &dirY);
        if (stats) stats->rayTests++;
        HitInfo2D hit;
        const SceneObject* obj = NULL;
        if (!TraceRayToSurface(ctx,
                               worldX,
                               worldY,
                               dirX,
                               dirY,
                               &hit,
                               &obj,
                               MAX_FEELER_DISTANCE)) {
            continue;
        }
        if (stats) stats->rayHits++;
        SurfaceSegment fallbackSegment = (SurfaceSegment){0};
        const SurfaceSegment* segment = SurfaceMeshFindSegment(mesh,
                                                               hit.objectIndex,
                                                               hit.px,
                                                               hit.py,
                                                               hit.nx,
                                                               hit.ny);
        if (!segment && obj && (obj->numPoints == 0 || strcmp(obj->type, "circle") == 0)) {
            double radius = obj->radius * obj->scale;
            double segLen = fmax(6.0, 0.12 * radius);
            double tx = -hit.ny;
            double ty = hit.nx;
            double tLen = hypot(tx, ty);
            if (tLen > GRID_EPSILON) {
                tx /= tLen;
                ty /= tLen;
            }
            double hx0 = hit.px - 0.5 * segLen * tx;
            double hy0 = hit.py - 0.5 * segLen * ty;
            double hx1 = hit.px + 0.5 * segLen * tx;
            double hy1 = hit.py + 0.5 * segLen * ty;
            fallbackSegment.x0 = hx0;
            fallbackSegment.y0 = hy0;
            fallbackSegment.x1 = hx1;
            fallbackSegment.y1 = hy1;
            fallbackSegment.nx = hit.nx;
            fallbackSegment.ny = hit.ny;
            fallbackSegment.length = segLen;
            fallbackSegment.objectIndex = hit.objectIndex;
            segment = &fallbackSegment;
        }
        if (!segment) {
            continue;
        }
        if (stats) stats->segmentFound++;
        if (!SegmentFacesPoint(segment, worldX, worldY)) {
            continue;
        }
        if (stats) stats->segmentFacing++;
        if ((segment->nx * dirX + segment->ny * dirY) >= 0.0) {
            continue;
        }
        if (!SegmentLitByLight(ctx, light, &hit)) {
            continue;
        }
        if (!SegmentSeesPixel(ctx, segment, worldX, worldY)) {
            continue;
        }
        float cacheRadiance = SampleCacheRadiance(ctx, &hit, worldX, worldY);
        if (cacheRadiance <= 0.0f) {
            cacheRadiance = ComputeDirectRadiance(light, hit.px, hit.py);
        }
        if (stats) stats->cacheContributions++;
        float baseRadiance = ComputeSegmentRadiance(segment,
                                                    light,
                                                    cacheRadiance,
                                                    worldX,
                                                    worldY);
        double toPixelX = worldX - hit.px;
        double toPixelY = worldY - hit.py;
        double pixelDistance = hypot(toPixelX, toPixelY);
        if (feelerDistanceLimit > 0.0 && pixelDistance > feelerDistanceLimit) {
            continue;
        }
        double distanceWeight = exp(-pixelDistance / INDIRECT_DISTANCE_DECAY) /
                                (1.0 + pixelDistance * pixelDistance * INDIRECT_DISTANCE_FALLOFF);
        double toSegX = hit.px - worldX;
        double toSegY = hit.py - worldY;
        NormalizeVector(&toSegX, &toSegY);
        double sensor = fmax(0.0, -(toSegX * dirX + toSegY * dirY));
        sensor = 0.5 + 0.5 * sensor;
        float contribution = (float)(baseRadiance * distanceWeight * sensor);
        if (cacheRadiance > 0.0f) {
            contribution = fminf(contribution, cacheRadiance);
        }
        if (perSampleClamp > 0.0f) {
            contribution = fminf(contribution, perSampleClamp);
        }
        sum += contribution;
    }
    float indirect = fminf(sum, 1.2f * directLimit);
    return indirect;
}

static void PerformIndirectLightingPass(IntegratorContext* ctx,
                                        const LightSource* light) {
    if (!ctx || !ctx->cache) {
        return;
    }
    size_t feelerTests = 0;
    size_t feelerHits = 0;
    IndirectStats stats = {0};
    double avgExtent = EstimateAverageObjectExtent(ctx);
    double feelerLimit = fmax(3.0 * avgExtent, 0.0);
    for (int y = 0; y < ctx->height; y++) {
        for (int x = 0; x < ctx->width; x++) {
            CameraPoint world = CameraScreenToWorld(&sceneSettings.camera,
                                                    x + 0.5,
                                                    y + 0.5,
                                                    ctx->width,
                                                    ctx->height);
            feelerTests++;
            float indirect = SampleIndirectLighting(ctx,
                                                    light,
                                                    world.x,
                                                    world.y,
                                                    x,
                                                    y,
                                                    feelerLimit,
                                                    &stats);
            if (indirect <= 0.0f) {
                continue;
            }
            feelerHits++;
            AccumulateEnergyAdd(ctx, x, y, indirect);
        }
    }
    printf("[CameraIntegr] Indirect pass: %zu pixels gained energy out of %zu tests.\n",
           feelerHits,
           feelerTests);
    printf("[CameraIntegr] Debug segments: rays=%zu hits=%zu segFound=%zu facing=%zu cache=%zu\n",
           stats.rayTests,
           stats.rayHits,
           stats.segmentFound,
           stats.segmentFacing,
           stats.cacheContributions);
}

static inline float TonemapEnergy(float energy) {
    float mapped = 1.0f - expf(-energy * CAMERA_TONEMAP_EXPOSURE);
    return powf(Clamp01(mapped), CAMERA_TONEMAP_GAMMA);
}

static void TonemapTiles(const IntegratorContext* ctx) {
    if (!ctx->tileGrid || !ctx->tileGrid->tiles || !ctx->pixelBuffer) return;
    for (size_t tileIndex = 0; tileIndex < ctx->tileGrid->count; tileIndex++) {
        const IntegratorTile* tile = &ctx->tileGrid->tiles[tileIndex];
        if (!tile->energy) continue;
        for (int ly = 0; ly < tile->height; ly++) {
            for (int lx = 0; lx < tile->width; lx++) {
                size_t localIndex = (size_t)ly * (size_t)tile->width + (size_t)lx;
                float tone = TonemapEnergy(tile->energy[localIndex]);
                int globalX = tile->originX + lx;
                int globalY = tile->originY + ly;
                size_t globalIndex = (size_t)globalY * (size_t)ctx->width + (size_t)globalX;
                ctx->pixelBuffer[globalIndex] = (Uint8)Clamp(tone * 255.0f, 0, 255);
            }
        }
    }
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

    if (usingTiles) {
        for (size_t tileIndex = 0; tileIndex < ctx->tileGrid->count; tileIndex++) {
            IntegratorTile* tile = &ctx->tileGrid->tiles[tileIndex];
            if (!tile->energy) continue;
            memset(tile->energy,
                   0,
                   (size_t)tile->width * (size_t)tile->height * sizeof(float));
        }
    } else if (usingBuffer) {
        memset(ctx->energyBuffer, 0, total * sizeof(float));
    }

    ts_start_timer("CameraPath Direct");
    PerformDirectLightingPass(ctx, light);
    ts_stop_timer("CameraPath Direct");

    ts_start_timer("CameraPath Indirect");
    PerformIndirectLightingPass(ctx, light);
    ts_stop_timer("CameraPath Indirect");

    if (!ctx->pixelBuffer) return;
    ts_start_timer("CameraPath Tonemap");
    if (usingTiles) {
        TonemapTiles(ctx);
    } else if (ctx->energyBuffer) {
        for (size_t i = 0; i < total; i++) {
            float tone = TonemapEnergy(ctx->energyBuffer[i]);
            ctx->pixelBuffer[i] = (Uint8)Clamp(tone * 255.0f, 0, 255);
        }
    } else {
        memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));
    }
    ts_stop_timer("CameraPath Tonemap");
}
