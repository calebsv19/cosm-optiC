#ifndef HYBRID_CAMERA_PATH_INTEGRATOR_H
#define HYBRID_CAMERA_PATH_INTEGRATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "render/integrator_common.h"
#include "render/material_bsdf.h"

/* Per-frame integrator configuration */
typedef struct {
    double directIntensityScale;
    double indirectVariance;
    double indirectHaloRadius;
    int    blurEnabled;
    double brightnessBoost;
} CameraIntegratorSettings;

/* Global integrator state */
typedef struct {
    int width;
    int height;

    /* scene + acceleration */
    UniformGrid* grid;
    SceneObject* objects;
    int objectCount;
    MaterialBSDF* materials;
    int materialCount;

    /* irradiance cache */
    IrradianceCache* cache;

    /* tiling (optional) */
    TileGrid* tiles;
    int       useTiles;

    /* output buffers */
    float*          energy;
    unsigned char*  pixels;
} CameraIntegrator;

/* Build/destroy integrator */
void CameraPathIntegrator_Init(CameraIntegrator* ci,
                               int width,
                               int height,
                               UniformGrid* grid,
                               SceneObject* objects,
                               int objectCount,
                               MaterialBSDF* materials,
                               int materialCount,
                               IrradianceCache* cache,
                               TileGrid* tiles);

void CameraPathIntegrator_Free(CameraIntegrator* ci);

/* Do a full frame render */
void CameraPathIntegrator_Render(CameraIntegrator* ci,
                                 const LightSource* light,
                                 double camX,
                                 double camY,
                                 const CameraIntegratorSettings* s);

/* Convenience: render using legacy IntegratorContext buffers. */
void CameraPathIntegratorRenderFromContext(IntegratorContext* ctx,
                                           const LightSource* light,
                                           const CameraIntegratorSettings* s,
                                           double camX,
                                           double camY);

#ifdef __cplusplus
}
#endif

#endif
