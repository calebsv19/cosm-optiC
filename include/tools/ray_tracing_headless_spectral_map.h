#ifndef TOOLS_RAY_TRACING_HEADLESS_SPECTRAL_MAP_H
#define TOOLS_RAY_TRACING_HEADLESS_SPECTRAL_MAP_H
#include "tools/ray_tracing_headless_spectral.h"
bool RayTracingHeadlessSpectralMap_Run(const RuntimeSpectralPencil *,
                                       const RuntimeNative3DPreparedFrame *, uint8_t *,
                                       RuntimeSpectralReadout *, char *, size_t);
void RayTracingHeadlessSpectralMap_Write(FILE *file, const RuntimeSpectralMapStats *stats,
                                         bool rendered);
#endif
