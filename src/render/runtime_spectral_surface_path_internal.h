#ifndef RUNTIME_SPECTRAL_SURFACE_PATH_INTERNAL_H
#define RUNTIME_SPECTRAL_SURFACE_PATH_INTERNAL_H
#include "render/runtime_spectral_surface_path.h"
#include "render/runtime_specular_bsdf_3d.h"
#include <math.h>

typedef struct {
    uint64_t key, stream_key;
    unsigned dimension, event, sample_index, cached_pair;
    double pair[2];
    bool qmc;
} PathRandom;
PathRandom spectral_path_random_init(uint64_t pixel, unsigned seed, unsigned sample, bool qmc);
void spectral_path_random_event(PathRandom *, unsigned event);
void spectral_path_random_fork(PathRandom *, uint64_t salt);
typedef struct {
    double direct, indirect, volume, caustic, caustic_bounce, volume_caustic, replaced_source;
    double depth[SPECTRAL_PATH_DEPTHS];
    double volume_order[4];
    double nonscattered_indirect[5];
    bool failed;
} PathSignal;
typedef struct {
    Vec3 direction;
    double distance, pdf, weight, selection_pdf, stratum_pdf;
    bool valid, delta;
    int source_index;
} SourceSample;
typedef struct {
    double rho, diffuse, specular, probability;
    RuntimePrincipledBSDF3D ggx;
    bool two_sided;
} PathMaterial;
typedef struct SpectralComponentWriter SpectralComponentWriter;
SpectralComponentWriter *spectral_components_begin(const char *, int, int);
bool spectral_components_pixel(SpectralComponentWriter *,const RuntimeSpectralPathProbe *);
bool spectral_components_finish(SpectralComponentWriter *,const char *,bool,RuntimeSpectralSurfacePathReadout *);
void spectral_components_abort(SpectralComponentWriter *);
struct RuntimeSpectralSurfacePath {
    char component_path[4096];
    bool component_overwrite;
    SpectralComponentWriter *component_writer;
    const RuntimeSpectralPencil *profile;
    const RuntimeScene3D *scene;
    RuntimeSpectralProfile *spectrum;
    int mapping[SPECTRAL_PATH_MATERIALS], source_count;
    double native_normalization;
    int glass_primitive;
    bool volume_caustics;
    RuntimeSpectralPencil *photon_profile;
    RuntimeSpectralSurfaceMap *caustic_map;
    RuntimeSpectralMapStats caustic_stats;
};
typedef struct { double ballistic, scattered, distance, conditional_pdf; } SpectralMediumSplit;
SpectralMediumSplit spectral_medium_split(double sigma_a,double sigma_s,double length,double u);
void spectral_nonscattered_add(PathSignal *,double,int,int,bool,int,int);
void spectral_surface_pdf_audit(const RuntimeSpectralSurfacePath *,RuntimeSpectralSurfacePathReadout *,Vec3,const SourceSample *,int);
struct json_object *spectral_nonscattered_json(const RuntimeSpectralSurfacePathConfig *,const RuntimeSpectralSurfacePathReadout *);
void spectral_path_signal_accumulate(PathSignal *,const PathSignal *);
int spectral_path_max_branches(const RuntimeSpectralSurfacePathConfig *);
double spectral_path_roulette_survival(const RuntimeSpectralSurfacePathConfig *,double beta,int volume_events);
PathSignal spectral_path_trace(const RuntimeSpectralSurfacePath *,Ray3D,double,RuntimeSpectralMeasure,
    PathRandom *,RuntimeSpectralSurfacePathReadout *,RuntimeSpectralSurfacePathReadout *);
double spectral_path_random(PathRandom *);
void spectral_path_audit_sample(RuntimeSpectralSurfacePathReadout *, int limit, int sample,
    double nm, double weight, double px, double py, Vec3 color, const PathSignal *,
    unsigned long long source_hits, unsigned long long volume_events);
struct json_object *spectral_path_audit_json(const RuntimeSpectralSurfacePathConfig *,
    const RuntimeSpectralSurfacePathReadout *);
typedef struct {
    Vec3 axis, u_axis, v_axis, normal;
    double one_minus_cosine, pdf;
    double x0,x1,y0,y1,h,solid_angle,b0,b1,angle_offset;
    bool active, light_only, rectangle;
} SpectralVolumeProposal;
bool spectral_volume_rectangle_init(SpectralVolumeProposal *,const RuntimeSpectralSurfacePathConfig *,Vec3);
Vec3 spectral_volume_sample_proposal(const SpectralVolumeProposal *,double,double);
void spectral_volume_diagnostic(const RuntimeSpectralSurfacePathConfig *,RuntimeSpectralSurfacePathReadout *,Vec3,int,int,int,double);
struct json_object *spectral_volume_diagnostics_json(const RuntimeSpectralSurfacePathReadout *);
double spectral_volume_phase(double g,double cosine);
SpectralVolumeProposal spectral_volume_proposal(const RuntimeSpectralSurfacePath *,Vec3,int order);
double spectral_volume_light_pdf(const SpectralVolumeProposal *,Vec3);
double spectral_volume_direction_pdf(double,Vec3,Vec3,const SpectralVolumeProposal *);
Vec3 spectral_volume_sample_direction(double,Vec3,const SpectralVolumeProposal *,double,double,double *,double *);
SourceSample spectral_volume_sample_light(const RuntimeSpectralSurfacePath *,Vec3,const SpectralVolumeProposal *,PathRandom *);
Vec3 spectral_path_direction(Vec3, double, double);
double spectral_path_source_intersection(const RuntimeSpectralSurfacePath *, Ray3D);
double spectral_path_source_pdf(const RuntimeSpectralSurfacePath *, Ray3D, double);
SourceSample spectral_path_sample_source(const RuntimeSpectralSurfacePath *, Vec3, double, PathRandom *);
bool spectral_path_material(const RuntimeSpectralSurfacePath *, const HitInfo3D *, double, PathMaterial *);
double spectral_path_material_eval(const PathMaterial *, const HitInfo3D *, Vec3, Vec3, double *);
bool spectral_path_material_sample(const PathMaterial *, const HitInfo3D *, Vec3, PathRandom *, Vec3 *, double *, double *);
RuntimeSpectralPathEvent *spectral_path_event(RuntimeSpectralSurfacePathReadout *, const char *, const char *, const char *,
    Vec3, Vec3, double, double, double, int, int);
#endif
