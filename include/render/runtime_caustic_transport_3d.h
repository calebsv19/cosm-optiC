#ifndef RENDER_RUNTIME_CAUSTIC_TRANSPORT_3D_H
#define RENDER_RUNTIME_CAUSTIC_TRANSPORT_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "math/vec3.h"
#include "render/runtime_caustic_settings_3d.h"
#include "render/runtime_caustic_surface_cache_3d.h"
#include "render/runtime_caustic_volume_cache_3d.h"
#include "render/runtime_scene_3d.h"

typedef struct {
    bool enabled;
    RuntimeCausticMode3D mode;
    bool volumeCacheRequested;
    bool surfaceCacheRequested;
    int sampleBudget;
    int maxPathDepth;
    RuntimeCausticTransportEmissionPolicy3D emissionPolicy;
    double surfaceRadianceScale;
    double surfaceFootprintScale;
    bool surfaceReceiverFallbackEnabled;
    bool debugExportEnabled;
} RuntimeCausticTransport3DRequestState;

typedef struct {
    bool requested;
    bool active;
    bool volumeCacheSuppressedNoSampleableVolume;
    bool cacheAllocated;
    bool surfaceCacheAllocated;
    uint64_t lightCount;
    uint64_t evaluatedPathCount;
    uint64_t emittedPathCount;
    uint64_t analyticSphereLensResolvedCount;
    uint64_t analyticSphereLensRejectedCount;
    uint64_t analyticSphereLensEvaluatedPathCount;
    uint64_t analyticSphereLensEmittedPathCount;
    double analyticSphereLensSampleWeight;
    double analyticSphereLensTotalSampleWeight;
    uint64_t analyticCylinderLensResolvedCount;
    uint64_t analyticCylinderLensRejectedCount;
    uint64_t analyticCylinderLensEvaluatedPathCount;
    uint64_t analyticCylinderLensEmittedPathCount;
    double analyticCylinderLensSampleWeight;
    double analyticCylinderLensTotalSampleWeight;
    uint64_t analyticPrismLensResolvedCount;
    uint64_t analyticPrismLensRejectedCount;
    uint64_t analyticPrismLensEvaluatedPathCount;
    uint64_t analyticPrismLensEmittedPathCount;
    double analyticPrismLensSampleWeight;
    double analyticPrismLensTotalSampleWeight;
    uint64_t analyticBowlLensResolvedCount;
    uint64_t analyticBowlLensRejectedCount;
    uint64_t analyticBowlLensEvaluatedPathCount;
    uint64_t analyticBowlLensEmittedPathCount;
    double analyticBowlLensSampleWeight;
    double analyticBowlLensTotalSampleWeight;
    uint64_t transparentHitCount;
    uint64_t specularEventCount;
    uint64_t volumeSegmentCount;
    uint64_t surfaceReceiverTraceMissCount;
    uint64_t surfaceReceiverDepthRejectCount;
    uint64_t surfaceReceiverHitCount;
    uint64_t surfaceReceiverFallbackCount;
    uint64_t depositAttemptCount;
    uint64_t depositAcceptedCount;
    uint64_t depositRejectedCount;
    double totalRadianceR;
    double totalRadianceG;
    double totalRadianceB;
    double maxRadiance;
    RuntimeCausticVolumeCacheDiagnostics3D cache;
    RuntimeCausticSurfaceCacheDiagnostics3D surfaceCache;
} RuntimeCausticTransport3DDiagnostics;

void RuntimeCausticTransport3D_ResetRequestState(void);
void RuntimeCausticTransport3D_SetRequestState(const RuntimeCausticSettings3D* settings);
RuntimeCausticTransport3DRequestState RuntimeCausticTransport3D_RequestState(void);
bool RuntimeCausticTransport3D_PopulateVolumeCache(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* cache,
    RuntimeCausticTransport3DDiagnostics* out_diagnostics);
bool RuntimeCausticTransport3D_PopulateCaches(
    const RuntimeScene3D* scene,
    RuntimeCausticVolumeCache3D* volume_cache,
    RuntimeCausticSurfaceCache3D* surface_cache,
    RuntimeCausticTransport3DDiagnostics* out_diagnostics);

#endif
