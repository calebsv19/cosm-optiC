#include "render/integrators/hybrid/integrator_cache.h"
#include "render/material_bsdf.h"
#include <float.h>
#include <math.h>

static inline void normalize(double* x, double* y) {
    double len = sqrt((*x)*(*x) + (*y)*(*y));
    if (len > 1e-12) {
        *x /= len;
        *y /= len;
    }
}

double CacheVarianceThreshold(double userCutoff)
{
    if (userCutoff <= 0.0)
        return 0.35;
    if (userCutoff < 0.05)
        return 0.05;
    if (userCutoff > 10.0)
        return 10.0;
    return userCutoff;
}

double CacheHaloRadius(const LightSource* light, double userScale)
{
    if (!light) return 12.0;
    double mul = (userScale > 0.0) ? userScale : 3.5;
    double base = light->radius * mul;
    return (base < 12.0) ? 12.0 : base;
}

const SurfaceIrradiance* CacheFindClosest(const IrradianceCache* cache,
                                          int objectIndex,
                                          double px,
                                          double py)
{
    if (!cache || !cache->data) return NULL;
    if (objectIndex < 0 || objectIndex >= cache->objectCount) return NULL;

    const SurfaceIrradiance* best = NULL;
    double bestDist2 = DBL_MAX;

    int count = cache->samplesPerObject;
    const SurfaceIrradiance* base = cache->data + (objectIndex * count);

    for (int i = 0; i < count; i++) {
        const SurfaceIrradiance* entry = &base[i];

        double dx = px - entry->px;
        double dy = py - entry->py;
        double d2 = dx*dx + dy*dy;

        if (d2 < bestDist2) {
            bestDist2 = d2;
            best = entry;
        }
    }

    return best;
}

float CacheSampleDirectional(const IrradianceCache* cache,
                             const HitInfo2D* hit,
                             double dirX,
                             double dirY,
                             const MaterialBSDF* mat,
                             double varianceCut,
                             float* debugRejectAlign,
                             float* debugRejectVar)
{
    if (!cache || !hit) return 0.0f;

    const SurfaceIrradiance* entry =
        CacheFindClosest(cache, hit->objectIndex, hit->px, hit->py);

    if (!entry) return 0.0f;

    normalize(&dirX, &dirY);

    double hemi = dirX * entry->nx + dirY * entry->ny;
    if (hemi <= 0.0)
        return 0.0f;

    double bestDot = -1.0;
    double bestMean = 0.0;

    for (int i = 0; i < IRRADIANCE_BIN_COUNT; i++) {
        const IrradianceBin* b = &entry->bins[i];
        if (!b->valid || b->samples == 0) continue;

        double dot = dirX * b->dirX + dirY * b->dirY;
        if (dot < 0.01) {
            if (debugRejectAlign) (*debugRejectAlign)++;
            continue;
        }

        double v = b->variance / (double)fmax(1, b->samples);
        if (v > varianceCut) {
            if (debugRejectVar) (*debugRejectVar)++;
            continue;
        }

        if (dot > bestDot) {
            bestDot = dot;
            bestMean = b->mean;
        }
    }

    if (bestMean <= 0.0)
        return 0.0f;

    double diffScale = 1.0;
    if (mat) {
        diffScale = (mat->diffuseWeight > 0.0)
            ? mat->diffuseWeight
            : mat->albedo;
    }

    return (float)(bestMean * diffScale);
}
