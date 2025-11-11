#include "render/camera_path_integrator.h"
#include "config/config_manager.h"
#include "render/fast_rng.h"
#include "render/ray_types.h"
#include "camera/camera.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define PATH_LIGHT_INTENSITY 2.0
#define PATH_FALLOFF_SCALE 0.8
#define CAMERA_ENERGY_BOOST 6.0f

static double RandomUnit(FastRNG* rng) {
    return FastRNGNextDouble(rng);
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

static void SampleLambertDirection(double nx, double ny, FastRNG* rng, double* dirX, double* dirY, double* pdf) {
    double u1 = RandomUnit(rng);
    double u2 = RandomUnit(rng);
    double cosTheta = sqrt(u1);
    double sinTheta = sqrt(fmax(0.0, 1.0 - u1));
    double sign = (u2 > 0.5) ? 1.0 : -1.0;
    double tx = -ny;
    double ty = nx;
    NormalizeVector(&tx, &ty);
    *dirX = cosTheta * nx + sign * sinTheta * tx;
    *dirY = cosTheta * ny + sign * sinTheta * ty;
    NormalizeVector(dirX, dirY);
    double cosTerm = fmax(*dirX * nx + *dirY * ny, 0.0);
    *pdf = cosTerm * INV_PI;
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

static double ComputeDirectLighting(const IntegratorContext* ctx,
                                    const LightSource* light,
                                    const HitInfo2D* hit,
                                    double albedo) {
    if (!light || !hit) return 0.0;
    double lx = light->x - hit->px;
    double ly = light->y - hit->py;
    double dist2 = lx * lx + ly * ly;
    if (dist2 < GRID_EPSILON) return PATH_LIGHT_INTENSITY;
    double dist = sqrt(dist2);
    double dirX = lx / dist;
    double dirY = ly / dist;
    double cosTerm = fmax(hit->nx * dirX + hit->ny * dirY, 0.0);
    if (cosTerm <= 0.0) return animSettings.environmentBrightness;

    double originX = hit->px + hit->nx * PATH_EPSILON;
    double originY = hit->py + hit->ny * PATH_EPSILON;
    if (IsOccluded(ctx, originX, originY, dirX, dirY, dist - PATH_EPSILON)) {
        return 0.0;
    }

    double falloff = PATH_LIGHT_INTENSITY / (dist * PATH_FALLOFF_SCALE + 1.0);
    double brdf = albedo * INV_PI;
    return falloff * brdf * cosTerm;
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
        radiance += ComputeDirectLighting(ctx, light, hit, albedo);
    }

    if (depthRemaining <= 1 || !ctx->uniformGrid) {
        return radiance;
    }

    double dirX, dirY, pdf;
    SampleLambertDirection(hit->nx, hit->ny, rng, &dirX, &dirY, &pdf);
    if (pdf <= GRID_EPSILON) {
        return radiance;
    }
    double cosTerm = fmax(dirX * hit->nx + dirY * hit->ny, 0.0);
    if (cosTerm <= GRID_EPSILON) {
        return radiance;
    }

    double originX = hit->px + hit->nx * PATH_EPSILON;
    double originY = hit->py + hit->ny * PATH_EPSILON;

    HitInfo2D bounceHit;
    const SceneObject* bounceObj = NULL;
    if (!TraceRayToSurface(ctx, originX, originY, dirX, dirY, &bounceHit, &bounceObj, 0.0)) {
        radiance += albedo * cosTerm * animSettings.environmentBrightness;
        return radiance;
    }

    double rrProb = 1.0;
    if (animSettings.pathRussianRoulette && depthRemaining <= animSettings.pathMaxDepth - 1) {
        rrProb = fmax(0.2, fmin(0.9, albedo));
        if (RandomUnit(rng) > rrProb) {
            return radiance;
        }
    }

    double bounceRadiance = TraceSurfaceRadiance(ctx, light, &bounceHit, bounceObj, rng, depthRemaining - 1);
    double throughput = albedo * cosTerm;
    if (animSettings.pathRussianRoulette && depthRemaining <= animSettings.pathMaxDepth - 1) {
        throughput /= rrProb;
    }
    radiance += throughput * bounceRadiance;
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
    FastRNG rng;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double sum = 0.0;
            FastRNGSeed(&rng,
                        ctx->frameSeed + ((uint64_t)y * 1315423911ULL) + ((uint64_t)x * 2654435761ULL),
                        animSettings.pathSeed + x + y * width);
            for (int s = 0; s < spp; s++) {
                double jitterX = RandomUnit(&rng);
                double jitterY = RandomUnit(&rng);
                CameraPoint sample = CameraScreenToWorld(&sceneSettings.camera,
                                                         x + jitterX,
                                                         y + jitterY,
                                                         ctx->width,
                                                         ctx->height);
                sum += ShadeSample(ctx, light, sample.x, sample.y, &rng, maxDepth);
            }
            WriteEnergySample(ctx, x, y, (float)(sum / spp));
        }
    }

    if (!ctx->pixelBuffer) return;
    if (usingTiles) {
        float maxEnergy = FindMaxTileEnergy(ctx->tileGrid);
        if (maxEnergy <= 0.0f) {
            memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));
            return;
        }
        TonemapTiles(ctx, 1.0f / maxEnergy);
        return;
    }

    if (!ctx->energyBuffer) {
        memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));
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
        return;
    }
    float invMax = 1.0f / maxEnergy;
    for (size_t i = 0; i < total; i++) {
        float normalized = ctx->energyBuffer[i] * invMax * CAMERA_ENERGY_BOOST;
        float tone = powf(Clamp01(normalized), 0.55f);
        ctx->pixelBuffer[i] = (Uint8)Clamp(tone * 255.0f, 0, 255);
    }
}
