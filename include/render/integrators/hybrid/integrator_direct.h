#ifndef INTEGRATOR_DIRECT_H
#define INTEGRATOR_DIRECT_H

#include <stddef.h>
#include "render/integrator_common.h"
#include "render/material_bsdf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width, height;
    UniformGrid* grid;
    unsigned char* pixelBuffer;
    float* energyBuffer;
    int useTiles;
    TileGrid* tileGrid;
} IntegratorDirectContext;

double DirectComputeRadiance(const LightSource* light,
                             double worldX,
                             double worldY,
                             double intensityScale);

float DirectEvaluate(const HitInfo2D* hit,
                     const LightSource* light,
                     const MaterialBSDF* mat,
                     double camX, double camY,
                     double intensityScale);

void DirectLightingPass(IntegratorDirectContext* ctx,
                        const LightSource* light,
                        double camX, double camY,
                        double intensityScale);
void DirectLightingPassRegion(IntegratorDirectContext* ctx,
                              const LightSource* light,
                              double camX, double camY,
                              double intensityScale,
                              int startX, int startY,
                              int endX, int endY);

#ifdef __cplusplus
}
#endif

#endif
