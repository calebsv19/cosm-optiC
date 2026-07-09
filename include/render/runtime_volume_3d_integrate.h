#ifndef RENDER_RUNTIME_VOLUME_3D_INTEGRATE_H
#define RENDER_RUNTIME_VOLUME_3D_INTEGRATE_H

#include <stdbool.h>

#include "render/runtime_visibility_3d.h"
#include "render/runtime_volume_3d.h"

bool RuntimeVolume3D_HasActiveExtinction(const RuntimeVolumeAttachment3D* attachment);
void RuntimeVolume3DMaterial_ResetTuning(void);
void RuntimeVolume3DMaterial_SetDensityScale(double scale);
void RuntimeVolume3DMaterial_SetDensityGamma(double gamma);
void RuntimeVolume3DMaterial_SetAbsorptionGain(double gain);
void RuntimeVolume3DMaterial_SetOpacityClamp(double clamp_value);
double RuntimeVolume3DMaterial_RemapDensity(double density);
double RuntimeVolume3DMaterial_ExtinctionDensity(double density);

RuntimeVisibility3DTransmittance RuntimeVolume3D_TransmittanceAlongRayRGB(
    const RuntimeVolumeAttachment3D* attachment,
    const Ray3D* ray,
    double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]]);

#endif
