#ifndef RENDER_RUNTIME_CAUSTIC_LENS_TRANSPORT_INTERNAL_3D_H
#define RENDER_RUNTIME_CAUSTIC_LENS_TRANSPORT_INTERNAL_3D_H

#include <math.h>

#include "render/runtime_caustic_lens_transport_3d.h"

static inline double lens_transport_clamp(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static inline bool lens_transport_finite_vec3(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static inline void lens_transport_build_basis(Vec3 axis, Vec3* out_u, Vec3* out_v) {
    Vec3 n = vec3_normalize(axis);
    Vec3 helper = fabs(n.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 u = vec3_normalize(vec3_cross(helper, n));
    Vec3 v = vec3_normalize(vec3_cross(n, u));
    if (!(vec3_length(u) > 1.0e-9) || !(vec3_length(v) > 1.0e-9)) {
        u = vec3(1.0, 0.0, 0.0);
        v = vec3(0.0, 1.0, 0.0);
    }
    if (out_u) *out_u = u;
    if (out_v) *out_v = v;
}

static inline bool lens_transport_intersect_cylinder_side(Vec3 center,
                                                   Vec3 axis,
                                                   double radius,
                                                   double half_height,
                                                   Vec3 origin,
                                                   Vec3 direction,
                                                   double min_t,
                                                   double* out_t,
                                                   Vec3* out_position,
                                                   Vec3* out_normal) {
    Vec3 rel = vec3_sub(origin, center);
    Vec3 rel_perp = vec3_sub(rel, vec3_scale(axis, vec3_dot(rel, axis)));
    Vec3 dir_perp = vec3_sub(direction, vec3_scale(axis, vec3_dot(direction, axis)));
    double a = vec3_dot(dir_perp, dir_perp);
    double b = 2.0 * vec3_dot(rel_perp, dir_perp);
    double c = vec3_dot(rel_perp, rel_perp) - radius * radius;
    double disc = 0.0;
    double root_disc = 0.0;
    double roots[2] = {0.0, 0.0};

    if (out_t) *out_t = 0.0;
    if (out_position) *out_position = vec3(0.0, 0.0, 0.0);
    if (out_normal) *out_normal = vec3(0.0, 0.0, 0.0);
    if (!(a > 1.0e-12) || !(radius > 1.0e-9) || !(half_height > 1.0e-9)) {
        return false;
    }
    disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return false;
    root_disc = sqrt(disc);
    roots[0] = (-b - root_disc) / (2.0 * a);
    roots[1] = (-b + root_disc) / (2.0 * a);
    for (int i = 0; i < 2; ++i) {
        double t = roots[i];
        Vec3 p;
        double axial = 0.0;
        Vec3 radial;
        if (!(t > min_t)) continue;
        p = vec3_add(origin, vec3_scale(direction, t));
        axial = vec3_dot(vec3_sub(p, center), axis);
        if (fabs(axial) > half_height + 1.0e-6) continue;
        radial = vec3_sub(vec3_sub(p, center), vec3_scale(axis, axial));
        if (!(vec3_length(radial) > 1.0e-9)) continue;
        if (out_t) *out_t = t;
        if (out_position) *out_position = p;
        if (out_normal) *out_normal = vec3_normalize(radial);
        return true;
    }
    return false;
}

static inline bool lens_transport_intersect_plane(Vec3 plane_point,
                                           Vec3 plane_normal,
                                           Vec3 origin,
                                           Vec3 direction,
                                           double min_t,
                                           double* out_t,
                                           Vec3* out_position) {
    double denom = vec3_dot(direction, plane_normal);
    double t = 0.0;
    if (out_t) *out_t = 0.0;
    if (out_position) *out_position = vec3(0.0, 0.0, 0.0);
    if (fabs(denom) <= 1.0e-10) return false;
    t = vec3_dot(vec3_sub(plane_point, origin), plane_normal) / denom;
    if (!(t > min_t)) return false;
    if (out_t) *out_t = t;
    if (out_position) *out_position = vec3_add(origin, vec3_scale(direction, t));
    return true;
}

#endif
