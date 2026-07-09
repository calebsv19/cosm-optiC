#ifndef INTEGRATOR_TONEMAP_H
#define INTEGRATOR_TONEMAP_H

#include <stddef.h>
#include <stdint.h>
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
float TonemapCurveWithFloor(float x, float floor);
uint8_t TonemapCurveToByteWithFloor(float x, uint8_t floor_byte);
void  TonemapApply(TonemapContext* ctx);
void  TonemapTiles(TonemapContext* ctx);
void  TonemapTile(TonemapContext* ctx, const IntegratorTile* tile);

#ifdef __cplusplus
}
#endif

#endif
