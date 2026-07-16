#ifndef RENDER_RUNTIME_RAY_3D_H
#define RENDER_RUNTIME_RAY_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_scene_3d.h"

typedef struct {
    Vec3 origin;
    Vec3 direction;
} Ray3D;

typedef struct {
    double t;
    Vec3 position;
    /* Compatibility alias for shadingNormal. */
    Vec3 normal;
    Vec3 geometricNormal;
    Vec3 shadingNormal;
    int triangleIndex;
    int localTriangleIndex;
    int primitiveIndex;
    int sceneObjectIndex;
    RuntimePrimitive3DSourceRef source;
    double baryU;
    double baryV;
    double baryW;
    bool hasObjectTextureCoord;
    Vec3 objectTextureCoord;
} HitInfo3D;

typedef enum RuntimeRay3DTraceRoute {
    RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH = 0,
    RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY = 1,
    RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS = 2
} RuntimeRay3DTraceRoute;

typedef struct RuntimeRay3DRouteStats {
    RuntimeRay3DTraceRoute requestedRoute;
    RuntimeRay3DTraceRoute activeRoute;
    bool traceContextStatsOwned;
    bool sceneAccelerationTraceCallbackBound;
    uint64_t traceCalls;
    uint64_t flattenedTraceCalls;
    uint64_t tlasTraceCalls;
    uint64_t tlasTraceHits;
    uint64_t tlasTraceMisses;
    uint64_t tlasTraceUnready;
    uint64_t tlasTraceUnsupported;
    uint64_t tlasTraceErrors;
    uint64_t accelerationFailureCalls;
    uint64_t flattenedFallbackCalls;
    uint64_t parityCheckedRays;
    uint64_t parityMismatches;
    char lastAccelerationFailureStatus[32];
    char lastParityMismatchReason[128];
} RuntimeRay3DRouteStats;

typedef int (*RuntimeRay3DSceneAccelerationTraceFirstHitFn)(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
    HitInfo3D* out_hit);

typedef struct RuntimeRay3DTraceContext {
    RuntimeRay3DTraceRoute requestedRoute;
    RuntimeRay3DTraceRoute activeRoute;
    RuntimeRay3DSceneAccelerationTraceFirstHitFn sceneAccelerationTraceFirstHit;
    RuntimeRay3DRouteStats routeStats;
} RuntimeRay3DTraceContext;

Ray3D RuntimeRay3D_Make(Vec3 origin, Vec3 direction);
Ray3D RuntimeRay3D_MakeOffset(Vec3 origin,
                              Vec3 normal,
                              Vec3 direction,
                              [[fisics::dim(length)]] [[fisics::unit(meter)]] double epsilon);
void HitInfo3D_Reset(HitInfo3D* hit);
Vec3 HitInfo3D_OffsetNormal(const HitInfo3D* hit);
/*
 * Returns the BRDF normal for a direction while preserving the geometric
 * surface's lit hemisphere.  This prevents a coarse triangle with smooth
 * vertex normals from producing a false, diagonal light terminator.
 */
Vec3 HitInfo3D_ShadingNormalForDirection(const HitInfo3D* hit, Vec3 direction);
/*
 * Returns the closest shading normal to Ns whose perfect reflection of the
 * incoming view ray exits the geometric Ng hemisphere.  Ng remains the ray
 * offset/intersection authority; this helper only constrains BRDF reflection.
 */
Vec3 HitInfo3D_ShadingNormalForReflection(const HitInfo3D* hit, Vec3 view_direction);
void RuntimeRay3D_ResolveTriangleNormals(const RuntimeTriangle3D* triangle,
                                        const Ray3D* ray,
                                        double bary_u,
                                        double bary_v,
                                        double bary_w,
                                        Vec3* out_geometric_normal,
                                        Vec3* out_shading_normal);

bool RuntimeRay3D_IntersectTriangle(const Ray3D* ray,
                                    const RuntimeTriangle3D* triangle,
                                    int triangle_index,
                                    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
                                    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
                                    HitInfo3D* out_hit);

bool RuntimeRay3D_TraceSceneFirstHit(const RuntimeScene3D* scene,
                                     const Ray3D* ray,
                                     [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
                                     [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
                                     HitInfo3D* out_hit);
bool RuntimeRay3D_TraceSceneFirstHitWithContext(RuntimeRay3DTraceContext* context,
                                                const RuntimeScene3D* scene,
                                                const Ray3D* ray,
                                                [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
                                                [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
                                                HitInfo3D* out_hit);

const char* RuntimeRay3DTraceRouteLabel(RuntimeRay3DTraceRoute route);
RuntimeRay3DTraceRoute RuntimeRay3D_DefaultTraceRoute(void);
RuntimeRay3DTraceRoute RuntimeRay3D_CurrentTraceRoute(void);
void RuntimeRay3DTraceContext_Init(RuntimeRay3DTraceContext* context);
void RuntimeRay3DTraceContext_SetTraceRoute(RuntimeRay3DTraceContext* context,
                                            RuntimeRay3DTraceRoute route);
void RuntimeRay3DTraceContext_SetSceneAccelerationTraceFirstHit(
    RuntimeRay3DTraceContext* context,
    RuntimeRay3DSceneAccelerationTraceFirstHitFn trace_first_hit);
void RuntimeRay3DTraceContext_ResetRouteStats(RuntimeRay3DTraceContext* context);
void RuntimeRay3DTraceContext_SnapshotRouteStats(
    const RuntimeRay3DTraceContext* context,
    RuntimeRay3DRouteStats* out_stats);
void RuntimeRay3D_SetSceneAccelerationTraceFirstHit(
    RuntimeRay3DSceneAccelerationTraceFirstHitFn trace_first_hit);
void RuntimeRay3D_SetTraceRoute(RuntimeRay3DTraceRoute route);
void RuntimeRay3D_SetTraceRouteForTests(RuntimeRay3DTraceRoute route);
void RuntimeRay3D_ResetTraceRouteForTests(void);
void RuntimeRay3D_ResetRouteStats(void);
void RuntimeRay3D_SnapshotRouteStats(RuntimeRay3DRouteStats* out_stats);

#endif
