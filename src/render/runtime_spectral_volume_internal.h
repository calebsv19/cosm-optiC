#pragma once
#include "render/runtime_spectral_volume.h"
#include "render/runtime_water_body_mesh_3d.h"

typedef struct {
    Vec3 incoming;
    double watts;
    uint64_t cell;
    int band, next;
    bool first;
} SpectralCollision;
struct RuntimeSpectralVolume {
    RuntimeSpectralVolumeConfig config;
    RuntimeVolumeAttachment3D vf;
    RuntimeWaterBodyMesh3DDesc water;
    float *heights;
    Vec3 lo, hi;
    int dims[3], frame;
    double time, dt;
    char digest[65], source_sha[65], frame_sha[65];
    SpectralCollision *points;
    int count, capacity, *heads;
    RuntimeSpectralBand ledger[SPECTRAL_MAP_MAX_BANDS];
};
/* Iterates occupied segments bounded by interpolation knots and optional gather cells. */
typedef struct {
    const RuntimeSpectralVolume *v;
    Ray3D ray;
    double t, end;
    bool cells;
} SpectralFieldRay;
bool spectral_field_begin(SpectralFieldRay *, const RuntimeSpectralVolume *, Ray3D, double, bool);
bool spectral_field_next(SpectralFieldRay *, double *, double *);
double spectral_field_mass(const SpectralFieldRay *, double, double);
