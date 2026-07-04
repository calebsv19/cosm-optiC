#include "render/runtime_disney_v2_estimator_3d.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double kRuntimeDisneyV2Estimator3DPdfEpsilon = 1e-12;
static const double kRuntimeDisneyV2Estimator3DPower = 2.0;
static const double kRuntimeDisneyV2Estimator3DPointLightPdf = 1.0;
static const double kRuntimeDisneyV2Estimator3DEmissiveAreaPdf = 1.0;
static const double kRuntimeDisneyV2Estimator3DMaxPdf = 1.0e6;

static double runtime_disney_v2_estimator_3d_clamp(double value,
                                                   double min_value,
                                                   double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

bool RuntimeDisneyV2_3D_ResolvePowerHeuristicMIS(
    double light_pdf,
    double bsdf_pdf,
    RuntimeDisneyV2_3DMisWeights* out_weights) {
    RuntimeDisneyV2_3DMisWeights weights = {0};
    double light_term = 0.0;
    double bsdf_term = 0.0;
    double total = 0.0;

    if (!out_weights) return false;
    light_pdf = fmax(light_pdf, 0.0);
    bsdf_pdf = fmax(bsdf_pdf, 0.0);
    light_term = light_pdf * light_pdf;
    bsdf_term = bsdf_pdf * bsdf_pdf;
    total = light_term + bsdf_term;

    weights.lightPdf = light_pdf;
    weights.bsdfPdf = bsdf_pdf;
    weights.heuristicPower = kRuntimeDisneyV2Estimator3DPower;
    if (total > kRuntimeDisneyV2Estimator3DPdfEpsilon) {
        weights.lightWeight = runtime_disney_v2_estimator_3d_clamp(light_term / total,
                                                                   0.0,
                                                                   1.0);
        weights.bsdfWeight = runtime_disney_v2_estimator_3d_clamp(bsdf_term / total,
                                                                  0.0,
                                                                  1.0);
    }

    *out_weights = weights;
    return total > kRuntimeDisneyV2Estimator3DPdfEpsilon;
}

double RuntimeDisneyV2_3D_EstimateDirectLightPdf(bool has_finite_light,
                                                 double finite_light_radius,
                                                 double finite_light_distance,
                                                 int emissive_candidate_count,
                                                 bool has_emissive_area_direct) {
    double pdf = 0.0;

    if (has_finite_light) {
        if (finite_light_radius > 1e-6 && finite_light_distance > 1e-6) {
            const double area = M_PI * finite_light_radius * finite_light_radius;
            const double distance2 = finite_light_distance * finite_light_distance;
            pdf = runtime_disney_v2_estimator_3d_clamp(distance2 / fmax(area, 1e-12),
                                                       kRuntimeDisneyV2Estimator3DPdfEpsilon,
                                                       kRuntimeDisneyV2Estimator3DMaxPdf);
        } else {
            pdf = kRuntimeDisneyV2Estimator3DPointLightPdf;
        }
    }

    if (has_emissive_area_direct || emissive_candidate_count > 0) {
        pdf = fmax(pdf, kRuntimeDisneyV2Estimator3DEmissiveAreaPdf);
    }

    return pdf;
}

double RuntimeDisneyV2_3D_EstimateDirectLightPdfForHit(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    bool has_emissive_area_direct) {
    double finite_light_distance = 0.0;
    int emissive_candidate_count = 0;

    if (!scene) return 0.0;
    if (scene->hasLight && hit) {
        finite_light_distance = vec3_length(vec3_sub(scene->light.position, hit->position));
    }
    if (scene->emissiveLightSet.valid) {
        emissive_candidate_count = scene->emissiveLightSet.candidateCount;
    } else if (scene->capabilities.valid) {
        emissive_candidate_count = scene->capabilities.emissiveLightCandidateCount;
    }

    return RuntimeDisneyV2_3D_EstimateDirectLightPdf(scene->hasLight,
                                                     scene->light.radius,
                                                     finite_light_distance,
                                                     emissive_candidate_count,
                                                     has_emissive_area_direct);
}

double RuntimeDisneyV2_3D_EstimateFiniteLightPdfForHit(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit) {
    double finite_light_distance = 0.0;

    if (!scene || !scene->hasLight) return 0.0;
    if (hit) {
        finite_light_distance = vec3_length(vec3_sub(scene->light.position, hit->position));
    }
    return RuntimeDisneyV2_3D_EstimateDirectLightPdf(scene->hasLight,
                                                     scene->light.radius,
                                                     finite_light_distance,
                                                     0,
                                                     false);
}

double RuntimeDisneyV2_3D_EstimateDirectLightPdfForHitWithAreaPdf(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    bool has_emissive_area_direct,
    double emissive_area_light_pdf) {
    double pdf = 0.0;

    if (!scene) return 0.0;
    pdf = RuntimeDisneyV2_3D_EstimateDirectLightPdfForHit(scene,
                                                          hit,
                                                          has_emissive_area_direct);
    if (has_emissive_area_direct && emissive_area_light_pdf > 0.0) {
        pdf = fmax(pdf, runtime_disney_v2_estimator_3d_clamp(
                            emissive_area_light_pdf,
                            kRuntimeDisneyV2Estimator3DPdfEpsilon,
                            kRuntimeDisneyV2Estimator3DMaxPdf));
    }
    return pdf;
}

static double runtime_disney_v2_estimator_3d_principled_luma(double r,
                                                             double g,
                                                             double b) {
    return runtime_disney_v2_estimator_3d_clamp((0.2126 * r) + (0.7152 * g) +
                                                    (0.0722 * b),
                                                0.0,
                                                1.0);
}

static void runtime_disney_v2_estimator_3d_lobe_probabilities(
    const RuntimePrincipledBSDF3D* bsdf,
    double* out_diffuse_probability,
    double* out_specular_probability,
    double* out_transmission_probability) {
    double diffuse_probability = 0.0;
    double specular_probability = 0.0;
    double transmission_probability = 0.0;
    double probability_total = 0.0;

    if (!bsdf) {
        if (out_diffuse_probability) *out_diffuse_probability = 1.0;
        if (out_specular_probability) *out_specular_probability = 0.0;
        if (out_transmission_probability) *out_transmission_probability = 0.0;
        return;
    }

    diffuse_probability = runtime_disney_v2_estimator_3d_clamp(bsdf->diffuseWeight,
                                                               0.0,
                                                               1.0);
    specular_probability = runtime_disney_v2_estimator_3d_clamp(bsdf->specularWeight,
                                                                0.0,
                                                                1.0);
    transmission_probability =
        runtime_disney_v2_estimator_3d_clamp(bsdf->transmissionWeight, 0.0, 1.0);
    probability_total = diffuse_probability + specular_probability +
                        transmission_probability;

    if (probability_total > 1e-9) {
        diffuse_probability /= probability_total;
        specular_probability /= probability_total;
        transmission_probability /= probability_total;
    } else {
        diffuse_probability = 1.0;
    }

    if (out_diffuse_probability) *out_diffuse_probability = diffuse_probability;
    if (out_specular_probability) *out_specular_probability = specular_probability;
    if (out_transmission_probability) {
        *out_transmission_probability = transmission_probability;
    }
}

static double runtime_disney_v2_estimator_3d_ggx_distribution(double alpha,
                                                              double cos_theta_h) {
    double clamped = runtime_disney_v2_estimator_3d_clamp(cos_theta_h, 0.0, 1.0);
    double alpha2 = alpha * alpha;
    double denom = (clamped * clamped * (alpha2 - 1.0)) + 1.0;
    denom = M_PI * denom * denom;
    if (denom <= 1e-12) return 0.0;
    return alpha2 / denom;
}

static double runtime_disney_v2_estimator_3d_transmission_pdf(
    const RuntimePrincipledBSDF3D* bsdf,
    Vec3 normal,
    Vec3 wi,
    Vec3 wo) {
    Vec3 oriented_normal = normal;
    Vec3 half_vector = vec3(0.0, 0.0, 0.0);
    double cos_i_signed = 0.0;
    double cos_o_signed = 0.0;
    double cos_i = 0.0;
    double cos_o = 0.0;
    double eta_i = 1.0;
    double eta_t = 1.0;
    double eta = 1.0;
    double sin2_t = 0.0;
    double f0 = 0.04;
    double fresnel = 0.0;
    double tint_luma = 1.0;
    double alpha = 0.0;
    double cos_theta_h = 0.0;
    double dot_i_h = 0.0;
    double dot_o_h = 0.0;
    double sqrt_denom = 0.0;
    double dwh_dwo = 0.0;
    double h_pdf = 0.0;
    double pdf = 0.0;

    if (!bsdf) return 0.0;
    normal = vec3_normalize(normal);
    wi = vec3_normalize(wi);
    wo = vec3_normalize(wo);
    if (!(vec3_length(normal) > 1e-9) ||
        !(vec3_length(wi) > 1e-9) ||
        !(vec3_length(wo) > 1e-9)) {
        return 0.0;
    }

    cos_i_signed = vec3_dot(normal, wi);
    cos_o_signed = vec3_dot(normal, wo);
    if (fabs(cos_i_signed) <= 1e-9 || fabs(cos_o_signed) <= 1e-9) {
        return 0.0;
    }
    if (cos_i_signed * cos_o_signed >= -1e-9) {
        return 0.0;
    }

    if (cos_i_signed < 0.0) {
        oriented_normal = vec3_scale(normal, -1.0);
        eta_i = bsdf->ior;
        eta_t = 1.0;
    } else {
        oriented_normal = normal;
        eta_i = 1.0;
        eta_t = bsdf->ior;
    }

    cos_i = runtime_disney_v2_estimator_3d_clamp(vec3_dot(oriented_normal, wi),
                                                 0.0,
                                                 1.0);
    cos_o = runtime_disney_v2_estimator_3d_clamp(-vec3_dot(oriented_normal, wo),
                                                 0.0,
                                                 1.0);
    if (cos_i <= 1e-9 || cos_o <= 1e-9) return 0.0;

    eta = eta_i / fmax(eta_t, 1e-9);
    sin2_t = eta * eta * fmax(0.0, 1.0 - (cos_i * cos_i));
    if (sin2_t >= 1.0) return 0.0;

    f0 = fmax(bsdf->dielectricF0, RuntimePrincipledBSDF3D_DielectricF0FromIor(bsdf->ior));
    f0 = runtime_disney_v2_estimator_3d_clamp(f0, 0.0, 1.0);
    fresnel = RuntimePrincipledBSDF3D_FresnelSchlick(cos_i, f0);
    if (fresnel >= 1.0 - 1e-9) return 0.0;

    half_vector = vec3_normalize(vec3_add(vec3_scale(wi, eta_i),
                                          vec3_scale(wo, eta_t)));
    if (!(vec3_length(half_vector) > 1e-9)) return 0.0;
    if (vec3_dot(half_vector, oriented_normal) < 0.0) {
        half_vector = vec3_scale(half_vector, -1.0);
    }

    dot_i_h = vec3_dot(wi, half_vector);
    dot_o_h = vec3_dot(wo, half_vector);
    if (dot_i_h <= 1e-9 || dot_o_h >= -1e-9) return 0.0;

    sqrt_denom = (eta_i * dot_i_h) + (eta_t * dot_o_h);
    if (fabs(sqrt_denom) <= 1e-9) return 0.0;

    cos_theta_h = runtime_disney_v2_estimator_3d_clamp(
        vec3_dot(oriented_normal, half_vector),
        0.0,
        1.0);
    if (cos_theta_h <= 1e-9) return 0.0;

    alpha = fmax(bsdf->roughness * bsdf->roughness, 1e-3);
    dwh_dwo = fabs((eta_t * eta_t * dot_o_h) / (sqrt_denom * sqrt_denom));
    h_pdf = runtime_disney_v2_estimator_3d_ggx_distribution(alpha, cos_theta_h) *
            cos_theta_h;
    tint_luma = fmax(runtime_disney_v2_estimator_3d_principled_luma(bsdf->baseColorR,
                                                                    bsdf->baseColorG,
                                                                    bsdf->baseColorB),
                     0.05);
    pdf = h_pdf * dwh_dwo * (1.0 - fresnel) * tint_luma;
    return runtime_disney_v2_estimator_3d_clamp(pdf,
                                                0.0,
                                                kRuntimeDisneyV2Estimator3DMaxPdf);
}

double RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(const RuntimePrincipledBSDF3D* principled,
                                                const HitInfo3D* hit,
                                                Vec3 incoming_dir,
                                                Vec3 light_dir,
                                                bool include_transmission) {
    RuntimePrincipledBSDF3D bsdf = {0};
    Vec3 normal = hit ? hit->normal : vec3(0.0, 1.0, 0.0);
    Vec3 wi = vec3_normalize(vec3_scale(incoming_dir, -1.0));
    Vec3 wo = vec3_normalize(light_dir);
    double cos_i = 0.0;
    double cos_o = 0.0;
    double diffuse_probability = 0.0;
    double specular_probability = 0.0;
    double transmission_probability = 0.0;
    double pdf = 0.0;

    if (!(vec3_length(wo) > 1e-9)) return 0.0;
    normal = vec3_normalize(normal);
    if (!(vec3_length(normal) > 1e-9)) {
        normal = vec3(0.0, 1.0, 0.0);
    }
    if (!(vec3_length(wi) > 1e-9)) {
        wi = normal;
    }
    bsdf = principled ? RuntimePrincipledBSDF3D_Normalize(*principled)
                      : RuntimePrincipledBSDF3D_Default();

    cos_i = vec3_dot(normal, wi);
    cos_o = vec3_dot(normal, wo);
    runtime_disney_v2_estimator_3d_lobe_probabilities(&bsdf,
                                                      &diffuse_probability,
                                                      &specular_probability,
                                                      &transmission_probability);

    if (fabs(cos_o) > 1e-9 && diffuse_probability > 0.0) {
        pdf += diffuse_probability * fabs(cos_o) / M_PI;
    }
    if (fabs(cos_o) > 1e-9 && specular_probability > 0.0) {
        Vec3 half_vector = vec3_normalize(vec3_add(wi, wo));
        const double cos_theta_h = runtime_disney_v2_estimator_3d_clamp(
            fabs(vec3_dot(normal, half_vector)),
            0.0,
            1.0);
        const double dot_i_h = runtime_disney_v2_estimator_3d_clamp(
            fabs(vec3_dot(wi, half_vector)),
            0.0,
            1.0);
        const double specular_pdf =
            RuntimePrincipledBSDF3D_GGXHalfVectorPdf(&bsdf, cos_theta_h, dot_i_h);
        pdf += specular_probability * specular_pdf;
    }
    if (include_transmission &&
        cos_i * cos_o < -1e-9 &&
        transmission_probability > 0.0) {
        const double transmission_pdf =
            runtime_disney_v2_estimator_3d_transmission_pdf(&bsdf, normal, wi, wo);
        pdf += transmission_probability * transmission_pdf;
    }

    return runtime_disney_v2_estimator_3d_clamp(pdf,
                                                0.0,
                                                kRuntimeDisneyV2Estimator3DMaxPdf);
}

double RuntimeDisneyV2_3D_EstimateDirectBsdfPdfForSceneLight(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimePrincipledBSDF3D* principled,
    Vec3 incoming_dir,
    bool include_transmission) {
    Vec3 light_dir = vec3(0.0, 0.0, 0.0);
    double light_distance = 0.0;

    if (!scene || !scene->hasLight || !hit) return 0.0;
    light_dir = vec3_sub(scene->light.position, hit->position);
    light_distance = vec3_length(light_dir);
    if (!(light_distance > 1e-9)) return 0.0;
    light_dir = vec3_scale(light_dir, 1.0 / light_distance);
    return RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(principled,
                                                    hit,
                                                    incoming_dir,
                                                    light_dir,
                                                    include_transmission);
}

int RuntimeDisneyV2_3D_RoughReflectionEstimatorSampleCount(double roughness) {
    roughness = runtime_disney_v2_estimator_3d_clamp(roughness, 0.0, 1.0);
    if (roughness <= 0.08) return 0;
    if (roughness < 0.30) return 2;
    return 4;
}

double RuntimeDisneyV2_3D_EstimatorSampleWeight(int sample_count) {
    if (sample_count <= 1) return 1.0;
    return 1.0 / (double)sample_count;
}
