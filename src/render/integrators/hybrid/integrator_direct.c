#include "render/integrators/hybrid/integrator_direct.h"
#include "render/integrators/hybrid/integrator_visibility.h"
#include "render/integrators/hybrid/integrator_energy.h"
#include "render/material_bsdf.h"
#include "config/config_manager.h"
#include "camera/camera.h"
#include "render/space_mode_adapter.h"
#include <math.h>

#ifndef PATH_EPSILON
#define PATH_EPSILON 0.0001
#endif

// Share the forward attenuation controls so UI toggles (none/linear/quad)
// apply to the hybrid/camera path direct term too.
static double ForwardFalloffDistance(const IntegratorDirectContext* ctx) {
    double d = animSettings.forwardDecay;
    if (d > 0.0) return d;

    double w = (ctx && ctx->width  > 0) ? (double)ctx->width  : (double)sceneSettings.windowWidth;
    double h = (ctx && ctx->height > 0) ? (double)ctx->height : (double)sceneSettings.windowHeight;
    if (w <= 0.0) w = 1200.0;
    if (h <= 0.0) h = 800.0;
    return hypot(w, h);
}

static double ForwardDistanceAttenuation(double distance, double scale, int mode) {
    if (mode == FORWARD_FALLOFF_MODE_NONE || scale <= 0.0) return 1.0;

    double safeScale  = fmax(scale, 1.0);
    double normalized = fmax(distance, 0.0) / safeScale;

    if (mode == FORWARD_FALLOFF_MODE_LINEAR) {
        return 1.0 / (1.0 + normalized);
    }
    double denom = 1.0 + normalized * normalized;
    return 1.0 / denom;
}

double DirectComputeRadiance(const LightSource* light,
                             double worldX,
                             double worldY,
                             double intensityScale)
{
    if (!light) return 0.0;

    double dx = light->x - worldX;
    double dy = light->y - worldY;
    double dist = sqrt(dx*dx + dy*dy);

    double falloffScale = ForwardFalloffDistance(NULL);
    int falloffMode = animSettings.forwardFalloffMode;
    double softness = fmax(animSettings.lightDecaySoftness, 0.1);
    double att = ForwardDistanceAttenuation(dist, falloffScale * softness, falloffMode);

    // Emphasize separation between modes: quadratic gets a steeper drop
    if (falloffMode == FORWARD_FALLOFF_MODE_QUADRATIC) {
        att *= att;
    }

    double base = intensityScale * att;
    return base > 0.0 ? base : 0.0;
}

float DirectEvaluate(const HitInfo2D* hit,
                     const LightSource* light,
                     const MaterialBSDF* mat,
                     double camX, double camY,
                     double intensityScale)
{
    if (!hit || !light) return 0.0f;

    double lx = light->x - hit->px;
    double ly = light->y - hit->py;

    double lLen = sqrt(lx*lx + ly*ly);
    if (lLen < PATH_EPSILON) return 0.0f;

    lx /= lLen;
    ly /= lLen;

    double vx = camX - hit->px;
    double vy = camY - hit->py;
    double vLen = sqrt(vx*vx + vy*vy);
    if (vLen < PATH_EPSILON) return 0.0f;

    vx /= vLen;
    vy /= vLen;

    double cosNL = lx*hit->nx + ly*hit->ny;
    if (cosNL <= 0.0) return 0.0f;

    double bsdfTerm = 1.0;
    if (mat) {
        bsdfTerm = MaterialBSDFEvaluateCos(mat,
                                           hit->nx, hit->ny,
                                           lx, ly,
                                           vx, vy);
    }

    if (bsdfTerm <= 0.0) return 0.0f;

    double radiance = DirectComputeRadiance(light, hit->px, hit->py, intensityScale);
    return (float)(radiance * bsdfTerm);
}

void DirectLightingPass(IntegratorDirectContext* ctx,
                        const LightSource* light,
                        double camX, double camY,
                        double intensityScale)
{
    DirectLightingPassRegion(ctx, light, camX, camY, intensityScale,
                             0, 0, ctx ? ctx->width : 0, ctx ? ctx->height : 0);
}

void DirectLightingPassRegion(IntegratorDirectContext* ctx,
                              const LightSource* light,
                              double camX, double camY,
                              double intensityScale,
                              int startX, int startY,
                              int endX, int endY)
{
    if (!ctx || !light) return;
    (void)camX;
    (void)camY;

    int minX = startX < 0 ? 0 : startX;
    int minY = startY < 0 ? 0 : startY;
    int maxX = endX > ctx->width ? ctx->width : endX;
    int maxY = endY > ctx->height ? ctx->height : endY;
    if (minX >= maxX || minY >= maxY) return;

    IntegratorEnergyContext ectx = {
        .width = ctx->width,
        .height = ctx->height,
        .useTiles = ctx->useTiles,
        .tileGrid = ctx->tileGrid,
        .energyBuffer = ctx->energyBuffer
    };
    SpaceModeViewContext view_ctx = SpaceModeAdapter_BuildViewContext(&sceneSettings.camera,
                                                                       ctx->width,
                                                                       ctx->height);

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            CameraPoint world = SpaceModeAdapter_ScreenToWorld(&view_ctx,
                                                                x + 0.5,
                                                                y + 0.5);
            double wx = world.x;
            double wy = world.y;

            if (!HasDirectLineOfSight(ctx->grid, wx, wy, light))
                continue;

            float rad = (float)DirectComputeRadiance(light, wx, wy, intensityScale);
            if (rad > 0.0f)
                AddEnergy(&ectx, x, y, rad);
        }
    }
}
