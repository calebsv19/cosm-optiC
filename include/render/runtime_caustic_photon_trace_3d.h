#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_TRACE_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_TRACE_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_caustic_lens_transport_3d.h"
#include "render/runtime_caustic_photon_settings_3d.h"

enum {
    RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_EVENTS = 16,
    RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS = 8
};

typedef struct {
    uint32_t maxDepth;
    double minTransportWeightLuma;
} RuntimeCausticPhotonTraceSettings3D;

typedef struct {
    uint64_t photonId;
    uint32_t depth;
    Vec3 position;
    Vec3 direction;
    /* Dimensionless accumulated transport response. Emitted photon power is
     * carried separately by throughput and must not drive path roulette. */
    Vec3 transportWeight;
    Vec3 throughput;
    double pathPdf;
    RuntimeCausticPhotonRejectReason3D rejectReason;
    bool active;
    bool terminated;
} RuntimeCausticPhotonPathState3D;

typedef struct {
    bool valid;
    RuntimeCausticPhotonSample3D sample;
    RuntimeCausticPhotonPathState3D initialState;
    RuntimeCausticPhotonPathState3D finalState;
    uint32_t eventCount;
    RuntimeCausticPhotonEvent3D events[RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_EVENTS];
    uint32_t dielectricEventCount;
    RuntimeCausticPhotonDielectricEvent3D dielectricEvents[
        RUNTIME_CAUSTIC_PHOTON_TRACE_MAX_DIELECTRIC_EVENTS];
    Vec3 postExitOrigin;
    Vec3 postExitDirection;
    double insideDistance;
    double receiverPlaneT;
    Vec3 receiverCrossing;
    RuntimeCausticPhotonDebugRecord3D debug;
    RuntimeCausticPhotonPathProvenance3D provenance;
} RuntimeCausticPhotonTrace3D;

void RuntimeCausticPhotonTrace3D_DefaultSettings(
    RuntimeCausticPhotonTraceSettings3D* settings);
void RuntimeCausticPhotonTrace3D_InitPathState(
    const RuntimeCausticPhotonSample3D* sample,
    RuntimeCausticPhotonPathState3D* out_state);
bool RuntimeCausticPhotonTrace3D_FromLensPath(
    const RuntimeCausticLensPath3D* lens_path,
    const RuntimeCausticPhotonSample3D* sample,
    const RuntimeCausticPhotonTraceSettings3D* settings,
    RuntimeCausticPhotonTrace3D* out_trace);
bool RuntimeCausticPhotonTrace3D_TraceMeshDielectricPath(
    const RuntimeCausticLensPath3D* mesh_dielectric_path,
    const RuntimeCausticPhotonSample3D* emitted_sample,
    const RuntimeCausticPhotonTraceSettings3D* settings,
    RuntimeCausticPhotonTrace3D* out_trace);
bool RuntimeCausticPhotonTrace3D_TraceSphereLens(
    const RuntimeCausticSphereLens3DDescriptor* sphere,
    const RuntimeCausticSphereLens3DLight* light,
    const RuntimeCausticSphereLens3DSample* sample,
    const RuntimeCausticPhotonTraceSettings3D* settings,
    uint64_t photon_id,
    uint32_t rng_seed,
    int scene_object_index,
    int primitive_index,
    RuntimeCausticPhotonTrace3D* out_trace);

#endif
