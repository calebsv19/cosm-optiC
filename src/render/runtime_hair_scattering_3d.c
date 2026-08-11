#include "render/runtime_hair_scattering_3d.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double hair_clamp(double value, double low, double high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static double hair_gaussian_profile(double x, double sigma) {
    const double variance = sigma * sigma;
    return exp(-(x * x) / (2.0 * variance));
}

static double hair_logistic(double x, double scale) {
    const double e = exp(-fabs(x) / scale);
    return e / (scale * ((1.0 + e) * (1.0 + e)));
}

static double hair_wrap_pi(double angle) {
    while (angle > M_PI) angle -= 2.0 * M_PI;
    while (angle < -M_PI) angle += 2.0 * M_PI;
    return angle;
}

static double hair_trimmed_logistic(double angle, double center, double scale) {
    const double x = hair_wrap_pi(angle - center);
    const double normalization = 1.0 - (2.0 / (1.0 + exp(M_PI / scale)));
    return hair_logistic(x, scale) / fmax(normalization, 1.0e-9);
}

static double hair_trimmed_logistic_profile(double angle,
                                            double center,
                                            double scale) {
    const double value = hair_trimmed_logistic(angle, center, scale);
    const double peak = hair_trimmed_logistic(center, center, scale);
    return hair_clamp(value / fmax(peak, 1.0e-9), 0.0, 1.0);
}

static Vec3 hair_perpendicular(Vec3 direction, Vec3 tangent, Vec3 fallback) {
    Vec3 projected = vec3_sub(direction, vec3_scale(tangent, vec3_dot(direction, tangent)));
    if (vec3_length(projected) <= 1.0e-9) {
        projected = vec3_sub(fallback, vec3_scale(tangent, vec3_dot(fallback, tangent)));
    }
    if (vec3_length(projected) <= 1.0e-9) {
        projected = fabs(tangent.x) < 0.8 ? vec3(1.0, 0.0, 0.0)
                                         : vec3(0.0, 1.0, 0.0);
        projected = vec3_sub(projected,
                             vec3_scale(tangent, vec3_dot(projected, tangent)));
    }
    return vec3_normalize(projected);
}

RuntimeHairOptics3D RuntimeHairOptics3D_Default(void) {
    RuntimeHairOptics3D optics = {0};
    optics.absorptionR = 0.35;
    optics.absorptionG = 0.70;
    optics.absorptionB = 1.20;
    optics.longitudinalRoughness = 0.30;
    optics.azimuthalRoughness = 0.35;
    optics.ior = 1.55;
    optics.cuticleTiltDegrees = 2.0;
    return optics;
}

RuntimeHairOptics3D RuntimeHairOptics3D_Normalize(
    const RuntimeHairOptics3D* optics) {
    RuntimeHairOptics3D normalized = RuntimeHairOptics3D_Default();
    if (!optics) return normalized;
    normalized.enabled = optics->enabled;
    normalized.absorptionR = hair_clamp(optics->absorptionR, 0.0, 8.0);
    normalized.absorptionG = hair_clamp(optics->absorptionG, 0.0, 8.0);
    normalized.absorptionB = hair_clamp(optics->absorptionB, 0.0, 8.0);
    normalized.longitudinalRoughness =
        hair_clamp(optics->longitudinalRoughness, 0.02, 1.0);
    normalized.azimuthalRoughness =
        hair_clamp(optics->azimuthalRoughness, 0.02, 1.0);
    normalized.ior = hair_clamp(optics->ior, 1.0, 2.5);
    normalized.cuticleTiltDegrees =
        hair_clamp(optics->cuticleTiltDegrees, -10.0, 10.0);
    return normalized;
}

bool RuntimeHairScattering3D_ShouldApply(
    bool has_curve_tangent,
    const RuntimeHairOptics3D* optics) {
    return has_curve_tangent && optics && optics->enabled;
}

bool RuntimeHairScattering3D_EvaluateSingleFiber(
    const RuntimeHairOptics3D* optics,
    Vec3 tangent,
    Vec3 geometric_normal,
    Vec3 light_direction,
    Vec3 view_direction,
    RuntimeHairScattering3DResult* out_result) {
    RuntimeHairScattering3DResult result = {0};
    RuntimeHairOptics3D p = RuntimeHairOptics3D_Normalize(optics);
    Vec3 wi_perp;
    Vec3 wo_perp;
    double theta_i;
    double theta_o;
    double delta_phi;
    double alpha;
    double beta_m;
    double beta_n;
    double cos_incident;
    double f0;
    double fresnel;
    double transmission_r;
    double transmission_g;
    double transmission_b;
    double longitudinal_r;
    double longitudinal_tt;
    double longitudinal_trt;
    double longitudinal_h;
    double azimuth_r;
    double azimuth_tt;
    double azimuth_trt;
    double azimuth_h;
    double r_weight;
    double tt_weight_r;
    double tt_weight_g;
    double tt_weight_b;
    double trt_weight_r;
    double trt_weight_g;
    double trt_weight_b;
    double h_weight_r;
    double h_weight_g;
    double h_weight_b;

    if (!out_result) return false;
    memset(out_result, 0, sizeof(*out_result));
    if (!p.enabled) return false;
    if (vec3_length(tangent) <= 1.0e-9 ||
        vec3_length(light_direction) <= 1.0e-9 ||
        vec3_length(view_direction) <= 1.0e-9) {
        return false;
    }

    tangent = vec3_normalize(tangent);
    light_direction = vec3_normalize(light_direction);
    view_direction = vec3_normalize(view_direction);
    wi_perp = hair_perpendicular(light_direction, tangent, geometric_normal);
    wo_perp = hair_perpendicular(view_direction, tangent, geometric_normal);
    theta_i = asin(hair_clamp(vec3_dot(light_direction, tangent), -1.0, 1.0));
    theta_o = asin(hair_clamp(vec3_dot(view_direction, tangent), -1.0, 1.0));
    delta_phi = atan2(vec3_dot(vec3_cross(wi_perp, wo_perp), tangent),
                      vec3_dot(wi_perp, wo_perp));

    alpha = p.cuticleTiltDegrees * M_PI / 180.0;
    beta_m = 0.08 + 0.48 * p.longitudinalRoughness;
    beta_n = 0.04 + 0.55 * p.azimuthalRoughness;
    longitudinal_r =
        fmax(hair_gaussian_profile(
                 theta_i + theta_o + (2.0 * alpha), beta_m),
             0.01);
    longitudinal_tt =
        fmax(hair_gaussian_profile(
                 theta_i + theta_o - alpha, beta_m * 0.85),
             0.01);
    longitudinal_trt =
        fmax(hair_gaussian_profile(
                 theta_i + theta_o - (4.0 * alpha), beta_m * 1.20),
             0.01);
    longitudinal_h =
        fmax(hair_gaussian_profile(
                 theta_i + theta_o - (6.0 * alpha), beta_m * 1.45),
             0.01);
    azimuth_r = hair_trimmed_logistic_profile(delta_phi, 0.0, beta_n);
    azimuth_tt =
        hair_trimmed_logistic_profile(delta_phi, M_PI, beta_n * 1.15);
    azimuth_trt = hair_trimmed_logistic_profile(
        delta_phi, 0.5 * M_PI, beta_n * 1.35);
    azimuth_h = hair_trimmed_logistic_profile(
        delta_phi, -0.5 * M_PI, beta_n * 1.60);

    cos_incident = sqrt(fmax(0.0, 1.0 - vec3_dot(light_direction, tangent) *
                                             vec3_dot(light_direction, tangent)));
    f0 = ((p.ior - 1.0) / (p.ior + 1.0));
    f0 *= f0;
    fresnel = f0 + (1.0 - f0) * pow(1.0 - cos_incident, 5.0);
    transmission_r = exp(-p.absorptionR / fmax(cos_incident, 0.15));
    transmission_g = exp(-p.absorptionG / fmax(cos_incident, 0.15));
    transmission_b = exp(-p.absorptionB / fmax(cos_incident, 0.15));

    r_weight = fresnel;
    tt_weight_r = (1.0 - fresnel) * (1.0 - fresnel) * transmission_r;
    tt_weight_g = (1.0 - fresnel) * (1.0 - fresnel) * transmission_g;
    tt_weight_b = (1.0 - fresnel) * (1.0 - fresnel) * transmission_b;
    trt_weight_r = tt_weight_r * fresnel * transmission_r;
    trt_weight_g = tt_weight_g * fresnel * transmission_g;
    trt_weight_b = tt_weight_b * fresnel * transmission_b;
    h_weight_r = trt_weight_r * fresnel * transmission_r /
                 fmax(1.0 - fresnel * transmission_r, 0.1);
    h_weight_g = trt_weight_g * fresnel * transmission_g /
                 fmax(1.0 - fresnel * transmission_g, 0.1);
    h_weight_b = trt_weight_b * fresnel * transmission_b /
                 fmax(1.0 - fresnel * transmission_b, 0.1);

    /*
     * The current renderer is a deterministic one-sample direct-light
     * estimator, not a solid-angle importance sampler for normalized hair
     * densities. Use unit-peak angular profiles and the energy-partitioning
     * path weights directly. This preserves lobe width/chroma while keeping
     * every channel bounded by the incident light instead of multiplying by
     * the former arbitrary 750x conversion factor.
     */
    result.r = r_weight * longitudinal_r * azimuth_r +
               tt_weight_r * longitudinal_tt * azimuth_tt +
               trt_weight_r * longitudinal_trt * azimuth_trt +
               h_weight_r * longitudinal_h * azimuth_h;
    result.g = r_weight * longitudinal_r * azimuth_r +
               tt_weight_g * longitudinal_tt * azimuth_tt +
               trt_weight_g * longitudinal_trt * azimuth_trt +
               h_weight_g * longitudinal_h * azimuth_h;
    result.b = r_weight * longitudinal_r * azimuth_r +
               tt_weight_b * longitudinal_tt * azimuth_tt +
               trt_weight_b * longitudinal_trt * azimuth_trt +
               h_weight_b * longitudinal_h * azimuth_h;
    result.r = hair_clamp(result.r, 0.0, 1.0);
    result.g = hair_clamp(result.g, 0.0, 1.0);
    result.b = hair_clamp(result.b, 0.0, 1.0);
    result.lobeR = r_weight * longitudinal_r * azimuth_r;
    result.lobeTT = (tt_weight_r + tt_weight_g + tt_weight_b) /
                    3.0 * longitudinal_tt * azimuth_tt;
    result.lobeTRT = (trt_weight_r + trt_weight_g + trt_weight_b) /
                     3.0 * longitudinal_trt * azimuth_trt;
    result.lobeHigherOrder =
        (h_weight_r + h_weight_g + h_weight_b) /
        3.0 * longitudinal_h * azimuth_h;
    result.longitudinal =
        longitudinal_r + longitudinal_tt + longitudinal_trt + longitudinal_h;
    result.azimuthal = azimuth_r + azimuth_tt + azimuth_trt + azimuth_h;
    result.valid = isfinite(result.r) && isfinite(result.g) && isfinite(result.b);
    *out_result = result;
    return result.valid;
}
