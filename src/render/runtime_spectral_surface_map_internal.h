#pragma once
#include "render/runtime_spectral_surface_map.h"
#include <stdint.h>
/* Immutable after population. Links are indices so replays sum in a fixed order. */
typedef struct {
    Vec3 position, incident;
    double vacuum_nm, watts;
    int64_t u, v;
    int primitive, band, source, next;
    bool front, scattered;
    float scale;
} Deposit;

struct RuntimeSpectralSurfaceMap {
    Deposit *records;
    int *heads;
    size_t mask;
    int count, continuum_bins;
    bool has_expanded;
    struct SpectralWideIndex *wide;
    double band_nm[SPECTRAL_MAP_MAX_BANDS];
    double direct_watts[SPECTRAL_MAP_LIGHTS][SPECTRAL_MAP_MAX_BANDS];
};


size_t spectral_surface_bucket(const RuntimeSpectralSurfaceMap *,int,int,int64_t,int64_t);
bool spectral_surface_cell(const RuntimePlane3D *,Vec3,double,int64_t *,int64_t *);
bool spectral_surface_wide_prepare(RuntimeSpectralSurfaceMap *,const RuntimeSpectralPencil *,const RuntimeScene3D *,int *);
