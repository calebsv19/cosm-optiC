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
typedef struct SpectralReconstruction SpectralReconstruction;
struct RuntimeSpectralVolume {
    SpectralReconstruction *reconstruction;
    Vec3 *positions; /* Only continuous modes allocate collision positions. */
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
bool spectral_camera_begin(RuntimeSpectralCameraCache *, SpectralFieldRay *, const RuntimeSpectralVolume *, Ray3D, double, bool);
bool spectral_camera_next(RuntimeSpectralCameraCache *, SpectralFieldRay *, double *, double *, double *);

unsigned spectral_volume_bucket(uint64_t, int);
bool spectral_reconstruction_build(RuntimeSpectralVolume *);
bool spectral_reconstruction_begin_ray(RuntimeSpectralVolume *, Ray3D);
double spectral_reconstruction_source(RuntimeSpectralVolume *, Vec3, int, double *);
double spectral_reconstruction_boundary(RuntimeSpectralVolume *, Ray3D, double, double);
void spectral_reconstruction_free(SpectralReconstruction *);
void spectral_reconstruction_stats(const RuntimeSpectralVolume *, RuntimeSpectralMapStats *);

bool spectral_reconstruction_accepting(const RuntimeSpectralVolume *);

bool spectral_mesh_camera_interval(const RuntimeSpectralVolume *, Ray3D, double,
                                   double);
bool spectral_reconstruction_fail(SpectralReconstruction *, const char *);
