#include <math.h>
#include <stdio.h>
#include <string.h>

#include "render/integrators/hybrid/camera_path_integrator.h"

#include "render/integrators/hybrid/integrator_energy.h"
#include "render/integrators/hybrid/integrator_direct.h"
#include "render/integrators/hybrid/integrator_indirect.h"
#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/integrators/hybrid/integrator_visibility.h"
#include "render/integrators/hybrid/integrator_cache.h"
#include "render/integrator_common.h"

#ifndef PATH_EPSILON
#define PATH_EPSILON 0.0001
#endif

void CameraPathIntegrator_Init(CameraIntegrator* ci,
                               int width,
                               int height,
                               UniformGrid* grid,
                               SceneObject* objects,
                               int objectCount,
                               MaterialBSDF* materials,
                               int materialCount,
                               IrradianceCache* cache,
                               TileGrid* tiles)
{
    ci->width        = width;
    ci->height       = height;
    ci->grid         = grid;
    ci->objects      = objects;
    ci->objectCount  = objectCount;
    ci->materials    = materials;
    ci->materialCount= materialCount;
    ci->cache        = cache;
    ci->tiles        = tiles;
    ci->useTiles     = (tiles != NULL);

    size_t total = (size_t)width * (size_t)height;

    ci->energy = (float*)malloc(sizeof(float) * total);
    ci->pixels = (unsigned char*)malloc(sizeof(unsigned char) * total);

    memset(ci->energy, 0, sizeof(float) * total);
    memset(ci->pixels, 0, total);
}

void CameraPathIntegrator_Free(CameraIntegrator* ci)
{
    if (ci->energy) { free(ci->energy); ci->energy = NULL; }
    if (ci->pixels) { free(ci->pixels); ci->pixels = NULL; }
}

static void DirectPass(CameraIntegrator* ci,
                       const LightSource* light,
                       double camX,
                       double camY,
                       double intensityScale,
                       double brightnessBoost,
                       size_t* outHits,
                       double* outMax)
{
    IntegratorDirectContext dctx = {
        .width       = ci->width,
        .height      = ci->height,
        .grid        = ci->grid,
        .pixelBuffer = ci->pixels,      /* not used for direct */
        .energyBuffer= ci->energy,
        .useTiles    = ci->useTiles,
        .tileGrid    = ci->tiles
    };

    size_t hits = 0;
    double maxE = 0.0;

    for (int y = 0; y < ci->height; y++) {
        for (int x = 0; x < ci->width; x++) {
            double wx = (double)x + 0.5;
            double wy = (double)y + 0.5;
            if (!HasDirectLineOfSight(ci->grid, wx, wy, light)) continue;
            hits++;
        }
    }

    DirectLightingPass(&dctx, light, camX, camY, intensityScale * brightnessBoost);

    // Scan energy for max (tiles or linear)
    if (ci->useTiles && ci->tiles && ci->tiles->tiles) {
        for (size_t ti = 0; ti < ci->tiles->count; ti++) {
            IntegratorTile* t = &ci->tiles->tiles[ti];
            if (!t->energy) continue;
            size_t total = (size_t)t->width * (size_t)t->height;
            for (size_t i = 0; i < total; i++) {
                if (t->energy[i] > maxE) maxE = t->energy[i];
            }
        }
    } else if (ci->energy) {
        size_t total = (size_t)ci->width * (size_t)ci->height;
        for (size_t i = 0; i < total; i++) {
            if (ci->energy[i] > maxE) maxE = ci->energy[i];
        }
    }

    if (outHits) *outHits = hits;
    if (outMax) *outMax = maxE;
}

static void IndirectPass(CameraIntegrator* ci,
                         const LightSource* light,
                         double camX,
                         double camY,
                         double userVariance,
                         double userHalo,
                         double intensityScale,
                         double brightnessBoost,
                         size_t* outHits,
                         double* outMax)
{
    IntegratorIndirectContext ictx = {
        .width        = ci->width,
        .height       = ci->height,
        .grid         = ci->grid,
        .cache        = ci->cache,
        .energyBuffer = ci->energy,
        .useTiles     = ci->useTiles,
        .tileGrid     = ci->tiles,
        .objects      = ci->objects,
        .objectCount  = ci->objectCount,
        .materials    = ci->materials,
        .materialCount= ci->materialCount
    };

    IndirectLightingPass(&ictx, light, camX, camY, userVariance, userHalo, intensityScale);
    if (brightnessBoost != 1.0) {
        // Simple scale of indirect buffer to mirror direct boost
        IntegratorEnergyContext ectx = {
            .width = ci->width,
            .height = ci->height,
            .useTiles = ci->useTiles,
            .tileGrid = ci->tiles,
            .energyBuffer = ci->energy
        };
        // Multiply energy by boost
        if (ectx.useTiles && ectx.tileGrid && ectx.tileGrid->tiles) {
            for (size_t i = 0; i < ectx.tileGrid->count; i++) {
                IntegratorTile* t = &ectx.tileGrid->tiles[i];
                if (!t->energy) continue;
                size_t total = (size_t)t->width * (size_t)t->height;
                for (size_t j = 0; j < total; j++) {
                    t->energy[j] *= (float)brightnessBoost;
                }
            }
        } else if (ectx.energyBuffer) {
            size_t total = (size_t)ectx.width * (size_t)ectx.height;
            for (size_t j = 0; j < total; j++) {
                ectx.energyBuffer[j] *= (float)brightnessBoost;
            }
        }
    }

    double maxE = 0.0;
    size_t hits = 0;
    if (ictx.useTiles && ictx.tileGrid && ictx.tileGrid->tiles) {
        for (size_t ti = 0; ti < ictx.tileGrid->count; ti++) {
            IntegratorTile* t = &ictx.tileGrid->tiles[ti];
            if (!t->energy) continue;
            size_t total = (size_t)t->width * (size_t)t->height;
            for (size_t i = 0; i < total; i++) {
                if (t->energy[i] > 0.0f) {
                    hits++;
                    if (t->energy[i] > maxE) maxE = t->energy[i];
                }
            }
        }
    } else if (ictx.energyBuffer) {
        size_t total = (size_t)ictx.width * (size_t)ictx.height;
        for (size_t i = 0; i < total; i++) {
            if (ictx.energyBuffer[i] > 0.0f) {
                hits++;
                if (ictx.energyBuffer[i] > maxE) maxE = ictx.energyBuffer[i];
            }
        }
    }
    if (outHits) *outHits = hits;
    if (outMax) *outMax = maxE;
}

static void OptionalBlur(CameraIntegrator* ci, int apply)
{
    if (!apply) return;

    IntegratorEnergyContext ectx = {
        .width        = ci->width,
        .height       = ci->height,
        .useTiles     = ci->useTiles,
        .tileGrid     = ci->tiles,
        .energyBuffer = ci->energy
    };

    BilateralBlurEnergy(&ectx, 1.2f, 0.6f);
}

static void FinalTonemap(CameraIntegrator* ci)
{
    TonemapContext tctx = {
        .width       = ci->width,
        .height      = ci->height,
        .useTiles    = ci->useTiles,
        .tiles       = ci->tiles,
        .energyBuffer= ci->energy,
        .pixelBuffer = ci->pixels
    };

    TonemapApply(&tctx);
}

void CameraPathIntegrator_Render(CameraIntegrator* ci,
                                 const LightSource* light,
                                 double camX,
                                 double camY,
                                 const CameraIntegratorSettings* s)
{
    if (!ci || !ci->pixels) return;

    int hasTiles = (ci->useTiles && ci->tiles && ci->tiles->tiles);
    int hasLinear = (!ci->useTiles && ci->energy);

    if (!hasTiles && !hasLinear) {
        fprintf(stderr, "[Hybrid] Missing energy buffers (tiles=%d linear=%d); skipping render\n",
                hasTiles, hasLinear);
        return;
    }

    IntegratorEnergyContext clearCtx = {
        .width = ci->width,
        .height = ci->height,
        .useTiles = ci->useTiles,
        .tileGrid = ci->tiles,
        .energyBuffer = ci->energy
    };
    ClearEnergy(&clearCtx);

    size_t directHits = 0, indirectHits = 0;
    double directMax = 0.0, indirectMax = 0.0;

    DirectPass(ci,
               light,
               camX,
               camY,
               s->directIntensityScale,
               s->brightnessBoost,
               &directHits,
               &directMax);

    IndirectPass(ci,
                 light,
                 camX,
                 camY,
                 s->indirectVariance,
                 s->indirectHaloRadius,
                 s->directIntensityScale,
                 s->brightnessBoost,
                 &indirectHits,
                 &indirectMax);

    OptionalBlur(ci, s->blurEnabled);
    FinalTonemap(ci);

    printf("[Hybrid] Direct hits: %zu maxE=%.4f | Indirect hits: %zu maxE=%.4f\n",
           directHits, directMax,
           indirectHits, indirectMax);
}

/* Adapter: use legacy IntegratorContext buffers */
void CameraPathIntegratorRenderFromContext(IntegratorContext* ctx,
                                           const LightSource* light,
                                           const CameraIntegratorSettings* s,
                                           double camX,
                                           double camY)
{
    if (!ctx || !ctx->pixelBuffer) return;

    int hasTiles = (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles);
    int hasLinear = (!ctx->useTiles && ctx->energyBuffer);

    if (!hasTiles && !hasLinear) {
        fprintf(stderr, "[Hybrid] Missing buffers in IntegratorContext (tiles=%d linear=%d); skipping render\n",
                hasTiles, hasLinear);
        return;
    }

    CameraIntegrator ci = {0};
    ci.width        = ctx->width;
    ci.height       = ctx->height;
    ci.grid         = ctx->uniformGrid ? (UniformGrid*)ctx->uniformGrid : NULL;
    ci.objects      = ctx->objects;
    ci.objectCount  = ctx->objectCount;
    ci.materials    = (MaterialBSDF*)ctx->materials;
    ci.materialCount= ctx->materialCount;
    ci.cache        = ctx->cache;
    ci.tiles        = ctx->tileGrid;
    ci.useTiles     = ctx->useTiles;
    ci.energy       = ctx->energyBuffer;   // reuse existing buffers
    ci.pixels       = ctx->pixelBuffer;

    CameraPathIntegrator_Render(&ci,
                                light,
                                camX,
                                camY,
                                s);
}
