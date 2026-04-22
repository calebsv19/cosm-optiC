#include "render/ray_tracing_mode_backend.h"

#include "import/runtime_scene_bridge.h"
#include "render/integrators/integrator_common.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

RayTracingRuntimeRoute RayTracingModeBackend_ResolveRoute(void) {
    RayTracingRuntimeRoute route;
    RuntimeSceneBridge3DScaffoldState scaffold = {0};
    bool compat_fallback = false;
    route.requestedMode = SpaceModeAdapter_ResolveMode(animSettings.spaceMode);
    route.projectionMode = route.requestedMode;
    route.backendLane = RAY_TRACING_BACKEND_CANONICAL_2D;
    route.routeFamily = RAY_TRACING_ROUTE_CANONICAL_2D;
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
        route.routeFamily = RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK;
        route.projectionMode = SPACE_MODE_2D;
        compat_fallback = true;
        route.usesRuntime3DScaffold = scaffold.valid;
        route.runtimeCameraZ = sceneSettings.cameraZ;
        route.scaffoldPrimitiveCount =
            scaffold.box_count + scaffold.plane_count + scaffold.triangle_mesh_count;
        if (scaffold.has_camera_seed || fabs(sceneSettings.cameraZ) > 1e-6) {
            double z_mag = fabs(route.runtimeCameraZ);
            route.rayOriginYOffset = fmin(240.0, z_mag * 0.25);
            if (route.runtimeCameraZ < 0.0) route.rayOriginYOffset = -route.rayOriginYOffset;
        }
    }

    route.fallbackTo2DProjection = compat_fallback;
    return route;
}

RayTracingViewCarrier RayTracingModeBackend_BuildViewCarrier(const Camera* camera,
                                                             int viewport_width,
                                                             int viewport_height,
                                                             const RayTracingRuntimeRoute* route) {
    RayTracingViewCarrier carrier;
    const Camera* active_camera = camera;
    Camera default_camera = {0};

    if (!active_camera) {
        active_camera = &default_camera;
    }

    carrier.family = RAY_TRACING_VIEW_CARRIER_CANONICAL_2D;
    carrier.viewContext.mode = SpaceModeAdapter_ResolveMode(SPACE_MODE_2D);
    carrier.viewContext.camera = active_camera;
    carrier.viewContext.viewportWidth = viewport_width;
    carrier.viewContext.viewportHeight = viewport_height;
    carrier.cameraXY.x = active_camera->x;
    carrier.cameraXY.y = active_camera->y;
    carrier.cameraZ = 0.0;
    carrier.originX = active_camera->x;
    carrier.originY = active_camera->y;
    carrier.originZ = 0.0;
    carrier.usesCompatProjectionFallback = false;
    carrier.hasRuntimeScaffoldCamera = false;

    if (!route) {
        return carrier;
    }

    carrier.viewContext.mode = SpaceModeAdapter_ResolveMode(route->projectionMode);
    carrier.viewContext.camera = active_camera;
    carrier.viewContext.viewportWidth = viewport_width;
    carrier.viewContext.viewportHeight = viewport_height;
    carrier.usesCompatProjectionFallback = route->fallbackTo2DProjection;
    carrier.hasRuntimeScaffoldCamera = route->usesRuntime3DScaffold;

    switch (route->routeFamily) {
        case RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK:
            carrier.family = RAY_TRACING_VIEW_CARRIER_COMPAT_3D;
            carrier.cameraZ = route->runtimeCameraZ;
            carrier.originY = active_camera->y + route->rayOriginYOffset;
            carrier.originZ = route->runtimeCameraZ;
            break;
        case RAY_TRACING_ROUTE_NATIVE_3D:
            carrier.family = RAY_TRACING_VIEW_CARRIER_NATIVE_3D;
            carrier.cameraZ = route->runtimeCameraZ;
            carrier.originY = active_camera->y + route->rayOriginYOffset;
            carrier.originZ = route->runtimeCameraZ;
            break;
        case RAY_TRACING_ROUTE_CANONICAL_2D:
        default:
            carrier.family = RAY_TRACING_VIEW_CARRIER_CANONICAL_2D;
            break;
    }

    return carrier;
}

RayTracingPrimitivePrepPlan RayTracingModeBackend_BuildPrimitivePrepPlan(
    const RayTracingRuntimeRoute* route,
    int scene_object_count) {
    RayTracingPrimitivePrepPlan plan;
    bool has_objects = (scene_object_count > 0);

    plan.family = RAY_TRACING_PRIMITIVE_PREP_CANONICAL_2D;
    plan.usesLegacySceneObjects = has_objects;
    plan.usesCompatPlaceholderObjects = false;
    plan.hasRuntimeScaffoldPrimitives = false;
    plan.scaffoldPrimitiveCount = 0;
    plan.enableSurfaceMeshPrep = has_objects;
    plan.enableTriangleMeshPrep = has_objects;
    plan.enableUniformGrid2D = true;
    plan.enableRay2DIntersections = true;

    if (!route) {
        return plan;
    }

    switch (route->routeFamily) {
        case RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK:
            plan.family = RAY_TRACING_PRIMITIVE_PREP_COMPAT_3D_PLACEHOLDER;
            plan.usesCompatPlaceholderObjects = route->usesRuntime3DScaffold;
            plan.hasRuntimeScaffoldPrimitives =
                route->usesRuntime3DScaffold && (route->scaffoldPrimitiveCount > 0);
            plan.scaffoldPrimitiveCount = route->scaffoldPrimitiveCount;
            break;
        case RAY_TRACING_ROUTE_NATIVE_3D:
            plan.family = RAY_TRACING_PRIMITIVE_PREP_NATIVE_3D;
            plan.usesLegacySceneObjects = false;
            plan.usesCompatPlaceholderObjects = false;
            plan.hasRuntimeScaffoldPrimitives =
                route->usesRuntime3DScaffold && (route->scaffoldPrimitiveCount > 0);
            plan.scaffoldPrimitiveCount = route->scaffoldPrimitiveCount;
            plan.enableSurfaceMeshPrep = false;
            plan.enableTriangleMeshPrep = false;
            plan.enableUniformGrid2D = false;
            plan.enableRay2DIntersections = false;
            break;
        case RAY_TRACING_ROUTE_CANONICAL_2D:
        default:
            break;
    }
    return plan;
}

RayTracingSceneDigestStatus RayTracingModeBackend_BuildSceneDigestStatus(
    const RayTracingRuntimeRoute* route) {
    RayTracingSceneDigestStatus status;
    RuntimeSceneBridge3DDigestState digest = {0};
    memset(&status, 0, sizeof(status));

    if (!route || !RayTracingModeBackend_IsCompat3DFallback(route)) {
        return status;
    }

    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    status.valid = digest.valid;
    status.scaffoldPrimitiveCount = route->scaffoldPrimitiveCount;
    if (!digest.valid) {
        return status;
    }

    status.hasSceneBounds = digest.has_scene_bounds;
    status.boundsEnabled = digest.bounds_enabled;
    status.boundsClampOnEdit = digest.bounds_clamp_on_edit;
    status.boundsMinX = digest.bounds_min_x;
    status.boundsMinY = digest.bounds_min_y;
    status.boundsMinZ = digest.bounds_min_z;
    status.boundsMaxX = digest.bounds_max_x;
    status.boundsMaxY = digest.bounds_max_y;
    status.boundsMaxZ = digest.bounds_max_z;
    status.hasConstructionPlane = digest.has_construction_plane;
    snprintf(status.constructionPlaneMode,
             sizeof(status.constructionPlaneMode),
             "%s",
             digest.construction_plane_mode);
    snprintf(status.constructionPlaneAxis,
             sizeof(status.constructionPlaneAxis),
             "%s",
             digest.construction_plane_axis);
    status.constructionPlaneOffset = digest.construction_plane_offset;
    status.digestPrimitiveCount = digest.primitive_count;
    status.planePrimitiveCount = digest.plane_primitive_count;
    status.rectPrismPrimitiveCount = digest.rect_prism_primitive_count;
    return status;
}

SpaceModeViewContext RayTracingModeBackend_BuildViewContext(const Camera* camera,
                                                            int viewport_width,
                                                            int viewport_height,
                                                            const RayTracingRuntimeRoute* route) {
    RayTracingViewCarrier carrier = RayTracingModeBackend_BuildViewCarrier(camera,
                                                                           viewport_width,
                                                                           viewport_height,
                                                                           route);
    return carrier.viewContext;
}

bool RayTracingModeBackend_IsControlled3D(const RayTracingRuntimeRoute* route) {
    return RayTracingModeBackend_IsCompat3DFallback(route);
}

RayTracingRouteFamily RayTracingModeBackend_RouteFamily(const RayTracingRuntimeRoute* route) {
    if (!route) return RAY_TRACING_ROUTE_CANONICAL_2D;
    return route->routeFamily;
}

bool RayTracingModeBackend_IsCanonical2D(const RayTracingRuntimeRoute* route) {
    return RayTracingModeBackend_RouteFamily(route) == RAY_TRACING_ROUTE_CANONICAL_2D;
}

bool RayTracingModeBackend_IsCompat3DFallback(const RayTracingRuntimeRoute* route) {
    return RayTracingModeBackend_RouteFamily(route) == RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK;
}

bool RayTracingModeBackend_IsNative3D(const RayTracingRuntimeRoute* route) {
    return RayTracingModeBackend_RouteFamily(route) == RAY_TRACING_ROUTE_NATIVE_3D;
}

const char* RayTracingModeBackend_Name(const RayTracingRuntimeRoute* route) {
    if (!route) return "2D(canonical)";
    if (RayTracingModeBackend_IsNative3D(route)) return "3D(native)";
    if (RayTracingModeBackend_IsCompat3DFallback(route)) return "3D(compat-fallback)";
    return "2D(canonical)";
}
