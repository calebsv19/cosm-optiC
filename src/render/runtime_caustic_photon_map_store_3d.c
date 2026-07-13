#include "render/runtime_caustic_photon_map_store_3d.h"

#include <string.h>

void RuntimeCausticPhotonMapStore3D_Init(RuntimeCausticPhotonMapStore3D* store) {
    if (!store) return;
    memset(store, 0, sizeof(*store));
    RuntimeCausticPhotonMap3D_Init(&store->surfaceMap);
    RuntimeCausticBeamMap3D_Init(&store->beamMap);
    RuntimeCausticPhotonMapLifecycle3D_Init(&store->lifecycle);
    store->initialized = true;
}

void RuntimeCausticPhotonMapStore3D_Free(RuntimeCausticPhotonMapStore3D* store) {
    if (!store) return;
    if (store->initialized) {
        RuntimeCausticBeamMap3D_Free(&store->beamMap);
        RuntimeCausticPhotonMap3D_Free(&store->surfaceMap);
    }
    memset(store, 0, sizeof(*store));
}

bool RuntimeCausticPhotonMapStore3D_Begin(
    RuntimeCausticPhotonMapStore3D* store,
    const RuntimeCausticPhotonMapLifecycleInput3D* input,
    RuntimeCausticPhotonMapLifecycleReadback3D* out_readback) {
    if (!store || !input || !out_readback) return false;
    if (!store->initialized) RuntimeCausticPhotonMapStore3D_Init(store);
    RuntimeCausticPhotonMapLifecycle3D_Evaluate(input, &store->lifecycle, out_readback);
    if (out_readback->rebuilt) {
        RuntimeCausticBeamMap3D_Free(&store->beamMap);
        RuntimeCausticPhotonMap3D_Free(&store->surfaceMap);
        RuntimeCausticBeamMap3D_Init(&store->beamMap);
        RuntimeCausticPhotonMap3D_Init(&store->surfaceMap);
        memset(&store->population, 0, sizeof(store->population));
        store->generation += 1u;
        store->rebuildCount += 1u;
    } else if (out_readback->reused) {
        store->reuseCount += 1u;
    }
    out_readback->generation = store->generation;
    out_readback->rebuildCount = store->rebuildCount;
    out_readback->reuseCount = store->reuseCount;
    return true;
}

void RuntimeCausticPhotonMapStore3D_CommitPopulation(
    RuntimeCausticPhotonMapStore3D* store,
    const RuntimeCausticPhotonMapPopulationReadback3D* population) {
    if (!store || !population) return;
    store->population = *population;
}
