#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_EMIT_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_EMIT_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_light_set_3d.h"

typedef struct {
    uint64_t sampleBudget;
    uint32_t baseSeed;
    uint64_t firstPhotonId;
    double defaultQueryRadius;
} RuntimeCausticPhotonEmissionSettings3D;

typedef struct {
    uint64_t requestedSampleBudget;
    uint64_t emittedPhotonCount;
    uint64_t rejectedPhotonCount;
    uint64_t sourceCount;
    uint64_t enabledSourceCount;
    uint64_t mapStoreAttemptCount;
    uint64_t mapStoreAcceptedCount;
    uint64_t mapStoreRejectedCount;
    uint32_t firstSeed;
    uint32_t lastSeed;
    double sourcePdfSum;
    Vec3 totalEmittedFlux;
    Vec3 totalStoredSurfaceFlux;
    RuntimeCausticPhotonRejectReason3D lastRejectReason;
} RuntimeCausticPhotonEmissionDiagnostics3D;

typedef struct {
    RuntimeCausticPhotonSample3D* samples;
    uint64_t sampleCapacity;
    uint64_t sampleCount;
    bool ownsSamples;
    RuntimeCausticPhotonEmissionDiagnostics3D diagnostics;
} RuntimeCausticPhotonEmissionBatch3D;

void RuntimeCausticPhotonEmission3D_DefaultSettings(
    RuntimeCausticPhotonEmissionSettings3D* settings);
void RuntimeCausticPhotonEmission3D_InitBatch(
    RuntimeCausticPhotonEmissionBatch3D* batch);
bool RuntimeCausticPhotonEmission3D_AllocateBatch(
    RuntimeCausticPhotonEmissionBatch3D* batch,
    uint64_t sample_capacity);
void RuntimeCausticPhotonEmission3D_ClearBatch(
    RuntimeCausticPhotonEmissionBatch3D* batch);
void RuntimeCausticPhotonEmission3D_FreeBatch(
    RuntimeCausticPhotonEmissionBatch3D* batch);
bool RuntimeCausticPhotonEmission3D_EmitFromLightSet(
    RuntimeCausticPhotonEmissionBatch3D* batch,
    const RuntimeLightSet3D* light_set,
    const RuntimeCausticPhotonEmissionSettings3D* settings,
    RuntimeCausticPhotonEmissionDiagnostics3D* out_diagnostics);
bool RuntimeCausticPhotonEmission3D_StoreSurfaceProxyRecords(
    RuntimeCausticPhotonMap3D* map,
    const RuntimeCausticPhotonEmissionBatch3D* batch,
    double query_radius,
    Vec3 receiver_normal,
    int receiver_scene_object_index,
    int receiver_primitive_index,
    int receiver_triangle_index,
    RuntimeCausticPhotonEmissionDiagnostics3D* out_diagnostics);

#endif
