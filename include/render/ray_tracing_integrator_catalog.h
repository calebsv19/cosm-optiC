#ifndef RENDER_RAY_TRACING_INTEGRATOR_CATALOG_H
#define RENDER_RAY_TRACING_INTEGRATOR_CATALOG_H

#include <stdbool.h>

#include "config/config_manager.h"

typedef enum {
    RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT = 0,
    RAY_TRACING_2D_INTEGRATOR_HYBRID = 1,
    RAY_TRACING_2D_INTEGRATOR_DIRECT_LIGHT = 2
} RayTracing2DIntegratorId;

typedef enum {
    RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT = 0,
    RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE = 1,
    RAY_TRACING_3D_INTEGRATOR_MATERIAL = 2,
    RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY = 3,
    RAY_TRACING_3D_INTEGRATOR_DISNEY = 4,
    RAY_TRACING_3D_INTEGRATOR_DISNEY_V2 = 5
} RayTracing3DIntegratorId;

typedef enum {
    RAY_TRACING_INTEGRATOR_FALLBACK_NONE = 0,
    RAY_TRACING_INTEGRATOR_FALLBACK_2D_INVALID = 1,
    RAY_TRACING_INTEGRATOR_FALLBACK_3D_INVALID = 2,
    RAY_TRACING_INTEGRATOR_FALLBACK_3D_UNSHIPPED = 3,
    RAY_TRACING_INTEGRATOR_FALLBACK_3D_COMPAT_DIRECT_LIGHT = 4
} RayTracingIntegratorFallbackReason;

typedef struct {
    int raw2D;
    int raw3D;
    int active2D;
    int active3D;
    bool uses3DCatalog;
    bool showPathToggles;
    int visibleCount;
    const char* buttonLabel;
} RayTracingIntegratorMenuState;

typedef struct {
    int raw2D;
    int raw3D;
    int normalized2D;
    int normalized3D;
    int activeLegacy2DMode;
    int active3DMode;
    bool uses3DCatalog;
    bool showPathToggles;
    bool buildIrradianceCache;
    bool tilePreviewEnabled;
    RayTracingIntegratorFallbackReason fallbackReason;
} RayTracingResolvedIntegratorState;

int RayTracingIntegratorCatalog_Clamp2D(int value);
int RayTracingIntegratorCatalog_Clamp3D(int value);
int RayTracingIntegratorCatalog_Default3D(void);
bool RayTracingIntegratorCatalog_Is3DShipped(int value);
int RayTracingIntegratorCatalog_Clamp3DToShipped(int value);
const char* RayTracingIntegratorCatalog_3DStatusLabel(int value);
void RayTracingIntegratorCatalog_NormalizeAnimationConfig(AnimationConfig* cfg);
RayTracingIntegratorMenuState RayTracingIntegratorCatalog_BuildMenuState(
    const AnimationConfig* cfg);
void RayTracingIntegratorCatalog_CycleActiveSelection(AnimationConfig* cfg);
RayTracingResolvedIntegratorState RayTracingIntegratorCatalog_ResolveRuntime(
    const AnimationConfig* cfg,
    SpaceMode requested_mode,
    bool native_route_active,
    bool compat_route_active,
    bool use_tiles_requested,
    bool tile_preview_requested);
const char* RayTracingIntegratorCatalog_FallbackReasonLabel(
    RayTracingIntegratorFallbackReason reason);

#endif
