#ifndef RENDER_RAY_TRACING_MODE_BACKEND_H
#define RENDER_RAY_TRACING_MODE_BACKEND_H

#include <stdbool.h>

#include "camera/camera.h"
#include "config/config_manager.h"
#include "render/space_mode_adapter.h"

typedef enum {
    RAY_TRACING_BACKEND_CANONICAL_2D = 0,
    RAY_TRACING_BACKEND_CONTROLLED_3D = 1
} RayTracingBackendLane;

typedef struct {
    SpaceMode requestedMode;
    SpaceMode projectionMode;
    RayTracingBackendLane backendLane;
    bool fallbackTo2DProjection;
    bool useTiles;
    int tileSize;
    int integratorMode;
    bool buildIrradianceCache;
    bool tilePreviewEnabled;
} RayTracingRuntimeRoute;

RayTracingRuntimeRoute RayTracingModeBackend_ResolveRoute(void);
SpaceModeViewContext RayTracingModeBackend_BuildViewContext(const Camera* camera,
                                                            int viewport_width,
                                                            int viewport_height,
                                                            const RayTracingRuntimeRoute* route);
bool RayTracingModeBackend_IsControlled3D(const RayTracingRuntimeRoute* route);
const char* RayTracingModeBackend_Name(const RayTracingRuntimeRoute* route);

#endif
