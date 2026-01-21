#include "render/integrators/hybrid/integrator_indirect.h"
#include "render/integrators/hybrid/integrator_sampling.h"
#include "render/integrators/hybrid/integrator_visibility.h"
#include "render/integrators/hybrid/integrator_cache.h"
#include "render/integrators/hybrid/integrator_direct.h"
#include "render/integrators/hybrid/integrator_energy.h"
#include "render/material_bsdf.h"
#include "config/config_manager.h"
#include "camera/camera.h"
#include "render/material_bsdf.h"

#include "scene/object_manager.h"

#include <math.h>
#include <float.h>

#ifndef GRID_EPSILON
#define GRID_EPSILON 0.0001
#endif
#ifndef MAX_INDIRECT_BRIGHTNESS
#define MAX_INDIRECT_BRIGHTNESS 6.0f
#endif

static double RenderQualityScale(void)
{
    switch (animSettings.renderQuality) {
        case RENDER_QUALITY_LOW:  return 0.5;
        case RENDER_QUALITY_HIGH: return 2.0;
        case RENDER_QUALITY_MEDIUM:
        default: return 1.0;
    }
}

static double EstimateAverageObjectExtent(const IntegratorIndirectContext* ctx)
{
    if (!ctx || !ctx->objects || ctx->objectCount <= 0)
        return 200.0;

    double sum = 0.0;
    int count = 0;

    for (int i = 0; i < ctx->objectCount; i++) {
        const SceneObject* obj = &ctx->objects[i];
        double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
        ComputeObjectBounds(obj, &minX, &minY, &maxX, &maxY);
        double extentX = maxX - minX;
        double extentY = maxY - minY;
        double ext = fmax(extentX, extentY);
        if (ext <= 0.0 && obj->radius > 0.0) {
            ext = obj->radius * obj->scale * 2.0;
        }
        if (ext > 0.0) {
            sum += ext;
            count++;
        }
    }

    if (count == 0) return 200.0;
    double avg = sum / (double)count;
    if (avg < 50.0) avg = 50.0;
    if (avg > 1200.0) avg = 1200.0;
    return avg;
}

static inline void normalize(double* x, double* y)
{
    double len = sqrt((*x)*(*x) + (*y)*(*y));
    if (len > 1e-12) {
        *x /= len;
        *y /= len;
    }
}

static inline double smooth_distance_weight(double d)
{
    double w1 = 1.0 / (1.0 + d * 0.006);
    double w2 = exp(-d * 0.0009);
    return w1 * w2;
}

float IndirectSamplePoint(const IntegratorIndirectContext* ctx,
                          const LightSource* light,
                          double worldX,
                          double worldY,
                          int px, int py,
                          int feelerCount,
                          double feelerLimit,
                          double varianceCut,
                          double haloRadius,
                          double intensityScale,
                          float* debugStats)
{
    if (!ctx || !ctx->grid)
        return 0.0f;

    double lx = light ? (light->x - worldX) : 0.0;
    double ly = light ? (light->y - worldY) : 0.0;
    double ldist = sqrt(lx*lx + ly*ly);

    if (light && ldist < haloRadius)
        return 0.0f;

    // Hybrid uses an infinite, top-down view: fixed view direction for all pixels.
    double viewX = 0.0;
    double viewY = -1.0;
    normalize(&viewX, &viewY);

    double baseNx = viewX;
    double baseNy = viewY;

    float sum = 0.0f;

    float rejAlign = 0.0f;
    float rejVar   = 0.0f;

    for (int i = 0; i < feelerCount; i++) {

        double jitter = PixelFeelerJitter(px, py, i);
        double dirX, dirY;

        FeelerDirection(i, feelerCount, jitter,
                        baseNx, baseNy,
                        &dirX, &dirY);
        normalize(&dirX, &dirY);

        HitInfo2D hit = {0};
        const SceneObject* obj = NULL;

        if (!TraceRayToSurface(ctx->grid,
                               worldX, worldY,
                               dirX, dirY,
                               &hit,
                               &obj,
                               1500.0))
            continue;

        double vx = worldX - hit.px;
        double vy = worldY - hit.py;
        double pdist = sqrt(vx*vx + vy*vy);

        if (pdist < GRID_EPSILON) continue;
        if (feelerLimit > 0.0 && pdist > feelerLimit) continue;

        double inX = -dirX;
        double inY = -dirY;
        double nnx, nny;
        OrientNormalForIncoming(inX, inY,
                                hit.nx, hit.ny,
                                &nnx, &nny);
        normalize(&nnx, &nny);

        MaterialBSDF* mat = NULL;
        if (ctx->materials &&
            hit.objectIndex >= 0 &&
            hit.objectIndex < ctx->materialCount)
            mat = &ctx->materials[hit.objectIndex];

        float cacheVal = 0.0f;
        if (ctx->cache) {
            cacheVal =
                CacheSampleDirectional(ctx->cache,
                                       &hit,
                                       vx, vy,
                                       mat,
                                       varianceCut,
                                       &rejAlign,
                                       &rejVar);
        }

        float indirect = cacheVal;

        if (indirect <= 0.0f) {
            if (light) {
                double lx2 = light->x - hit.px;
                double ly2 = light->y - hit.py;
                double llen = sqrt(lx2*lx2 + ly2*ly2);
                if (llen > GRID_EPSILON) {
                    lx2 /= llen;
                    ly2 /= llen;
                }

                double vx2 = vx / pdist;
                double vy2 = vy / pdist;

                double cosTerm = mat
                    ? MaterialBSDFEvaluateCos(mat, nnx, nny, lx2, ly2, vx2, vy2)
                    : fmax(0.0, vx2 * nnx + vy2 * nny);

                if (cosTerm > 0.0)
                    indirect = (float)(DirectComputeRadiance(light,
                                                             hit.px,
                                                             hit.py,
                                                             intensityScale) *
                                       cosTerm);
            }
        }

        if (indirect <= 0.0f)
            continue;

        double vf = fmax(0.0,
                         (vx * nnx + vy * nny) / pdist);
        if (vf <= 0.01)
            continue;

        double distW = smooth_distance_weight(pdist);

        double vdx = vx / pdist;
        double vdy = vy / pdist;

        double bsdfTerm = 1.0;
        if (mat) {
            // Incoming = direction toward the pixel, outgoing = view-to-hit (camera)
            double Lx = vdx;
            double Ly = vdy;

            double Vx = viewX;
            double Vy = viewY;

            bsdfTerm = MaterialBSDFEvaluateCos(mat,
                                               nnx, nny,
                                               Lx,  Ly,
                                               Vx,  Vy);
        }

        float weighted = (float)(indirect * distW * bsdfTerm);

        if (weighted > MAX_INDIRECT_BRIGHTNESS)
            weighted = MAX_INDIRECT_BRIGHTNESS;

        sum += weighted;
    }

    if (debugStats) {
        debugStats[0] += rejAlign;
        debugStats[1] += rejVar;
    }

    if (sum > MAX_INDIRECT_BRIGHTNESS)
        sum = MAX_INDIRECT_BRIGHTNESS;

    return sum;
}

void IndirectLightingPass(IntegratorIndirectContext* ctx,
                          const LightSource* light,
                          double userVariance,
                          double userHalo,
                          double intensityScale)
{
    IndirectLightingPassRegion(ctx, light, userVariance, userHalo, intensityScale,
                               0, 0, ctx ? ctx->width : 0, ctx ? ctx->height : 0);
}

void IndirectLightingPassRegion(IntegratorIndirectContext* ctx,
                                const LightSource* light,
                                double userVariance,
                                double userHalo,
                                double intensityScale,
                                int startX, int startY,
                                int endX, int endY)
{
    if (!ctx)
        return;

    float debug[2] = {0};

    double varianceCut = CacheVarianceThreshold(userVariance);
    double haloRadius  = CacheHaloRadius(light, userHalo);

    IntegratorEnergyContext ectx = {
        .width = ctx->width,
        .height = ctx->height,
        .useTiles = ctx->useTiles,
        .tileGrid = ctx->tileGrid,
        .energyBuffer = ctx->energyBuffer
    };

    int minX = startX < 0 ? 0 : startX;
    int minY = startY < 0 ? 0 : startY;
    int maxX = endX > ctx->width ? ctx->width : endX;
    int maxY = endY > ctx->height ? ctx->height : endY;
    if (minX >= maxX || minY >= maxY) return;

    double avgExtent = EstimateAverageObjectExtent(ctx);
    double baseFeelerLimit = fmax(3.0 * avgExtent, 300.0);
    double feelerLimitMax = 1500.0;
    double feelerLimitMin = 200.0;
    if (baseFeelerLimit > feelerLimitMax) baseFeelerLimit = feelerLimitMax;
    if (baseFeelerLimit < feelerLimitMin) baseFeelerLimit = feelerLimitMin;

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            CameraPoint world = CameraScreenToWorld(&sceneSettings.camera,
                                                    x + 0.5,
                                                    y + 0.5,
                                                    ctx->width,
                                                    ctx->height);
            double wx = world.x;
            double wy = world.y;

            // Skip indirect if the direct pass already lit this pixel.
            if (ReadEnergy(&ectx, x, y) > 0.0f)
                continue;

            float directScale = (float)DirectComputeRadiance(light, wx, wy, intensityScale);
            float qualityScale = (float)RenderQualityScale();
            int feelerCount = DetermineFeelerCount(directScale, qualityScale);

            float gi = IndirectSamplePoint(ctx, light,
                                           wx, wy,
                                           x, y,
                                           feelerCount,
                                           baseFeelerLimit,
                                           varianceCut,
                                           haloRadius,
                                           intensityScale,
                                           debug);

            if (gi > 0.0f)
                AddEnergy(&ectx, x, y, gi);
        }
    }
}
