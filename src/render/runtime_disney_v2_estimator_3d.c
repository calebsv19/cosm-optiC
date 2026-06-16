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
