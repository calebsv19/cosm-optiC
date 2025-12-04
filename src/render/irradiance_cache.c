#include "render/irradiance_cache.h"

#include "camera/camera.h"
#include "config/config_manager.h"
#include "render/ray_types.h"
#include "render/uniform_grid.h"
#include "render/material_bsdf.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#define CACHE_SAMPLES_PER_BIN 4
#define CACHE_STEP_DISTANCE 8.0
#define CACHE_MAX_DISTANCE 400.0
#define CACHE_DISTANCE_DECAY 150.0

static void Normalize(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-9) {
        *x /= len;
        *y /= len;
    }
}

static double HashDouble(int x, int y, int s) {
    uint32_t h = (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)(s * 83492791);
    h ^= h >> 13;
    h *= 0x5bd1e995;
    h ^= h >> 15;
    return (double)(h & 0xFFFFFF) / (double)0x1000000;
}

static bool SampleObjectPoint(const SceneObject* obj,
                              double t,
                              double* outX,
                              double* outY,
                              double* outNx,
                              double* outNy) {
    if (!obj || !outX || !outY) return false;
    if (strcmp(obj->type, "circle") == 0) {
        double radius = obj->radius * obj->scale;
        double angle = t * 2.0 * M_PI;
        double dirX = cos(angle);
        double dirY = sin(angle);
        *outX = obj->x + radius * dirX;
        *outY = obj->y + radius * dirY;
        if (outNx && outNy) {
            *outNx = dirX;
            *outNy = dirY;
        }
        return true;
    }

    if (obj->numPoints < 2) {
        return false;
    }

    double perimeter = 0.0;
    for (int i = 0; i < obj->numPoints; i++) {
        int next = (i + 1) % obj->numPoints;
        double x1 = obj->shapePoints[i][0] + obj->x;
        double y1 = obj->shapePoints[i][1] + obj->y;
        double x2 = obj->shapePoints[next][0] + obj->x;
        double y2 = obj->shapePoints[next][1] + obj->y;
        double dx = x2 - x1;
        double dy = y2 - y1;
        perimeter += sqrt(dx * dx + dy * dy);
    }
    if (perimeter < 1e-6) return false;

    double target = t * perimeter;
    double accum = 0.0;
    for (int i = 0; i < obj->numPoints; i++) {
        int next = (i + 1) % obj->numPoints;
        double x1 = obj->shapePoints[i][0] + obj->x;
        double y1 = obj->shapePoints[i][1] + obj->y;
        double x2 = obj->shapePoints[next][0] + obj->x;
        double y2 = obj->shapePoints[next][1] + obj->y;
        double dx = x2 - x1;
        double dy = y2 - y1;
        double segLen = sqrt(dx * dx + dy * dy);
        if (segLen <= 1e-6) continue;
        if (accum + segLen >= target) {
            double localT = (target - accum) / segLen;
            *outX = x1 + dx * localT;
            *outY = y1 + dy * localT;
            if (outNx && outNy) {
                *outNx = dy / segLen;
                *outNy = -dx / segLen;
                double centerDx = *outX - obj->x;
                double centerDy = *outY - obj->y;
                if ((*outNx * centerDx + *outNy * centerDy) < 0.0) {
                    *outNx = -*outNx;
                    *outNy = -*outNy;
                }
            }
            return true;
        }
        accum += segLen;
    }
    return false;
}

// Raytrace-based directional sample for cache fill
static double TraceDirectionalEnergy(const UniformGrid* grid,
                                     const SceneObject* objects,
                                     int objectCount,
                                     double originX,
                                     double originY,
                                     double dirX,
                                     double dirY,
                                     const LightSource* light,
                                     bool includeIndirect) {
    if (!grid || !objects || objectCount <= 0) return 0.0;
    Normalize(&dirX, &dirY);
    Ray2D ray = {
        .ox = originX + dirX * PATH_EPSILON,
        .oy = originY + dirY * PATH_EPSILON,
        .dx = dirX,
        .dy = dirY
    };
    HitInfo2D hit = {0};
    hit.objectIndex = -1;
    hit.triangleIndex = -1;
    hit.baryW = 1.0;
    if (!UniformGridTraceRay(grid, &ray, PATH_EPSILON, CACHE_MAX_DISTANCE, &hit)) {
        return 0.0;
    }
    if (hit.objectIndex < 0 || hit.objectIndex >= objectCount) return 0.0;
    const SceneObject* obj = &objects[hit.objectIndex];
    MaterialBSDF bsdf = {0};
    MaterialBSDFInitFromSceneObject(obj, &bsdf);

    // Direct light visibility + intensity falloff
    double lx = light ? (light->x - hit.px) : 0.0;
    double ly = light ? (light->y - hit.py) : 0.0;
    double lightDist2 = lx * lx + ly * ly;
    Normalize(&lx, &ly);
    double ndotl = fmax(0.0, hit.nx * lx + hit.ny * ly);

    double directVis = 0.0;
    if (light && ndotl > 0.0) {
        Ray2D lray = { hit.px + hit.nx * PATH_EPSILON, hit.py + hit.ny * PATH_EPSILON, lx, ly };
        HitInfo2D lhit = {0};
        if (!UniformGridTraceRay(grid, &lray, PATH_EPSILON, CACHE_MAX_DISTANCE, &lhit)) {
            directVis = 1.0;
        }
    }

    double radiance = 0.0;
    if (ndotl > 0.0 && directVis > 0.0) {
        double radius = light ? fmax(light->radius, 1.0) : 1.0;
        double area = M_PI * radius * radius;
        double lightTerm = animSettings.lightIntensity * area / (lightDist2 + radius * radius);
        double direct = ndotl * directVis * lightTerm;
        double diffuseScale = bsdf.diffuseWeight > 0.0 ? bsdf.diffuseWeight : bsdf.albedo;
        radiance += direct * diffuseScale;
    }

    // Multi-sample reflected probe weighted by material reflectivity/roughness
    if (includeIndirect && bsdf.reflectivity > 0.0) {
        // Treat dirX/dirY as outgoing; convert to incoming for reflection math
        double inX = -dirX;
        double inY = -dirY;
        Normalize(&inX, &inY);
        double inDot = inX * hit.nx + inY * hit.ny;
        double rx = inX - 2.0 * inDot * hit.nx;
        double ry = inY - 2.0 * inDot * hit.ny;
        Normalize(&rx, &ry);
        int reflSamples = 4;
        double jitterCone = fmax(0.05, bsdf.roughness * 0.8); // radians
        double reflAccum = 0.0;
        for (int rs = 0; rs < reflSamples; rs++) {
            double h = HashDouble(hit.objectIndex, rs, (int)(hit.px * 13 + hit.py * 7 + rs * 3));
            double angle = (h * 2.0 - 1.0) * jitterCone;
            double ca = cos(angle);
            double sa = sin(angle);
            double rtx = rx * ca - ry * sa;
            double rty = rx * sa + ry * ca;
            Normalize(&rtx, &rty);
            // Hemisphere guard
            if (rtx * hit.nx + rty * hit.ny <= 0.0) {
                continue;
            }

            Ray2D rray = { hit.px + hit.nx * PATH_EPSILON, hit.py + hit.ny * PATH_EPSILON, rtx, rty };
            HitInfo2D rhit = {0};
            if (UniformGridTraceRay(grid, &rray, PATH_EPSILON, CACHE_MAX_DISTANCE, &rhit)) {
                const SceneObject* robj = (rhit.objectIndex >= 0 && rhit.objectIndex < objectCount)
                                              ? &objects[rhit.objectIndex] : NULL;
                if (robj) {
                    MaterialBSDF rbsdf = {0};
                    MaterialBSDFInitFromSceneObject(robj, &rbsdf);
                    double rlx = light ? (light->x - rhit.px) : 0.0;
                    double rly = light ? (light->y - rhit.py) : 0.0;
                    double rdist2 = rlx * rlx + rly * rly;
                    Normalize(&rlx, &rly);
                    double rndotl = fmax(0.0, rhit.nx * rlx + rhit.ny * rly);
                    double rvis = 0.0;
                    if (light && rndotl > 0.0) {
                        Ray2D rlray = { rhit.px + rhit.nx * PATH_EPSILON, rhit.py + rhit.ny * PATH_EPSILON, rlx, rly };
                        HitInfo2D rlhit = {0};
                        if (!UniformGridTraceRay(grid, &rlray, PATH_EPSILON, CACHE_MAX_DISTANCE, &rlhit)) {
                            rvis = 1.0;
                        }
                    }
                    if (rndotl > 0.0 && rvis > 0.0) {
                        double radius = light ? fmax(light->radius, 1.0) : 1.0;
                        double area = M_PI * radius * radius;
                        double lightTerm = animSettings.lightIntensity * area / (rdist2 + radius * radius);
                        double reflContribution = rndotl * rvis * lightTerm * rbsdf.albedo * bsdf.reflectivity;
                        reflAccum += reflContribution;
                    }
                }
            }
        }
        if (reflSamples > 0) {
            radiance += reflAccum / (double)reflSamples;
        }
    }

    return radiance;
}

static double MeasureOcclusion(const UniformGrid* grid,
                               double originX,
                               double originY,
                               double dirX,
                               double dirY) {
    if (!grid || (!grid->objectCells && !grid->triangleCells)) return 0.0;
    Normalize(&dirX, &dirY);
    Ray2D ray = {
        .ox = originX,
        .oy = originY,
        .dx = dirX,
        .dy = dirY
    };
    HitInfo2D hit = {0};
    hit.objectIndex = -1;
    hit.triangleIndex = -1;
    hit.baryW = 1.0;
    if (UniformGridTraceRay(grid, &ray, PATH_EPSILON, CACHE_MAX_DISTANCE, &hit)) {
        return hit.t;
    }
    return 0.0;
}

static void ResetEntry(SurfaceIrradiance* entry,
                       double px,
                       double py,
                       double nx,
                       double ny) {
    if (!entry) return;
    entry->px = px;
    entry->py = py;
    entry->nx = nx;
    entry->ny = ny;
    for (int i = 0; i < IRRADIANCE_BIN_COUNT; i++) {
        entry->bins[i].dirX = 0.0;
        entry->bins[i].dirY = 0.0;
        entry->bins[i].mean = 0.0;
        entry->bins[i].variance = 0.0;
        entry->bins[i].distance = 0.0;
        entry->bins[i].samples = 0;
        entry->bins[i].valid = false;
    }
}

static void AccumulateBin(IrradianceBin* bin, double value) {
    if (!bin) return;
    bin->samples++;
    double delta = value - bin->mean;
    bin->mean += delta / (double)bin->samples;
    double delta2 = value - bin->mean;
    bin->variance += delta * delta2;
    bin->valid = true;
}

// Simple wrap-around smoothing to tame noisy bins
static void SmoothEntryBins(SurfaceIrradiance* entry) {
    if (!entry) return;
    double tmp[IRRADIANCE_BIN_COUNT];
    for (int i = 0; i < IRRADIANCE_BIN_COUNT; i++) {
        int prev = (i - 1 + IRRADIANCE_BIN_COUNT) % IRRADIANCE_BIN_COUNT;
        int next = (i + 1) % IRRADIANCE_BIN_COUNT;
        double center = entry->bins[i].mean;
        double left = entry->bins[prev].mean;
        double right = entry->bins[next].mean;
        double varCenter = entry->bins[i].variance;
        double varLeft = entry->bins[prev].variance;
        double varRight = entry->bins[next].variance;
        double weightCenter = 0.5;
        double weightNeighbors = 0.25;
        // If neighbors are noisy relative to center, downweight them slightly
        double centerVar = varCenter + 1e-6;
        if (varLeft > 4.0 * centerVar) weightNeighbors *= 0.5;
        if (varRight > 4.0 * centerVar) weightNeighbors *= 0.5;
        double norm = weightCenter + 2.0 * weightNeighbors;
        tmp[i] = (weightNeighbors * left + weightCenter * center + weightNeighbors * right) / norm;
    }
    for (int i = 0; i < IRRADIANCE_BIN_COUNT; i++) {
        entry->bins[i].mean = tmp[i];
    }
}

static void NormalizeEntryBins(SurfaceIrradiance* entry) {
    if (!entry) return;
    double maxMean = 0.0;
    for (int i = 0; i < IRRADIANCE_BIN_COUNT; i++) {
        if (entry->bins[i].mean > maxMean) {
            maxMean = entry->bins[i].mean;
        }
    }
    if (maxMean <= 0.0) return;
    double inv = 1.0 / fmax(maxMean, 1e-6);
    for (int i = 0; i < IRRADIANCE_BIN_COUNT; i++) {
        entry->bins[i].mean *= inv;
    }
}

static void FillEntryBins(const UniformGrid* grid,
                          SurfaceIrradiance* entry,
                          const SceneObject* objects,
                          int objectCount,
                          const SceneObject* traceObjects,
                          int traceObjectCount,
                          const LightSource* light,
                          bool includeIndirect) {
    if (!entry || !objects || objectCount <= 0) return;
    const SceneObject* traceList = (traceObjects && traceObjectCount > 0)
        ? traceObjects
        : objects;
    int traceCount = (traceObjects && traceObjectCount > 0)
        ? traceObjectCount
        : objectCount;
    double nx = entry->nx;
    double ny = entry->ny;
    double tx = -ny;
    double ty = nx;
    Normalize(&tx, &ty);
    double hemisphereStep = M_PI / (double)IRRADIANCE_BIN_COUNT;
    for (int binIndex = 0; binIndex < IRRADIANCE_BIN_COUNT; binIndex++) {
        double angle = -M_PI_2 + (binIndex + 0.5) * hemisphereStep;
        double dirX = nx * cos(angle) + tx * sin(angle);
        double dirY = ny * cos(angle) + ty * sin(angle);
        Normalize(&dirX, &dirY);
        entry->bins[binIndex].dirX = dirX;
        entry->bins[binIndex].dirY = dirY;
        for (int s = 0; s < CACHE_SAMPLES_PER_BIN; s++) {
            double jitter = ((double)s + 0.5) / (double)CACHE_SAMPLES_PER_BIN - 0.5;
            double jitterAngle = angle + jitter * hemisphereStep * 0.5;
            double sampleDirX = nx * cos(jitterAngle) + tx * sin(jitterAngle);
            double sampleDirY = ny * cos(jitterAngle) + ty * sin(jitterAngle);
            Normalize(&sampleDirX, &sampleDirY);
            double value = TraceDirectionalEnergy(grid,
                                                  traceList,
                                                  traceCount,
                                                  entry->px + entry->nx * PATH_EPSILON,
                                                  entry->py + entry->ny * PATH_EPSILON,
                                                  sampleDirX,
                                                  sampleDirY,
                                                  light,
                                                  includeIndirect);
            AccumulateBin(&entry->bins[binIndex], value);
        }
        entry->bins[binIndex].distance = MeasureOcclusion(grid,
                                                          entry->px + entry->nx * PATH_EPSILON,
                                                          entry->py + entry->ny * PATH_EPSILON,
                                                          dirX,
                                                          dirY);
    }
}

bool IrradianceCacheFill(IrradianceCache* cache,
                         const SceneObject* objects,
                         int objectCount,
                         const LightSource* light,
                         const UniformGrid* grid,
                         const float* energyBuffer,
                         int width,
                         int height,
                         double maxEnergy,
                         const SceneObject* traceObjects,
                         int traceObjectCount,
                         const LightSource* traceLight,
                         bool includeIndirectReflections) {
    (void)energyBuffer;
    (void)width;
    (void)height;
    (void)maxEnergy;
    if (!cache || !cache->data || !grid || !objects || objectCount <= 0) return false;
    const SceneObject* traceList = traceObjects;
    int traceCount = traceObjectCount;
    if (!traceList || traceCount <= 0) {
        traceList = objects;
        traceCount = objectCount;
    }
    const LightSource* activeLight = traceLight ? traceLight : light;
    int samplesPerObject = cache->samplesPerObject > 0 ? cache->samplesPerObject : 1;
    for (int objIdx = 0; objIdx < objectCount; objIdx++) {
        const SceneObject* obj = &objects[objIdx];
        for (int sample = 0; sample < samplesPerObject; sample++) {
            SurfaceIrradiance* entry = IrradianceCacheGet(cache, objIdx, sample);
            if (!entry) continue;
            double sampleX, sampleY, nx, ny;
            double t = ((double)sample + 0.5) / (double)samplesPerObject;
            if (!SampleObjectPoint(obj, t, &sampleX, &sampleY, &nx, &ny)) {
                memset(entry, 0, sizeof(*entry));
                continue;
            }
            ResetEntry(entry, sampleX, sampleY, nx, ny);
            FillEntryBins(grid,
                          entry,
                          objects,
                          objectCount,
                          traceList,
                          traceCount,
                          activeLight,
                          includeIndirectReflections);
            SmoothEntryBins(entry);
            SmoothEntryBins(entry);
            NormalizeEntryBins(entry);
        }
    }
    return true;
}
