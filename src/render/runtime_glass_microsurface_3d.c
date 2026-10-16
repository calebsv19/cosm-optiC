/* Smith GGX dielectric microsurface random walk, following Heitz et al. 2016,
 * Multiple-Scattering Microfacet BSDFs with the Smith Model, algorithms 1/3.
 * CDF-space free flights are independent of the chosen height distribution.
 * This is a sampled transport estimator, not an analytic marginal BSDF/PDF.
 */
#include "render/runtime_physical_glass_3d.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

static uint64_t mix(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}
static double random_open(uint64_t *state) {
    *state += UINT64_C(0x9e3779b97f4a7c15);
    return ((mix(*state) >> 12) + .5) * 0x1p-52;
}
static Vec3 unit(Vec3 v) {
    double l = hypot(hypot(v.x, v.y), v.z);
    return l > 0 ? vec3_scale(v, 1 / l) : vec3(0, 0, 1);
}
/* Spherical-cap GGX VNDF sampling (Dupuy/Benyoub 2023). Unlike the
 * upper-hemisphere disk construction this also supports downward-facing
 * incident directions encountered inside the microscopic random walk. */
static Vec3 visible_normal(Vec3 wi, double alpha, double u, double v) {
    Vec3 stretched = unit(vec3(alpha * wi.x, alpha * wi.y, wi.z));
    double z = 1 - u * (1 + stretched.z);
    double radius = sqrt(fmax(0, (1 - z) * (1 + z)));
    double phi = 6.2831853071795864769 * v;
    Vec3 h = vec3_add(stretched, vec3(radius * cos(phi), radius * sin(phi), z));
    return unit(vec3(alpha * h.x, alpha * h.y, fmax(0, h.z)));
}
/* True means escape. Signed Lambda must be negative below the horizon.
 * Store both log(CDF) and log(1-CDF) to avoid cancellation when switching
 * media at very high/low heights. Horizontal flights stay at the same height. */
static bool flight(Vec3 wr, double alpha, double u, double *log_cdf) {
    double z = wr.z;
    if (fabs(z) < 1e-15)
        return false;
    double radial = alpha * alpha * (wr.x * wr.x + wr.y * wr.y);
    double az = fabs(z), root = sqrt(z * z + radial);
    double positive = radial / (2 * az * (root + az));
    double lambda = z > 0 ? positive : -1 - positive;
    if (lambda == 0)
        return true;
    double next = *log_cdf - log1p(-u) / lambda;
    if (z > 0 && next >= 0)
        return true;
    *log_cdf = fmin(next, -0x1p-53);
    return false;
}
bool RuntimePhysicalGlass3D_SampleMicrosurface(const RuntimeMaterialPayload3D *p,
                                               Vec3 normal, Vec3 incoming,
                                               double from, double to, double u,
                                               double v, double choice,
                                               RuntimePhysicalGlass3DSample *out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!p || !isfinite(from) || !isfinite(to) || from <= 0 || to <= 0 ||
        !isfinite(p->bsdf.roughness) || !isfinite(u) || !isfinite(v) ||
        !isfinite(choice) || u < 0 || u >= 1 || v < 0 || v >= 1 ||
        choice < 0 || choice >= 1 || vec3_length(normal) < 1e-12 ||
        vec3_length(incoming) < 1e-12) return false;
    Vec3 n = unit(normal), inc = unit(incoming);
    if (vec3_dot(n, inc) > 0) n = vec3_scale(n, -1);
    Vec3 guide = fabs(n.z) < .9 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    Vec3 tx = unit(vec3_cross(guide, n)), ty = vec3_cross(n, tx);
    Vec3 wr = vec3(vec3_dot(inc, tx), vec3_dot(inc, ty), vec3_dot(inc, n));
    if (!(wr.z < 0)) return false;
    uint64_t a, b, c;
    memcpy(&a, &u, sizeof(a)); memcpy(&b, &v, sizeof(b)); memcpy(&c, &choice, sizeof(c));
    uint64_t rng = mix(a) ^ mix(b + 1) ^ mix(c + 2);
    double alpha = fmax(1e-8, p->bsdf.roughness * p->bsdf.roughness);
    double log_outside = 0, log_inside = -INFINITY, survival_weight = 1;
    bool outside = true;
    unsigned order = 0;
    RuntimeMaterialPayload3D physical = *p;
    physical.thinWalled = false;
    physical.bsdf.reflectivity = 0;
    for (;;) {
        double *height = outside ? &log_outside : &log_inside;
        if (flight(outside ? wr : vec3_scale(wr, -1), alpha, random_open(&rng), height)) {
            out->direction = unit(vec3_add(vec3_add(vec3_scale(tx, wr.x),
                                                    vec3_scale(ty, wr.y)), vec3_scale(n, wr.z)));
            out->reflected = outside;
            out->weight = survival_weight * (outside ? 1 : (from / to) * (from / to));
            out->scatteringOrder = order;
            /* No marginal solid-angle PDF is available. Do not feed this
             * sampled estimator into directional MIS as though pdf=1. */
            out->hasMarginalPdf = false;
            out->pdf = 0;
            out->valid = order > 0 && isfinite(out->weight);
            return out->valid;
        }
        if (outside) log_inside = log(-expm1(log_outside));
        else log_outside = log(-expm1(log_inside));
        /* Rare long paths use compensated roulette, never a black hard cap. */
        if (order >= 64 && order % 64 == 0) {
            if (random_open(&rng) < .5) return false;
            survival_weight *= 2;
        }
        Vec3 wi = vec3_scale(wr, -1);
        double su = order == 0 ? u : random_open(&rng);
        double sv = order == 0 ? v : random_open(&rng);
        double branch = order == 0 ? choice : random_open(&rng);
        Vec3 facet = visible_normal(outside ? wi : vec3_scale(wi, -1), alpha, su, sv);
        if (!outside) facet = vec3_scale(facet, -1);
        RuntimeDielectricTransport3D event;
        if (!RuntimeDielectricTransport3D_ResolveInterface(&physical, facet, wr,
                                                           outside ? from : to,
                                                           outside ? to : from, &event)) return false;
        if (branch < event.fresnel) wr = event.reflectionDir;
        else { wr = event.refractionDir; outside = !outside; }
        ++order;
        if (!isfinite(wr.x) || !isfinite(wr.y) || !isfinite(wr.z)) return false;
    }
}
