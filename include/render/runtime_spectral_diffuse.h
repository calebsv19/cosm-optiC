#ifndef RENDER_RUNTIME_SPECTRAL_DIFFUSE_H
#define RENDER_RUNTIME_SPECTRAL_DIFFUSE_H
#include "render/runtime_spectral_film.h"

/* Local calibration route: a finite Lambertian panel and one further diffuse
 * reflection. Additional planes can occlude, but are not bounce sources. */
bool RuntimeSpectralDiffuse_Run(const RuntimeSpectralPencil *profile,
    const RuntimeNative3DPreparedFrame *frame, uint8_t *pixels,
    RuntimeSpectralReadout *out, char *diag, size_t size);
/* Mixed film: attenuation on the sky and panel light legs. Receiver
 * reflectance and camera throughput are applied by the film. */
typedef struct RuntimeSpectralSkyLighting RuntimeSpectralSkyLighting;
RuntimeSpectralSkyLighting *RuntimeSpectralSkyLighting_Create(const RuntimeSpectralPencil *,
                                                               const RuntimeScene3D *);
void RuntimeSpectralSkyLighting_Free(RuntimeSpectralSkyLighting *);
void RuntimeSpectralSkyLighting_Evaluate(const RuntimeSpectralSkyLighting *,
    const RuntimeSpectralPathReadout *, double nm, double *direct, double *indirect);
#endif
