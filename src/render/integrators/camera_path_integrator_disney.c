#include "render/integrators/camera_path_integrator_disney.h"
#include "config/config_manager.h"
#include "render/ray_types.h"
#include "render/space_mode_adapter.h"
#include "camera/camera.h"
#include "render/timer_hud_api.h"
#include "material/material_manager.h"
#include "render/material_bsdf.h"
#include "render/light_pdf.h"

#include <SDL2/SDL.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define CAMERA_TONEMAP_GAMMA 0.55f
#define CAMERA_TONEMAP_EXPOSURE 0.6f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline void NormalizeVector(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-9) {
        *x /= len;
        *y /= len;
    }
}

static inline void BuildGroundBSDF(MaterialBSDF* bsdf) {
    if (!bsdf) return;
    memset(bsdf, 0, sizeof(*bsdf));
    bsdf->albedo = 1.0;
    bsdf->diffuseWeight = 1.0;
    bsdf->weightSum = 1.0;
    bsdf->roughness = 1.0;
    bsdf->model = MATERIAL_BSDF_LAMBERT;
}

static inline void CameraViewDirection(double rotation, double* vx, double* vy) {
    // In an orthographic camera, the view direction is screen -Y rotated by camera rotation.
    double c = cos(rotation);
    double s = sin(rotation);
    *vx = s;
    *vy = -c;
    NormalizeVector(vx, vy);
}

static inline double DisneyFalloffDistance(const IntegratorContext* ctx) {
    double d = animSettings.forwardDecay;
    if (d > 0.0) return d;
    double w = (ctx && ctx->width  > 0) ? (double)ctx->width  : (double)sceneSettings.windowWidth;
    double h = (ctx && ctx->height > 0) ? (double)ctx->height : (double)sceneSettings.windowHeight;
    if (w <= 0.0) w = 1200.0;
    if (h <= 0.0) h = 800.0;
    return hypot(w, h);
}

static inline double DisneyDistanceAttenuation(const IntegratorContext* ctx, double distance) {
    int mode = animSettings.forwardFalloffMode;
    if (mode == FORWARD_FALLOFF_MODE_NONE) return 1.0;
    double scale = DisneyFalloffDistance(ctx) * fmax(animSettings.lightDecaySoftness, 0.1);
    if (scale < 1.0) scale = 1.0;
    double normalized = fmax(distance, 0.0) / scale;
    switch (mode) {
        case FORWARD_FALLOFF_MODE_LINEAR:
            return 1.0 / (1.0 + normalized);
        case FORWARD_FALLOFF_MODE_QUADRATIC:
        default:
            return 1.0 / (1.0 + normalized * normalized);
    }
}

// --- Tile/energy helpers ---
static bool FetchTileSample(const IntegratorContext* ctx,
                            int pixelX,
                            int pixelY,
                            IntegratorTile** outTile,
                            int* outLocalX,
                            int* outLocalY) {
    if (!ctx || !ctx->tileGrid || !ctx->tileGrid->tiles) return false;
    if (pixelX < 0 || pixelY < 0 || pixelX >= ctx->width || pixelY >= ctx->height) return false;
    TileGrid* grid = ctx->tileGrid;
    int tileSize = grid->tileSize > 0 ? grid->tileSize : 1;
    int tileX = pixelX / tileSize;
    int tileY = pixelY / tileSize;
    if (tileX < 0 || tileY < 0 || tileX >= grid->tilesX || tileY >= grid->tilesY) return false;
    size_t index = (size_t)tileY * (size_t)grid->tilesX + (size_t)tileX;
    if (index >= grid->count) return false;
    IntegratorTile* tile = &grid->tiles[index];
    if (!tile->energy) return false;
    int localX = pixelX - tile->originX;
    int localY = pixelY - tile->originY;
    if (localX < 0 || localY < 0 || localX >= tile->width || localY >= tile->height) return false;
    if (outTile) *outTile = tile;
    if (outLocalX) *outLocalX = localX;
    if (outLocalY) *outLocalY = localY;
    return true;
}

static float ReadEnergySample(const IntegratorContext* ctx, int pixelX, int pixelY) {
    if (!ctx) return 0.0f;
    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        IntegratorTile* tile = NULL;
        int localX = 0, localY = 0;
        if (!FetchTileSample(ctx, pixelX, pixelY, &tile, &localX, &localY)) return 0.0f;
        if (!tile->energy) return 0.0f;
        size_t idx = (size_t)localY * (size_t)tile->width + (size_t)localX;
        if (idx >= (size_t)(tile->width * tile->height)) return 0.0f;
        return tile->energy[idx];
    }
    if (ctx->energyBuffer) {
        if (pixelX < 0 || pixelY < 0 || pixelX >= ctx->width || pixelY >= ctx->height) return 0.0f;
        size_t idx = (size_t)pixelY * (size_t)ctx->width + (size_t)pixelX;
        return ctx->energyBuffer[idx];
    }
    return 0.0f;
}

static void WriteEnergySample(const IntegratorContext* ctx, int pixelX, int pixelY, float value) {
    if (!ctx) return;
    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        IntegratorTile* tile = NULL;
        int lx = 0, ly = 0;
        if (!FetchTileSample(ctx, pixelX, pixelY, &tile, &lx, &ly)) return;
        if (!tile->energy) return;
        size_t idx = (size_t)ly * (size_t)tile->width + (size_t)lx;
        if (idx >= (size_t)(tile->width * tile->height)) return;
        tile->energy[idx] = value;
    } else if (ctx->energyBuffer) {
        if (pixelX < 0 || pixelY < 0 || pixelX >= ctx->width || pixelY >= ctx->height) return;
        size_t idx = (size_t)pixelY * (size_t)ctx->width + (size_t)pixelX;
        ctx->energyBuffer[idx] = value;
    }
}

static void ClearEnergyBuffer(IntegratorContext* ctx) {
    if (!ctx) return;
    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        for (size_t i = 0; i < ctx->tileGrid->count; i++) {
            IntegratorTile* tile = &ctx->tileGrid->tiles[i];
            if (tile->energy) memset(tile->energy, 0, (size_t)(tile->width * tile->height) * sizeof(float));
        }
    } else if (ctx->energyBuffer) {
        size_t total = (size_t)ctx->width * (size_t)ctx->height;
        memset(ctx->energyBuffer, 0, total * sizeof(float));
    }
}

static inline float TonemapEnergy(float energy) {
    float mapped = 1.0f - expf(-energy * CAMERA_TONEMAP_EXPOSURE);
    return powf(Clamp01(mapped), CAMERA_TONEMAP_GAMMA);
}

static void TonemapTiles(const IntegratorContext* ctx) {
    if (!ctx->tileGrid || !ctx->tileGrid->tiles || !ctx->pixelBuffer) return;
    for (size_t t = 0; t < ctx->tileGrid->count; t++) {
        const IntegratorTile* tile = &ctx->tileGrid->tiles[t];
        if (!tile->energy) continue;
        for (int y = 0; y < tile->height; y++) {
            for (int x = 0; x < tile->width; x++) {
                size_t idx = (size_t)y * (size_t)tile->width + (size_t)x;
                float tone = TonemapEnergy(tile->energy[idx]);
                int px = tile->originX + x;
                int py = tile->originY + y;
                if (px < 0 || py < 0 || px >= ctx->width || py >= ctx->height) continue;
                size_t outIdx = (size_t)py * (size_t)ctx->width + (size_t)px;
                ctx->pixelBuffer[outIdx] = (Uint8)Clamp(tone * 255.0f, 0, 255);
            }
        }
    }
}

static void AccumulateEnergyAdd(const IntegratorContext* ctx,
                                int pixelX,
                                int pixelY,
                                float value) {
    if (!ctx || value <= 0.0f) return;
    float current = ReadEnergySample(ctx, pixelX, pixelY);
    WriteEnergySample(ctx, pixelX, pixelY, current + value);
}

// Exposure utilities (local to camera integrator)
typedef struct {
    double sum;
    double maxValue;
    size_t samples;
} EnergyStats;

static inline void EnergyStatsAccumulate(EnergyStats* stats, float value) {
    if (!stats || value <= 0.0f) return;
    stats->sum += value;
    stats->samples++;
    if (value > stats->maxValue) stats->maxValue = value;
}

static double ComputeExposureScale(const EnergyStats* stats) {
    if (!stats || stats->samples == 0) return 1.0;
    double mean = stats->sum / (double)stats->samples;
    double scale = (mean > 1e-6) ? (1.0 / (mean * 4.0)) : 1.0;
    if (stats->maxValue > mean * 8.0) {
        double alt = 1.0 / (stats->maxValue * 0.5);
        scale = fmax(scale, alt);
    }
    if (scale <= 0.0) scale = 1.0;
    return scale;
}

static void ApplyExposureBuffer(IntegratorContext* ctx) {
    if (!ctx || !ctx->energyBuffer) return;
    EnergyStats stats = {0};
    size_t total = (size_t)ctx->width * (size_t)ctx->height;
    for (size_t i = 0; i < total; i++) {
        EnergyStatsAccumulate(&stats, ctx->energyBuffer[i]);
    }
    double scale = ComputeExposureScale(&stats);
    if (fabs(scale - 1.0) < 1e-6) return;
    for (size_t i = 0; i < total; i++) {
        ctx->energyBuffer[i] = (float)(ctx->energyBuffer[i] * scale);
    }
}

static void ApplyExposureTiles(IntegratorContext* ctx) {
    if (!ctx || !ctx->tileGrid || !ctx->tileGrid->tiles) return;
    EnergyStats stats = {0};
    for (size_t t = 0; t < ctx->tileGrid->count; t++) {
        const IntegratorTile* tile = &ctx->tileGrid->tiles[t];
        if (!tile->energy) continue;
        size_t total = (size_t)tile->width * (size_t)tile->height;
        for (size_t i = 0; i < total; i++) {
            EnergyStatsAccumulate(&stats, tile->energy[i]);
        }
    }
    double scale = ComputeExposureScale(&stats);
    if (fabs(scale - 1.0) < 1e-6) return;
    for (size_t t = 0; t < ctx->tileGrid->count; t++) {
        IntegratorTile* tile = &ctx->tileGrid->tiles[t];
        if (!tile->energy) continue;
        size_t total = (size_t)tile->width * (size_t)tile->height;
        for (size_t i = 0; i < total; i++) {
            tile->energy[i] = (float)(tile->energy[i] * scale);
        }
    }
}

static inline const MaterialBSDF* GetMaterial(const IntegratorContext* ctx,
                                              int objectIndex,
                                              MaterialBSDF* scratch,
                                              const SceneObject* obj) {
    if (ctx && ctx->materials && objectIndex >= 0 && objectIndex < ctx->materialCount) {
        return &ctx->materials[objectIndex];
    }
    if (scratch && obj) {
        MaterialBSDFInitFromSceneObject(obj, scratch);
        return scratch;
    }
    return NULL;
}

static bool TraceRayToSurface(const IntegratorContext* ctx,
                              double ox, double oy,
                              double dx, double dy,
                              HitInfo2D* outHit,
                              const SceneObject** outObj) {
    if (!ctx || !ctx->uniformGrid) return false;
    double len = sqrt(dx*dx + dy*dy);
    if (len <= GRID_EPSILON) return false;
    dx /= len; dy /= len;
    Ray2D ray = SpaceModeAdapter_MakeOffsetRay(ox, oy, dx, dy, PATH_EPSILON);
    HitInfo2D h;
    SpaceModeAdapter_ResetHit(&h);
    if (!UniformGridTraceRay(ctx->uniformGrid, &ray, PATH_EPSILON, DBL_MAX, &h)) return false;
    if (outHit) *outHit = h;
    if (outObj && ctx->objects && h.objectIndex >= 0 && h.objectIndex < ctx->objectCount) {
        *outObj = &ctx->objects[h.objectIndex];
    }
    return true;
}

static inline void OrientNormalForIncoming(HitInfo2D* hit, double inDirX, double inDirY) {
    if (!hit) return;
    NormalizeVector(&inDirX, &inDirY);
    double dot = inDirX * hit->nx + inDirY * hit->ny;
    if (dot < 0.0) {
        hit->nx = -hit->nx;
        hit->ny = -hit->ny;
    }
}

static inline void SampleDisk(FastRNG* rng, double* dx, double* dy) {
    double u1 = FastRNGNextDouble(rng);
    double u2 = FastRNGNextDouble(rng);
    double r = sqrt(u1);
    double theta = 2.0 * M_PI * u2;
    *dx = r * cos(theta);
    *dy = r * sin(theta);
}

static double SampleDirectLight(const IntegratorContext* ctx,
                                const LightSource* light,
                                FastRNG* rng,
                                const HitInfo2D* hit,
                                const MaterialBSDF* bsdf,
                                double inDirX,
                                double inDirY,
                                double normalZ,
                                double lightHeight) {
    if (!ctx || !light || !rng || !hit || !bsdf) return 0.0;
    int directSamples = 8;
    double accum = 0.0;
    for (int i = 0; i < directSamples; i++) {
        double lx, ly;
        SampleDisk(rng, &lx, &ly);
        lx *= light->radius;
        ly *= light->radius;
        double lpX = light->x + lx;
        double lpY = light->y + ly;
        double dirX = lpX - hit->px;
        double dirY = lpY - hit->py;
        double distXY2 = dirX * dirX + dirY * dirY;
        double dist = sqrt(distXY2 + lightHeight * lightHeight);
        if (dist <= PATH_EPSILON) continue;
        dirX /= sqrt(distXY2 > 1e-12 ? distXY2 : 1.0);
        dirY /= sqrt(distXY2 > 1e-12 ? distXY2 : 1.0);
        double dirZ = lightHeight / dist;

        Ray2D shadow = SpaceModeAdapter_MakeOffsetRay(hit->px, hit->py, hit->nx, hit->ny, PATH_EPSILON);
        shadow.dx = dirX;
        shadow.dy = dirY;
        HitInfo2D block;
        SpaceModeAdapter_ResetHit(&block);
        if (UniformGridTraceRay(ctx->uniformGrid, &shadow, PATH_EPSILON, dist - PATH_EPSILON, &block)) {
            continue;
        }

        double cosOn = fmax(0.0, hit->nx * dirX + hit->ny * dirY + normalZ * dirZ);
        if (cosOn <= 0.0) continue;

        double pdfLight = CircleLightPdfSolidAngle(light, hit->px, hit->py, lightHeight);
        double area = M_PI * light->radius * light->radius;
        double att = DisneyDistanceAttenuation(ctx, dist);
        double radiance = animSettings.lightIntensity * area * att;

        double inX = -dirX;
        double inY = -dirY;
        double outX = inDirX;
        double outY = inDirY;
        double bsdfVal = MaterialBSDFEvaluateCos3(bsdf,
                                                  hit->nx, hit->ny, normalZ,
                                                  inX, inY, dirZ,
                                                  outX, outY, normalZ * -1.0);
        if (bsdfVal <= 0.0) continue;
        double pdfBsdf = MaterialBSDFAngularPdf3(bsdf,
                                                 hit->nx, hit->ny, normalZ,
                                                 inDirX, inDirY, normalZ * -1.0,
                                                 dirX, dirY, dirZ);
        double misW = (pdfBsdf > 0.0 && pdfLight > 0.0)
            ? (pdfLight / (pdfLight + pdfBsdf))
            : 1.0;
        double contrib = radiance * bsdfVal * cosOn / fmax(pdfLight, 1e-8);
        accum += contrib * misW;
    }
    if (directSamples > 0) accum /= (double)directSamples;
    return accum;
}

static double PathTracePixel(const IntegratorContext* ctx,
                             const LightSource* light,
                             int px,
                             int py,
                             FastRNG* rng) {
    if (!ctx || !ctx->uniformGrid) return 0.0;
    const bool topDownGroundShading = true;
    int maxDepth = (animSettings.pathMaxDepth > 0) ? animSettings.pathMaxDepth : 4;
    int minDepth = 2;
    double throughput = 1.0;
    double radiance = 0.0;
    double envLight = (animation_config_environment_light_mode_clamp(
                           animSettings.environmentLightMode) == ENVIRONMENT_LIGHT_MODE_AMBIENT &&
                       animSettings.environmentBrightness > 0.0)
                          ? fmin(animSettings.environmentBrightness, 255.0) / 255.0
                          : 0.0;

    double jitterX = FastRNGNextDouble(rng) - 0.5;
    double jitterY = FastRNGNextDouble(rng) - 0.5;
    SpaceModeViewContext view_ctx = SpaceModeAdapter_BuildViewContext(&sceneSettings.camera,
                                                                       ctx->width,
                                                                       ctx->height);
    CameraPoint world = SpaceModeAdapter_ScreenToWorld(&view_ctx,
                                                       px + 0.5 + jitterX,
                                                       py + 0.5 + jitterY);
    double lightHeight = (animSettings.lightHeight > 0.0) ? animSettings.lightHeight : 8.0;
    double viewDirX = 0.0, viewDirY = -1.0;
    CameraViewDirection(sceneSettings.camera.rotation, &viewDirX, &viewDirY);

    // Top-down shading: start at the pixel's world position.
    double ox = world.x;
    double oy = world.y;
    double dx = viewDirX;
    double dy = viewDirY;

    bool useSyntheticGround = topDownGroundShading;
    if (!topDownGroundShading) {
        // Pinhole fallback: origin at camera, direction through pixel
        ox = sceneSettings.camera.x;
        oy = sceneSettings.camera.y;
        dx = world.x - ox;
        dy = world.y - oy;
        NormalizeVector(&dx, &dy);
        if (fabs(dx) < 1e-6 && fabs(dy) < 1e-6) { dx = 0.0; dy = 1.0; }
        useSyntheticGround = false;
    }

    for (int depth = 0; depth < maxDepth; depth++) {
        HitInfo2D hit;
        SpaceModeAdapter_ResetHit(&hit);
        const SceneObject* obj = NULL;
        MaterialBSDF matScratch = {0};
        const MaterialBSDF* bsdf = NULL;

        if (useSyntheticGround && depth == 0) {
            hit.px = ox;
            hit.py = oy;
            // Fixed ground normal pointing up; view is straight down. Use planar up (0,1) for sampling.
            hit.nx = 0.0;
            hit.ny = 1.0;
            BuildGroundBSDF(&matScratch);
            bsdf = &matScratch;
        } else {
            if (!TraceRayToSurface(ctx, ox, oy, dx, dy, &hit, &obj)) {
                if (envLight > 0.0) {
                    radiance += throughput * envLight;
                }
                break;
            }
            bsdf = GetMaterial(ctx, hit.objectIndex, &matScratch, obj);
            if (!bsdf) break;
        }

        double inDirX = -dx;
        double inDirY = -dy;
        double inDirZ = 0.0;
        double normalZ = (useSyntheticGround && depth == 0) ? 1.0 : 0.0;
        if (!useSyntheticGround || depth != 0) {
            OrientNormalForIncoming(&hit, inDirX, inDirY);
        }

        // Emissive hit (one-sided)
        double ndotI = fmax(0.0, -(inDirX * hit.nx + inDirY * hit.ny + normalZ * inDirZ));
        if (bsdf->emissive > 0.0 && ndotI > 0.0) {
            radiance += throughput * bsdf->emissive;
        }

        double direct = SampleDirectLight(ctx, light, rng, &hit, bsdf, inDirX, inDirY, normalZ, lightHeight);
        if (direct > 0.0) {
            radiance += throughput * direct;
        }

        BSDFSample s = {0};
        if (!MaterialBSDFSample(bsdf, hit.nx, hit.ny, inDirX, inDirY, inDirZ, rng, &s)) {
            break;
        }
        if (s.pdf <= 1e-8 || s.weight <= 0.0) break;

        double Tprev = throughput;
        double bsdfThroughput = s.weight / s.pdf;

        // MIS for BSDF-sampled hit on light (point-in-disk test)
        if (light) {
            double lx = light->x - hit.px;
            double ly = light->y - hit.py;
            double t = lx * s.dirX + ly * s.dirY;
            if (t > PATH_EPSILON) {
                double projX = hit.px + s.dirX * t;
                double projY = hit.py + s.dirY * t;
                double dxL = projX - light->x;
                double dyL = projY - light->y;
                double d2 = dxL * dxL + dyL * dyL;
                if (d2 <= light->radius * light->radius) {
                    double dist2 = t * t;
                    double dist3 = sqrt(dist2 + lightHeight * lightHeight);
                    double cosOn = fmax(0.0, hit.nx * s.dirX + hit.ny * s.dirY + normalZ * s.dirZ);
                    if (cosOn > 0.0) {
                        Ray2D shadow = SpaceModeAdapter_MakeOffsetRay(hit.px, hit.py, hit.nx, hit.ny, PATH_EPSILON);
                        shadow.dx = s.dirX;
                        shadow.dy = s.dirY;
                        HitInfo2D block;
                        SpaceModeAdapter_ResetHit(&block);
                        if (!UniformGridTraceRay(ctx->uniformGrid, &shadow, PATH_EPSILON, t - PATH_EPSILON, &block)) {
                            double area = M_PI * light->radius * light->radius;
                            double att = DisneyDistanceAttenuation(ctx, dist3);
                            double radianceL = animSettings.lightIntensity * area * att;
                            double pdfLight = CircleLightPdfSolidAngle(light, hit.px, hit.py, lightHeight);
                            double pdfBsdf = s.pdf;
                            double misW = (pdfLight > 0.0 && pdfBsdf > 0.0)
                                ? (pdfBsdf / (pdfLight + pdfBsdf))
                                : 1.0;
                            radiance += Tprev * bsdfThroughput * radianceL * misW;
                        }
                    }
                }
            }
        }

        throughput = Tprev * bsdfThroughput;
        // Optional clamp can be re-enabled for debugging extreme fireflies.
        if (throughput <= 0.0) break;

        if (animSettings.pathRussianRoulette && depth + 1 >= minDepth) {
            double p = fmax(0.05, fmin(1.0, throughput));
            double r = FastRNGNextDouble(rng);
            if (r > p) break;
            throughput /= p;
        }

        ox = hit.px + hit.nx * PATH_EPSILON;
        oy = hit.py + hit.ny * PATH_EPSILON;
        dx = s.dirX;
        dy = s.dirY;
        NormalizeVector(&dx, &dy);
    }

    return radiance;
}

void CameraPathIntegratorRenderDisney(IntegratorContext* ctx, const LightSource* light) {
    if (!ctx) return;
    bool usingTiles = (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles);
    bool usingBuffer = (!usingTiles && ctx->energyBuffer);
    if (!usingTiles && !usingBuffer) return;

    ts_session_start_timer(timer_hud_session(), "CameraPath Clear");
    ClearEnergyBuffer(ctx);
    ts_session_stop_timer(timer_hud_session(), "CameraPath Clear");

    int spp = (animSettings.pathSamplesPerPixel > 0) ? animSettings.pathSamplesPerPixel : 1;
    if (spp < 4) spp = 4; // safety baseline to reduce structured artifacts

    ts_session_start_timer(timer_hud_session(), "CameraPath Trace");
    for (int y = 0; y < ctx->height; y++) {
        for (int x = 0; x < ctx->width; x++) {
            double pixelEnergy = 0.0;
            for (int s = 0; s < spp; s++) {
                FastRNG rng;
                SeedPixelRNG(&rng, ctx->frameSeed, x, y, (uint32_t)s);
                pixelEnergy += PathTracePixel(ctx, light, x, y, &rng);
            }
            pixelEnergy /= (double)spp;
            AccumulateEnergyAdd(ctx, x, y, (float)pixelEnergy);
        }
    }
    ts_session_stop_timer(timer_hud_session(), "CameraPath Trace");

    // Auto exposure to counter very dim outputs
    if (usingTiles) {
        ApplyExposureTiles(ctx);
    } else {
        ApplyExposureBuffer(ctx);
    }

    if (!ctx->pixelBuffer) return;
    ts_session_start_timer(timer_hud_session(), "CameraPath Tonemap");
    if (usingTiles) {
        TonemapTiles(ctx);
    } else if (ctx->energyBuffer) {
        size_t total = (size_t)ctx->width * (size_t)ctx->height;
        for (size_t i = 0; i < total; i++) {
            float tone = TonemapEnergy(ctx->energyBuffer[i]);
            ctx->pixelBuffer[i] = (Uint8)Clamp(tone * 255.0f, 0, 255);
        }
    } else {
        size_t total = (size_t)ctx->width * (size_t)ctx->height;
        memset(ctx->pixelBuffer, 0, total * sizeof(Uint8));
    }
    ts_session_stop_timer(timer_hud_session(), "CameraPath Tonemap");
}
