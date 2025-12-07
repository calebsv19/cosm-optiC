#include "render/integrators/direct_light_integrator.h"
#include "config/config_manager.h"
#include "render/ray_types.h"
#include "camera/camera.h"
#include "render/uniform_grid.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define DIRECT_TONEMAP_GAMMA 0.55f
#define DIRECT_TONEMAP_EXPOSURE 0.6f

static inline void NormalizeVec(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-9) {
        *x /= len;
        *y /= len;
    }
}

static bool HasDirectLineOfSight(const IntegratorContext* ctx,
                                 double worldX,
                                 double worldY,
                                 const LightSource* light) {
    if (!ctx || !light) return false;
    double dx = light->x - worldX;
    double dy = light->y - worldY;
    double dist = hypot(dx, dy);
    if (dist <= PATH_EPSILON) {
        return true;
    }
    NormalizeVec(&dx, &dy);
    double originX = worldX + dx * PATH_EPSILON;
    double originY = worldY + dy * PATH_EPSILON;
    double maxDist = fmax(dist - 2.0 * PATH_EPSILON, PATH_EPSILON);
    if (!ctx->uniformGrid) {
        return true;
    }
    Ray2D ray = { originX, originY, dx, dy };
    HitInfo2D hit = {0};
    return !UniformGridTraceRay(ctx->uniformGrid, &ray, PATH_EPSILON, maxDist, &hit);
}

static float ComputeDirectRadiance(const LightSource* light,
                                   double worldX,
                                   double worldY) {
    if (!light) return 0.0f;
    double dx = light->x - worldX;
    double dy = light->y - worldY;
    double dist = hypot(dx, dy);

    // Distance scale (falloff distance) and softness from settings
    double falloffScale = animSettings.forwardDecay;
    if (falloffScale <= 0.0) {
        double w = (double)sceneSettings.windowWidth;
        double h = (double)sceneSettings.windowHeight;
        if (w <= 0.0) w = 1200.0;
        if (h <= 0.0) h = 800.0;
        falloffScale = hypot(w, h);
    }
    double softness = fmax(animSettings.lightDecaySoftness, 0.1);
    double scale = falloffScale * softness;
    if (scale < 1.0) scale = 1.0;

    // Pure attenuation modes:
    // NONE: constant (clamped by radius)
    // LINEAR: 1 / (1 + d/scale)
    // QUADRATIC: 1 / (1 + (d/scale)^2)
    double normalized = dist / scale;
    double att = 1.0;
    switch (animSettings.forwardFalloffMode) {
        case FORWARD_FALLOFF_MODE_LINEAR:
            att = 1.0 / (1.0 + normalized);
            break;
        case FORWARD_FALLOFF_MODE_NONE:
            att = 1.0;
            break;
        case FORWARD_FALLOFF_MODE_QUADRATIC:
        default:
            att = 1.0 / (1.0 + normalized * normalized);
            break;
    }

    // Clamp near-field to avoid singularities
    if (att < 0.0) att = 0.0;

    // Simpler energy: base intensity scaled by attenuation; omit huge area term to prevent white-out
    double radiance = animSettings.lightIntensity * att;
    return (float)radiance;
}

static inline float TonemapEnergy(float energy) {
    float mapped = 1.0f - expf(-energy * DIRECT_TONEMAP_EXPOSURE);
    return powf(fmaxf(0.0f, fminf(1.0f, mapped)), DIRECT_TONEMAP_GAMMA);
}

void DirectLightIntegratorRender(IntegratorContext* ctx, const LightSource* light) {
    if (!ctx || !ctx->pixelBuffer || !light) return;

    size_t total = (size_t)ctx->width * (size_t)ctx->height;
    if (ctx->energyBuffer) {
        memset(ctx->energyBuffer, 0, total * sizeof(float));
    }
    memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));

    for (int y = 0; y < ctx->height; y++) {
        for (int x = 0; x < ctx->width; x++) {
            CameraPoint world = CameraScreenToWorld(&sceneSettings.camera,
                                                    x + 0.5,
                                                    y + 0.5,
                                                    ctx->width,
                                                    ctx->height);
            double lx = light->x - world.x;
            double ly = light->y - world.y;
            NormalizeVec(&lx, &ly);
            if (!HasDirectLineOfSight(ctx, world.x, world.y, light)) {
                continue;
            }
            float rad = ComputeDirectRadiance(light, world.x, world.y);
            if (ctx->energyBuffer) {
                ctx->energyBuffer[(size_t)y * (size_t)ctx->width + (size_t)x] = rad;
            }
            Uint8 tone = (Uint8)fmaxf(0.0f, fminf(255.0f, TonemapEnergy(rad) * 255.0f));
            ctx->pixelBuffer[(size_t)y * (size_t)ctx->width + (size_t)x] = tone;
        }
    }
}
