#include "render/runtime_curve_primitive_3d.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_curve_blas_3d.h"

static bool runtime_curve_vec3_finite(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static Vec3 runtime_curve_point_tangent(const Vec3 *points,
                                        size_t point_index,
                                        size_t points_per_strand) {
    Vec3 tangent;
    size_t local = point_index % points_per_strand;
    if (local == 0u) {
        tangent = vec3_sub(points[point_index + 1u], points[point_index]);
    } else if (local + 1u == points_per_strand) {
        tangent = vec3_sub(points[point_index], points[point_index - 1u]);
    } else {
        tangent = vec3_sub(points[point_index + 1u], points[point_index - 1u]);
    }
    return vec3_normalize(tangent);
}

void RuntimeCurveAsset3D_Init(RuntimeCurveAsset3D *asset) {
    if (!asset) return;
    memset(asset, 0, sizeof(*asset));
}

void RuntimeCurveAsset3D_Free(RuntimeCurveAsset3D *asset) {
    if (!asset) return;
    RuntimeCurveAsset3D_ClearBLAS(asset);
    free(asset->primitives);
    RuntimeCurveAsset3D_Init(asset);
}

bool RuntimeCurveAsset3D_CopyFrom(RuntimeCurveAsset3D *dst,
                                  const RuntimeCurveAsset3D *src) {
    RuntimeCurvePrimitive3D *primitives = NULL;
    if (!dst || !src || dst == src) return dst == src && dst != NULL;
    if (src->primitiveCount > 0u) {
        if (!src->primitives ||
            src->primitiveCount > SIZE_MAX / sizeof(*primitives)) {
            return false;
        }
        primitives = malloc(src->primitiveCount * sizeof(*primitives));
        if (!primitives) return false;
        memcpy(primitives,
               src->primitives,
               src->primitiveCount * sizeof(*primitives));
    }
    RuntimeCurveAsset3D_Free(dst);
    dst->primitives = primitives;
    dst->primitiveCount = src->primitiveCount;
    dst->blasDirty = src->primitiveCount > 0u;
    if (dst->primitiveCount > 0u && !RuntimeCurveAsset3D_BuildBLAS(dst)) {
        RuntimeCurveAsset3D_Free(dst);
        return false;
    }
    return true;
}

bool RuntimeCurveAsset3D_BuildFromPolylineStrands(
    RuntimeCurveAsset3D *asset,
    const Vec3 *points,
    const double *radii,
    size_t strand_count,
    size_t points_per_strand) {
    RuntimeCurvePrimitive3D *primitives = NULL;
    size_t primitive_count = 0u;

    if (!asset || !points || !radii || strand_count == 0u ||
        points_per_strand < 2u) {
        return false;
    }
    if (strand_count > SIZE_MAX / (points_per_strand - 1u)) return false;
    primitive_count = strand_count * (points_per_strand - 1u);
    primitives = calloc(primitive_count, sizeof(*primitives));
    if (!primitives) return false;

    for (size_t strand = 0u; strand < strand_count; ++strand) {
        const size_t base = strand * points_per_strand;
        for (size_t segment = 0u; segment + 1u < points_per_strand; ++segment) {
            const size_t point0 = base + segment;
            const size_t point1 = point0 + 1u;
            const size_t primitive_index =
                strand * (points_per_strand - 1u) + segment;
            RuntimeCurvePrimitive3D *primitive = &primitives[primitive_index];
            if (!runtime_curve_vec3_finite(points[point0]) ||
                !runtime_curve_vec3_finite(points[point1]) ||
                !isfinite(radii[point0]) || !isfinite(radii[point1]) ||
                !(radii[point0] > 0.0) || !(radii[point1] > 0.0) ||
                vec3_length(vec3_sub(points[point1], points[point0])) <= 1.0e-12) {
                free(primitives);
                return false;
            }
            primitive->p0 = points[point0];
            primitive->p1 = points[point1];
            primitive->tangent0 =
                runtime_curve_point_tangent(points, point0, points_per_strand);
            primitive->tangent1 =
                runtime_curve_point_tangent(points, point1, points_per_strand);
            if (vec3_length(primitive->tangent0) <= 1.0e-12 ||
                vec3_length(primitive->tangent1) <= 1.0e-12) {
                free(primitives);
                return false;
            }
            primitive->radius0 = radii[point0];
            primitive->radius1 = radii[point1];
            primitive->strandIndex = (int)strand;
            primitive->segmentIndex = (int)segment;
            primitive->hasRootCap = segment == 0u;
            primitive->hasTipCap = segment + 2u == points_per_strand;
        }
    }

    RuntimeCurveAsset3D_Free(asset);
    asset->primitives = primitives;
    asset->primitiveCount = primitive_count;
    asset->blasDirty = true;
    return true;
}

typedef struct RuntimeCurveCandidate3D {
    bool found;
    double t;
    double u;
    double radius;
    Vec3 normal;
} RuntimeCurveCandidate3D;

static void runtime_curve_consider_candidate(RuntimeCurveCandidate3D *best,
                                             double t,
                                             double u,
                                             double radius,
                                             Vec3 normal,
                                             double t_min,
                                             double t_max) {
    if (!best || !isfinite(t) || t < t_min || t > t_max) return;
    if (best->found && t >= best->t) return;
    normal = vec3_normalize(normal);
    if (vec3_length(normal) <= 1.0e-12) return;
    best->found = true;
    best->t = t;
    best->u = u;
    best->radius = radius;
    best->normal = normal;
}

static void runtime_curve_intersect_cap(const Ray3D *ray,
                                        Vec3 center,
                                        Vec3 axis,
                                        double radius,
                                        double u,
                                        double t_min,
                                        double t_max,
                                        RuntimeCurveCandidate3D *best) {
    const double denominator = vec3_dot(ray->direction, axis);
    double t = 0.0;
    Vec3 point;
    Vec3 offset;
    if (fabs(denominator) <= 1.0e-12) return;
    t = vec3_dot(vec3_sub(center, ray->origin), axis) / denominator;
    if (t < t_min || t > t_max || (best->found && t >= best->t)) return;
    point = vec3_add(ray->origin, vec3_scale(ray->direction, t));
    offset = vec3_sub(point, center);
    offset = vec3_sub(offset, vec3_scale(axis, vec3_dot(offset, axis)));
    if (vec3_dot(offset, offset) > radius * radius + 1.0e-12) return;
    runtime_curve_consider_candidate(best, t, u, radius, axis, t_min, t_max);
}

bool RuntimeRay3D_IntersectCurvePrimitive(
    const Ray3D *ray,
    const RuntimeCurvePrimitive3D *primitive,
    int primitive_index,
    double t_min,
    double t_max,
    HitInfo3D *out_hit) {
    RuntimeCurveCandidate3D best = {0};
    Vec3 axis_vector;
    Vec3 axis;
    Vec3 origin_offset;
    Vec3 origin_perp;
    Vec3 direction_perp;
    double length = 0.0;
    double origin_axis = 0.0;
    double direction_axis = 0.0;
    double radius_slope = 0.0;
    double radius_at_origin = 0.0;
    double qa = 0.0;
    double qb = 0.0;
    double qc = 0.0;

    if (!ray || !primitive || !out_hit || !runtime_curve_vec3_finite(ray->origin) ||
        !runtime_curve_vec3_finite(ray->direction) ||
        vec3_length(ray->direction) <= 1.0e-12 ||
        !(primitive->radius0 > 0.0) || !(primitive->radius1 > 0.0)) {
        return false;
    }
    axis_vector = vec3_sub(primitive->p1, primitive->p0);
    length = vec3_length(axis_vector);
    if (!(length > 1.0e-12) || !isfinite(length)) return false;
    axis = vec3_scale(axis_vector, 1.0 / length);
    origin_offset = vec3_sub(ray->origin, primitive->p0);
    origin_axis = vec3_dot(origin_offset, axis);
    direction_axis = vec3_dot(ray->direction, axis);
    origin_perp = vec3_sub(origin_offset, vec3_scale(axis, origin_axis));
    direction_perp =
        vec3_sub(ray->direction, vec3_scale(axis, direction_axis));
    radius_slope = (primitive->radius1 - primitive->radius0) / length;
    radius_at_origin = primitive->radius0 + radius_slope * origin_axis;
    qa = vec3_dot(direction_perp, direction_perp) -
         radius_slope * radius_slope * direction_axis * direction_axis;
    qb = 2.0 * (vec3_dot(origin_perp, direction_perp) -
                radius_at_origin * radius_slope * direction_axis);
    qc = vec3_dot(origin_perp, origin_perp) -
         radius_at_origin * radius_at_origin;

    if (fabs(qa) > 1.0e-15) {
        const double discriminant = qb * qb - 4.0 * qa * qc;
        if (discriminant >= 0.0) {
            const double root = sqrt(fmax(0.0, discriminant));
            const double roots[2] = {
                (-qb - root) / (2.0 * qa),
                (-qb + root) / (2.0 * qa)};
            for (size_t i = 0u; i < 2u; ++i) {
                const double t = roots[i];
                const double z = origin_axis + t * direction_axis;
                const double u = z / length;
                if (z >= 0.0 && z <= length) {
                    const Vec3 point =
                        vec3_add(ray->origin, vec3_scale(ray->direction, t));
                    const Vec3 center =
                        vec3_add(primitive->p0, vec3_scale(axis, z));
                    const Vec3 radial = vec3_sub(point, center);
                    const double radius =
                        primitive->radius0 +
                        (primitive->radius1 - primitive->radius0) * u;
                    const Vec3 normal =
                        vec3_sub(vec3_normalize(radial),
                                 vec3_scale(axis, radius_slope));
                    runtime_curve_consider_candidate(
                        &best, t, u, radius, normal, t_min, t_max);
                }
            }
        }
    } else if (fabs(qb) > 1.0e-15) {
        const double t = -qc / qb;
        const double z = origin_axis + t * direction_axis;
        if (z >= 0.0 && z <= length) {
            const double u = z / length;
            const Vec3 point =
                vec3_add(ray->origin, vec3_scale(ray->direction, t));
            const Vec3 center = vec3_add(primitive->p0, vec3_scale(axis, z));
            const Vec3 radial = vec3_sub(point, center);
            const double radius =
                primitive->radius0 +
                (primitive->radius1 - primitive->radius0) * u;
            runtime_curve_consider_candidate(
                &best,
                t,
                u,
                radius,
                vec3_sub(vec3_normalize(radial),
                         vec3_scale(axis, radius_slope)),
                t_min,
                t_max);
        }
    }

    if (primitive->hasRootCap) {
        runtime_curve_intersect_cap(ray,
                                    primitive->p0,
                                    vec3_scale(axis, -1.0),
                                    primitive->radius0,
                                    0.0,
                                    t_min,
                                    t_max,
                                    &best);
    }
    if (primitive->hasTipCap) {
        runtime_curve_intersect_cap(ray,
                                    primitive->p1,
                                    axis,
                                    primitive->radius1,
                                    1.0,
                                    t_min,
                                    t_max,
                                    &best);
    }
    if (!best.found) return false;

    HitInfo3D_Reset(out_hit);
    out_hit->t = best.t;
    out_hit->position =
        vec3_add(ray->origin, vec3_scale(ray->direction, best.t));
    out_hit->geometricNormal = best.normal;
    out_hit->shadingNormal = best.normal;
    out_hit->normal = best.normal;
    out_hit->hasCurveTangent = true;
    out_hit->curveTangent = vec3_normalize(vec3_add(
        vec3_scale(primitive->tangent0, 1.0 - best.u),
        vec3_scale(primitive->tangent1, best.u)));
    if (vec3_length(out_hit->curveTangent) <= 1.0e-12) {
        out_hit->curveTangent = axis;
    }
    out_hit->curveU = best.u;
    out_hit->curveRadius = best.radius;
    out_hit->curvePrimitiveIndex = primitive_index;
    out_hit->curveStrandIndex = primitive->strandIndex;
    out_hit->curveSegmentIndex = primitive->segmentIndex;
    return true;
}
