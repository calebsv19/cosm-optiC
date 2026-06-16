#include "render/runtime_ray_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

#include <float.h>
#include <string.h>

static const double kRuntimeRay3DDeterminantEpsilon = 1e-9;
static const double kRuntimeRay3DMinimumOffsetEpsilon = 1e-9;

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

static Vec3 runtime_ray_3d_orient_shading_normal(Vec3 normal, const Ray3D* ray) {
    normal = vec3_normalize(normal);
    if (!ray || vec3_length(normal) <= 1e-9) return normal;
    if (vec3_dot(normal, ray->direction) > 0.0) {
        return vec3_scale(normal, -1.0);
    }
    return normal;
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
    hit.normal = runtime_ray_3d_orient_shading_normal(
        runtime_ray_3d_triangle_normal(triangle),
        ray);
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

bool RuntimeRay3D_TraceSceneFirstHit(const RuntimeScene3D* scene,
                                     const Ray3D* ray,
                                     [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_min,
                                     [[fisics::dim(length)]] [[fisics::unit(meter)]] double t_max,
                                     HitInfo3D* out_hit) {
    if (!scene || !ray || !out_hit) return false;
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
            return runtime_ray_3d_trace_scene_first_hit_flat(scene, ray, t_min, t_max, out_hit);
        }
        HitInfo3D_Reset(out_hit);
        return false;
    }
    RuntimeTriangleBVH3D_RecordFlatFallback(false);
    return runtime_ray_3d_trace_scene_first_hit_flat(scene, ray, t_min, t_max, out_hit);
}
