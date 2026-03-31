#include "render/integrators/forward_light_integrator.h"
#include "config/config_manager.h"
#include "render/fast_rng.h"
#include "render/ray_types.h"
#include "render/space_mode_adapter.h"
#include "render/material_bsdf.h"
#include "camera/camera.h"
#include <SDL2/SDL.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

// -----------------------------------------
// Tunables (kept same unless needed for fix)
// -----------------------------------------
#define MAX_BOUNCES 3
#define MIN_ENERGY 0.0001
#define FORWARD_TONEMAP_GAMMA 0.5f
#define FORWARD_STEP 0.75
#define FORWARD_MAX_TRAVEL 6000.0
#define FORWARD_PRIMARY_SCALE 2.0
#define FORWARD_SECONDARY_SCALE 0.6
#define FORWARD_DEBUG_REFLECTIONS 0

// Reference triangle area for modest per-hit scaling
static const double TRIANGLE_AREA_REFERENCE = 80.0;

// -----------------------------------------
// Thread payload
// -----------------------------------------
typedef struct {
    double sourceX, sourceY;
    int rayStart, rayEnd;
    const IntegratorContext* ctx;
    FastRNG rng;
} ThreadData;

// -----------------------------------------
// Energy stats for auto exposure
// -----------------------------------------
typedef struct {
    double sum;
    double maxValue;
    size_t samples;
} EnergyStats;

// ---- Protos
static double ForwardFalloffDistance(const IntegratorContext* ctx);
static double ForwardDistanceAttenuation(double distance, double scale, int mode);
static void   EnergyStatsAccumulate(EnergyStats* stats, float value);
static float  EnergyStatsExposure(const EnergyStats* stats);
static float  ComputeTileExposure(const TileGrid* grid);
static float  ComputeBufferExposure(const IntegratorContext* ctx);
static Uint8  ForwardEnergyToPixel(float energy, float exposure);

// -----------------------------------------
// Helpers
// -----------------------------------------
static inline void NormalizeVec(double* x, double* y) {
    double len = sqrt((*x) * (*x) + (*y) * (*y));
    if (len > 1e-9) {
        *x /= len;
        *y /= len;
    }
}

static inline void OrientNormalForIncoming(const HitInfo2D* hit,
                                           double inDirX,
                                           double inDirY,
                                           double* outNx,
                                           double* outNy) {
    double nx = hit ? hit->nx : 0.0;
    double ny = hit ? hit->ny : 1.0;
    NormalizeVec(&inDirX, &inDirY);
    if ((nx * inDirX + ny * inDirY) < 0.0) {
        nx = -nx;
        ny = -ny;
    }
    if (outNx) *outNx = nx;
    if (outNy) *outNy = ny;
}

static inline const MaterialBSDF* GetMaterial(const IntegratorContext* ctx, int objectIndex) {
    if (!ctx || !ctx->materials) return NULL;
    if (objectIndex < 0 || objectIndex >= ctx->materialCount) return NULL;
    return &ctx->materials[objectIndex];
}

static inline double Cross2(double ax, double ay, double bx, double by) {
    return ax * by - ay * bx;
}

static bool ComputeViewBounds(const IntegratorContext* ctx,
                              double* outMinX, double* outMinY,
                              double* outMaxX, double* outMaxY) {
    if (!ctx) return false;
    SpaceModeViewContext view_ctx = SpaceModeAdapter_BuildViewContext(&sceneSettings.camera,
                                                                       ctx->width,
                                                                       ctx->height);
    double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    bool initialised = false;
    for (int sx = 0; sx <= 1; sx++) {
        for (int sy = 0; sy <= 1; sy++) {
            double screenX = (sx == 0) ? 0.0 : (double)(ctx->width);
            double screenY = (sy == 0) ? 0.0 : (double)(ctx->height);
            CameraPoint world = SpaceModeAdapter_ScreenToWorld(&view_ctx, screenX, screenY);
            if (!initialised) {
                minX = maxX = world.x;
                minY = maxY = world.y;
                initialised = true;
            } else {
                if (world.x < minX) minX = world.x;
                if (world.x > maxX) maxX = world.x;
                if (world.y < minY) minY = world.y;
                if (world.y > maxY) maxY = world.y;
            }
        }
    }
    if (!initialised) {
        return false;
    }
    if (outMinX) *outMinX = minX;
    if (outMaxX) *outMaxX = maxX;
    if (outMinY) *outMinY = minY;
    if (outMaxY) *outMaxY = maxY;
    return true;
}

static double DistanceToSceneBounds(const IntegratorContext* ctx,
                                    double ox, double oy,
                                    double dx, double dy) {
    double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;
    bool haveView = ComputeViewBounds(ctx, &minX, &minY, &maxX, &maxY);
    if (!haveView) {
        minX = minY = -FORWARD_MAX_TRAVEL;
        maxX = maxY = FORWARD_MAX_TRAVEL;
    }
    if (ctx && ctx->uniformGrid) {
        const UniformGrid* grid = ctx->uniformGrid;
        if (grid->minX < minX) minX = grid->minX;
        if (grid->minY < minY) minY = grid->minY;
        if (grid->maxX > maxX) maxX = grid->maxX;
        if (grid->maxY > maxY) maxY = grid->maxY;
    }

    double invDx = (fabs(dx) < GRID_EPSILON) ? DBL_MAX : 1.0 / dx;
    double invDy = (fabs(dy) < GRID_EPSILON) ? DBL_MAX : 1.0 / dy;

    double tx1 = (minX - ox) * invDx;
    double tx2 = (maxX - ox) * invDx;
    double ty1 = (minY - oy) * invDy;
    double ty2 = (maxY - oy) * invDy;

    double tEnter = fmax(fmin(tx1, tx2), fmin(ty1, ty2));
    double tExit  = fmin(fmax(tx1, tx2), fmax(ty1, ty2));

    if (tExit < PATH_EPSILON || tExit < tEnter) {
        return FORWARD_MAX_TRAVEL;
    }
    double exitT = tExit;
    if (!isfinite(exitT) || exitT <= PATH_EPSILON) {
        exitT = FORWARD_MAX_TRAVEL;
    }
    return exitT;
}

// Trace to first surface (triangles via grid)
static bool TraceRayToSurface(const IntegratorContext* ctx,
                              double originX, double originY,
                              double dirX, double dirY,
                              HitInfo2D* hit, const SceneObject** outObj,
                              double maxDistance)
{
    if (!ctx || !ctx->uniformGrid) return false;

    double dx = dirX, dy = dirY;
    double len = sqrt(dx*dx + dy*dy);
    if (len <= GRID_EPSILON) return false;
    dx /= len; dy /= len;

    Ray2D ray = SpaceModeAdapter_MakeRay(originX, originY, dx, dy);
    HitInfo2D h;
    SpaceModeAdapter_ResetHit(&h);

    double tMin = PATH_EPSILON;
    double tMax = (maxDistance > 0.0) ? maxDistance : DBL_MAX;

    if (!UniformGridTraceRay(ctx->uniformGrid, &ray, tMin, tMax, &h)) {
        return false;
    }
    if (hit) *hit = h;
    if (outObj) *outObj = &ctx->objects[h.objectIndex];
    return true;
}

// Interpolate vertex normal; return gentle area scale
static double ApplyTriangleHitProperties(const IntegratorContext* ctx, HitInfo2D* hit) {
    if (!ctx || !ctx->triangleMesh || !hit || hit->triangleIndex < 0) return 1.0;

    const TriangleMesh* mesh = ctx->triangleMesh;
    if (hit->triangleIndex < 0 || hit->triangleIndex >= mesh->triangleCount) return 1.0;

    const TriangleFace*   f  = &mesh->triangles[hit->triangleIndex];
    const TriangleVertex* v0 = &mesh->vertices[f->v0];
    const TriangleVertex* v1 = &mesh->vertices[f->v1];
    const TriangleVertex* v2 = &mesh->vertices[f->v2];

    double area2 = Cross2(v1->x - v0->x, v1->y - v0->y, v2->x - v0->x, v2->y - v0->y);
    double area  = fabs(area2) * 0.5;
    double areaScale = Clamp(area / TRIANGLE_AREA_REFERENCE, 0.4, 2.5);

    double u = hit->baryU, v = hit->baryV, w = hit->baryW;
    double sum = u + v + w;
    if (!isfinite(sum) || fabs(sum) < 1e-6) {
        double denom = (v1->y - v2->y) * (v0->x - v2->x) + (v2->x - v1->x) * (v0->y - v2->y);
        if (fabs(denom) > 1e-6) {
            u = ((v1->y - v2->y) * (hit->px - v2->x) + (v2->x - v1->x) * (hit->py - v2->y)) / denom;
            v = ((v2->y - v0->y) * (hit->px - v2->x) + (v0->x - v2->x) * (hit->py - v2->y)) / denom;
            w = 1.0 - u - v;
        } else {
            u = v = 0.0; w = 1.0;
        }
    } else if (fabs(sum - 1.0) > 1e-3) {
        u /= sum; v /= sum; w /= sum;
    }
    hit->baryU = u; hit->baryV = v; hit->baryW = w;

    double nx = u*v0->nx + v*v1->nx + w*v2->nx;
    double ny = u*v0->ny + v*v1->ny + w*v2->ny;
    double nL = sqrt(nx*nx + ny*ny);
    if (nL > GRID_EPSILON) { nx /= nL; ny /= nL; }
    hit->nx = nx; hit->ny = ny;

    return areaScale;
}

// World→pixel & energy write (tiles or linear buffer)
static bool WorldToPixel(double wx, double wy,
                         int w, int h,
                         int* outIdx, int* sx, int* sy)
{
    SpaceModeViewContext view_ctx = SpaceModeAdapter_BuildViewContext(&sceneSettings.camera, w, h);
    CameraPoint sc = SpaceModeAdapter_WorldToScreen(&view_ctx, wx, wy);
    int ix = (int)lround(sc.x);
    int iy = (int)lround(sc.y);
    if (sx) *sx = ix; if (sy) *sy = iy;
    if (ix < 0 || ix >= w || iy < 0 || iy >= h) return false;
    if (outIdx) *outIdx = iy * w + ix;
    return true;
}

static bool DepositEnergy(const IntegratorContext* ctx,
                          double wx, double wy,
                          double e,
                          bool clampValue,
                          bool isDirect)
{
    if (e <= 0.0) return true;

    int pixelIndex, sx = 0, sy = 0;
    if (!WorldToPixel(wx, wy, ctx->width, ctx->height, &pixelIndex, &sx, &sy)) {
        return false; // left the viewport
    }

    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        int tileSize = ctx->tileGrid->tileSize;
        int tx = sx / tileSize, ty = sy / tileSize;
        if (tx < 0 || ty < 0 || tx >= ctx->tileGrid->tilesX || ty >= ctx->tileGrid->tilesY) return false;

        size_t tidx = (size_t)ty * (size_t)ctx->tileGrid->tilesX + (size_t)tx;
        IntegratorTile* tile = &ctx->tileGrid->tiles[tidx];
        if (!tile->energy) return false;

        int lx = sx - tile->originX, ly = sy - tile->originY;
        if (lx < 0 || ly < 0 || lx >= tile->width || ly >= tile->height) return false;

        size_t lidx = (size_t)ly * (size_t)tile->width + (size_t)lx;
        float* slot = &tile->energy[lidx];
        if (clampValue) {
            if (e > *slot) *slot = (float)e;
        } else {
            *slot += (float)e;
        }
        return true;
    }

    // Linear buffer path (+optional direct channel)
    if (ctx->energyBuffer) {
        float* slot = &ctx->energyBuffer[pixelIndex];
        float* dch  = (ctx->directEnergyBuffer ? &ctx->directEnergyBuffer[pixelIndex] : NULL);

        if (dch && isDirect && e > *dch) {
            *dch = (float)e;
        }
        if (clampValue) {
            if (e > *slot) *slot = (float)e;
        } else {
            *slot += (float)e;
        }
    }
    return true;
}

// Deposit along a ray segment with distance attenuation
static double DepositSegmentEnergy(const IntegratorContext* ctx,
                                   double sx, double sy,
                                   double dx, double dy,
                                   double length,
                                   double energy,
                                   double scale,
                                   double step,
                                   double clampValue,
                                   bool clampDeposits,
                                   bool isDirect)
{
    if (!ctx || length <= 0.0 || energy <= 0.0) return energy;

    double L = sqrt(dx*dx + dy*dy);
    if (L <= GRID_EPSILON) return energy;
    dx /= L; dy /= L;

    double traveled = 0.0;
    bool   earlyOut = false;
    double falloffDist = ForwardFalloffDistance(ctx);
    int    falloffMode = animSettings.forwardFalloffMode;
    bool   useFalloff  = (falloffMode != FORWARD_FALLOFF_MODE_NONE) && (falloffDist > 0.0);

    for (double t = 0.0; t < length; t += step) {
        double px = sx + dx * t;
        double py = sy + dy * t;

        // Softer, longer-tail attenuation (linear default; quadratic optional)
        double att = useFalloff ? ForwardDistanceAttenuation(t, falloffDist, falloffMode) : 1.0;

        double deposit = energy * scale * att;
        if (clampValue > 0.0 && deposit > clampValue) deposit = clampValue;

        if (!DepositEnergy(ctx, px, py, deposit, clampDeposits, isDirect)) {
            traveled = t;
            earlyOut = true;
            break;
        }
        traveled = t + step;
    }
    if (!earlyOut) traveled = length;

    double eff = fmin(traveled, length);
    double remain = useFalloff ? ForwardDistanceAttenuation(eff, falloffDist, falloffMode) : .8;
    return energy * remain;
}

// Optional blur after integration (unchanged)
static void ApplyEnergyDiffusionBuffer(float* buffer,
                                       int width,
                                       int height,
                                       int radius,
                                       double strength) {
    if (!buffer || radius <= 0 || strength <= 0.0) return;
    size_t total = (size_t)width * (size_t)height;
    float* temp = (float*)malloc(total * sizeof(float));
    if (!temp) return;

    int r = radius > 20 ? 20 : radius;
    if (r < 1) { free(temp); return; }

    float sigma = (float)r * 0.5f + 0.5f;
    float twoSigmaSq = 2.0f * sigma * sigma;
    float blend = (float)Clamp01(strength);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float accum = 0.0f, wsum = 0.0f;
            for (int dy = -r; dy <= r; dy++) {
                int sy = y + dy; if (sy < 0 || sy >= height) continue;
                for (int dx = -r; dx <= r; dx++) {
                    int sx = x + dx; if (sx < 0 || sx >= width) continue;
                    float d2 = (float)(dx*dx + dy*dy);
                    float ww = expf(-d2 / twoSigmaSq);
                    accum += buffer[sy * width + sx] * ww;
                    wsum  += ww;
                }
            }
            float blurred  = (wsum > 0.0f) ? (accum / wsum) : buffer[y * width + x];
            float original = buffer[y * width + x];
            temp[y * width + x] = (1.0f - blend) * original + blend * blurred;
        }
    }
    memcpy(buffer, temp, total * sizeof(float));
    free(temp);
}

static void CopyTilesToBuffer(const IntegratorContext* ctx, float* buffer, size_t total) {
    if (!ctx || !ctx->tileGrid || !buffer) return;
    memset(buffer, 0, total * sizeof(float));
    TileGrid* grid = ctx->tileGrid;
    for (size_t ti = 0; ti < grid->count; ti++) {
        const IntegratorTile* tile = &grid->tiles[ti];
        if (!tile->energy) continue;
        for (int ly = 0; ly < tile->height; ly++) {
            for (int lx = 0; lx < tile->width; lx++) {
                size_t localIndex = (size_t)ly * (size_t)tile->width + (size_t)lx;
                int gx = tile->originX + lx;
                int gy = tile->originY + ly;
                if (gx < 0 || gy < 0 || gx >= ctx->width || gy >= ctx->height) continue;
                size_t globalIndex = (size_t)gy * (size_t)ctx->width + (size_t)gx;
                buffer[globalIndex] = tile->energy[localIndex];
            }
        }
    }
}

static void CopyBufferToTiles(const IntegratorContext* ctx, const float* buffer) {
    if (!ctx || !ctx->tileGrid || !buffer) return;
    TileGrid* grid = ctx->tileGrid;
    for (size_t ti = 0; ti < grid->count; ti++) {
        IntegratorTile* tile = &grid->tiles[ti];
        if (!tile->energy) continue;
        for (int ly = 0; ly < tile->height; ly++) {
            for (int lx = 0; lx < tile->width; lx++) {
                int gx = tile->originX + lx;
                int gy = tile->originY + ly;
                if (gx < 0 || gy < 0 || gx >= ctx->width || gy >= ctx->height) continue;
                size_t globalIndex = (size_t)gy * (size_t)ctx->width + (size_t)gx;
                size_t localIndex = (size_t)ly * (size_t)tile->width + (size_t)lx;
                tile->energy[localIndex] = buffer[globalIndex];
            }
        }
    }
}

// Tonemap helpers (unchanged except exposure compute path)
static void TonemapTile(const IntegratorContext* ctx, const IntegratorTile* tile, float exposure) {
    if (!tile || !tile->energy) return;
    for (int ly = 0; ly < tile->height; ly++) {
        for (int lx = 0; lx < tile->width; lx++) {
            size_t lidx = (size_t)ly * (size_t)tile->width + (size_t)lx;
            Uint8 p = ForwardEnergyToPixel(tile->energy[lidx], exposure);
            int gx = tile->originX + lx, gy = tile->originY + ly;
            size_t gidx = (size_t)gy * (size_t)ctx->width + (size_t)gx;
            ctx->pixelBuffer[gidx] = p;
        }
    }
}

typedef struct {
    IntegratorContext* ctx;
    float exposure;
    SDL_atomic_t cursor;
} TileJobPayload;

static int TileWorker(void* data) {
    TileJobPayload* pay = (TileJobPayload*)data;
    TileGrid* grid = pay->ctx->tileGrid;
    if (!grid) return 0;
    while (true) {
        int idx = SDL_AtomicAdd(&pay->cursor, 1);
        if (idx >= (int)grid->count) break;
        TonemapTile(pay->ctx, &grid->tiles[idx], pay->exposure);
    }
    return 0;
}

static void ConvertEnergyToPixels(IntegratorContext* ctx) {
    if (!ctx->pixelBuffer) return;

    if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles) {
        if (ctx->tileGrid->count == 0) {
            memset(ctx->pixelBuffer, 0, (size_t)ctx->width * (size_t)ctx->height * sizeof(Uint8));
            return;
        }
        float exposure = ComputeTileExposure(ctx->tileGrid);
        if (exposure <= 0.0f) {
            memset(ctx->pixelBuffer, 0, (size_t)ctx->width * (size_t)ctx->height * sizeof(Uint8));
            return;
        }

        TileJobPayload payload = {.ctx = ctx, .exposure = exposure};
        SDL_AtomicSet(&payload.cursor, 0);

        int workers = SDL_GetCPUCount();
        if (workers <= 0) workers = 4;
        if (workers > (int)ctx->tileGrid->count) workers = (int)ctx->tileGrid->count;
        if (workers < 1) workers = 1;

        SDL_Thread** ts = (SDL_Thread**)malloc((size_t)workers * sizeof(SDL_Thread*));
        for (int i = 0; i < workers; i++) {
            ts[i] = SDL_CreateThread(TileWorker, "TileWorker", &payload);
        }
        for (int i = 0; i < workers; i++) if (ts[i]) SDL_WaitThread(ts[i], NULL);
        free(ts);
    } else if (ctx->energyBuffer) {
        int total = ctx->width * ctx->height;
        float exposure = ComputeBufferExposure(ctx);
        if (exposure <= 0.0f) {
            memset(ctx->pixelBuffer, 0, (size_t)total * sizeof(Uint8));
            return;
        }
        for (int i = 0; i < total; i++) {
            ctx->pixelBuffer[i] = ForwardEnergyToPixel(ctx->energyBuffer[i], exposure);
        }
    }
}

// Exposure estimation
static inline void EnergyStatsAccumulate(EnergyStats* stats, float value) {
    if (!stats || value <= 0.0f) return;
    stats->sum += value;
    stats->samples++;
    if (value > stats->maxValue) stats->maxValue = value;
}

static float EnergyStatsExposure(const EnergyStats* stats) {
    if (!stats || stats->samples == 0) return 1.0f;
    double mean = stats->sum / (double)stats->samples;
    double scale = mean * 6.0;
    if (stats->maxValue > scale * 4.0) {
        scale = (scale * 0.5) + (stats->maxValue * 0.1);
    }
    if (scale <= 1e-4) scale = stats->maxValue > 0.0 ? stats->maxValue : 1e-4;
    return (float)(1.0 / scale);
}

static float ComputeBufferExposure(const IntegratorContext* ctx) {
    if (!ctx || !ctx->energyBuffer) return 1.0f;
    size_t total = (size_t)ctx->width * (size_t)ctx->height;
    EnergyStats s = {0};
    for (size_t i = 0; i < total; i++) {
        EnergyStatsAccumulate(&s, ctx->energyBuffer[i]);
    }
    return EnergyStatsExposure(&s);
}

static float ComputeTileExposure(const TileGrid* grid) {
    if (!grid || !grid->tiles) return 1.0f;
    EnergyStats s = {0};
    for (size_t i = 0; i < grid->count; i++) {
        const IntegratorTile* t = &grid->tiles[i];
        if (!t->energy) continue;
        size_t total = (size_t)t->width * (size_t)t->height;
        for (size_t j = 0; j < total; j++) EnergyStatsAccumulate(&s, t->energy[j]);
    }
    return EnergyStatsExposure(&s);
}

static Uint8 ForwardEnergyToPixel(float energy, float exposure) {
    float e = fmaxf(energy, 0.0f);
    float mapped = 1.0f - expf(-e * exposure);
    float tone = powf(Clamp01(mapped), FORWARD_TONEMAP_GAMMA);
    return (Uint8)Clamp(tone * 255.0f, 0, 255);
}

// Clamp for bounce (unchanged)
static double ForwardClampForBounce(int bounce) {
    double base = animSettings.lightIntensity * FORWARD_PRIMARY_SCALE * 1.5;
    if (bounce <= 0) return base;
    return 1e9;
}

// -----------------------------------------
// Main photon tracing with deposits
// -----------------------------------------
static void TracePhotonPath(const IntegratorContext* ctx,
                            FastRNG* rng,
                            double originX, double originY,
                            double dirX, double dirY,
                            double initialThroughput,
                            int bounceOffset)
{
    if (!ctx || !rng) return;
    if (bounceOffset >= MAX_BOUNCES) return;

    double throughput = initialThroughput;
    double ox = originX, oy = originY;
    double dx = dirX,  dy = dirY;

    for (int bounce = 0; bounce < MAX_BOUNCES; bounce++) {
        HitInfo2D hit = {0};
        hit.objectIndex = -1;
        hit.triangleIndex = -1;
        hit.baryW = 1.0;

        const SceneObject* obj = NULL;
        bool haveHit = TraceRayToSurface(ctx, ox, oy, dx, dy, &hit, &obj, 0.0);

        double travel = haveHit ? fmax(hit.t, FORWARD_STEP)
                                : DistanceToSceneBounds(ctx, ox, oy, dx, dy);
        double triScale = 1.0;
        if (haveHit) triScale = ApplyTriangleHitProperties(ctx, &hit);

        int  effBounce   = bounce + bounceOffset;
        bool isPrimary   = (effBounce == 0);
        double depositK  = isPrimary ? FORWARD_PRIMARY_SCALE : FORWARD_SECONDARY_SCALE;

#if FORWARD_DEBUG_REFLECTIONS
        if (isPrimary) depositK = 0.0;
#endif
        bool clampDeposits = false;
        bool isDirect      = (effBounce == 0);

        // Deposit along the flight segment with softened attenuation
        throughput = DepositSegmentEnergy(ctx, ox, oy, dx, dy,
                                          travel, throughput, depositK,
                                          FORWARD_STEP, ForwardClampForBounce(effBounce),
                                          clampDeposits, isDirect);

        if (!haveHit) break;

        // Hit point deposit (kept modest)
        double hitE = throughput * (isPrimary ? 1.0 : (0.3 / (effBounce + 1)));
        hitE *= triScale;
#if FORWARD_DEBUG_REFLECTIONS
        if (!isPrimary) DepositEnergy(ctx, hit.px, hit.py, hitE, true, false);
#else
        DepositEnergy(ctx, hit.px, hit.py, hitE, !isPrimary, isDirect);
#endif

        // BSDF sampling for next directions
        const MaterialBSDF* material = GetMaterial(ctx, hit.objectIndex);
        MaterialBSDF fb = {0};
        if (!material && obj) { MaterialBSDFInitFromSceneObject(obj, &fb); material = &fb; }
        if (!material) break;

        double inDirX = -dx;
        double inDirY = -dy;
        NormalizeVec(&inDirX, &inDirY);
        double shadedNx, shadedNy;
        OrientNormalForIncoming(&hit, inDirX, inDirY, &shadedNx, &shadedNy);

        int secondaryCount = (bounce == 0) ? 3 : 1;
        bool spawned = false;
        for (int i = 0; i < secondaryCount; i++) {
            BSDFSample s;
            if (!MaterialBSDFSample(material, shadedNx, shadedNy, inDirX, inDirY, 0.0, rng, &s)) continue;
            if (s.pdf <= 1e-8 || s.weight <= 0.0) continue;

            double nextT = throughput * (s.weight / s.pdf);
            nextT = ClampThroughput(nextT, 0.0, 1e6);
            if (nextT < MIN_ENERGY) continue;

            if (animSettings.pathRussianRoulette && bounce >= 1) {
                if (ShouldTerminatePath(nextT, animSettings.rouletteThreshold, rng)) continue;
            }

            if (!spawned) {
                ox = hit.px + shadedNx * PATH_EPSILON;
                oy = hit.py + shadedNy * PATH_EPSILON;
                dx = s.dirX; dy = s.dirY;
                throughput = nextT;
                spawned = true;
            } else {
                TracePhotonPath(ctx, rng,
                                hit.px + shadedNx * PATH_EPSILON,
                                hit.py + shadedNy * PATH_EPSILON,
                                s.dirX, s.dirY,
                                nextT, effBounce + 1);
            }
        }
        if (!spawned) break;
    }
}

// Emit a contiguous angle range
static void EmitRayRange(const IntegratorContext* ctx, FastRNG* rng,
                         double sx, double sy, int rayStart, int rayEnd)
{
    for (int r = rayStart; r < rayEnd; r++) {
        double ang = (2.0 * M_PI * r) / sceneSettings.rays;
        double dx = cos(ang), dy = sin(ang);
        TracePhotonPath(ctx, rng, sx, sy, dx, dy, animSettings.lightIntensity, 0);
    }
}

// Single-thread path
static void FillRays(const IntegratorContext* ctx, const LightSource* light) {
    FastRNG rng; FastRNGSeed(&rng, ctx->frameSeed ^ 0x4d595df4d0f33173ULL, 0x631c8ULL);
    EmitRayRange(ctx, &rng, light->x, light->y, 0, sceneSettings.rays);
}

static int RayCalculationWorker(void* data) {
    ThreadData* td = (ThreadData*)data;
    EmitRayRange(td->ctx, &td->rng, td->sourceX, td->sourceY, td->rayStart, td->rayEnd);
    return 0;
}

// Entry
void ForwardLightIntegratorRender(IntegratorContext* ctx, const LightSource* light) {
    if (!ctx || !light) return;

    if (animSettings.lightMode == 0) {
        FillRays(ctx, light);
    } else {
        const int NUM_THREADS = 4;
        SDL_Thread* threads[NUM_THREADS];
        ThreadData   td[NUM_THREADS];
        int per = sceneSettings.rays / NUM_THREADS;
        for (int i = 0; i < NUM_THREADS; i++) {
            td[i] = (ThreadData){
                .sourceX = light->x, .sourceY = light->y,
                .rayStart = i * per,
                .rayEnd   = (i == NUM_THREADS - 1) ? sceneSettings.rays : (i + 1) * per,
                .ctx = ctx
            };
            FastRNGSeed(&td[i].rng,
                        ctx->frameSeed ^ ((uint64_t)i + 1ULL) * 0x9E3779B185EBCA87ULL,
                        (uint64_t)td[i].rayStart + 1ULL);
            threads[i] = SDL_CreateThread(RayCalculationWorker, "RayWorker", &td[i]);
        }
        for (int i = 0; i < NUM_THREADS; i++) SDL_WaitThread(threads[i], NULL);
    }

    if (!ctx->useTiles && animSettings.lightDiffusionEnabled &&
        animSettings.lightDiffusionRadius > 0 && ctx->energyBuffer) {
        ApplyEnergyDiffusionBuffer(ctx->energyBuffer,
                                   ctx->width,
                                   ctx->height,
                                   animSettings.lightDiffusionRadius,
                                   animSettings.lightDiffusionStrength);
    }
    else if (ctx->useTiles && ctx->tileGrid && ctx->tileGrid->tiles &&
             animSettings.lightDiffusionEnabled &&
             animSettings.lightDiffusionRadius > 0) {
        size_t total = (size_t)ctx->width * (size_t)ctx->height;
        float* blurBuffer = (float*)malloc(total * sizeof(float));
        if (blurBuffer) {
            CopyTilesToBuffer(ctx, blurBuffer, total);
            ApplyEnergyDiffusionBuffer(blurBuffer,
                                       ctx->width,
                                       ctx->height,
                                       animSettings.lightDiffusionRadius,
                                       animSettings.lightDiffusionStrength);
            CopyBufferToTiles(ctx, blurBuffer);
            free(blurBuffer);
        }
    }
    ConvertEnergyToPixels(ctx);
}

// -----------------------------------------------------------
// Fix 1: much larger default decay distance (no hard cutoff)
// Fix 2: softer attenuation curve (linear by default; quad opt)
// -----------------------------------------------------------
static double ForwardFalloffDistance(const IntegratorContext* ctx) {
    // If user provided a decay distance, use it.
    double d = animSettings.forwardDecay;
    if (d > 0.0) return d;

    // Default: diagonal of the active render (long reach).
    double w = (ctx && ctx->width  > 0) ? (double)ctx->width  : (double)sceneSettings.windowWidth;
    double h = (ctx && ctx->height > 0) ? (double)ctx->height : (double)sceneSettings.windowHeight;
    if (w <= 0.0) w = 1200.0;
    if (h <= 0.0) h = 800.0;
    return hypot(w, h);
}

static double ForwardDistanceAttenuation(double distance, double scale, int mode) {
    if (mode == FORWARD_FALLOFF_MODE_NONE || scale <= 0.0) return 1.0;

    double safeScale  = fmax(scale, 1.0);
    double softness   = fmax(animSettings.lightDecaySoftness, 0.1);
    double normalized = fmax(distance, 0.0) / (safeScale * softness);

    // Default to a slow linear-style soft rolloff; quadratic optional.
    if (mode == FORWARD_FALLOFF_MODE_LINEAR) {
        // 1 / (1 + d/scale) — long smooth tail
        return 1.0 / (1.0 + normalized);
    }
    // Quadratic mode kept for stylistic beams; now less aggressive by scale.
    double denom = 1.0 + normalized * normalized;
    return 1.0 / denom;
}
