#ifndef RENDER_RUNTIME_NATIVE_3D_RENDER_PHOTON_PREPARE_H
#define RENDER_RUNTIME_NATIVE_3D_RENDER_PHOTON_PREPARE_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_settings_3d.h"
#include "render/runtime_native_3d_render.h"

/* Preparation is serial. Snapshot only after the renderer has joined workers.
 * Counts and sums include preflight; latest is the most recent real preparation. */
typedef struct {
    bool available;
    uint64_t preparationCount;
    double fingerprintWallMs, mapBuildWallMs, emissionWallMs;
    double pathTransportWallMs, supportPreparationWallMs, probeAndDepositWallMs;
    RuntimeCausticPhotonRenderCallsiteReadback3D latest;
    RuntimeCausticPhotonSampleSupportReadback3D support;
} RuntimeCausticPhotonPreparationStats3D;
void RuntimeNative3DRender_SnapshotPhotonPreparation(
    RuntimeCausticPhotonPreparationStats3D* out);

void runtime_native_3d_prepare_populate_photon_render_prep(
    RuntimeNative3DPreparedFrame* frame);
uint64_t RuntimeNative3DRender_CausticPhotonSurfaceRecordCount(void);
bool RuntimeNative3DRender_CausticPhotonSurfaceRecordAt(
    uint64_t index,
    RuntimeCausticPhotonMapRecord3D* out_record);
bool runtime_native_3d_prepare_photon_volume_requested(void);

#endif
