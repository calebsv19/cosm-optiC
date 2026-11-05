#ifndef RUNTIME_SPECTRAL_CONVERGENCE_H
#define RUNTIME_SPECTRAL_CONVERGENCE_H
#include "render/runtime_native_3d_tile_scheduler.h"
#include "render/runtime_spectral_film.h"
bool RuntimeSpectralConvergence_Render(
    const RuntimeSpectralPencil *profile,
    const RuntimeNative3DPreparedFrame *frame, uint8_t *pixels,
    const RuntimeNative3DTileSchedulerCancelToken *cancel,
    RuntimeNative3DTileSchedulerProgressCallback progress, void *user,
    RuntimeSpectralReadout *out, char *diag, size_t size);
#endif
