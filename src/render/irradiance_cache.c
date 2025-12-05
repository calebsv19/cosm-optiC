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

// When a forward-probe buffer is provided, dampen far samples so cache bins
// stay local to the surfel being seeded.
#define CACHE_FORWARD_DECAY 0.0025

static void Normalize(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-9) {
        *x /= len;
        *y /= len;
    }
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
static double SampleForwardProbe(const float* energyBuffer,
                                 int width,
                                 int height,
                                 double worldX,
                                 double worldY,
                                 double distance) {
    if (!energyBuffer || width <= 0 || height <= 0) return 0.0;
    CameraPoint screen = CameraWorldToScreen(&sceneSettings.camera,
                                             worldX,
                                             worldY,
                                             width,
                                             height);
    int px = (int)floor(screen.x + 0.5);
    int py = (int)floor(screen.y + 0.5);
    if (px < 0 || py < 0 || px >= width || py >= height) {
        return 0.0;
    }
    size_t idx = (size_t)py * (size_t)width + (size_t)px;
    double value = energyBuffer[idx];
    // Apply gentle decay so distant forward samples don't dominate
    if (distance > 0.0) {
        value *= exp(-CACHE_FORWARD_DECAY * distance);
    }
    return value;
}

static double TraceDirectionalEnergy(const UniformGrid* grid,
                                     const SceneObject* objects,
                                     int objectCount,
                                     double originX,
                                     double originY,
                                     double dirX,
                                     double dirY,
                                     const LightSource* light,
                                     const float* energyBuffer,
                                     int bufferWidth,
                                     int bufferHeight,
                                     bool includeIndirect) {
    if (!grid || !objects || objectCount <= 0) return 0.0;
    Normalize(&dirX, &dirY);
    // Hemisphere guard: only trace directions in front of the surface normal
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
    double nx = hit.nx;
    double ny = hit.ny;
    Normalize(&nx, &ny);
    // Reject if outgoing direction is behind the surface
    if (dirX * nx + dirY * ny <= 0.0) {
        return 0.0;
    }

    // Direct light visibility + intensity falloff
    double lx = light ? (light->x - hit.px) : 0.0;
    double ly = light ? (light->y - hit.py) : 0.0;
    double lightDist2 = lx * lx + ly * ly;
    Normalize(&lx, &ly);
    double ndotl = fmax(0.0, nx * lx + ny * ly);

    double directVis = 0.0;
    if (light && ndotl > 0.0) {
        Ray2D lray = { hit.px + nx * PATH_EPSILON, hit.py + ny * PATH_EPSILON, lx, ly };
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
        double bsdfCos = MaterialBSDFEvaluateCos(&bsdf, nx, ny, lx, ly, dirX, dirY);
        radiance += directVis * lightTerm * bsdfCos;
    }

    // Optional indirect pickup from the forward-probe buffer
    if (includeIndirect && energyBuffer) {
        double probe = SampleForwardProbe(energyBuffer,
                                          bufferWidth,
                                          bufferHeight,
                                          hit.px,
                                          hit.py,
                                          hit.t);
        if (probe > 0.0) {
            radiance += probe;
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
        const IrradianceBin* bC = &entry->bins[i];
        const IrradianceBin* bL = &entry->bins[prev];
        const IrradianceBin* bR = &entry->bins[next];
        double center = (bC->valid && bC->samples > 0) ? bC->mean : 0.0;
        double left = (bL->valid && bL->samples > 0) ? bL->mean : 0.0;
        double right = (bR->valid && bR->samples > 0) ? bR->mean : 0.0;
        double varCenter = (bC->valid && bC->samples > 0) ? bC->variance : 0.0;
        double varLeft = (bL->valid && bL->samples > 0) ? bL->variance : varCenter;
        double varRight = (bR->valid && bR->samples > 0) ? bR->variance : varCenter;
        double weightCenter = bC->valid ? 0.5 : 0.0;
        double weightNeighbors = 0.25;
        // If neighbors are noisy relative to center, downweight them slightly
        double centerVar = varCenter + 1e-6;
        if (varLeft > 4.0 * centerVar) weightNeighbors *= 0.5;
        if (varRight > 4.0 * centerVar) weightNeighbors *= 0.5;
        if (!bL->valid) weightNeighbors = 0.0;
        if (!bR->valid) weightNeighbors = 0.0;
        double norm = weightCenter + 2.0 * weightNeighbors;
        if (norm > 1e-6) {
            tmp[i] = (weightNeighbors * left + weightCenter * center + weightNeighbors * right) / norm;
        } else {
            tmp[i] = center;
        }
    }
    for (int i = 0; i < IRRADIANCE_BIN_COUNT; i++) {
        entry->bins[i].mean = tmp[i];
    }
}

static void FillEntryBins(const UniformGrid* grid,
                          SurfaceIrradiance* entry,
                          const SceneObject* objects,
                          int objectCount,
                          const SceneObject* traceObjects,
                          int traceObjectCount,
                          const LightSource* light,
                          const float* energyBuffer,
                          int bufferWidth,
                          int bufferHeight,
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
            if (sampleDirX * entry->nx + sampleDirY * entry->ny <= 0.0) {
                continue;
            }
            double value = TraceDirectionalEnergy(grid,
                                                  traceList,
                                                  traceCount,
                                                  entry->px + entry->nx * PATH_EPSILON,
                                                  entry->py + entry->ny * PATH_EPSILON,
                                                  sampleDirX,
                                                  sampleDirY,
                                                  light,
                                                  energyBuffer,
                                                  bufferWidth,
                                                  bufferHeight,
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
                          energyBuffer,
                          width,
                          height,
                          includeIndirectReflections);
            SmoothEntryBins(entry);
            SmoothEntryBins(entry);
        }
    }
    return true;
}
