#include "render/integrators/hybrid/integrator_tonemap.h"
#include <math.h>
#include <string.h>

static inline float clamp01(float x) {
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

float TonemapCurve(float x)
{
    if (x <= 0.0f) return 0.0f;

    float m = x / (1.0f + x);
    m = powf(clamp01(m), 0.55f);
    return m;
}

void TonemapTiles(TonemapContext* ctx)
{
    if (!ctx || !ctx->tiles || !ctx->pixelBuffer)
        return;

    TileGrid* g = ctx->tiles;

    for (size_t ti = 0; ti < g->count; ti++) {
        IntegratorTile* t = &g->tiles[ti];
        if (!t->energy) continue;

        for (int y = 0; y < t->height; y++) {
            for (int x = 0; x < t->width; x++) {

                size_t idx = (size_t)y * (size_t)t->width + (size_t)x;
                float e = t->energy[idx];

                float tone = TonemapCurve(e);
                int px = t->originX + x;
                int py = t->originY + y;

                if (px < 0 || py < 0 || px >= ctx->width || py >= ctx->height)
                    continue;

                size_t outIdx = (size_t)py * (size_t)ctx->width + (size_t)px;
                ctx->pixelBuffer[outIdx] = (unsigned char)(tone * 255.0f);
            }
        }
    }
}

void TonemapTile(TonemapContext* ctx, const IntegratorTile* tile)
{
    if (!ctx || !tile || !tile->energy || !ctx->pixelBuffer)
        return;

    for (int y = 0; y < tile->height; y++) {
        for (int x = 0; x < tile->width; x++) {
            size_t idx = (size_t)y * (size_t)tile->width + (size_t)x;
            float e = tile->energy[idx];

            float tone = TonemapCurve(e);
            int px = tile->originX + x;
            int py = tile->originY + y;

            if (px < 0 || py < 0 || px >= ctx->width || py >= ctx->height)
                continue;

            size_t outIdx = (size_t)py * (size_t)ctx->width + (size_t)px;
            ctx->pixelBuffer[outIdx] = (unsigned char)(tone * 255.0f);
        }
    }
}

void TonemapApply(TonemapContext* ctx)
{
    if (!ctx || !ctx->pixelBuffer)
        return;

    if (ctx->useTiles && ctx->tiles) {
        TonemapTiles(ctx);
        return;
    }

    if (!ctx->energyBuffer)
        return;

    size_t total = (size_t)ctx->width * (size_t)ctx->height;

    for (size_t i = 0; i < total; i++) {
        float tone = TonemapCurve(ctx->energyBuffer[i]);
        ctx->pixelBuffer[i] = (unsigned char)(tone * 255.0f);
    }
}
