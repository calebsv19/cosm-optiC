#ifndef INTEGRATOR_CACHE_H
#define INTEGRATOR_CACHE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "render/integrator_common.h"

typedef struct MaterialBSDF MaterialBSDF;

double CacheVarianceThreshold(double userCutoff);
double CacheHaloRadius(const LightSource* light, double userScale);

const SurfaceIrradiance* CacheFindClosest(const IrradianceCache* cache,
                                          int objectIndex,
                                          double px,
                                          double py);

float CacheSampleDirectional(const IrradianceCache* cache,
                             const HitInfo2D* hit,
                             double dirX,
                             double dirY,
                             const MaterialBSDF* mat,
                             double varianceCut,
                             float* debugRejectAlign,
                             float* debugRejectVar);

#ifdef __cplusplus
}
#endif

#endif
