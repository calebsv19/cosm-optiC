#ifndef RENDER_RUNTIME_SPECTRAL_PENCIL_H
#define RENDER_RUNTIME_SPECTRAL_PENCIL_H

#include <stdbool.h>
#include <stdint.h>
#include "render/runtime_spectral_profile.h"
#include "render/runtime_ray_3d.h"

/* Local headless calibration instrument. Exact quadrature over discrete atoms. */
#define RUNTIME_SPECTRAL_PENCIL_MAX_LINES 16
typedef struct {
    double vacuum_nm, watts, absorption_per_m;
    uint64_t sample_id;
} RuntimeSpectralPencilLine;

enum {
    SPECTRAL_MAP_LIGHTS = 4,
    SPECTRAL_MAP_KNOTS = 32,
    SPECTRAL_MAP_BINS = 128,
    SPECTRAL_MAP_MAX_BANDS = SPECTRAL_MAP_BINS + SPECTRAL_MAP_LIGHTS * RUNTIME_SPECTRAL_PENCIL_MAX_LINES
};
typedef struct {
    Vec3 origin, direction;
    double aperture_m, angular_radius_rad;
    const char *spectrum_model; /* Static parser-owned label; NULL for tabulated and line sources. */
    int nlines, nknots;
    RuntimeSpectralLine lines[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
    RuntimeSpectralKnot knots[129]; /* generated spectrum grid, at most four resolved peaks */
} RuntimeSpectralLamp;
typedef struct {
    bool enabled;
    int samples, bins, source_count;
    unsigned seed;
    double absorption_per_m;
    int absorption_knots;
    double absorption_curve[SPECTRAL_MAP_KNOTS][2]; /* vacuum nm, inverse meters */
    char config_hash[65];
    RuntimeSpectralLamp sources[SPECTRAL_MAP_LIGHTS];
} RuntimeSpectralMapConfig;

typedef struct {
    int count;
    double points[SPECTRAL_MAP_KNOTS][2];
} RuntimeSpectralCurve;

typedef struct {
    RuntimeSpectralCurve response;
    bool quantum_efficiency;
} RuntimeSpectralSensor;

typedef struct {
    RuntimeSpectralMapConfig map;
    double exterior_ior, glass_roughness;
    RuntimeSpectralCurve reflectance;
    bool use_scene_texture;
    int sensor_count, display_sensor;
    RuntimeSpectralSensor sensors[4];
    char glass_id[64], receiver_id[64];
    Vec3 origin_m, direction;
    double constant_ior, receiver_radius_m, exposure;
    bool use_n_bk7;
    int count;
    RuntimeSpectralPencilLine lines[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
} RuntimeSpectralPencil;

typedef struct {
    Vec3 position, direction, receiver_rgb;
    double weight, glass_m, vacuum_nm;
    int interfaces, primitive;
    bool receiver, receiver_front, depth_limited;
} RuntimeSpectralPathReadout;

typedef struct {
    double nm, width_nm, watts, center_radiance;
    Vec3 moment;
    unsigned count;
} RuntimeSpectralBand;
typedef struct {
    char key[65];
    int band_count, launched, deposits, direct_arrivals, missed, depth_limited;
    unsigned long long candidates;
    double build_ms, film_ms;
    bool reused;
    RuntimeSpectralBand bands[SPECTRAL_MAP_MAX_BANDS];
} RuntimeSpectralMapStats;
typedef struct {
    RuntimeSpectralMapStats map;
    Vec3 center_pixel_xyz;
    double sensor_signal[4];
    RuntimeSpectralPathReadout forward[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
    RuntimeSpectralPathReadout camera[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
    double ior[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
} RuntimeSpectralReadout;
bool RuntimeSpectralLamp_PrepareModel(RuntimeSpectralLamp *lamp, double watts, double temperature,
                                      int peaks, const double center[4], const double sigma[4],
                                      const double weight[4]);
double RuntimeSpectral_Uniform64(uint64_t id);
double RuntimeSpectralCurve_Value(const RuntimeSpectralCurve *curve, double nm);
double RuntimeSpectralPencil_Index(const RuntimeSpectralPencil *p, double vacuum_nm);
bool RuntimeSpectralPencil_Trace(const RuntimeScene3D *scene, const RuntimeSpectralPencil *p,
                                 const RuntimeSpectralPencilLine *line, double index, Ray3D ray,
                                 bool camera, RuntimeSpectralPathReadout *out);
#endif
