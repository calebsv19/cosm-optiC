#include "render/integrators/hybrid/integrator_energy.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static inline float clampf(float x, float a, float b) {
    return x < a ? a : (x > b ? b : x);
}

static int fetch_tile(const IntegratorEnergyContext* ctx,
                      int x, int y,
                      IntegratorTile** outTile,
                      int* lx, int* ly)
{
    if (!ctx || !ctx->tileGrid || !ctx->tileGrid->tiles)
        return 0;

    if (x < 0 || y < 0 || x >= ctx->width || y >= ctx->height)
        return 0;

    TileGrid* g = ctx->tileGrid;
    int ts = g->tileSize > 0 ? g->tileSize : 1;

    int tx = x / ts;
    int ty = y / ts;

    if (tx < 0 || ty < 0 || tx >= g->tilesX || ty >= g->tilesY)
        return 0;

    size_t idx = (size_t)ty * (size_t)g->tilesX + (size_t)tx;
    if (idx >= g->count) return 0;

    IntegratorTile* t = &g->tiles[idx];
    if (!t->energy) return 0;

    int lx0 = x - t->originX;
    int ly0 = y - t->originY;
    if (lx0 < 0 || ly0 < 0 || lx0 >= t->width || ly0 >= t->height)
        return 0;

    *outTile = t;
    *lx = lx0;
    *ly = ly0;
    return 1;
}

float ReadEnergy(const IntegratorEnergyContext* ctx, int x, int y)
{
    if (!ctx) return 0.0f;

    if (ctx->useTiles) {
        IntegratorTile* t = NULL;
        int lx, ly;
        if (!fetch_tile(ctx, x, y, &t, &lx, &ly))
            return 0.0f;

        size_t idx = (size_t)ly * (size_t)t->width + (size_t)lx;
        return t->energy[idx];
    }

    if (!ctx->energyBuffer) return 0.0f;
    if (x < 0 || y < 0 || x >= ctx->width || y >= ctx->height)
        return 0.0f;

    return ctx->energyBuffer[(size_t)y * (size_t)ctx->width + (size_t)x];
}

void WriteEnergy(const IntegratorEnergyContext* ctx, int x, int y, float v)
{
    if (!ctx) return;

    if (ctx->useTiles) {
        IntegratorTile* t = NULL;
        int lx, ly;
        if (!fetch_tile(ctx, x, y, &t, &lx, &ly))
            return;

        size_t idx = (size_t)ly * (size_t)t->width + (size_t)lx;
        t->energy[idx] = v;
        return;
    }

    if (!ctx->energyBuffer) return;
    if (x < 0 || y < 0 || x >= ctx->width || y >= ctx->height)
        return;

    ctx->energyBuffer[(size_t)y * (size_t)ctx->width + (size_t)x] = v;
}

void ClearEnergy(IntegratorEnergyContext* ctx)
{
    if (!ctx) return;

    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        for (size_t i = 0; i < ctx->tileGrid->count; i++) {
            IntegratorTile* t = &ctx->tileGrid->tiles[i];

            if (t->energy) {
                memset(t->energy, 0, (size_t)t->width * (size_t)t->height * sizeof(float));
            }
        }
        return;
    }

    if (ctx->energyBuffer) {
        size_t total = (size_t)ctx->width * (size_t)ctx->height;
        memset(ctx->energyBuffer, 0, total * sizeof(float));
    }
}

void AddEnergy(const IntegratorEnergyContext* ctx, int x, int y, float v)
{
    if (!ctx || v <= 0.0f) return;

    float cur = ReadEnergy(ctx, x, y);
    WriteEnergy(ctx, x, y, cur + v);
}

void BilateralBlurEnergy(IntegratorEnergyContext* ctx,
                         float spatialSigma,
                         float rangeSigma)
{
    if (!ctx || ctx->useTiles) return;
    if (!ctx->energyBuffer) return;

    int w = ctx->width;
    int h = ctx->height;
    size_t total = (size_t)w * (size_t)h;

    float* tmp = (float*)malloc(total * sizeof(float));
    if (!tmp) return;

    int radius = (int)fmax(1.0f, spatialSigma * 2.0f);
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
                    float rangeW  = expf(-(dr * dr) / twoRangeVar);

                    float wgt = spatialW * rangeW;
                    accum += neighbor * wgt;
                    weightSum += wgt;
                }
            }

            tmp[(size_t)y * (size_t)w + (size_t)x] =
                (weightSum > 1e-6f) ? (accum / weightSum) : center;
        }
    }

    memcpy(ctx->energyBuffer, tmp, total * sizeof(float));
    free(tmp);
}
