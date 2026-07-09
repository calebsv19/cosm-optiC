#ifndef RENDER_RUNTIME_CAUSTIC_BOOTSTRAP_3D_H
#define RENDER_RUNTIME_CAUSTIC_BOOTSTRAP_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "math/vec3.h"
#include "render/runtime_caustic_settings_3d.h"
#include "render/runtime_caustic_volume_cache_3d.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"
#include "render/runtime_scene_3d.h"

typedef struct {
    bool enabled;
    bool temporaryAnalyticBridge;
    RuntimeCausticMode3D mode;
    bool volumeCacheRequested;
    int sampleBudget;
} RuntimeCausticBootstrap3DRequestState;

typedef struct {
    bool enabled;
    bool temporaryAnalyticBridge;
    bool requested;
    bool probeBuilt;
    bool cacheAllocated;
    bool populated;
    uint64_t candidateCellCount;
    uint64_t depositAttemptCount;
    uint64_t depositAcceptedCount;
    uint64_t depositRejectedCount;
    double totalRadianceR;
    double totalRadianceG;
    double totalRadianceB;
    double maxRadiance;
    Vec3 funnelStart;
    Vec3 funnelEnd;
    double funnelLength;
    double sourceRadius;
    double receiverRadius;
    RuntimeDisneyV2CausticSidecarProbe3D probe;
    RuntimeCausticVolumeCacheDiagnostics3D cache;
} RuntimeCausticBootstrap3DDiagnostics;

void RuntimeCausticBootstrap3D_ResetRequestState(void);
void RuntimeCausticBootstrap3D_SetRequestState(const RuntimeCausticSettings3D* settings);
RuntimeCausticBootstrap3DRequestState RuntimeCausticBootstrap3D_RequestState(void);
bool RuntimeCausticBootstrap3D_PopulateAnalyticVolumeCache(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticBootstrap3DDiagnostics* out_diagnostics);

#endif
