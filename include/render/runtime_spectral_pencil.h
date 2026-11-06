#ifndef RENDER_RUNTIME_SPECTRAL_PENCIL_H
#define RENDER_RUNTIME_SPECTRAL_PENCIL_H

#include <stdbool.h>
#include <stdint.h>
#include "render/runtime_spectral_sampling.h"
#include "render/runtime_spectral_profile.h"
#include "render/runtime_ray_3d.h"

/* Local headless calibration instrument. Exact quadrature over discrete atoms. */
#define RUNTIME_SPECTRAL_PENCIL_MAX_LINES 16
typedef struct RuntimeSpectralCameraCache RuntimeSpectralCameraCache;
typedef struct {
    double vacuum_nm, watts, absorption_per_m;
    uint64_t sample_id;
    int band;
    RuntimeSpectralCameraCache *camera_cache; /* Borrowed, private to this camera region. */
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
    bool adaptive_scattered;
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

typedef enum {
    SPECTRAL_VOLUME_NONE,
    SPECTRAL_VOLUME_BOX,
    SPECTRAL_VOLUME_VF3D,
    SPECTRAL_VOLUME_WATER
} RuntimeSpectralVolumeKind;
typedef enum {
    SPECTRAL_RECONSTRUCTION_BOX,
    SPECTRAL_RECONSTRUCTION_CONTINUOUS,
    SPECTRAL_RECONSTRUCTION_ADAPTIVE
} RuntimeSpectralReconstructionMode;
typedef struct {
    RuntimeSpectralVolumeKind kind;
    RuntimeSpectralReconstructionMode reconstruction;
    bool disable_camera_cache; /* Reference camera traversal/source switch; false preserves exact reuse. */
    double phase_tolerance; /* Relative HG bound; zero requests exact per-ray evaluation. */
    RuntimeSpectralCurve absorption, scattering;
    double g, cell_m;
    Vec3 lo, hi;
    int frame_index;
    char path[4096], profile_hash[65];
} RuntimeSpectralVolumeConfig;
typedef struct RuntimeSpectralVolume RuntimeSpectralVolume;
typedef struct {
    RuntimeSpectralSamplingConfig sampling;
    RuntimeSpectralVolumeConfig volume;
    RuntimeSpectralVolume *active_volume; /* borrowed during headless preparation/render */
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
    int interfaces, primitive, scatters;
    double inscattered, single_scatter;
    bool volume_absorbed;
    bool receiver, receiver_front, depth_limited;
} RuntimeSpectralPathReadout;

typedef struct {
    double nm, width_nm, watts, center_radiance;
    double center_single, center_multiple;
    double launched_w, receiver_w, escaped_w, absorbed_w, limited_w, optical_loss_w;
    double first_scatter_w, higher_scatter_w, cosine_sum;
    unsigned long long collisions;
    Vec3 moment;
    unsigned count;
} RuntimeSpectralBand;
typedef struct {
    char key[65];
    int band_count, launched, deposits, direct_arrivals, missed, depth_limited;
    int expanded_deposits;
    bool adaptive_scattered;
    unsigned long long candidates;
    double build_ms, film_ms;
    bool reused;
    char volume_digest[65], volume_source_sha[65], volume_frame_sha[65];
    int volume_frame;
    double volume_time, volume_dt;
    unsigned long long reconstruction_bytes, reconstruction_peak_bytes;
    unsigned long long reconstruction_nodes, reconstruction_links;
    unsigned long long reconstruction_cached_queries, reconstruction_exact_queries;
    unsigned long long reconstruction_spatial_hits, reconstruction_spatial_misses;
    int reconstruction_regions;
    unsigned long long camera_cache_hits, camera_cache_misses, camera_cache_segments, camera_cache_overflows;
    size_t camera_cache_bytes;
    RuntimeSpectralBand bands[SPECTRAL_MAP_MAX_BANDS];
} RuntimeSpectralMapStats;
typedef struct {
    RuntimeSpectralSamplingStats sampling;
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
