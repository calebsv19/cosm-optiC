#pragma once
#include "render/runtime_spectral_pencil.h"

RuntimeSpectralVolume *RuntimeSpectralVolume_Load(const RuntimeSpectralVolumeConfig *, char *, size_t);
void RuntimeSpectralVolume_Free(RuntimeSpectralVolume *);
void RuntimeSpectralVolume_Identity(const RuntimeSpectralVolume *, RuntimeSpectralMapStats *);
const char *RuntimeSpectralVolume_Digest(const RuntimeSpectralVolume *);
double RuntimeSpectralVolume_Transmission(const RuntimeSpectralVolume *, Ray3D, double, double);
/* Handles a single exterior segment. A forward scattering event restarts the ray. */
bool RuntimeSpectralVolume_Segment(RuntimeSpectralVolume *, const RuntimeSpectralPencilLine *, Ray3D *,
                                   double, bool, int, RuntimeSpectralPathReadout *, bool *);
void RuntimeSpectralVolume_ScatterStats(const RuntimeSpectralVolume *, RuntimeSpectralMapStats *);

/* Bind geometry before emitting photons. Continuous modes refuse unsupported boundaries. */
bool RuntimeSpectralVolume_BindScene(RuntimeSpectralVolume *, const RuntimeScene3D *, char *, size_t);
const char *RuntimeSpectralVolume_Error(const RuntimeSpectralVolume *);
bool RuntimeSpectralVolume_PrepareReconstruction(RuntimeSpectralVolume *);
