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
    double scattered_support_m; /* Optional maximum support for scattered receiver records only. */
    int scattered_neighbors;
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
    SPECTRAL_RECONSTRUCTION_ADAPTIVE,
    SPECTRAL_RECONSTRUCTION_KERNEL
} RuntimeSpectralReconstructionMode;
typedef struct {
    RuntimeSpectralVolumeKind kind;
    RuntimeSpectralReconstructionMode reconstruction;
    bool disable_camera_cache; /* Reference camera traversal/source switch; false preserves exact reuse. */
    double phase_tolerance; /* Relative HG bound; zero requests exact per-ray evaluation. */
    RuntimeSpectralCurve absorption, scattering;
    double g, cell_m;
    double reconstruction_cell_m; /* Optional finer evaluation lattice; photon support stays cell_m. */
    double higher_scatter_support_m; /* Kernel mode: optional sparse higher-order bandwidth. */
    double local_refinement_step_m; /* Camera quadrature only; no change to medium/averaging. */
    Vec3 local_refinement_lo, local_refinement_hi;
    Vec3 lo, hi;
    int frame_index;
    char path[4096], profile_hash[65];
    char mie_phase_path[4096];
} RuntimeSpectralVolumeConfig;
typedef struct RuntimeSpectralVolume RuntimeSpectralVolume;
typedef struct {
    bool enabled;
    int x, y;
} RuntimeSpectralProbeConfig;
typedef struct {
    bool enabled, indirect;
    int grid;
    char panel_id[64];
    RuntimeSpectralCurve panel_reflectance;
    int sky_grid; /* Zero selects the existing point source. */
    Vec3 sky_axis;
    double sky_half_angle_deg;
    double sky_radiance[RUNTIME_SPECTRAL_PENCIL_MAX_LINES];
} RuntimeSpectralDiffuseConfig;
typedef struct {
    double radiance, deposited_watts, vacuum_nm, support_m, kernel_per_m2;
    Vec3 incident;
    int record, source;
    bool scattered;
} RuntimeSpectralDepositProbe;
typedef struct {
    double direct, indirect, sky_direct, mapped_surface, single_scatter, multiple_scatter;
    double mean_receiver_reflectance;
    RuntimeSpectralDepositProbe largest_deposit;
    double camera_throughput;
    unsigned long long candidates;
    unsigned samples, interfaces, depth_limited;
} RuntimeSpectralProbeBand;
typedef struct {
    bool valid, hit;
    int primitive, panel_accepted, panel_occluded;
    Vec3 position, xyz, signed_rgb;
    double direct_geometry, indirect_geometry, base_support_m, scattered_support_m;
    RuntimeSpectralProbeBand bands[SPECTRAL_MAP_MAX_BANDS];
} RuntimeSpectralProbeReadout;
enum { SPECTRAL_PATH_MATERIALS = 32, SPECTRAL_PATH_PROBES = 8, SPECTRAL_PATH_DEPTHS = 13 };
typedef struct {
    char object_id[64];
    RuntimeSpectralCurve reflectance;
    double roughness;
    bool ggx, two_sided;
} RuntimeSpectralPathMaterial;
typedef struct {
    char name[32];
    int x, y;
} RuntimeSpectralPathProbeConfig;
typedef struct {
    bool enabled, sky;
    int caustic_passes, caustic_photons_per_pass, caustic_seed, caustic_bands;
    double caustic_absorption;
    bool caustic_guided, glass_split_first, glass_split_bounded;
    bool medium_split_first, glass_split_camera_exit, fog_survival, volume_light_mis, volume_light_only, volume_rectangle_mis;
    bool native_sources, native_materials; /* Explicitly reported RGB compatibility adapters. */
    bool linear_components; /* Final jobs export unexposed float64 XYZ unless opted out. */
    int samples, max_depth, seed, material_count, probe_count, output_component;
    Vec3 origin, axis_u, axis_v, normal, sky_axis;
    double width, height, sky_angle;
    int line_count, knot_count;
    RuntimeSpectralLine lines[16];
    RuntimeSpectralKnot knots[32];
    RuntimeSpectralPathMaterial materials[SPECTRAL_PATH_MATERIALS];
    RuntimeSpectralPathProbeConfig probes[SPECTRAL_PATH_PROBES];
    bool medium;
    Vec3 medium_lo, medium_hi;
    RuntimeSpectralCurve sigma_a, sigma_s;
    double g, caustic_cell_m;
    bool camera_qmc; /* Explicit candidate; defaults remain hashed until qualified. */
    bool sample_audit_enabled;
    int sample_audit_x, sample_audit_y, sample_audit_limit;
    bool trace_enabled;
    int trace_x, trace_y, trace_sample;
} RuntimeSpectralSurfacePathConfig;
enum { SPECTRAL_PATH_TRACE_EVENTS = 128 };
typedef struct {
    char event[24], object_id[64], occluder_id[64], reason[32], component[24];
    Vec3 position, direction, normal;
    bool integrated_light, distant_source, wavelength_is_line;
    double wavelength_pdf, wavelength_weight, continuation_weight, selection_pdf, stratum_pdf;
    double wavelength_nm, beta_in, beta_out, reflectance, distance;
    double bsdf_pdf, light_pdf, mis_weight, contribution, survival;
    double bsdf_fcos, source_weight, transmittance, absorption_per_m, scattering_per_m;
    int surface_depth, volume_events;
} RuntimeSpectralPathEvent;
typedef struct {
    Vec3 direct, indirect, volume, total, caustic, caustic_bounce, volume_caustic;
    double luminance_variance, mean_nm, depth_radiance[SPECTRAL_PATH_DEPTHS];
    Vec3 nonscattered_indirect[5]; /* Disjoint subset of indirect; see diagnostics. */
    Vec3 volume_order[4]; /* Ordinary camera scattering: 1, 2, 3-4, 5+. */
} RuntimeSpectralPathProbe;
enum { SPECTRAL_SAMPLE_AUDIT_LIMIT = 16 };
typedef struct {
    int sample;
    double wavelength_nm, wavelength_weight, pixel_x, pixel_y;
    Vec3 xyz;
    double component_y[11]; /* Unaveraged sample XYZ-Y; volume subsets are not additive. */
    unsigned long long source_hits, volume_events;
} RuntimeSpectralSampleAudit;
typedef struct {
    unsigned long long attempts, angular, angular_missed, rectangle, missed, occluded, visible, source_hits;
    double nee_sum, nee_sum2, nee_max, hit_sum, hit_sum2, hit_max;
} RuntimeSpectralVolumeLightBucket;
typedef struct {
    RuntimeSpectralVolumeLightBucket volume_light[16];
    Vec3 film_nonscattered_indirect[5];
    unsigned long long surface_nee_after_multiple_scattering, surface_nee_pdf_mismatches;
    double surface_nee_pdf_relative_error_max;
    unsigned long long paths, surface_events, volume_events, absorbed, escaped, limited;
    unsigned long long surface_absorbed, medium_absorbed, backface, bsdf_rejected, roulette;
    unsigned long long event_limited, depth_limited, source_hit, dark_escape;
    Vec3 film_direct, film_indirect, film_volume, film_caustic, film_caustic_bounce, film_replaced_source;
    Vec3 film_volume_caustic;
    Vec3 film_volume_order[4];
    unsigned long long volume_order_contributions[4];
    bool components_written;
    unsigned long long components_bytes;
    char components_file[256],components_sha256[65];
    unsigned long long medium_splits, internal_interface_splits, specular_roulette_skipped;
    unsigned long long fog_roulette_events, fog_roulette_survived;
    unsigned long long glass_reflections, glass_transmissions, glass_tir, replaced_source_hits;
    unsigned long long replaced_volume_source_hits;
    unsigned long long first_interface_splits;
    double caustic_radius_m;
    unsigned long long pixels;
    int source_count;
    struct {char id[64];int kind,radiometry;} sources[32];
    int sample_audit_count, sample_audit_observed;
    Vec3 sample_audit_sum;
    RuntimeSpectralSampleAudit sample_audit[SPECTRAL_SAMPLE_AUDIT_LIMIT];
    int trace_count;
    bool trace_truncated;
    RuntimeSpectralPathEvent trace[SPECTRAL_PATH_TRACE_EVENTS];
    RuntimeSpectralPathProbe probes[SPECTRAL_PATH_PROBES];
} RuntimeSpectralSurfacePathReadout;
typedef struct {
    RuntimeSpectralSurfacePathConfig surface_path;
    bool legacy_direct; /* Explicit opt-out; omission resolves ordinary V3 surface transport. */
    RuntimeSpectralSamplingConfig sampling;
    RuntimeSpectralDiffuseConfig diffuse;
    RuntimeSpectralProbeConfig probe;
    RuntimeSpectralVolumeConfig volume;
    RuntimeSpectralVolume *active_volume; /* borrowed during headless preparation/render */
    RuntimeSpectralMapConfig map;
    double exterior_ior, glass_roughness;
    bool soft_display, exact_surface_observer;
    int strip_x, strip_y, strip_count;
    int pixel_grid; /* Regular subpixel quadrature. Zero selects one centered ray. */
    int fresnel_camera_samples; /* Zero preserves ballistic smooth glass. */
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
    int fresnel_reflections, total_internal_reflections;
    double glass_absorbed_w, medium_absorbed_w, medium_scattered_w;
    double inscattered, single_scatter;
    bool volume_absorbed;
    bool receiver, receiver_front, depth_limited;
} RuntimeSpectralPathReadout;

typedef struct {
    double nm, width_nm, watts, center_radiance;
    double center_single, center_multiple;
    double launched_w, receiver_w, escaped_w, absorbed_w, limited_w, optical_loss_w;
    double glass_absorbed_w, medium_absorbed_w, medium_scattered_w;
    unsigned long long fresnel_reflections, total_internal_reflections;
    double first_scatter_w, higher_scatter_w, cosine_sum;
    unsigned long long collisions;
    Vec3 moment;
    unsigned count;
} RuntimeSpectralBand;
typedef struct {
    char key[65], launch_key[65];
    int band_count, launched, deposits, direct_arrivals, missed, depth_limited;
    int expanded_deposits;
    bool fresnel_transport;
    bool adaptive_scattered;
    double scattered_support_m; /* Optional maximum support for scattered receiver records only. */
    int scattered_neighbors;
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
    RuntimeSpectralSurfacePathReadout surface_path;
    RuntimeSpectralProbeReadout probe;
    RuntimeSpectralMapStats map;
    Vec3 center_pixel_xyz;
    Vec3 strip_xyz[512];
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
