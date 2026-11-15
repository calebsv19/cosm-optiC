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
double RuntimeSpectralSurfaceMap_ProbeRadiance(const RuntimeSpectralSurfaceMap *,
    const RuntimeSpectralPencil *, const RuntimeScene3D *, const RuntimeSpectralPathReadout *, int band,
    unsigned long long *candidates, double *direct, RuntimeSpectralDepositProbe *deposit);
typedef struct {
    double radiance;
    Vec3 xyz;
} RuntimeSpectralDepositColor;
/* Color moments cover deposited power only. The returned scalar also
 * includes direct illumination and retains the native radiometric convention. */
double RuntimeSpectralSurfaceMap_Gather(const RuntimeSpectralSurfaceMap *,
    const RuntimeSpectralPencil *, const RuntimeScene3D *, const RuntimeSpectralPathReadout *, int band,
    unsigned long long *candidates, double *direct, RuntimeSpectralDepositProbe *deposit,
    RuntimeSpectralDepositColor *color);
/* Area-source map irradiance divided by emitted radiance in the same band.
 * Reflectance and camera wavelength compensation belong to the caller. */
double RuntimeSpectralSurfaceMap_AreaResponse(const RuntimeSpectralSurfaceMap *,
    const RuntimeSpectralPencil *, const RuntimeScene3D *, const RuntimeSpectralPathReadout *,
    double nm, RuntimeSpectralMeasure measure, unsigned long long *candidates);
double RuntimeSpectralSurfaceMap_Absorption(const RuntimeSpectralMapConfig *, double vacuum_nm);
size_t RuntimeSpectralSurfaceMap_RecordBytes(void);
#endif

/* Reconstruct the first medium scatter after the forward specular chain. */
bool RuntimeSpectralSurfaceMap_AreaVolumeResponse(const RuntimeSpectralSurfaceMap *,
    const RuntimeSpectralPencil *, Ray3D, double limit, double nm, RuntimeSpectralMeasure, double *);
void RuntimeSpectralSurfaceMap_LaunchKey(const RuntimeSpectralPencil *, const RuntimeScene3D *, char key[65]);
