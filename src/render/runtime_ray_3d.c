#include "render/runtime_ray_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const double kRuntimeRay3DDeterminantEpsilon = 1e-9;
static const double kRuntimeRay3DMinimumOffsetEpsilon = 1e-9;
static const RuntimeRay3DTraceRoute kRuntimeRay3DDefaultTraceRoute =
    RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS;
static RuntimeRay3DTraceContext gRuntimeRay3DDefaultTraceContext;
static bool gRuntimeRay3DDefaultTraceContextInitialized = false;

static void runtime_ray_3d_counter_increment(uint64_t* counter) {
    if (!counter) return;
    (void)__atomic_fetch_add(counter, 1u, __ATOMIC_RELAXED);
}

static uint64_t runtime_ray_3d_counter_load(const uint64_t* counter) {
    if (!counter) return 0u;
    return __atomic_load_n(counter, __ATOMIC_RELAXED);
}

typedef enum RuntimeSceneAcceleration3DTraceStatusForRayRoute {
    RUNTIME_RAY_3D_ACCEL_TRACE_UNREADY = 0,
    RUNTIME_RAY_3D_ACCEL_TRACE_MISS = 1,
    RUNTIME_RAY_3D_ACCEL_TRACE_HIT = 2,
    RUNTIME_RAY_3D_ACCEL_TRACE_UNSUPPORTED = 3,
    RUNTIME_RAY_3D_ACCEL_TRACE_ERROR = 4
} RuntimeSceneAcceleration3DTraceStatusForRayRoute;

static RuntimeRay3DTraceRoute runtime_ray_3d_sanitize_trace_route(
    RuntimeRay3DTraceRoute route) {
    if (route < RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH ||
        route > RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS) {
        return RuntimeRay3D_DefaultTraceRoute();
    }
    return route;
}

static void runtime_ray_3d_trace_context_refresh_stats_header(
    RuntimeRay3DTraceContext* context) {
    if (!context) return;
    context->routeStats.requestedRoute = context->requestedRoute;
    context->routeStats.activeRoute = context->activeRoute;
    context->routeStats.traceContextStatsOwned = true;
    context->routeStats.sceneAccelerationTraceCallbackBound =
        context->sceneAccelerationTraceFirstHit != NULL;
}

void RuntimeRay3DTraceContext_Init(RuntimeRay3DTraceContext* context) {
    if (!context) return;
    memset(context, 0, sizeof(*context));
    context->requestedRoute = RuntimeRay3D_DefaultTraceRoute();
    context->activeRoute = context->requestedRoute;
    runtime_ray_3d_trace_context_refresh_stats_header(context);
}

static RuntimeRay3DTraceContext* runtime_ray_3d_default_trace_context(void) {
    if (!gRuntimeRay3DDefaultTraceContextInitialized) {
        RuntimeRay3DTraceContext_Init(&gRuntimeRay3DDefaultTraceContext);
        gRuntimeRay3DDefaultTraceContextInitialized = true;
    }
    return &gRuntimeRay3DDefaultTraceContext;
}

void RuntimeRay3DTraceContext_SetTraceRoute(RuntimeRay3DTraceContext* context,
                                            RuntimeRay3DTraceRoute route) {
    if (!context) return;
    route = runtime_ray_3d_sanitize_trace_route(route);
    context->requestedRoute = route;
    context->activeRoute = route;
    runtime_ray_3d_trace_context_refresh_stats_header(context);
}

void RuntimeRay3DTraceContext_SetSceneAccelerationTraceFirstHit(
    RuntimeRay3DTraceContext* context,
    RuntimeRay3DSceneAccelerationTraceFirstHitFn trace_first_hit) {
    if (!context) return;
    context->sceneAccelerationTraceFirstHit = trace_first_hit;
    runtime_ray_3d_trace_context_refresh_stats_header(context);
}

void RuntimeRay3DTraceContext_ResetRouteStats(RuntimeRay3DTraceContext* context) {
    RuntimeRay3DTraceRoute requested_route;
    RuntimeRay3DTraceRoute active_route;
    RuntimeRay3DSceneAccelerationTraceFirstHitFn trace_first_hit;
    if (!context) return;
    requested_route = context->requestedRoute;
    active_route = context->activeRoute;
    trace_first_hit = context->sceneAccelerationTraceFirstHit;
    memset(&context->routeStats, 0, sizeof(context->routeStats));
    context->requestedRoute = requested_route;
    context->activeRoute = active_route;
    context->sceneAccelerationTraceFirstHit = trace_first_hit;
    runtime_ray_3d_trace_context_refresh_stats_header(context);
}

void RuntimeRay3DTraceContext_SnapshotRouteStats(
    const RuntimeRay3DTraceContext* context,
    RuntimeRay3DRouteStats* out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(*out_stats));
    if (!context) return;
    out_stats->requestedRoute = context->requestedRoute;
    out_stats->activeRoute = context->activeRoute;
    out_stats->traceContextStatsOwned = true;
    out_stats->sceneAccelerationTraceCallbackBound =
        context->sceneAccelerationTraceFirstHit != NULL;
    out_stats->traceCalls =
        runtime_ray_3d_counter_load(&context->routeStats.traceCalls);
    out_stats->flattenedTraceCalls =
        runtime_ray_3d_counter_load(&context->routeStats.flattenedTraceCalls);
    out_stats->tlasTraceCalls =
        runtime_ray_3d_counter_load(&context->routeStats.tlasTraceCalls);
    out_stats->tlasTraceHits =
        runtime_ray_3d_counter_load(&context->routeStats.tlasTraceHits);
    out_stats->tlasTraceMisses =
        runtime_ray_3d_counter_load(&context->routeStats.tlasTraceMisses);
    out_stats->tlasTraceUnready =
        runtime_ray_3d_counter_load(&context->routeStats.tlasTraceUnready);
    out_stats->tlasTraceUnsupported =
        runtime_ray_3d_counter_load(&context->routeStats.tlasTraceUnsupported);
    out_stats->tlasTraceErrors =
        runtime_ray_3d_counter_load(&context->routeStats.tlasTraceErrors);
    out_stats->accelerationFailureCalls =
        runtime_ray_3d_counter_load(&context->routeStats.accelerationFailureCalls);
    out_stats->flattenedFallbackCalls =
        runtime_ray_3d_counter_load(&context->routeStats.flattenedFallbackCalls);
    out_stats->parityCheckedRays =
        runtime_ray_3d_counter_load(&context->routeStats.parityCheckedRays);
    out_stats->parityMismatches =
        runtime_ray_3d_counter_load(&context->routeStats.parityMismatches);
    snprintf(out_stats->lastAccelerationFailureStatus,
             sizeof(out_stats->lastAccelerationFailureStatus),
             "%s",
             context->routeStats.lastAccelerationFailureStatus);
    snprintf(out_stats->lastParityMismatchReason,
             sizeof(out_stats->lastParityMismatchReason),
             "%s",
             context->routeStats.lastParityMismatchReason);
}

static Vec3 runtime_ray_3d_triangle_normal(const RuntimeTriangle3D* triangle) {
    Vec3 edge1;
    Vec3 edge2;
    Vec3 normal;
    if (!triangle) return vec3(0.0, 0.0, 0.0);
    normal = vec3_normalize(triangle->normal);
    if (vec3_length(normal) > 1e-9) {
        return normal;
    }
    edge1 = vec3_sub(triangle->p1, triangle->p0);
    edge2 = vec3_sub(triangle->p2, triangle->p0);
    return vec3_normalize(vec3_cross(edge1, edge2));
}

void RuntimeRay3D_ResolveTriangleNormals(const RuntimeTriangle3D* triangle,
                                        const Ray3D* ray,
                                        double bary_u,
                                        double bary_v,
                                        double bary_w,
                                        Vec3* out_geometric_normal,
                                        Vec3* out_shading_normal) {
    Vec3 geometric = runtime_ray_3d_triangle_normal(triangle);
    Vec3 shading = geometric;
    bool flip_for_ray = false;

    if (triangle && triangle->hasVertexNormals) {
        Vec3 interpolated =
            vec3_add(vec3_add(vec3_scale(triangle->vertexNormal0, bary_u),
                              vec3_scale(triangle->vertexNormal1, bary_v)),
                     vec3_scale(triangle->vertexNormal2, bary_w));
        interpolated = vec3_normalize(interpolated);
        if (vec3_length(interpolated) > 1e-9) {
            shading = interpolated;
        }
    }
    if (vec3_dot(shading, geometric) < 0.0) {
        shading = vec3_scale(shading, -1.0);
    }
    flip_for_ray = ray && vec3_dot(geometric, ray->direction) > 0.0;
    if (flip_for_ray) {
        geometric = vec3_scale(geometric, -1.0);
        shading = vec3_scale(shading, -1.0);
    }
    if (out_geometric_normal) *out_geometric_normal = geometric;
    if (out_shading_normal) *out_shading_normal = shading;
}

Ray3D RuntimeRay3D_Make(Vec3 origin, Vec3 direction) {
    Ray3D ray = {0};
    ray.origin = origin;
    ray.direction = vec3_normalize(direction);
    return ray;
}

Ray3D RuntimeRay3D_MakeOffset(Vec3 origin,
                              Vec3 normal,
                              Vec3 direction,
                              [[fisics::dim(length)]] [[fisics::unit(meter)]] double epsilon) {
    Ray3D ray = RuntimeRay3D_Make(origin, direction);
    Vec3 offset_normal = vec3_normalize(normal);
    double side = 1.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double minimum_epsilon =
        kRuntimeRay3DMinimumOffsetEpsilon;
    if (epsilon < minimum_epsilon) {
        epsilon = minimum_epsilon;
    }
    if (vec3_length(offset_normal) <= 1e-9) {
        return ray;
    }
    if (vec3_dot(ray.direction, offset_normal) < 0.0) {
        side = -1.0;
    }
    ray.origin = vec3_add(origin, vec3_scale(offset_normal, epsilon * side));
    return ray;
}

void HitInfo3D_Reset(HitInfo3D* hit) {
    if (!hit) return;
    memset(hit, 0, sizeof(*hit));
    hit->t = DBL_MAX;
    hit->triangleIndex = -1;
    hit->localTriangleIndex = -1;
    hit->primitiveIndex = -1;
    hit->sceneObjectIndex = -1;
    hit->source.kind = RUNTIME_PRIMITIVE_3D_KIND_INVALID;
}

Vec3 HitInfo3D_OffsetNormal(const HitInfo3D* hit) {
    if (!hit) return vec3(0.0, 0.0, 0.0);
    if (vec3_length(hit->geometricNormal) > 1e-9) {
        return hit->geometricNormal;
    }
    return hit->normal;
}

Vec3 HitInfo3D_ShadingNormalForDirection(const HitInfo3D* hit, Vec3 direction) {
    Vec3 geometric = vec3(0.0, 0.0, 0.0);
    Vec3 shading = vec3(0.0, 0.0, 0.0);
    double geometric_dot = 0.0;
    double shading_dot = 0.0;
    double blend = 0.0;

    if (!hit) return vec3(0.0, 1.0, 0.0);
    geometric = vec3_normalize(hit->geometricNormal);
    shading = vec3_normalize(hit->shadingNormal);
    if (vec3_length(shading) <= 1e-9) shading = vec3_normalize(hit->normal);
    direction = vec3_normalize(direction);
    if (vec3_length(geometric) <= 1e-9) return shading;
    if (vec3_length(shading) <= 1e-9 || vec3_length(direction) <= 1e-9) return geometric;
    geometric_dot = vec3_dot(geometric, direction);
    shading_dot = vec3_dot(shading, direction);
    if (geometric_dot <= 1e-9 || shading_dot >= 1e-9) return shading;

    blend = (-shading_dot + 1e-6) / (geometric_dot - shading_dot);
    blend = fmin(1.0, fmax(0.0, blend));
    shading = vec3_normalize(vec3_add(vec3_scale(shading, 1.0 - blend),
                                       vec3_scale(geometric, blend)));
    return vec3_length(shading) > 1e-9 ? shading : geometric;
}

Vec3 HitInfo3D_ShadingNormalForReflection(const HitInfo3D* hit, Vec3 view_direction) {
    const double hemisphere_epsilon = 1e-6;
    Vec3 geometric = vec3(0.0, 0.0, 0.0);
    Vec3 shading = vec3(0.0, 0.0, 0.0);
    Vec3 incident = vec3(0.0, 0.0, 0.0);
    Vec3 candidate = vec3(0.0, 0.0, 0.0);
    Vec3 reflected = vec3(0.0, 0.0, 0.0);
    double low = 0.0;
    double high = 1.0;

    if (!hit) return vec3(0.0, 1.0, 0.0);
    geometric = vec3_normalize(hit->geometricNormal);
    shading = vec3_normalize(hit->shadingNormal);
    if (vec3_length(shading) <= 1e-9) shading = vec3_normalize(hit->normal);
    if (vec3_length(geometric) <= 1e-9) geometric = shading;
    view_direction = vec3_normalize(view_direction);
    if (vec3_length(shading) <= 1e-9) return geometric;
    if (vec3_length(geometric) <= 1e-9 || vec3_length(view_direction) <= 1e-9) {
        return shading;
    }
    if (vec3_dot(shading, geometric) < 0.0) shading = vec3_scale(shading, -1.0);

    incident = vec3_scale(view_direction, -1.0);
    reflected = vec3_normalize(vec3_sub(
        incident,
        vec3_scale(shading, 2.0 * vec3_dot(shading, incident))));
    if (vec3_dot(reflected, geometric) > hemisphere_epsilon) return shading;

    reflected = vec3_normalize(vec3_sub(
        incident,
        vec3_scale(geometric, 2.0 * vec3_dot(geometric, incident))));
    if (vec3_dot(reflected, geometric) <= hemisphere_epsilon) return geometric;

    for (int iteration = 0; iteration < 32; ++iteration) {
        const double blend = 0.5 * (low + high);
        candidate = vec3_normalize(vec3_add(vec3_scale(shading, 1.0 - blend),
                                             vec3_scale(geometric, blend)));
        reflected = vec3_normalize(vec3_sub(
            incident,
            vec3_scale(candidate, 2.0 * vec3_dot(candidate, incident))));
        if (vec3_dot(reflected, geometric) > hemisphere_epsilon) {
            high = blend;
        } else {
            low = blend;
        }
    }

    candidate = vec3_normalize(vec3_add(vec3_scale(shading, 1.0 - high),
                                         vec3_scale(geometric, high)));
    return vec3_length(candidate) > 1e-9 ? candidate : geometric;
}

bool RuntimeRay3D_IntersectTriangle(const Ray3D* ray,
                                    const RuntimeTriangle3D* triangle,
                                    int triangle_index,
                                    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
                                    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
                                    HitInfo3D* out_hit) {
    Vec3 edge1;
    Vec3 edge2;
    Vec3 pvec;
    Vec3 tvec;
    Vec3 qvec;
    double det = 0.0;
    double inv_det = 0.0;
    double bary_v = 0.0;
    double bary_w = 0.0;
    double bary_u = 0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t = 0.0;
    HitInfo3D hit = {0};

    if (!ray || !triangle || !out_hit) return false;
    if (vec3_length(ray->direction) <= 1e-9) return false;

    edge1 = vec3_sub(triangle->p1, triangle->p0);
    edge2 = vec3_sub(triangle->p2, triangle->p0);
    pvec = vec3_cross(ray->direction, edge2);
    det = vec3_dot(edge1, pvec);
    if (det > -kRuntimeRay3DDeterminantEpsilon &&
        det < kRuntimeRay3DDeterminantEpsilon) {
        return false;
    }

    inv_det = 1.0 / det;
    tvec = vec3_sub(ray->origin, triangle->p0);
    bary_v = vec3_dot(tvec, pvec) * inv_det;
    if (bary_v < 0.0 || bary_v > 1.0) {
        return false;
    }

    qvec = vec3_cross(tvec, edge1);
    bary_w = vec3_dot(ray->direction, qvec) * inv_det;
    if (bary_w < 0.0 || (bary_v + bary_w) > 1.0) {
        return false;
    }

    t = vec3_dot(edge2, qvec) * inv_det;
    if (t < t_min || t > t_max) {
        return false;
    }

    bary_u = 1.0 - bary_v - bary_w;
    HitInfo3D_Reset(&hit);
    hit.t = t;
    hit.position = vec3_add(ray->origin, vec3_scale(ray->direction, t));
    RuntimeRay3D_ResolveTriangleNormals(triangle,
                                       ray,
                                       bary_u,
                                       bary_v,
                                       bary_w,
                                       &hit.geometricNormal,
                                       &hit.shadingNormal);
    hit.normal = hit.shadingNormal;
    hit.triangleIndex = triangle_index;
    hit.localTriangleIndex = triangle->localTriangleIndex;
    hit.primitiveIndex = triangle->primitiveIndex;
    hit.sceneObjectIndex = triangle->sceneObjectIndex;
    hit.baryU = bary_u;
    hit.baryV = bary_v;
    hit.baryW = bary_w;
    if (triangle->hasObjectTextureCoords) {
        hit.hasObjectTextureCoord = true;
        hit.objectTextureCoord =
            vec3_add(vec3_add(vec3_scale(triangle->objectTexture0, bary_u),
                              vec3_scale(triangle->objectTexture1, bary_v)),
                     vec3_scale(triangle->objectTexture2, bary_w));
    }

    *out_hit = hit;
    return true;
}

static void runtime_ray_3d_apply_source_ref(const RuntimeScene3D* scene, HitInfo3D* hit) {
    if (!scene || !hit) return;
    if (hit->primitiveIndex >= 0 && hit->primitiveIndex < scene->primitiveCount) {
        hit->source = scene->primitives[hit->primitiveIndex].source;
        hit->sceneObjectIndex = scene->primitives[hit->primitiveIndex].source.sceneObjectIndex;
    }
}

const char* RuntimeRay3DTraceRouteLabel(RuntimeRay3DTraceRoute route) {
    switch (route) {
        case RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY:
            return "tlas_blas_parity";
        case RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS:
            return "tlas_blas";
        case RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH:
        default:
            return "flattened_bvh";
    }
}

RuntimeRay3DTraceRoute RuntimeRay3D_DefaultTraceRoute(void) {
    return kRuntimeRay3DDefaultTraceRoute;
}

RuntimeRay3DTraceRoute RuntimeRay3D_CurrentTraceRoute(void) {
    return runtime_ray_3d_default_trace_context()->activeRoute;
}

void RuntimeRay3D_SetSceneAccelerationTraceFirstHit(
    RuntimeRay3DSceneAccelerationTraceFirstHitFn trace_first_hit) {
    RuntimeRay3DTraceContext_SetSceneAccelerationTraceFirstHit(
        runtime_ray_3d_default_trace_context(),
        trace_first_hit);
}

void RuntimeRay3D_SetTraceRoute(RuntimeRay3DTraceRoute route) {
    RuntimeRay3DTraceContext_SetTraceRoute(runtime_ray_3d_default_trace_context(),
                                           route);
}

void RuntimeRay3D_SetTraceRouteForTests(RuntimeRay3DTraceRoute route) {
    RuntimeRay3D_SetTraceRoute(route);
}

void RuntimeRay3D_ResetTraceRouteForTests(void) {
    RuntimeRay3D_SetTraceRoute(RuntimeRay3D_DefaultTraceRoute());
}

void RuntimeRay3D_ResetRouteStats(void) {
    RuntimeRay3DTraceContext_ResetRouteStats(runtime_ray_3d_default_trace_context());
}

void RuntimeRay3D_SnapshotRouteStats(RuntimeRay3DRouteStats* out_stats) {
    RuntimeRay3DTraceContext_SnapshotRouteStats(
        runtime_ray_3d_default_trace_context(),
        out_stats);
}

static bool runtime_ray_3d_trace_scene_first_hit_flat(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
    HitInfo3D* out_hit) {
    HitInfo3D best_hit = {0};
    bool found = false;
    if (!scene || !ray || !out_hit) return false;

    HitInfo3D_Reset(&best_hit);
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        HitInfo3D hit = {0};
        if (!RuntimeRay3D_IntersectTriangle(ray,
                                            &scene->triangleMesh.triangles[i],
                                            i,
                                            t_min,
                                            found ? best_hit.t : t_max,
                                            &hit)) {
            continue;
        }
        runtime_ray_3d_apply_source_ref(scene, &hit);
        best_hit = hit;
        found = true;
    }

    if (!found) {
        HitInfo3D_Reset(out_hit);
        return false;
    }

    *out_hit = best_hit;
    return true;
}

static bool runtime_ray_3d_trace_scene_first_hit_flattened(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
    HitInfo3D* out_hit) {
    if (RuntimeTriangleMesh3D_HasReadyBVH(&scene->triangleMesh)) {
        HitInfo3D hit = {0};
        RuntimeTriangleBVH3DTraceResult trace_result =
            RuntimeTriangleBVH3D_TraceFirstHitStatus(&scene->triangleMesh,
                                                     ray,
                                                     t_min,
                                                     t_max,
                                                     &hit);
        if (trace_result == RUNTIME_TRIANGLE_BVH_3D_TRACE_HIT) {
            runtime_ray_3d_apply_source_ref(scene, &hit);
            *out_hit = hit;
            return true;
        }
        if (trace_result == RUNTIME_TRIANGLE_BVH_3D_TRACE_OVERFLOW) {
            RuntimeTriangleBVH3D_RecordFlatFallback(true);
            return runtime_ray_3d_trace_scene_first_hit_flat(scene,
                                                             ray,
                                                             t_min,
                                                             t_max,
                                                             out_hit);
        }
        HitInfo3D_Reset(out_hit);
        return false;
    }
    RuntimeTriangleBVH3D_RecordFlatFallback(false);
    return runtime_ray_3d_trace_scene_first_hit_flat(scene, ray, t_min, t_max, out_hit);
}

static void runtime_ray_3d_record_tlas_status(
    RuntimeRay3DTraceContext* context,
    RuntimeSceneAcceleration3DTraceStatusForRayRoute status) {
    RuntimeRay3DRouteStats* stats = context ? &context->routeStats : NULL;
    if (!stats) return;
    runtime_ray_3d_counter_increment(&stats->tlasTraceCalls);
    switch (status) {
        case RUNTIME_RAY_3D_ACCEL_TRACE_HIT:
            runtime_ray_3d_counter_increment(&stats->tlasTraceHits);
            break;
        case RUNTIME_RAY_3D_ACCEL_TRACE_MISS:
            runtime_ray_3d_counter_increment(&stats->tlasTraceMisses);
            break;
        case RUNTIME_RAY_3D_ACCEL_TRACE_UNREADY:
            runtime_ray_3d_counter_increment(&stats->tlasTraceUnready);
            break;
        case RUNTIME_RAY_3D_ACCEL_TRACE_UNSUPPORTED:
            runtime_ray_3d_counter_increment(&stats->tlasTraceUnsupported);
            break;
        case RUNTIME_RAY_3D_ACCEL_TRACE_ERROR:
        default:
            runtime_ray_3d_counter_increment(&stats->tlasTraceErrors);
            break;
    }
}

static void runtime_ray_3d_record_acceleration_failure(
    RuntimeRay3DTraceContext* context,
    RuntimeSceneAcceleration3DTraceStatusForRayRoute status) {
    RuntimeRay3DRouteStats* stats = context ? &context->routeStats : NULL;
    const char* label = "error";
    if (!stats) return;
    if (status == RUNTIME_RAY_3D_ACCEL_TRACE_UNREADY) {
        label = "unready";
    } else if (status == RUNTIME_RAY_3D_ACCEL_TRACE_UNSUPPORTED) {
        label = "unsupported";
    }
    runtime_ray_3d_counter_increment(&stats->accelerationFailureCalls);
    snprintf(stats->lastAccelerationFailureStatus,
             sizeof(stats->lastAccelerationFailureStatus),
             "%s",
             label);
}

static void runtime_ray_3d_set_parity_mismatch(RuntimeRay3DTraceContext* context,
                                               const char* reason) {
    RuntimeRay3DRouteStats* stats = context ? &context->routeStats : NULL;
    if (!stats) return;
    snprintf(stats->lastParityMismatchReason,
             sizeof(stats->lastParityMismatchReason),
             "%s",
             (reason && reason[0]) ? reason : "unknown");
}

static bool runtime_ray_3d_hits_match(const HitInfo3D* expected,
                                      const HitInfo3D* actual,
                                      char* reason,
                                      size_t reason_size) {
    if (!expected || !actual) {
        snprintf(reason, reason_size, "missing_hit");
        return false;
    }
    if (expected->triangleIndex != actual->triangleIndex) {
        snprintf(reason, reason_size, "triangle_index");
        return false;
    }
    if (expected->localTriangleIndex != actual->localTriangleIndex) {
        snprintf(reason, reason_size, "local_triangle_index");
        return false;
    }
    if (expected->primitiveIndex != actual->primitiveIndex) {
        snprintf(reason, reason_size, "primitive_index");
        return false;
    }
    if (expected->sceneObjectIndex != actual->sceneObjectIndex) {
        snprintf(reason, reason_size, "scene_object_index");
        return false;
    }
    if (strcmp(expected->source.objectId, actual->source.objectId) != 0) {
        snprintf(reason, reason_size, "source_object_id");
        return false;
    }
    if (fabs(expected->t - actual->t) > 1e-7) {
        snprintf(reason, reason_size, "hit_t");
        return false;
    }
    if (fabs(expected->baryU - actual->baryU) > 1e-7 ||
        fabs(expected->baryV - actual->baryV) > 1e-7 ||
        fabs(expected->baryW - actual->baryW) > 1e-7) {
        snprintf(reason, reason_size, "barycentric");
        return false;
    }
    if (vec3_length(vec3_sub(expected->geometricNormal,
                             actual->geometricNormal)) > 1e-7) {
        snprintf(reason, reason_size, "geometric_normal");
        return false;
    }
    if (vec3_length(vec3_sub(expected->shadingNormal,
                             actual->shadingNormal)) > 1e-7) {
        snprintf(reason, reason_size, "shading_normal");
        return false;
    }
    if (expected->hasObjectTextureCoord != actual->hasObjectTextureCoord) {
        snprintf(reason, reason_size, "object_texture_presence");
        return false;
    }
    return true;
}

static bool runtime_ray_3d_trace_scene_first_hit_parity(
    RuntimeRay3DTraceContext* context,
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
    HitInfo3D* out_hit) {
    HitInfo3D flattened_hit = {0};
    HitInfo3D tlas_hit = {0};
    bool flattened_found = false;
    RuntimeSceneAcceleration3DTraceStatusForRayRoute tlas_status;
    bool tlas_found = false;
    char reason[128] = {0};
    RuntimeRay3DRouteStats* stats = context ? &context->routeStats : NULL;

    if (!stats) return false;
    if (context->sceneAccelerationTraceFirstHit) {
        tlas_status = (RuntimeSceneAcceleration3DTraceStatusForRayRoute)
            context->sceneAccelerationTraceFirstHit(scene,
                                                    ray,
                                                    t_min,
                                                    t_max,
                                                    &tlas_hit);
    } else {
        HitInfo3D_Reset(&tlas_hit);
        tlas_status = RUNTIME_RAY_3D_ACCEL_TRACE_UNREADY;
    }
    runtime_ray_3d_record_tlas_status(context, tlas_status);
    tlas_found = tlas_status == RUNTIME_RAY_3D_ACCEL_TRACE_HIT;

    runtime_ray_3d_counter_increment(&stats->flattenedTraceCalls);
    flattened_found = runtime_ray_3d_trace_scene_first_hit_flattened(scene,
                                                                     ray,
                                                                     t_min,
                                                                     t_max,
                                                                     &flattened_hit);
    runtime_ray_3d_counter_increment(&stats->parityCheckedRays);
    if (flattened_found != tlas_found) {
        runtime_ray_3d_counter_increment(&stats->parityMismatches);
        if (flattened_found) {
            snprintf(reason,
                     sizeof(reason),
                     "tlas_%d_flattened_hit:%s:%d:t=%.6g",
                     (int)tlas_status,
                     flattened_hit.source.objectId,
                     flattened_hit.triangleIndex,
                     flattened_hit.t);
        } else {
            snprintf(reason,
                     sizeof(reason),
                     "tlas_hit_flattened_miss:%s:%d:t=%.6g",
                     tlas_hit.source.objectId,
                     tlas_hit.triangleIndex,
                     tlas_hit.t);
        }
        runtime_ray_3d_set_parity_mismatch(context, reason);
    } else if (flattened_found &&
               !runtime_ray_3d_hits_match(&flattened_hit,
                                           &tlas_hit,
                                           reason,
                                           sizeof(reason))) {
        runtime_ray_3d_counter_increment(&stats->parityMismatches);
        runtime_ray_3d_set_parity_mismatch(context, reason);
    }

    if (flattened_found) {
        *out_hit = flattened_hit;
        return true;
    }
    HitInfo3D_Reset(out_hit);
    return false;
}

static bool runtime_ray_3d_trace_scene_first_hit_tlas_blas(
    RuntimeRay3DTraceContext* context,
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
    HitInfo3D* out_hit) {
    RuntimeSceneAcceleration3DTraceStatusForRayRoute tlas_status;
    RuntimeRay3DRouteStats* stats = context ? &context->routeStats : NULL;
    if (!stats) return false;
    if (context->sceneAccelerationTraceFirstHit) {
        tlas_status = (RuntimeSceneAcceleration3DTraceStatusForRayRoute)
            context->sceneAccelerationTraceFirstHit(scene,
                                                    ray,
                                                    t_min,
                                                    t_max,
                                                    out_hit);
    } else {
        HitInfo3D_Reset(out_hit);
        tlas_status = RUNTIME_RAY_3D_ACCEL_TRACE_UNREADY;
    }
    runtime_ray_3d_record_tlas_status(context, tlas_status);
    if (tlas_status == RUNTIME_RAY_3D_ACCEL_TRACE_HIT) {
        return true;
    }
    if (tlas_status == RUNTIME_RAY_3D_ACCEL_TRACE_MISS) {
        HitInfo3D_Reset(out_hit);
        return false;
    }
    /*
     * TLAS/BLAS is fail-closed. Compatibility tracing is an explicit route,
     * and parity is the explicit dual-trace route; an acceleration defect must
     * never silently turn one ray into a full-scene scan.
     */
    runtime_ray_3d_record_acceleration_failure(context, tlas_status);
    HitInfo3D_Reset(out_hit);
    return false;
}

bool RuntimeRay3D_TraceSceneFirstHit(const RuntimeScene3D* scene,
                                     const Ray3D* ray,
                                     [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
                                     [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
                                     HitInfo3D* out_hit) {
    return RuntimeRay3D_TraceSceneFirstHitWithContext(
        runtime_ray_3d_default_trace_context(),
        scene,
        ray,
        t_min,
        t_max,
        out_hit);
}

bool RuntimeRay3D_TraceSceneFirstHitWithContext(RuntimeRay3DTraceContext* context,
                                                const RuntimeScene3D* scene,
                                                const Ray3D* ray,
                                                [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
                                                [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
                                                HitInfo3D* out_hit) {
    RuntimeRay3DRouteStats* stats = NULL;
    RuntimeRay3DTraceRoute active_route;
    if (!context) context = runtime_ray_3d_default_trace_context();
    if (!scene || !ray || !out_hit) return false;
    active_route = runtime_ray_3d_sanitize_trace_route(context->activeRoute);
    stats = &context->routeStats;
    runtime_ray_3d_counter_increment(&stats->traceCalls);

    if (active_route == RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY) {
        return runtime_ray_3d_trace_scene_first_hit_parity(context,
                                                           scene,
                                                           ray,
                                                           t_min,
                                                           t_max,
                                                           out_hit);
    }
    if (active_route == RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS) {
        return runtime_ray_3d_trace_scene_first_hit_tlas_blas(context,
                                                              scene,
                                                              ray,
                                                              t_min,
                                                              t_max,
                                                              out_hit);
    }

    runtime_ray_3d_counter_increment(&stats->flattenedTraceCalls);
    return runtime_ray_3d_trace_scene_first_hit_flattened(scene, ray, t_min, t_max, out_hit);
}
