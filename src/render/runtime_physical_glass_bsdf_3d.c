#include "render/runtime_physical_glass_3d.h"
#include <math.h>
#include <string.h>
static double sq(double x) { return x * x; }
/* Exact unpolarized dielectric Fresnel. Equal media is its analytic limit. */
double RuntimePhysicalGlass3D_Fresnel(double c, double from, double to) {
    return RuntimeDielectricTransport3D_FresnelExact(c, from, to);
}
static double g1(Vec3 w, double a) {
    double c = fabs(w.z);
    if (c <= 1e-14)
        return 0;
    return 2 / (1 + sqrt(1 + a * a * (w.x * w.x + w.y * w.y) / (c * c)));
}
/* GGX visible-normal sampling. The same sampled facet owns both lobes. */
static Vec3 facet(Vec3 wo, double a, double u, double v) {
    Vec3 vh = vec3_normalize(vec3(a * wo.x, a * wo.y, wo.z));
    double l = vh.x * vh.x + vh.y * vh.y;
    Vec3 t1 = l > 0 ? vec3(-vh.y / sqrt(l), vh.x / sqrt(l), 0) : vec3(1, 0, 0);
    Vec3 t2 = vec3_cross(vh, t1);
    double r = sqrt(u), phi = 6.283185307179586 * v;
    double p = r * cos(phi), q = r * sin(phi), s = .5 * (1 + vh.z);
    q = (1 - s) * sqrt(fmax(0, 1 - p * p)) + s * q;
    Vec3 nh = vec3_add(vec3_add(vec3_scale(t1, p), vec3_scale(t2, q)),
                       vec3_scale(vh, sqrt(fmax(0, 1 - p * p - q * q))));
    return vec3_normalize(vec3(a * nh.x, a * nh.y, fmax(0, nh.z)));
}
bool RuntimePhysicalGlass3D_SampleSingleScatter(const RuntimeMaterialPayload3D *p, Vec3 normal, Vec3 incoming,
                                   double from, double to, double u, double v, double choice,
                                   RuntimePhysicalGlass3DSample *out) {
    if (!out)
        return false;
    memset(out, 0, sizeof(*out));
    if (!p || !(from > 0) || !(to > 0))
        return false;
    Vec3 n = vec3_normalize(normal), inc = vec3_normalize(incoming);
    if (vec3_dot(n, inc) > 0)
        n = vec3_scale(n, -1);
    Vec3 guide = fabs(n.z) < .9 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    Vec3 tx = vec3_normalize(vec3_cross(guide, n)), ty = vec3_cross(n, tx);
    Vec3 wo = vec3(-vec3_dot(inc, tx), -vec3_dot(inc, ty), -vec3_dot(inc, n));
    if (!(wo.z > 0))
        return false;
    out->delta = from == to || p->bsdf.roughness <= 1e-8;
    double a = fmax(1e-8, p->bsdf.roughness * p->bsdf.roughness);
    Vec3 h = out->delta ? vec3(0, 0, 1) : facet(wo, a, fmin(u, 1 - 1e-12), v);
    Vec3 wh = vec3_add(vec3_add(vec3_scale(tx, h.x), vec3_scale(ty, h.y)), vec3_scale(n, h.z));
    RuntimeMaterialPayload3D physical = *p;
    physical.bsdf.reflectivity = 0;
    physical.thinWalled = false;
    if (!RuntimeDielectricTransport3D_ResolveInterface(&physical, wh, inc, from, to,
                                                       &out->interface))
        return false;
    /* Avoid numerical TIR at a grazing, equal-index boundary. */
    if (from == to) {
        out->interface.totalInternalReflection = false;
        out->interface.hasRefraction = true;
        out->interface.refractionDir = inc;
    }
    double f = RuntimePhysicalGlass3D_Fresnel(vec3_dot(wo, h), from, to);
    out->interface.fresnel = f;
    if (p->thinWalled)
        f = 2 * f / (1 + f); /* paired parallel interfaces, zero thickness */
    out->reflected = choice < f;
    out->direction = out->reflected  ? out->interface.reflectionDir
                     : p->thinWalled ? inc
                                     : out->interface.refractionDir;
    Vec3 wi = vec3(vec3_dot(out->direction, tx), vec3_dot(out->direction, ty),
                   vec3_dot(out->direction, n));
    if (out->reflected ? wi.z <= 0 : wi.z >= 0)
        return false;
    double branch = out->reflected ? f : 1 - f;
    double scale = out->reflected || p->thinWalled ? 1 : sq(from / to);
    if (out->delta) {
        out->pdf = branch;
        out->weight = scale;
    } else {
        double g_o = g1(wo, a), g_i = g1(wi, a);
        double d = a * a / (3.141592653589793 * sq((a * a - 1) * h.z * h.z + 1));
        double ph = d * g_o * fabs(vec3_dot(wo, h)) / wo.z;
        double jac;
        if (out->reflected)
            jac = 1 / (4 * fabs(vec3_dot(wo, h)));
        else if (p->thinWalled) {
            out->delta = true;
            jac = 1;
            ph = 1;
        } else {
            double denom = vec3_dot(wi, h) + vec3_dot(wo, h) * from / to;
            jac = fabs(vec3_dot(wi, h)) / sq(denom);
        }
        out->pdf = branch * ph * jac;
        double g = 1 / (1 / g_o + 1 / g_i - 1);
        out->weight = (p->thinWalled && !out->reflected ? 1 : g / g_o) * scale;
    }
    out->hasMarginalPdf = true;
    out->scatteringOrder = 1;
    out->valid = isfinite(out->pdf) && out->pdf > 0 && isfinite(out->weight) && out->weight >= 0;
    return out->valid;
}

bool RuntimePhysicalGlass3D_EvaluateSingleScatter(const RuntimeMaterialPayload3D *p, Vec3 normal, Vec3 incoming,
                                     Vec3 outgoing, double from, double to, double *f_cos,
                                     double *pdf) {
    if (!f_cos || !pdf)
        return false;
    *f_cos = *pdf = 0;
    if (!p || from == to || p->thinWalled || p->bsdf.roughness <= 1e-8)
        return false;
    Vec3 n = vec3_normalize(normal), wo = vec3_scale(vec3_normalize(incoming), -1),
         wi = vec3_normalize(outgoing);
    if (vec3_dot(n, wo) < 0)
        n = vec3_scale(n, -1);
    double co = vec3_dot(wo, n), ci = vec3_dot(wi, n);
    if (!(co > 0) || ci == 0)
        return false;
    bool reflected = ci > 0;
    double eta = to / from;
    Vec3 h = vec3_normalize(vec3_add(wo, vec3_scale(wi, reflected ? 1 : eta)));
    if (vec3_dot(n, h) < 0)
        h = vec3_scale(h, -1);
    double oh = vec3_dot(wo, h), ih = vec3_dot(wi, h), ch = vec3_dot(n, h);
    if (!(oh > 0) || ih * ci <= 0)
        return false;
    double a = fmax(1e-8, p->bsdf.roughness * p->bsdf.roughness);
    Vec3 vo = vec3(sqrt(fmax(0, 1 - co * co)), 0, co), vi = vec3(sqrt(fmax(0, 1 - ci * ci)), 0, ci);
    double go = g1(vo, a), gi = g1(vi, a), g = 1 / (1 / go + 1 / gi - 1);
    double d = a * a / (3.141592653589793 * sq((a * a - 1) * ch * ch + 1));
    double f = RuntimePhysicalGlass3D_Fresnel(oh, from, to), ph = d * go * oh / co;
    if (reflected) {
        *pdf = f * ph / (4 * oh);
        *f_cos = f * d * g / (4 * co);
    } else {
        double denominator = sq(ih + oh / eta);
        *pdf = (1 - f) * ph * fabs(ih) / denominator;
        *f_cos = (1 - f) * d * g * fabs(ih * oh) / (co * denominator) * sq(from / to);
    }
    return isfinite(*pdf) && isfinite(*f_cos) && *pdf >= 0 && *f_cos >= 0;
}

/* Smooth/equal-index and authored thin-sheet contracts retain their existing
 * estimator. Solid rough interfaces use the complete microsurface walk. */
bool RuntimePhysicalGlass3D_Sample(const RuntimeMaterialPayload3D *p, Vec3 normal, Vec3 incoming,
                                   double from, double to, double u, double v, double choice,
                                   RuntimePhysicalGlass3DSample *out) {
    if (p && !p->thinWalled && from != to && p->bsdf.roughness > 1e-8)
        return RuntimePhysicalGlass3D_SampleMicrosurface(p, normal, incoming, from, to,
                                                        u, v, choice, out);
    return RuntimePhysicalGlass3D_SampleSingleScatter(p, normal, incoming, from, to,
                                                     u, v, choice, out);
}
