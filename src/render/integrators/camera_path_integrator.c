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
#define INDIRECT_GI_DOWNSAMPLE 4
#define INDIRECT_DIRECT_SKIP_THRESHOLD 5.0f
#define INDIRECT_DISTANCE_FALLOFF 0.0015f
#define INDIRECT_DISTANCE_DECAY 80.0
#define MAX_FEELER_DISTANCE 1500.0
#define INDIRECT_FEELER_BASE 16
#define INDIRECT_FEELER_THRESHOLD_MID 4.0f
#define INDIRECT_FEELER_THRESHOLD_DARK 1.5f
#define INDIRECT_FEELER_THRESHOLD_NIGHT 0.3f

typedef struct {
    size_t rayTests;
    size_t rayHits;
    size_t segmentFound;
    size_t segmentFacing;
    size_t cacheContributions;
} IndirectStats;

static inline void NormalizeVector(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-9) {
        *x /= len;
        *y /= len;
    }
}

static inline double HashDouble(int x, int y, int s) {
    uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)(s * 83492791);
    h ^= h >> 13;
    h *= 0x5bd1e995;
    h ^= h >> 15;
    return (double)(h & 0xFFFFFF) / (double)0x1000000;
}

static double PixelFeelerJitter(int pixelX, int pixelY, int feelerIndex) {
    double base = HashDouble(pixelX, pixelY, feelerIndex);
    return base * 2.0 - 1.0;
}

static void FeelerDirection(int index,
                            int total,
                            double jitter,
                            double* outX,
                            double* outY) {
    if (total <= 0) total = 1;
    double baseAngle = (2.0 * M_PI * (double)index) / (double)total;
    double jitterAngle = 0.5 * (M_PI / (double)total) * jitter;
    double angle = baseAngle + jitterAngle;
    *outX = cos(angle);
    *outY = sin(angle);
}

static double RenderQualityScale(void) {
    switch (animSettings.renderQuality) {
        case RENDER_QUALITY_LOW: return 0.5;
        case RENDER_QUALITY_HIGH: return 2.0;
        case RENDER_QUALITY_MEDIUM:
        default: return 1.0;
    }
}

static int DetermineFeelerCount(float directLimit) {
    int base = INDIRECT_FEELER_BASE;
    if (directLimit < INDIRECT_FEELER_THRESHOLD_MID) base = 24;
    if (directLimit < INDIRECT_FEELER_THRESHOLD_DARK) base = 32;
    if (directLimit < INDIRECT_FEELER_THRESHOLD_NIGHT) base = 48;
    double scaled = base * RenderQualityScale();
    int count = (int)lround(scaled);
    if (count < 8) count = 8;
    return count;
}

static bool IsOccluded(const IntegratorContext* ctx,
                       double originX,
                       double originY,
                       double dirX,
                       double dirY,
                       double maxDistance);

static bool FetchTileSample(const IntegratorContext* ctx,
                            int pixelX,
                            int pixelY,
                            IntegratorTile** outTile,
                            int* outLocalX,
                            int* outLocalY) {
    if (!ctx || !ctx->tileGrid || !ctx->tileGrid->tiles) {
        return false;
    }
    if (pixelX < 0 || pixelY < 0 || pixelX >= ctx->width || pixelY >= ctx->height) {
        return false;
    }
    TileGrid* grid = ctx->tileGrid;
    int tileSize = grid->tileSize > 0 ? grid->tileSize : 1;
    int tileX = pixelX / tileSize;
    int tileY = pixelY / tileSize;
    if (tileX < 0 || tileY < 0 || tileX >= grid->tilesX || tileY >= grid->tilesY) {
        return false;
    }
    size_t index = (size_t)tileY * (size_t)grid->tilesX + (size_t)tileX;
    if (index >= grid->count) {
        return false;
    }
    IntegratorTile* tile = &grid->tiles[index];
    if (!tile->energy) return false;
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
        if (!tile->energy) return 0.0f;
        size_t index = (size_t)localY * (size_t)tile->width + (size_t)localX;
        if (index >= (size_t)(tile->width * tile->height)) {
            return 0.0f;
        }
        return tile->energy[index];
    } else if (ctx->energyBuffer) {
        if (pixelX < 0 || pixelY < 0 || pixelX >= ctx->width || pixelY >= ctx->height) {
            return 0.0f;
        }
        size_t index = (size_t)pixelY * (size_t)ctx->width + (size_t)pixelX;
        return ctx->energyBuffer[index];
    }
    return 0.0f;
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
        if (!tile->energy) return;
        size_t index = (size_t)localY * (size_t)tile->width + (size_t)localX;
        if (index >= (size_t)(tile->width * tile->height)) {
            return;
        }
        tile->energy[index] = value;
    } else if (ctx->energyBuffer) {
        if (pixelX < 0 || pixelY < 0 || pixelX >= ctx->width || pixelY >= ctx->height) {
            return;
        }
        size_t index = (size_t)pixelY * (size_t)ctx->width + (size_t)pixelX;
        ctx->energyBuffer[index] = value;
    }
}

static void ClearEnergyBuffer(IntegratorContext* ctx) {
    if (!ctx) return;
    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        for (size_t i = 0; i < ctx->tileGrid->count; i++) {
            IntegratorTile* tile = &ctx->tileGrid->tiles[i];
            if (!tile->energy) continue;
            memset(tile->energy, 0, (size_t)(tile->width * tile->height) * sizeof(float));
        }
    } else if (ctx->energyBuffer) {
        size_t total = (size_t)ctx->width * (size_t)ctx->height;
        memset(ctx->energyBuffer, 0, total * sizeof(float));
    }
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

static bool SegmentFacesPoint(const SurfaceSegment* segment,
                              double px,
                              double py) {
    if (!segment) return false;
    double cx = 0.5 * (segment->x0 + segment->x1);
    double cy = 0.5 * (segment->y0 + segment->y1);
    double vx = px - cx;
    double vy = py - cy;
    double dot = vx * segment->nx + vy * segment->ny;
    return dot > 0.0;
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
    if (!ctx || !ctx->uniformGrid || !light) return false;
    double dx = light->x - worldX;
    double dy = light->y - worldY;
    double dist = hypot(dx, dy);
    if (dist <= PATH_EPSILON) {
        return true;
    }
    NormalizeVector(&dx, &dy);
    double originX = worldX + dx * PATH_EPSILON;
    double originY = worldY + dy * PATH_EPSILON;
    double maxDist = fmax(dist - 2.0 * PATH_EPSILON, PATH_EPSILON);
    return !IsOccluded(ctx, originX, originY, dx, dy, maxDist);
}

static bool TraceRayToSurface(const IntegratorContext* ctx,
                              double worldX,
                              double worldY,
                              double dirX,
                              double dirY,
                              HitInfo2D* outHit,
                              const SceneObject** outObj,
                              double maxDistance) {
    if (!ctx || !ctx->uniformGrid) return false;
    Ray2D ray = {
        .ox = worldX + dirX * PATH_EPSILON,
        .oy = worldY + dirY * PATH_EPSILON,
        .dx = dirX,
        .dy = dirY
    };
    HitInfo2D tmp = {0};
    if (!UniformGridTraceRay(ctx->uniformGrid, &ray, PATH_EPSILON, maxDistance, &tmp)) {
        return false;
    }
    if (outHit) *outHit = tmp;
    if (outObj && ctx->objects &&
        tmp.objectIndex >= 0 && tmp.objectIndex < ctx->objectCount) {
        *outObj = &ctx->objects[tmp.objectIndex];
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
    Ray2D ray = {
        .ox = originX,
        .oy = originY,
        .dx = dirX,
        .dy = dirY
    };
    HitInfo2D tmp = {0};
    return UniformGridTraceRay(ctx->uniformGrid, &ray, PATH_EPSILON, maxDistance, &tmp);
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
    (void)light;
    if (!segment || radiance <= 0.0f) return 0.0f;
    double cx = 0.5 * (segment->x0 + segment->x1);
    double cy = 0.5 * (segment->y0 + segment->y1);
    double dirX = worldX - cx;
    double dirY = worldY - cy;
    double dist2 = dirX * dirX + dirY * dirY;
    double distance = sqrt(dist2);
    if (distance <= PATH_EPSILON) {
        return (float)radiance;
    }
    NormalizeVector(&dirX, &dirY);
    double dot = dirX * segment->nx + dirY * segment->ny;
    if (dot <= 0.0) {
        return 0.0f;
    }
    double lengthFactor = fmax(segment->length, 1.0);
    double attenuation = dot * lengthFactor / (1.0 + dist2 * INDIRECT_DISTANCE_FALLOFF);
    double segmentRadiance = radiance * attenuation;
    return (float)segmentRadiance;
}

static double EstimateAverageObjectExtent(const IntegratorContext* ctx) {
    if (!ctx || !ctx->objects || ctx->objectCount <= 0) {
        return 0.0;
    }
    double sum = 0.0;
    for (int i = 0; i < ctx->objectCount; i++) {
        const SceneObject* obj = &ctx->objects[i];
        double extent = 0.0;
        if (obj->radius > 0.0 && strcmp(obj->type, "circle") == 0) {
            extent = obj->radius * obj->scale;
        } else {
            double minX, minY, maxX, maxY;
            ComputeObjectBounds(obj, &minX, &minY, &maxX, &maxY);
            extent = hypot(maxX - minX, maxY - minY);
        }
        sum += fmax(extent, 1.0);
    }
    return sum / (double)ctx->objectCount;
}

static float SampleCacheRadiance(const IntegratorContext* ctx,
                                 const HitInfo2D* hit,
                                 double worldX,
                                 double worldY) {
    if (!ctx || !hit) return 0.0f;
    const SurfaceIrradiance* entry = FindClosestCacheEntry(ctx,
                                                          hit->objectIndex,
                                                          hit->px,
                                                          hit->py);
    if (!entry) return 0.0f;
    double vx = worldX - entry->px;
    double vy = worldY - entry->py;
    double dirX = vx;
    double dirY = vy;
    NormalizeVector(&dirX, &dirY);
    double bestDot = -1.0;
    double bestMean = 0.0;
    double bestDistance = 0.0;
    for (int i = 0; i < IRRADIANCE_BIN_COUNT; i++) {
        const IrradianceBin* bin = &entry->bins[i];
        if (!bin->valid || bin->samples == 0) continue;
        double dot = dirX * bin->dirX + dirY * bin->dirY;
        if (dot > bestDot) {
            bestDot = dot;
            bestMean = bin->mean;
            bestDistance = bin->distance;
        }
    }
    if (bestMean <= 0.0) {
        return 0.0f;
    }
    double distance = sqrt(vx * vx + vy * vy);
    double falloff = exp(-distance / INDIRECT_DISTANCE_DECAY);
    double occlusion = (bestDistance > PATH_EPSILON)
        ? exp(-distance / bestDistance)
        : 1.0;
    double weighted = bestMean * falloff * occlusion;
    return (float)weighted;
}

static void AccumulateEnergyAdd(const IntegratorContext* ctx,
                                int pixelX,
                                int pixelY,
                                float value) {
    if (!ctx || value <= 0.0f) return;
    float current = ReadEnergySample(ctx, pixelX, pixelY);
    WriteEnergySample(ctx, pixelX, pixelY, current + value);
}

static float SampleIndirectLighting(const IntegratorContext* ctx,
                                    const LightSource* light,
                                    double worldX,
                                    double worldY,
                                    int pixelX,
                                    int pixelY,
                                    double feelerDistanceLimit,
                                    int feelerCount,
                                    float directLimit,
                                    IndirectStats* stats) {
    if (!ctx || !ctx->uniformGrid || !ctx->cache || !ctx->mesh) return 0.0f;
    const SurfaceMesh* mesh = ctx->mesh;
    if (!mesh->segments || mesh->segmentCount == 0) return 0.0f;

    if (directLimit > INDIRECT_DIRECT_SKIP_THRESHOLD) {
        return 0.0f;
    }

    float perSampleClamp = (directLimit > 0.0f)
        ? (directLimit / (float)feelerCount)
        : 0.0f;
    float sum = 0.0f;

    for (int i = 0; i < feelerCount; i++) {
        double jitter = PixelFeelerJitter(pixelX, pixelY, i);
        double dirX, dirY;
        FeelerDirection(i, feelerCount, jitter, &dirX, &dirY);
        NormalizeVector(&dirX, &dirY);
        if (stats) stats->rayTests++;

        HitInfo2D hit = {0};
        hit.objectIndex = -1;
        hit.triangleIndex = -1;
        hit.baryW = 1.0;
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

        float cacheRadiance = SampleCacheRadiance(ctx, &hit, worldX, worldY);
        if (cacheRadiance <= 0.0f) {
            if (!SegmentLitByLight(ctx, light, &hit)) {
                continue;
            }
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
                size_t idx = (size_t)ly * (size_t)tile->width + (size_t)lx;
                float tone = TonemapEnergy(tile->energy[idx]);
                int pxX = tile->originX + lx;
                int pxY = tile->originY + ly;
                if (pxX < 0 || pxY < 0 || pxX >= ctx->width || pxY >= ctx->height) {
                    continue;
                }
                size_t outIndex = (size_t)pxY * (size_t)ctx->width + (size_t)pxX;
                ctx->pixelBuffer[outIndex] = (Uint8)Clamp(tone * 255.0f, 0, 255);
            }
        }
    }
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

    int giScale = INDIRECT_GI_DOWNSAMPLE;
    if (giScale < 1) giScale = 1;

    for (int y = 0; y < ctx->height; y += giScale) {
        for (int x = 0; x < ctx->width; x += giScale) {
            CameraPoint world = CameraScreenToWorld(&sceneSettings.camera,
                                                    x + 0.5,
                                                    y + 0.5,
                                                    ctx->width,
                                                    ctx->height);
            float directLimit = ComputeDirectRadiance(light, world.x, world.y);
            int feelerCount = DetermineFeelerCount(directLimit);
            feelerTests += (size_t)feelerCount;
            float indirect = SampleIndirectLighting(ctx,
                                                    light,
                                                    world.x,
                                                    world.y,
                                                    x,
                                                    y,
                                                    feelerLimit,
                                                    feelerCount,
                                                    directLimit,
                                                    &stats);
            if (indirect <= 0.0f) {
                continue;
            }
            feelerHits++;

            int yEnd = y + giScale;
            if (yEnd > ctx->height) yEnd = ctx->height;
            int xEnd = x + giScale;
            if (xEnd > ctx->width) xEnd = ctx->width;

            for (int yy = y; yy < yEnd; yy++) {
                for (int xx = x; xx < xEnd; xx++) {
                    AccumulateEnergyAdd(ctx, xx, yy, indirect);
                }
            }
        }
    }
    printf("[CameraIntegr] Indirect pass: %zu GI samples produced energy out of %zu tests.\n",
           feelerHits,
           feelerTests);
    printf("[CameraIntegr] Debug segments: rays=%zu hits=%zu segFound=%zu facing=%zu cache=%zu\n",
           stats.rayTests,
           stats.rayHits,
           stats.segmentFound,
           stats.segmentFacing,
           stats.cacheContributions);
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

    ts_start_timer("CameraPath Clear");
    ClearEnergyBuffer(ctx);
    ts_stop_timer("CameraPath Clear");

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
