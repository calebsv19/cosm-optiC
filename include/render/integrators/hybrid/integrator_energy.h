#ifndef INTEGRATOR_ENERGY_H
#define INTEGRATOR_ENERGY_H

#include <stddef.h>
#include "render/integrator_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width, height;
    int useTiles;
    TileGrid* tileGrid;
    float* energyBuffer;
} IntegratorEnergyContext;

float ReadEnergy(const IntegratorEnergyContext* ctx, int x, int y);
void  WriteEnergy(const IntegratorEnergyContext* ctx, int x, int y, float v);
void  ClearEnergy(IntegratorEnergyContext* ctx);
void  AddEnergy(const IntegratorEnergyContext* ctx, int x, int y, float v);

void BilateralBlurEnergy(IntegratorEnergyContext* ctx,
                         float spatialSigma,
                         float rangeSigma);

#ifdef __cplusplus
}
#endif

#endif
