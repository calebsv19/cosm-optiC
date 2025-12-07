#include "render/integrators/camera_path_integrator_old_version.h"
#include "config/config_manager.h"
#include "render/ray_types.h"
#include "camera/camera.h"
#include "render/timer_hud_api.h"
#include "material/material_manager.h"
#include "render/material_bsdf.h"
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
#define INDIRECT_GI_DOWNSAMPLE 1
#define INDIRECT_DISTANCE_FALLOFF 0.0015f
#define INDIRECT_DISTANCE_DECAY 80.0
#define MAX_FEELER_DISTANCE 1500.0
#define INDIRECT_FEELER_BASE 16
#define INDIRECT_FEELER_THRESHOLD_MID 4.0f
#define INDIRECT_FEELER_THRESHOLD_DARK 1.5f
#define INDIRECT_FEELER_THRESHOLD_NIGHT 0.3f
#define MAX_INDIRECT_BRIGHTNESS 6.0f
#define CACHE_BIN_DOT_EPS 1e-4

typedef struct {
    size_t rayTests;
    size_t rayHits;
    size_t segmentFound;
    size_t segmentFacing;
    size_t cacheContributions;
    size_t cacheRejectVariance;
    size_t cacheRejectAlignment;
} IndirectStats;

static inline void NormalizeVector(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-9) {
        *x /= len;
        *y /= len;
    }
}

static inline void OrientNormalForIncoming(const HitInfo2D* hit,
                                           double inDirX,
                                           double inDirY,
                                           double* outNx,
                                           double* outNy) {
    double nx = hit ? hit->nx : 0.0;
    double ny = hit ? hit->ny : 1.0;
    NormalizeVector(&inDirX, &inDirY);
    if ((nx * inDirX + ny * inDirY) < 0.0) {
        nx = -nx;
        ny = -ny;
    }
    if (outNx) *outNx = nx;
    if (outNy) *outNy = ny;
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
                            double baseNx,
                            double baseNy,
                            double* outX,
                            double* outY) {
    if (total <= 0) total = 1;
    double baseAngle = atan2(baseNy, baseNx);
    double hemisphereAngle = (M_PI * (double)index) / (double)total;
    double jitterAngle = 0.5 * (M_PI / (double)total) * jitter;
    double angle = baseAngle + hemisphereAngle + jitterAngle;
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

static inline double CacheVarianceThreshold(void) {
    double v = animSettings.cacheVarianceCutoff;
    if (v <= 0.0) v = 0.35;
    return Clamp(v, 0.05, 10.0);
}

static inline double CacheHaloRadius(const LightSource* light) {
    double mul = animSettings.cacheHaloRadius;
    if (mul <= 0.0) mul = 3.5;
    double base = light ? light->radius * mul : 0.0;
    if (base < 12.0) base = 12.0;
    return base;
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
    double dist = hypot(dx, dy);

    double falloffScale = animSettings.forwardDecay;
    if (falloffScale <= 0.0) {
        double w = (double)sceneSettings.windowWidth;
        double h = (double)sceneSettings.windowHeight;
        if (w <= 0.0) w = 1200.0;
        if (h <= 0.0) h = 800.0;
        falloffScale = hypot(w, h);
    }
    double softness = fmax(animSettings.lightDecaySoftness, 0.1);
    double scale = fmax(falloffScale * softness, 1.0);
    double normalized = dist / scale;

    double att = 1.0;
    switch (animSettings.forwardFalloffMode) {
        case FORWARD_FALLOFF_MODE_LINEAR:
            att = 1.0 / (1.0 + normalized);
            break;
        case FORWARD_FALLOFF_MODE_NONE:
            att = 1.0;
            break;
        case FORWARD_FALLOFF_MODE_QUADRATIC:
        default:
            att = 1.0 / (1.0 + normalized * normalized);
            break;
    }

    double radiance = animSettings.lightIntensity * att;
    return (float)radiance;
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

static float SampleCacheDirectional(const IntegratorContext* ctx,
                                    const HitInfo2D* hit,
                                    double dirX,
                                    double dirY,
                                    const MaterialBSDF* material,
                                    IndirectStats* stats) {
    if (!ctx || !hit) return 0.0f;
    const SurfaceIrradiance* entry = FindClosestCacheEntry(ctx,
                                                          hit->objectIndex,
                                                          hit->px,
                                                          hit->py);
    if (!entry) return 0.0f;
    NormalizeVector(&dirX, &dirY);
    if (dirX * entry->nx + dirY * entry->ny <= 0.0) {
        return 0.0f;
    }
    const MaterialBSDF* mat = material;
    if (!mat && ctx->materials &&
        hit->objectIndex >= 0 && hit->objectIndex < ctx->materialCount) {
        mat = &ctx->materials[hit->objectIndex];
    }

    double bestDot = -1.0;
    double bestMean = 0.0;
    double varianceCutoff = CacheVarianceThreshold();
    for (int i = 0; i < IRRADIANCE_BIN_COUNT; i++) {
        const IrradianceBin* bin = &entry->bins[i];
        if (!bin->valid || bin->samples == 0) continue;
        double dot = dirX * bin->dirX + dirY * bin->dirY;
        if (dot < CACHE_BIN_DOT_EPS) {
            if (stats) stats->cacheRejectAlignment++;
            continue;
        }
        double variance = bin->variance / (double)fmax(1, bin->samples);
        if (variance > varianceCutoff) {
            if (stats) stats->cacheRejectVariance++;
            continue;
        }
        if (dot > bestDot) {
            bestDot = dot;
            bestMean = bin->mean;
        }
    }
    if (bestMean <= 0.0) {
        return 0.0f;
    }
    double diffuseScale = 1.0;
    if (mat) {
        diffuseScale = (mat->diffuseWeight > 0.0) ? mat->diffuseWeight : mat->albedo;
    }
    return (float)(bestMean * diffuseScale);
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
    (void)directLimit;
    if (!ctx || !ctx->uniformGrid || !ctx->cache || !ctx->mesh) return 0.0f;
    const SurfaceMesh* mesh = ctx->mesh;
    if (!mesh->segments || mesh->segmentCount == 0) return 0.0f;

    if (light) {
        double lx = light->x - worldX;
        double ly = light->y - worldY;
        double dist = hypot(lx, ly);
        double haloRadius = CacheHaloRadius(light);
        if (dist < haloRadius) {
            return 0.0f;
        }
    }

    float sum = 0.0f;

    for (int i = 0; i < feelerCount; i++) {
        double jitter = PixelFeelerJitter(pixelX, pixelY, i);
        double dirX, dirY;
        double baseNx = worldX - sceneSettings.camera.x;
        double baseNy = worldY - sceneSettings.camera.y;
        if (fabs(baseNx) < 1e-6 && fabs(baseNy) < 1e-6) {
            baseNx = 1.0; baseNy = 0.0;
        }
        NormalizeVector(&baseNx, &baseNy);
        FeelerDirection(i, feelerCount, jitter, baseNx, baseNy, &dirX, &dirY);
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

        double vx = worldX - hit.px;
        double vy = worldY - hit.py;
        double toPixelX = vx;
        double toPixelY = vy;
        double pixelDistance = hypot(toPixelX, toPixelY);
        double inDirX = -dirX;
        double inDirY = -dirY;
        NormalizeVector(&inDirX, &inDirY);
        double shadedNx, shadedNy;
        OrientNormalForIncoming(&hit, inDirX, inDirY, &shadedNx, &shadedNy);

        const MaterialBSDF* surfaceMat = NULL;
        MaterialBSDF tempMat = {0};
        if (ctx->materials &&
            hit.objectIndex >= 0 && hit.objectIndex < ctx->materialCount) {
            surfaceMat = &ctx->materials[hit.objectIndex];
        } else if (obj) {
            MaterialBSDFInitFromSceneObject(obj, &tempMat);
            surfaceMat = &tempMat;
        }

        float diffuseRadiance = SampleCacheDirectional(ctx,
                                                       &hit,
                                                       vx,
                                                       vy,
                                                       surfaceMat,
                                                       stats);
        if (diffuseRadiance <= 0.0f) {
            if (!SegmentLitByLight(ctx, light, &hit)) {
                continue;
            }
            double lx = light ? (light->x - hit.px) : 0.0;
            double ly = light ? (light->y - hit.py) : 0.0;
            double lLen = hypot(lx, ly);
            if (lLen > GRID_EPSILON) {
                lx /= lLen;
                ly /= lLen;
            }
            double viewDirX = (pixelDistance > GRID_EPSILON) ? (vx / pixelDistance) : 0.0;
            double viewDirY = (pixelDistance > GRID_EPSILON) ? (vy / pixelDistance) : 0.0;
            double bsdfTerm = surfaceMat
                ? MaterialBSDFEvaluateCos(surfaceMat, shadedNx, shadedNy, lx, ly, viewDirX, viewDirY)
                : fmax(0.0, viewDirX * shadedNx + viewDirY * shadedNy);
            if (bsdfTerm <= 0.0) {
                continue;
            }
            diffuseRadiance = (float)(ComputeDirectRadiance(light, hit.px, hit.py) * bsdfTerm);
        }
        if (stats) stats->cacheContributions++;

        if (feelerDistanceLimit > 0.0 && pixelDistance > feelerDistanceLimit) {
            continue;
        }
        double viewFacing = (pixelDistance > GRID_EPSILON)
            ? fmax(0.0, (toPixelX * shadedNx + toPixelY * shadedNy) / pixelDistance)
            : 0.0;
        if (viewFacing <= 0.01) {
            continue;
        }
        double distanceWeight = 1.0 / (1.0 + pixelDistance * 0.01);

        double viewDirX = (pixelDistance > GRID_EPSILON) ? (toPixelX / pixelDistance) : 0.0;
        double viewDirY = (pixelDistance > GRID_EPSILON) ? (toPixelY / pixelDistance) : 0.0;
        double cacheBsdf = surfaceMat
            ? MaterialBSDFEvaluateCos(surfaceMat, shadedNx, shadedNy, shadedNx, shadedNy, viewDirX, viewDirY)
            : viewFacing;
        float weighted = (float)(diffuseRadiance * distanceWeight * cacheBsdf);
        if (weighted <= 0.0f) {
            continue;
        }

        if (surfaceMat) {
            const MaterialBSDF* bsdf = surfaceMat;
            if (bsdf->reflectivity > 0.05) {
                double inX = dirX;
                double inY = dirY;
                double dotIn = inX * hit.nx + inY * hit.ny;
                if (dotIn >= 0.0) {
                    goto add_contrib;
                }
                NormalizeVector(&inX, &inY);
                double rx = inX - 2.0 * dotIn * hit.nx;
                double ry = inY - 2.0 * dotIn * hit.ny;
                NormalizeVector(&rx, &ry);
                if (rx * hit.nx + ry * hit.ny <= 0.0) {
                    goto add_contrib;
                }
                float reflAccum = 0.0f;
                int reflSamples = 4;
                double maxJitter = fmax(0.05, bsdf->roughness * 0.8);
                double tx = -ry;
                double ty = rx;
                NormalizeVector(&tx, &ty);
                for (int rs = 0; rs < reflSamples; rs++) {
                    double h = HashDouble(pixelX, pixelY, i * 31 + rs * 17);
                    double angle = maxJitter * (h * 2.0 - 1.0);
                    double ca = cos(angle);
                    double sa = sin(angle);
                    double rjx = rx * ca + tx * sa;
                    double rjy = ry * ca + ty * sa;
                    NormalizeVector(&rjx, &rjy);
                    if (rjx * hit.nx + rjy * hit.ny <= 0.0) {
                        continue;
                    }
                    float reflSample = SampleCacheDirectional(ctx, &hit, rjx, rjy, surfaceMat, stats);
                    double reflBsdf = surfaceMat
                        ? MaterialBSDFEvaluateCos(surfaceMat, shadedNx, shadedNy, rjx, rjy, viewDirX, viewDirY)
                        : 1.0;
                    if (reflBsdf <= 0.0) {
                        continue;
                    }
                    if (reflSample > MAX_INDIRECT_BRIGHTNESS) {
                        reflSample = MAX_INDIRECT_BRIGHTNESS;
                    }
                    reflAccum += (float)(reflSample * reflBsdf);
                }
                if (reflSamples > 0) {
                    reflAccum /= (float)reflSamples;
                }
                weighted += reflAccum * (float)bsdf->reflectivity;

                if (bsdf->roughness < 0.05 && bsdf->reflectivity > 0.5) {
                    double sharpX = rx;
                    double sharpY = ry;
                    NormalizeVector(&sharpX, &sharpY);
                    if (sharpX * hit.nx + sharpY * hit.ny > 0.0) {
                        float sharpSample = SampleCacheDirectional(ctx, &hit, sharpX, sharpY, surfaceMat, stats);
                        double sharpBsdf = surfaceMat
                            ? MaterialBSDFEvaluateCos(surfaceMat, shadedNx, shadedNy, sharpX, sharpY, viewDirX,
                                                      viewDirY)
                            : 1.0;
                        if (sharpBsdf <= 0.0) {
                            continue;
                        }
                        if (sharpSample > MAX_INDIRECT_BRIGHTNESS) {
                            sharpSample = MAX_INDIRECT_BRIGHTNESS;
                        }
                        weighted += (float)(sharpSample * sharpBsdf * bsdf->reflectivity);
                    }
                }
            }
        }
add_contrib:
        if (weighted > MAX_INDIRECT_BRIGHTNESS) {
            weighted = MAX_INDIRECT_BRIGHTNESS;
        }
        sum += weighted;
    }

    if (sum > MAX_INDIRECT_BRIGHTNESS) {
        sum = MAX_INDIRECT_BRIGHTNESS;
    }
    return sum;
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

static void BilateralBlurEnergyBuffer(IntegratorContext* ctx, float spatialSigma, float rangeSigma) {
    if (!ctx || !ctx->energyBuffer) return;
    int w = ctx->width;
    int h = ctx->height;
    size_t total = (size_t)w * (size_t)h;
    float* tmp = (float*)malloc(total * sizeof(float));
    if (!tmp) return;
    int radius = (int)fmax(1.0, spatialSigma * 2.0);
    float twoSpatialVar = 2.0f * spatialSigma * spatialSigma;
    float twoRangeVar = 2.0f * rangeSigma * rangeSigma;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float center = ctx->energyBuffer[(size_t)y * (size_t)w + (size_t)x];
            float accum = 0.0f;
            float weightSum = 0.0f;
            for (int ky = -radius; ky <= radius; ky++) {
                int sy = y + ky;
                if (sy < 0) sy = 0;
                if (sy >= h) sy = h - 1;
                for (int kx = -radius; kx <= radius; kx++) {
                    int sx = x + kx;
                    if (sx < 0) sx = 0;
                    if (sx >= w) sx = w - 1;
                    float neighbor = ctx->energyBuffer[(size_t)sy * (size_t)w + (size_t)sx];
                    float ds = (float)(kx * kx + ky * ky);
                    float dr = neighbor - center;
                    float spatialW = expf(-ds / twoSpatialVar);
                    float rangeW = expf(-(dr * dr) / twoRangeVar);
                    float wgt = spatialW * rangeW;
                    accum += neighbor * wgt;
                    weightSum += wgt;
                }
            }
            if (weightSum > 1e-6f) {
                accum /= weightSum;
            }
            tmp[(size_t)y * (size_t)w + (size_t)x] = accum;
        }
    }
    memcpy(ctx->energyBuffer, tmp, total * sizeof(float));
    free(tmp);
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
    printf("[CameraIntegr] Debug segments: rays=%zu hits=%zu segFound=%zu facing=%zu cache=%zu varReject=%zu alignReject=%zu\n",
           stats.rayTests,
           stats.rayHits,
           stats.segmentFound,
           stats.segmentFacing,
           stats.cacheContributions,
           stats.cacheRejectVariance,
           stats.cacheRejectAlignment);
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

    if (!usingTiles && ctx->energyBuffer) {
        BilateralBlurEnergyBuffer(ctx, 1.2f, 0.6f);
    }

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
