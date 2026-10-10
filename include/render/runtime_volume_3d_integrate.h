#ifndef RENDER_RUNTIME_VOLUME_3D_INTEGRATE_H
#define RENDER_RUNTIME_VOLUME_3D_INTEGRATE_H

#include <fisics/extensions.h>

#include <stdbool.h>

#include "render/runtime_visibility_3d.h"
#include "render/runtime_volume_3d.h"

bool RuntimeVolume3D_HasActiveExtinction(const RuntimeVolumeAttachment3D* attachment);
typedef struct {
    double density_scale,density_gamma,absorption_gain,opacity_clamp,opacity;
    bool physical_smoke, sky_illumination, cell_centered;
    double reference_concentration_kg_m3, mass_extinction_m2_kg, scattering_albedo, anisotropy, sky_error_budget;
} RuntimeVolume3DMaterialTuning;
bool RuntimeVolume3DMaterial_SetSmokeOptics(double reference, double extinction, double albedo, double anisotropy, bool sky);
void RuntimeVolume3DMaterial_SetCellCentered(bool enabled);
bool RuntimeVolume3DMaterial_SetSkyErrorBudget(double budget);
RuntimeVolume3DMaterialTuning RuntimeVolume3DMaterial_GetTuning(void);
void RuntimeVolume3DMaterial_RestoreTuning(RuntimeVolume3DMaterialTuning tuning);
void RuntimeVolume3DMaterial_ResetTuning(void);
void RuntimeVolume3DMaterial_SetDensityScale(double scale);
void RuntimeVolume3DMaterial_SetDensityGamma(double gamma);
void RuntimeVolume3DMaterial_SetAbsorptionGain(double gain);
void RuntimeVolume3DMaterial_SetOpacityClamp(double clamp_value);
void RuntimeVolume3DMaterial_SetOpacity(double opacity);
double RuntimeVolume3DMaterial_RemapDensity(double density);
double RuntimeVolume3DMaterial_ExtinctionDensity(double density);

RuntimeVisibility3DTransmittance RuntimeVolume3D_TransmittanceAlongRayRGB(
    const RuntimeVolumeAttachment3D* attachment,
    const Ray3D* ray,
    double t_min FISICS_DIM(length) FISICS_UNIT(meter),
    double t_max FISICS_DIM(length) FISICS_UNIT(meter));

#endif
