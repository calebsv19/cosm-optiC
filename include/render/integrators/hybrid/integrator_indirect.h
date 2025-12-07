#ifndef INTEGRATOR_INDIRECT_H
#define INTEGRATOR_INDIRECT_H

#include <stddef.h>
#include "render/integrator_common.h"
#include "render/material_bsdf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width, height;
    UniformGrid* grid;
    IrradianceCache* cache;
    float* energyBuffer;
    int useTiles;
    TileGrid* tileGrid;
    SceneObject* objects;
    int objectCount;
    MaterialBSDF* materials;
    int materialCount;
} IntegratorIndirectContext;

float IndirectSamplePoint(const IntegratorIndirectContext* ctx,
                          const LightSource* light,
                          double worldX,
                          double worldY,
                          int px, int py,
                          int feelerCount,
                          double feelerLimit,
                          double varianceCut,
                          double haloRadius,
                          double camX, double camY,
                          double intensityScale,
                          float* debugStats);

void IndirectLightingPass(IntegratorIndirectContext* ctx,
                          const LightSource* light,
                          double camX, double camY,
                          double userVariance,
                          double userHalo,
                          double intensityScale);

#ifdef __cplusplus
}
#endif

#endif
