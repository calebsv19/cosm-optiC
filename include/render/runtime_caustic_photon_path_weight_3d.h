#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_PATH_WEIGHT_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_PATH_WEIGHT_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_photon_settings_3d.h"

bool RuntimeCausticPhotonPathWeight3D_ApplyThroughputRatio(
    Vec3 weight_before,
    Vec3 throughput_before,
    Vec3 throughput_after,
    Vec3* out_weight);

#endif
