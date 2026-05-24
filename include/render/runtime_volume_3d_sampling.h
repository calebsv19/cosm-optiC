#ifndef RENDER_RUNTIME_VOLUME_3D_SAMPLING_H
#define RENDER_RUNTIME_VOLUME_3D_SAMPLING_H

#include <stdbool.h>

#include "render/runtime_ray_3d.h"
#include "render/runtime_volume_3d.h"

bool RuntimeVolume3D_HasSampleableDensity(const RuntimeVolumeAttachment3D* attachment);
bool RuntimeVolume3D_ClipRayToBounds(const RuntimeVolumeAttachment3D* attachment,
                                     const Ray3D* ray,
                                     double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                     double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                     double* out_t_enter,
                                     double* out_t_exit);
float RuntimeVolume3D_SampleDensityAtPosition(const RuntimeVolumeAttachment3D* attachment,
                                              Vec3 position);

#endif
