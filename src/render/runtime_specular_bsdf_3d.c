#include "render/runtime_specular_bsdf_3d.h"
#include <math.h>

static const double pi = 3.14159265358979323846;
static double clamp01(double x) { return fmax(0.0, fmin(1.0, x)); }
static double alpha_for(const RuntimePrincipledBSDF3D *bsdf) {
    return fmax(bsdf->roughness * bsdf->roughness, 1e-3);
}
static double distribution(double alpha, double nh) {
    const double a2 = alpha * alpha;
    const double denominator = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / (pi * denominator * denominator);
}
static double masking(double alpha, double cosine) {
    if (!(cosine > 0.0)) return 0.0;
    return 2.0 / (1.0 + sqrt(1.0 + alpha * alpha *
        fmax(0.0, 1.0 - cosine * cosine) / (cosine * cosine)));
}
static bool valid_sides(const HitInfo3D *hit, Vec3 view, Vec3 direction, Vec3 normal) {
    Vec3 geometric = vec3_normalize(HitInfo3D_OffsetNormal(hit));
    return vec3_dot(view, normal) > 1e-9 && vec3_dot(direction, normal) > 1e-9 &&
           vec3_dot(view, geometric) > 1e-9 && vec3_dot(direction, geometric) > 1e-9;
}
double RuntimeSpecularBSDF3D_Pdf(const RuntimePrincipledBSDF3D *bsdf,
    const HitInfo3D *hit, Vec3 view, Vec3 direction) {
    if (!bsdf || !bsdf->valid || !hit) return 0.0;
    view = vec3_normalize(view);
    direction = vec3_normalize(direction);
    Vec3 normal = HitInfo3D_ShadingNormalForReflection(hit, view);
    if (!valid_sides(hit, view, direction, normal)) return 0.0;
    Vec3 half = vec3_normalize(vec3_add(view, direction));
    double nh = vec3_dot(normal, half), vh = vec3_dot(view, half);
    if (!(nh > 0.0) || !(vh > 1e-9)) return 0.0;
    return distribution(alpha_for(bsdf), nh) * nh / (4.0 * vh);
}
RuntimeSpecularBSDF3DSample RuntimeSpecularBSDF3D_Sample(
    const RuntimePrincipledBSDF3D *bsdf, const HitInfo3D *hit, Vec3 view,
    double lobe_probability, double u, double v) {
    RuntimeSpecularBSDF3DSample sample = {0};
    if (!bsdf || !bsdf->valid || !hit || !(lobe_probability > 0.0)) return sample;
    view = vec3_normalize(view);
    Vec3 normal = HitInfo3D_ShadingNormalForReflection(hit, view);
    Vec3 axis = fabs(normal.z) < 0.999 ? vec3(0,0,1) : vec3(0,1,0);
    Vec3 tangent = vec3_normalize(vec3_cross(axis, normal));
    Vec3 bitangent = vec3_cross(normal, tangent);
    double alpha = alpha_for(bsdf);
    double phi = 2.0 * pi * clamp01(u);
    v = fmin(clamp01(v), 1.0 - 1e-12);
    double nh = sqrt((1.0 - v) / (1.0 + (alpha * alpha - 1.0) * v));
    double sine = sqrt(fmax(0.0, 1.0 - nh * nh));
    Vec3 half = vec3_normalize(vec3_add(vec3_scale(normal, nh),
        vec3_add(vec3_scale(tangent, sine * cos(phi)), vec3_scale(bitangent, sine * sin(phi)))));
    double vh = vec3_dot(view, half);
    if (!(vh > 1e-9)) return sample;
    sample.direction = vec3_normalize(vec3_sub(vec3_scale(half, 2.0 * vh), view));
    double conditional_pdf = RuntimeSpecularBSDF3D_Pdf(bsdf, hit, view, sample.direction);
    if (!(conditional_pdf > 0.0)) return sample;
    sample.pdf = lobe_probability * conditional_pdf;
    sample.cosTheta = vec3_dot(normal, sample.direction);
    double nv = vec3_dot(normal, view);
    double scale = distribution(alpha, nh) * masking(alpha, nv) * masking(alpha, sample.cosTheta) /
                   (4.0 * nv * sample.pdf);
    sample.throughputR = RuntimePrincipledBSDF3D_FresnelSchlick(vh, bsdf->specularF0R) * scale;
    sample.throughputG = RuntimePrincipledBSDF3D_FresnelSchlick(vh, bsdf->specularF0G) * scale;
    sample.throughputB = RuntimePrincipledBSDF3D_FresnelSchlick(vh, bsdf->specularF0B) * scale;
    sample.valid = isfinite(sample.throughputR) && isfinite(sample.throughputG) && isfinite(sample.throughputB);
    if (!sample.valid) return (RuntimeSpecularBSDF3DSample){0};
    return sample;
}
