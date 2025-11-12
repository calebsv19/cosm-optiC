#include "render/irradiance_cache.h"

#include "camera/camera.h"
#include "config/config_manager.h"
#include "render/ray_types.h"
#include "render/uniform_grid.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

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

static double SampleEnergyBufferAt(const float* buffer,
                                   int width,
                                   int height,
                                   double worldX,
                                   double worldY) {
    if (!buffer) return 0.0;
    CameraPoint screen = CameraWorldToScreen(&sceneSettings.camera,
                                             worldX,
                                             worldY,
                                             width,
                                             height);
    int sx = (int)lround(screen.x);
    int sy = (int)lround(screen.y);
    if (sx < 0 || sx >= width || sy < 0 || sy >= height) {
        return 0.0;
    }
    size_t idx = (size_t)sy * (size_t)width + (size_t)sx;
    return buffer[idx];
}

static double IntegrateDirectionalEnergy(const UniformGrid* grid,
                                         const float* buffer,
                                         int width,
                                         int height,
                                         double originX,
                                         double originY,
                                         double dirX,
                                         double dirY,
                                         double maxEnergy) {
    (void)maxEnergy;
    if (!buffer) return 0.0;
    Normalize(&dirX, &dirY);
    double maxDistance = CACHE_MAX_DISTANCE;
    if (grid && (grid->objectCells || grid->triangleCells)) {
        Ray2D ray = {
            .ox = originX,
            .oy = originY,
            .dx = dirX,
            .dy = dirY
        };
        HitInfo2D hit;
        if (UniformGridTraceRay(grid, &ray, PATH_EPSILON, CACHE_MAX_DISTANCE, &hit)) {
            maxDistance = hit.t;
        }
    }
    double accum = 0.0;
    int validSamples = 0;
    int samples = CACHE_SAMPLES_PER_BIN;
    for (int i = 1; i <= samples; i++) {
        double dist = (double)i * CACHE_STEP_DISTANCE;
        if (dist > maxDistance) break;
        double px = originX + dirX * dist;
        double py = originY + dirY * dist;
        double sample = SampleEnergyBufferAt(buffer, width, height, px, py);
        double weight = exp(-dist / CACHE_DISTANCE_DECAY);
        accum += sample * weight;
        validSamples++;
    }
    if (validSamples == 0) {
        return 0.0;
    }
    double mean = accum / (double)validSamples;
    return fmax(mean, 0.0);
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
    HitInfo2D hit;
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

static void FillEntryBins(const UniformGrid* grid,
                          SurfaceIrradiance* entry,
                          const float* buffer,
                          int width,
                          int height,
                          double maxEnergy) {
    if (!entry) return;
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
            double value = IntegrateDirectionalEnergy(grid,
                                                      buffer,
                                                      width,
                                                      height,
                                                      entry->px + entry->nx * PATH_EPSILON,
                                                      entry->py + entry->ny * PATH_EPSILON,
                                                      sampleDirX,
                                                      sampleDirY,
                                                      maxEnergy);
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
                         double maxEnergy) {
    (void)light;
    if (!cache || !cache->data || !energyBuffer) return false;
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
            FillEntryBins(grid, entry, energyBuffer, width, height, maxEnergy);
        }
    }
    return true;
}
