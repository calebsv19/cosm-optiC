#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_PHOTON_PREPARE_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_PHOTON_PREPARE_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_settings_3d.h"
#include "render/runtime_native_3d_render.h"

void runtime_native_3d_prepare_populate_photon_render_prep(
    RuntimeNative3DPreparedFrame* frame);
uint64_t RuntimeNative3DRender_CausticPhotonSurfaceRecordCount(void);
bool RuntimeNative3DRender_CausticPhotonSurfaceRecordAt(
    uint64_t index,
    RuntimeCausticPhotonMapRecord3D* out_record);

#endif
