#ifndef RENDER_RUNTIME_CAUSTIC_SETTINGS_3D_H
#define RENDER_RUNTIME_CAUSTIC_SETTINGS_3D_H

#include <stdbool.h>

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

typedef struct {
    RuntimeCausticMode3D mode;
    bool volumeCacheEnabled;
    bool surfaceCacheEnabled;
    int sampleBudget;
    int maxPathDepth;
    double surfaceRadianceScale;
    double surfaceFootprintScale;
    bool surfaceReceiverFallbackEnabled;
    RuntimeCausticCacheGridMode3D cacheGridMode;
    bool debugSummaryEnabled;
    bool debugExportEnabled;
} RuntimeCausticSettings3D;

typedef struct {
    RuntimeCausticMode3D mode;
    bool analyticSidecarRequested;
    bool volumeCacheRequested;
    bool surfaceCacheRequested;
    RuntimeCausticCacheState3D volumeCacheState;
    RuntimeCausticCacheState3D surfaceCacheState;
    bool pathEmissionActive;
    bool transportReserved;
} RuntimeCausticReadback3D;

void RuntimeCausticSettings3D_Default(RuntimeCausticSettings3D* settings);
RuntimeCausticMode3D RuntimeCausticMode3D_FromLabel(const char* label);
const char* RuntimeCausticMode3D_Label(RuntimeCausticMode3D mode);
const char* RuntimeCausticCacheGridMode3D_Label(RuntimeCausticCacheGridMode3D mode);
const char* RuntimeCausticCacheState3D_Label(RuntimeCausticCacheState3D state);
RuntimeCausticReadback3D RuntimeCausticSettings3D_Phase0Readback(
    const RuntimeCausticSettings3D* settings,
    bool analytic_sidecar_requested);

#endif
