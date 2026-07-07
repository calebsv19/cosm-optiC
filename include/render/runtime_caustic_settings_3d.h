#ifndef RENDER_RUNTIME_CAUSTIC_SETTINGS_3D_H
#define RENDER_RUNTIME_CAUSTIC_SETTINGS_3D_H

#include <stdbool.h>

#include "render/runtime_caustic_lens_transport_3d.h"

typedef enum {
    RUNTIME_CAUSTIC_MODE_OFF = 0,
    RUNTIME_CAUSTIC_MODE_ANALYTIC = 1,
    RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE = 2,
    RUNTIME_CAUSTIC_MODE_TRANSPORT = 3
} RuntimeCausticMode3D;

typedef enum {
    RUNTIME_CAUSTIC_CACHE_GRID_NONE = 0,
    RUNTIME_CAUSTIC_CACHE_GRID_VF3D_ALIGNED = 1
} RuntimeCausticCacheGridMode3D;

typedef enum {
    RUNTIME_CAUSTIC_CACHE_STATE_NONE = 0,
    RUNTIME_CAUSTIC_CACHE_STATE_REQUESTED_NOT_ALLOCATED = 1,
    RUNTIME_CAUSTIC_CACHE_STATE_ALLOCATED_EMPTY = 2,
    RUNTIME_CAUSTIC_CACHE_STATE_POPULATED = 3,
    RUNTIME_CAUSTIC_CACHE_STATE_SAMPLED = 4
} RuntimeCausticCacheState3D;

typedef enum {
    RUNTIME_CAUSTIC_TRANSPORT_EMISSION_TRIANGLE_TARGETS = 0,
    RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_SPHERE_LENS = 1,
    RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS = 2,
    RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_CYLINDER_LENS_FOCUSED = 3,
    RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_PRISM_LENS = 4,
    RUNTIME_CAUSTIC_TRANSPORT_EMISSION_ANALYTIC_BOWL_LENS = 5,
    RUNTIME_CAUSTIC_TRANSPORT_EMISSION_MESH_DIELECTRIC_LENS = 6
} RuntimeCausticTransportEmissionPolicy3D;

typedef struct {
    RuntimeCausticMode3D mode;
    bool volumeCacheEnabled;
    bool surfaceCacheEnabled;
    int sampleBudget;
    int maxPathDepth;
    RuntimeCausticTransportEmissionPolicy3D emissionPolicy;
    double surfaceRadianceScale;
    double surfaceFootprintScale;
    bool surfaceReceiverFallbackEnabled;
    RuntimeCausticCacheGridMode3D cacheGridMode;
    bool debugSummaryEnabled;
    bool debugExportEnabled;
    bool hasTraversalProfileOverride;
    RuntimeCausticLensTraversalProfile3D traversalProfileOverride;
} RuntimeCausticSettings3D;

typedef struct {
    RuntimeCausticMode3D mode;
    bool analyticSidecarRequested;
    bool volumeCacheRequested;
    bool surfaceCacheRequested;
    RuntimeCausticCacheState3D volumeCacheState;
    RuntimeCausticCacheState3D surfaceCacheState;
    RuntimeCausticTransportEmissionPolicy3D emissionPolicy;
    bool pathEmissionActive;
    bool transportReserved;
} RuntimeCausticReadback3D;

void RuntimeCausticSettings3D_Default(RuntimeCausticSettings3D* settings);
RuntimeCausticMode3D RuntimeCausticMode3D_FromLabel(const char* label);
const char* RuntimeCausticMode3D_Label(RuntimeCausticMode3D mode);
RuntimeCausticTransportEmissionPolicy3D
RuntimeCausticTransportEmissionPolicy3D_FromLabel(const char* label);
const char* RuntimeCausticTransportEmissionPolicy3D_Label(
    RuntimeCausticTransportEmissionPolicy3D policy);
const char* RuntimeCausticCacheGridMode3D_Label(RuntimeCausticCacheGridMode3D mode);
const char* RuntimeCausticCacheState3D_Label(RuntimeCausticCacheState3D state);
RuntimeCausticReadback3D RuntimeCausticSettings3D_Phase0Readback(
    const RuntimeCausticSettings3D* settings,
    bool analytic_sidecar_requested);

#endif
