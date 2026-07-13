#ifndef RENDER_RUNTIME_CAUSTIC_PHOTON_PATH_TRANSPORT_3D_H
#define RENDER_RUNTIME_CAUSTIC_PHOTON_PATH_TRANSPORT_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_photon_scene_trace_3d.h"

typedef struct {
    RuntimeCausticPhotonSceneTraceSettings3D sceneTrace;
    RuntimePathDepthPolicy3D depthPolicy;
    bool applyRoulette;
    bool continueTotalInternalReflection;
    RuntimeCausticPhotonMediumFailurePolicy3D mediumFailurePolicy;
    bool hasInitialMediumStack;
    RuntimeCausticPhotonMediumStack3D initialMediumStack;
} RuntimeCausticPhotonPathTransportSettings3D;

void RuntimeCausticPhotonPathTransport3D_DefaultSettings(
    RuntimeCausticPhotonPathTransportSettings3D* settings);
bool RuntimeCausticPhotonPathTransport3D_Trace(
    const RuntimeScene3D* scene,
    const RuntimeCausticPhotonSample3D* sample,
    const RuntimeCausticPhotonPathTransportSettings3D* settings,
    RuntimeCausticPhotonSceneTrace3D* out_trace);

#endif
