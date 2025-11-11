#include "render/irradiance_cache.h"
#include "render/fast_rng.h"
#include <math.h>
#include <stddef.h>

static double RandomUnit(FastRNG* rng) {
    return FastRNGNextDouble(rng);
}

static void Normalize(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-9) {
        *x /= len;
        *y /= len;
    }
}

static void FillObjectSlot(const SceneObject* obj,
                           const LightSource* light,
                           SurfaceIrradiance* slots,
                           int sampleCount) {
    FastRNG rng;
    FastRNGSeed(&rng, (uint64_t)(uintptr_t)obj ^ 0x1f123ULL, 0x991347ULL);
    for (int i = 0; i < sampleCount; i++) {
        double angle = (2.0 * M_PI) * ((double)i / (double)sampleCount);
        double dirX = cos(angle) + (RandomUnit(&rng) - 0.5) * 0.25;
        double dirY = sin(angle) + (RandomUnit(&rng) - 0.5) * 0.25;
        Normalize(&dirX, &dirY);
        double startX = obj->x;
        double startY = obj->y;
        double t = 0.0;
        double bestIntensity = 0.0;
        for (int step = 0; step < 40; step++) {
            double px = startX + dirX * t;
            double py = startY + dirY * t;
            double dx = light->x - px;
            double dy = light->y - py;
            double dist2 = dx * dx + dy * dy;
        if (dist2 < light->radius * light->radius) {
                bestIntensity = 1.0;
                break;
            }
            t += 8.0;
        }
        slots[i].intensity = bestIntensity;
        slots[i].dirX = dirX;
        slots[i].dirY = dirY;
    }
}

bool IrradianceCacheFill(IrradianceCache* cache,
                         const SceneObject* objects,
                         int objectCount,
                         const LightSource* light,
                         const UniformGrid* grid) {
    (void)grid;
    if (!cache || !cache->data) return false;
    for (int i = 0; i < objectCount; i++) {
        SurfaceIrradiance* slots = IrradianceCacheGet(cache, i, 0);
        if (!slots) continue;
        FillObjectSlot(&objects[i], light, slots, cache->samplesPerObject > 0 ? cache->samplesPerObject : 1);
    }
    return true;
}
