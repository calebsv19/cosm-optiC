#ifndef RENDER_RUNTIME_SPECTRAL_PENCIL_H
#define RENDER_RUNTIME_SPECTRAL_PENCIL_H

#include <stdbool.h>
#include "render/runtime_ray_3d.h"

/* Local headless calibration instrument. Exact quadrature over discrete atoms. */
#define RUNTIME_SPECTRAL_PENCIL_MAX_LINES 16
typedef struct {
    double vacuum_nm, watts, absorption_per_m;
} RuntimeSpectralPencilLine;

typedef struct {
    char glass_id[64], receiver_id[64];
    Vec3 origin_m, direction;
    double constant_ior, receiver_radius_m, exposure;
    bool use_n_bk7;
    int count;
    RuntimeSpectralPencilLine lines[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
} RuntimeSpectralPencil;

typedef struct {
    Vec3 position, direction;
    double weight, glass_m, vacuum_nm;
    int interfaces;
    bool receiver, receiver_front, depth_limited;
} RuntimeSpectralPathReadout;

typedef struct {
    Vec3 center_pixel_xyz;
    RuntimeSpectralPathReadout forward[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
    RuntimeSpectralPathReadout camera[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
    double ior[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
} RuntimeSpectralReadout;
double RuntimeSpectralPencil_Index(const RuntimeSpectralPencil *p, double vacuum_nm);
bool RuntimeSpectralPencil_Trace(const RuntimeScene3D *scene, const RuntimeSpectralPencil *p, int line,
                                 double index, Ray3D ray, bool camera, RuntimeSpectralPathReadout *out);
#endif
