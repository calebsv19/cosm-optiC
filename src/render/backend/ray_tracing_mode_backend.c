#include "render/ray_tracing_mode_backend.h"

#include "import/runtime_scene_bridge.h"
#include "render/integrators/integrator_common.h"
#include <math.h>

RayTracingRuntimeRoute RayTracingModeBackend_ResolveRoute(void) {
    RayTracingRuntimeRoute route;
    RuntimeSceneBridge3DScaffoldState scaffold = {0};
    route.requestedMode = SpaceModeAdapter_ResolveMode(animSettings.spaceMode);
    route.projectionMode = route.requestedMode;
    route.backendLane = RAY_TRACING_BACKEND_CANONICAL_2D;
    route.fallbackTo2DProjection = false;
    route.usesRuntime3DScaffold = false;
    route.runtimeCameraZ = 0.0;
    route.rayOriginYOffset = 0.0;
    route.scaffoldPrimitiveCount = 0;

    route.integratorMode = animSettings.integratorMode;
    if (route.integratorMode < 0) route.integratorMode = 0;
    if (route.integratorMode > 2) route.integratorMode = 2;

    route.useTiles = animSettings.useTiledRenderer;
    route.tileSize = route.useTiles ? ClampTileSize(animSettings.tileSize) : 0;
    route.buildIrradianceCache = (route.integratorMode == 1);
    route.tilePreviewEnabled =
        route.useTiles && animSettings.tilePreviewEnabled && (route.integratorMode == 1);

    if (route.requestedMode == SPACE_MODE_3D) {
        runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
        route.backendLane = RAY_TRACING_BACKEND_CONTROLLED_3D;
        route.projectionMode = SPACE_MODE_2D;
        route.fallbackTo2DProjection = true;
        route.usesRuntime3DScaffold = scaffold.valid;
        route.runtimeCameraZ = scaffold.camera_z;
        route.scaffoldPrimitiveCount =
            scaffold.box_count + scaffold.plane_count + scaffold.triangle_mesh_count;
        if (scaffold.has_camera_seed) {
            double z_mag = fabs(scaffold.camera_z);
            route.rayOriginYOffset = fmin(240.0, z_mag * 0.25);
            if (scaffold.camera_z < 0.0) route.rayOriginYOffset = -route.rayOriginYOffset;
        }
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
