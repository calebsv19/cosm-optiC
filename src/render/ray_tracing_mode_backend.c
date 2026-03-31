#include "render/ray_tracing_mode_backend.h"

#include "render/integrators/integrator_common.h"

RayTracingRuntimeRoute RayTracingModeBackend_ResolveRoute(void) {
    RayTracingRuntimeRoute route;
    route.requestedMode = SpaceModeAdapter_ResolveMode(animSettings.spaceMode);
    route.projectionMode = route.requestedMode;
    route.backendLane = RAY_TRACING_BACKEND_CANONICAL_2D;
    route.fallbackTo2DProjection = false;

    route.integratorMode = animSettings.integratorMode;
    if (route.integratorMode < 0) route.integratorMode = 0;
    if (route.integratorMode > 2) route.integratorMode = 2;

    route.useTiles = animSettings.useTiledRenderer;
    route.tileSize = route.useTiles ? ClampTileSize(animSettings.tileSize) : 0;
    route.buildIrradianceCache = (route.integratorMode == 1);
    route.tilePreviewEnabled =
        route.useTiles && animSettings.tilePreviewEnabled && (route.integratorMode == 1);

    if (route.requestedMode == SPACE_MODE_3D) {
        route.backendLane = RAY_TRACING_BACKEND_CONTROLLED_3D;
        route.projectionMode = SPACE_MODE_2D;
        route.fallbackTo2DProjection = true;
    }

    return route;
}

SpaceModeViewContext RayTracingModeBackend_BuildViewContext(const Camera* camera,
                                                            int viewport_width,
                                                            int viewport_height,
                                                            const RayTracingRuntimeRoute* route) {
    if (!route) {
        return SpaceModeAdapter_BuildViewContext(camera, viewport_width, viewport_height);
    }
    return SpaceModeAdapter_BuildViewContextForMode(route->projectionMode,
                                                    camera,
                                                    viewport_width,
                                                    viewport_height);
}

bool RayTracingModeBackend_IsControlled3D(const RayTracingRuntimeRoute* route) {
    if (!route) return false;
    return route->backendLane == RAY_TRACING_BACKEND_CONTROLLED_3D;
}

const char* RayTracingModeBackend_Name(const RayTracingRuntimeRoute* route) {
    if (!route) return "2D";
    return RayTracingModeBackend_IsControlled3D(route) ? "3D(controlled)" : "2D";
}
