#ifndef INTEGRATOR_TONEMAP_H
#define INTEGRATOR_TONEMAP_H

#include <stddef.h>
#include "render/integrator_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width, height;
    int useTiles;
    TileGrid* tiles;
    float* energyBuffer;
    unsigned char* pixelBuffer;
} TonemapContext;

float TonemapCurve(float x);
void  TonemapApply(TonemapContext* ctx);
void  TonemapTiles(TonemapContext* ctx);

#ifdef __cplusplus
}
#endif

#endif
