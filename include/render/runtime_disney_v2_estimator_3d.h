#ifndef RENDER_RUNTIME_DISNEY_V2_ESTIMATOR_3D_H
#define RENDER_RUNTIME_DISNEY_V2_ESTIMATOR_3D_H

#include <stdbool.h>

#include "render/runtime_ray_3d.h"

typedef struct {
    double lightPdf;
    double bsdfPdf;
    double lightWeight;
    double bsdfWeight;
    double heuristicPower;
} RuntimeDisneyV2_3DMisWeights;

bool RuntimeDisneyV2_3D_ResolvePowerHeuristicMIS(
    double light_pdf,
    double bsdf_pdf,
    RuntimeDisneyV2_3DMisWeights* out_weights);

double RuntimeDisneyV2_3D_EstimateDirectLightPdf(bool has_finite_light,
                                                 double finite_light_radius,
                                                 double finite_light_distance,
                                                 int emissive_candidate_count,
                                                 bool has_emissive_area_direct);

double RuntimeDisneyV2_3D_EstimateDirectLightPdfForHit(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    bool has_emissive_area_direct);

int RuntimeDisneyV2_3D_RoughReflectionEstimatorSampleCount(double roughness);

double RuntimeDisneyV2_3D_EstimatorSampleWeight(int sample_count);

#endif
