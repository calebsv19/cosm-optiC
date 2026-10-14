#ifndef RENDER_RUNTIME_VOLUME_3D_OPTICS_H
#define RENDER_RUNTIME_VOLUME_3D_OPTICS_H
#include <stdbool.h>

/* Value-owned render policy; cache bytes and authoring state remain separate. */
typedef struct {
    double density_scale,density_gamma,absorption_gain,opacity_clamp,opacity;
    bool physical_smoke, sky_illumination, cell_centered;
    double reference_concentration_kg_m3, mass_extinction_m2_kg, scattering_albedo, anisotropy, sky_error_budget;
} RuntimeVolume3DMaterialTuning;
typedef struct {double strength,caustic_strength,step_scale,tint[3];} RuntimeVolume3DScatterTuning;
typedef struct {
    RuntimeVolume3DMaterialTuning material;
    RuntimeVolume3DScatterTuning scatter;
} RuntimeVolume3DOpticalSnapshot;
#endif
