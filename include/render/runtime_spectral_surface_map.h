#ifndef RENDER_RUNTIME_SPECTRAL_SURFACE_MAP_H
#define RENDER_RUNTIME_SPECTRAL_SURFACE_MAP_H
#include "render/runtime_spectral_pencil.h"

typedef struct RuntimeSpectralSurfaceMap RuntimeSpectralSurfaceMap;
void RuntimeSpectralSurfaceMap_Key(const RuntimeSpectralPencil *, const RuntimeScene3D *, char key[65]);
RuntimeSpectralSurfaceMap *RuntimeSpectralSurfaceMap_Build(const RuntimeSpectralPencil *,
                                                           const RuntimeScene3D *,
                                                           RuntimeSpectralMapStats *);
void RuntimeSpectralSurfaceMap_Free(RuntimeSpectralSurfaceMap *);
/* Returns radiance integrated over one wavelength bin, or the exact line mass. */
double RuntimeSpectralSurfaceMap_Radiance(const RuntimeSpectralSurfaceMap *,
                                          const RuntimeSpectralPencil *, const RuntimeScene3D *,
                                          const RuntimeSpectralPathReadout *, int band,
                                          unsigned long long *candidates);
double RuntimeSpectralSurfaceMap_Absorption(const RuntimeSpectralMapConfig *, double vacuum_nm);
size_t RuntimeSpectralSurfaceMap_RecordBytes(void);
#endif
