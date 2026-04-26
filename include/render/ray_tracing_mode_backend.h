#ifndef RENDER_RAY_TRACING_MODE_BACKEND_H
#define RENDER_RAY_TRACING_MODE_BACKEND_H

#include <stdbool.h>

#include "camera/camera.h"
#include "config/config_manager.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/space_mode_adapter.h"

typedef enum {
    RAY_TRACING_BACKEND_CANONICAL_2D = 0,
    RAY_TRACING_BACKEND_CONTROLLED_3D = 1
} RayTracingBackendLane;

typedef enum {
    RAY_TRACING_ROUTE_CANONICAL_2D = 0,
    RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK = 1,
    RAY_TRACING_ROUTE_NATIVE_3D = 2
} RayTracingRouteFamily;

typedef enum {
    RAY_TRACING_VIEW_CARRIER_CANONICAL_2D = 0,
    RAY_TRACING_VIEW_CARRIER_COMPAT_3D = 1,
    RAY_TRACING_VIEW_CARRIER_NATIVE_3D = 2
} RayTracingViewCarrierFamily;

typedef struct {
    RayTracingRouteFamily routeFamily;
    SpaceMode requestedMode;
    SpaceMode projectionMode;
    RayTracingBackendLane backendLane;
    bool fallbackTo2DProjection;
    bool usesRuntime3DScaffold;
    double runtimeCameraZ;
    double rayOriginYOffset;
    int scaffoldPrimitiveCount;
    bool useTiles;
    int tileSize;
    int integratorMode;
    RayTracing3DIntegratorId integratorMode3D;
    bool integratorUses3DCatalog;
    bool buildIrradianceCache;
    bool tilePreviewEnabled;
    RayTracingIntegratorFallbackReason integratorFallbackReason;
} RayTracingRuntimeRoute;

typedef struct {
    RayTracingViewCarrierFamily family;
    SpaceModeViewContext viewContext;
    CameraPoint cameraXY;
    double cameraZ;
    double originX;
    double originY;
    double originZ;
    bool usesCompatProjectionFallback;
    bool hasRuntimeScaffoldCamera;
} RayTracingViewCarrier;

typedef enum {
    RAY_TRACING_PRIMITIVE_PREP_CANONICAL_2D = 0,
    RAY_TRACING_PRIMITIVE_PREP_COMPAT_3D_PLACEHOLDER = 1,
    RAY_TRACING_PRIMITIVE_PREP_NATIVE_3D = 2
} RayTracingPrimitivePrepFamily;

typedef struct {
    RayTracingPrimitivePrepFamily family;
    bool usesLegacySceneObjects;
    bool usesCompatPlaceholderObjects;
    bool hasRuntimeScaffoldPrimitives;
    int scaffoldPrimitiveCount;
    bool enableSurfaceMeshPrep;
    bool enableTriangleMeshPrep;
    bool enableUniformGrid2D;
    bool enableRay2DIntersections;
} RayTracingPrimitivePrepPlan;

typedef struct {
    bool valid;
    bool hasSceneBounds;
    bool boundsEnabled;
    bool boundsClampOnEdit;
    double boundsMinX;
    double boundsMinY;
    double boundsMinZ;
    double boundsMaxX;
    double boundsMaxY;
    double boundsMaxZ;
    bool hasConstructionPlane;
    char constructionPlaneMode[24];
    char constructionPlaneAxis[12];
    double constructionPlaneOffset;
    int digestPrimitiveCount;
    int planePrimitiveCount;
    int rectPrismPrimitiveCount;
    int scaffoldPrimitiveCount;
} RayTracingSceneDigestStatus;

RayTracingRuntimeRoute RayTracingModeBackend_ResolveRoute(void);
RayTracingViewCarrier RayTracingModeBackend_BuildViewCarrier(const Camera* camera,
                                                             int viewport_width,
                                                             int viewport_height,
                                                             const RayTracingRuntimeRoute* route);
RayTracingPrimitivePrepPlan RayTracingModeBackend_BuildPrimitivePrepPlan(
    const RayTracingRuntimeRoute* route,
    int scene_object_count);
RayTracingSceneDigestStatus RayTracingModeBackend_BuildSceneDigestStatus(
    const RayTracingRuntimeRoute* route);
SpaceModeViewContext RayTracingModeBackend_BuildViewContext(const Camera* camera,
                                                            int viewport_width,
                                                            int viewport_height,
                                                            const RayTracingRuntimeRoute* route);
RayTracingRouteFamily RayTracingModeBackend_RouteFamily(const RayTracingRuntimeRoute* route);
bool RayTracingModeBackend_IsCanonical2D(const RayTracingRuntimeRoute* route);
bool RayTracingModeBackend_IsCompat3DFallback(const RayTracingRuntimeRoute* route);
bool RayTracingModeBackend_IsNative3D(const RayTracingRuntimeRoute* route);
bool RayTracingModeBackend_IsControlled3D(const RayTracingRuntimeRoute* route);
const char* RayTracingModeBackend_Name(const RayTracingRuntimeRoute* route);
const char* RayTracingModeBackend_IntegratorStatusLabel(const RayTracingRuntimeRoute* route);

#endif
