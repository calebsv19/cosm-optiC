#ifndef RENDER_RUNTIME_SPECTRAL_SURFACE_PATH_H
#define RENDER_RUNTIME_SPECTRAL_SURFACE_PATH_H
#include "render/runtime_spectral_film.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include <stdio.h>

typedef struct RuntimeSpectralSurfacePath RuntimeSpectralSurfacePath;
bool RuntimeSpectralSurfacePath_ConfigureOutput(RuntimeSpectralSurfacePath *,const char *,bool);
void RuntimeSpectralSurfacePath_Defaults(RuntimeSpectralPencil *);
RuntimeSpectralSurfacePath *RuntimeSpectralSurfacePath_Create(const RuntimeSpectralPencil *,
    const RuntimeScene3D *, int, int, char *, size_t);
void RuntimeSpectralSurfacePath_Free(RuntimeSpectralSurfacePath *);
void RuntimeSpectralSurfacePath_GetMetadata(const RuntimeSpectralSurfacePath *,RuntimeSpectralReadout *);
bool RuntimeSpectralSurfacePath_Region(RuntimeSpectralSurfacePath *,
    const RuntimeNative3DPreparedFrame *, int, int, int, int, uint8_t *, float *, int,
    RuntimeSpectralReadout *, const RuntimeNative3DTileSchedulerCancelToken *, char *, size_t);
bool RuntimeSpectralSurfacePath_Render(RuntimeSpectralSurfacePath *,
    const RuntimeNative3DPreparedFrame *, uint8_t *, RuntimeSpectralReadout *,
    const RuntimeNative3DTileSchedulerCancelToken *, RuntimeNative3DTileSchedulerProgressCallback,
    void *, char *, size_t);
bool RuntimeSpectralSurfacePath_Run(const RuntimeSpectralPencil *,
    const RuntimeNative3DPreparedFrame *, uint8_t *, RuntimeSpectralReadout *, char *, size_t);
void RuntimeSpectralSurfacePath_Write(FILE *, const RuntimeSpectralPencil *,
                                     const RuntimeSpectralReadout *);
#endif
