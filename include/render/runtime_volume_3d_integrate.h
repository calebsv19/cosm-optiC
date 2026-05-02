#ifndef RENDER_RUNTIME_VOLUME_3D_INTEGRATE_H
#define RENDER_RUNTIME_VOLUME_3D_INTEGRATE_H

#include <stdbool.h>

#include "render/runtime_visibility_3d.h"
#include "render/runtime_volume_3d.h"

bool RuntimeVolume3D_HasActiveExtinction(const RuntimeVolumeAttachment3D* attachment);

RuntimeVisibility3DTransmittance RuntimeVolume3D_TransmittanceAlongRayRGB(
    const RuntimeVolumeAttachment3D* attachment,
    const Ray3D* ray,
    double t_min,
    double t_max);

#endif
