#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_PATH_SCHEDULER_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_PATH_SCHEDULER_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_photon_integration_3d.h"

bool RuntimeCausticPhotonPathScheduler3D_PopulateOwnedMaps(
    const RuntimeScene3D* scene,
    RuntimeCausticPhotonMap3D* surface_map,
    RuntimeCausticBeamMap3D* beam_map,
    const RuntimeCausticLensShape3D* emission_lenses,
    uint32_t emission_lens_count,
    const RuntimeCausticPhotonIntegrationSettings3D* settings,
    RuntimeCausticPhotonMapPopulationReadback3D* out_readback);

#endif
