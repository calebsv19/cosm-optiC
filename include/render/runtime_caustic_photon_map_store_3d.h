#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_MAP_STORE_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_MAP_STORE_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_integration_3d.h"

typedef struct {
    bool initialized;
    RuntimeCausticPhotonMap3D surfaceMap;
    RuntimeCausticBeamMap3D beamMap;
    RuntimeCausticPhotonMapLifecycleState3D lifecycle;
    RuntimeCausticPhotonMapPopulationReadback3D population;
    uint64_t generation;
    uint64_t rebuildCount;
    uint64_t reuseCount;
} RuntimeCausticPhotonMapStore3D;

void RuntimeCausticPhotonMapStore3D_Init(RuntimeCausticPhotonMapStore3D* store);
void RuntimeCausticPhotonMapStore3D_Free(RuntimeCausticPhotonMapStore3D* store);
bool RuntimeCausticPhotonMapStore3D_Begin(
    RuntimeCausticPhotonMapStore3D* store,
    const RuntimeCausticPhotonMapLifecycleInput3D* input,
    RuntimeCausticPhotonMapLifecycleReadback3D* out_readback);
void RuntimeCausticPhotonMapStore3D_CommitPopulation(
    RuntimeCausticPhotonMapStore3D* store,
    const RuntimeCausticPhotonMapPopulationReadback3D* population);
#endif
