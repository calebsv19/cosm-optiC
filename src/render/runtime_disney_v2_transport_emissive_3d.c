#include "render/runtime_disney_v2_transport_internal_3d.h"

#include <math.h>
#include <string.h>

bool RuntimeDisneyV2_3D_AccumulateEmissiveMaterialHit(
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled,
    const Ray3D* ray,
    double throughput_r,
    double throughput_g,
    double throughput_b,
    double mis_weight_bsdf,
    int vertex_index,
    bool recursive_channel,
    RuntimeDisneyV2_3DPathState* io_state,
    double* out_contribution_r,
    double* out_contribution_g,
    double* out_contribution_b,
    RuntimeDisneyV2_3DResult* io_result) {
    double emissive_strength = 0.0;
    double emissive_r = 0.0;
    double emissive_g = 0.0;
    double emissive_b = 0.0;
    double facing = 1.0;
    double contribution_r = 0.0;
    double contribution_g = 0.0;
    double contribution_b = 0.0;

    if (out_contribution_r) *out_contribution_r = 0.0;
    if (out_contribution_g) *out_contribution_g = 0.0;
    if (out_contribution_b) *out_contribution_b = 0.0;
    if (!hit || !payload || !payload->valid || !io_result) return false;

    emissive_strength = principled && principled->valid
                            ? principled->emissiveStrength
                            : payload->emissive;
    if (!(emissive_strength > 1e-9)) {
        return false;
    }

    emissive_r = principled && principled->valid ? principled->emissiveR : payload->baseColorR;
    emissive_g = principled && principled->valid ? principled->emissiveG : payload->baseColorG;
    emissive_b = principled && principled->valid ? principled->emissiveB : payload->baseColorB;
    if (ray && vec3_length(ray->direction) > 1e-9) {
        facing = runtime_disney_v2_transport_3d_clamp01(
            fabs(vec3_dot(hit->normal, vec3_scale(vec3_normalize(ray->direction), -1.0))));
    }

    contribution_r = throughput_r * emissive_r * emissive_strength * facing * mis_weight_bsdf;
    contribution_g = throughput_g * emissive_g * emissive_strength * facing * mis_weight_bsdf;
    contribution_b = throughput_b * emissive_b * emissive_strength * facing * mis_weight_bsdf;
    if (!(runtime_disney_v2_transport_3d_luma(contribution_r,
                                              contribution_g,
                                              contribution_b) > 1e-12)) {
        return false;
    }

    if (recursive_channel) {
        io_result->recursiveBsdfRadianceR += contribution_r;
        io_result->recursiveBsdfRadianceG += contribution_g;
        io_result->recursiveBsdfRadianceB += contribution_b;
    } else {
        io_result->stochasticBsdfRadianceR += contribution_r;
        io_result->stochasticBsdfRadianceG += contribution_g;
        io_result->stochasticBsdfRadianceB += contribution_b;
    }
    runtime_disney_v2_transport_3d_record_bsdf_sample_contribution(
        io_result,
        vertex_index,
        contribution_r,
        contribution_g,
        contribution_b,
        RUNTIME_DISNEY_V2_3D_EMITTER_EMISSIVE_MATERIAL);

    if (io_state) {
        io_state->emitterHit = true;
        io_state->emitterWins = true;
        io_state->emitterHitInfo.hit = true;
        io_state->emitterHitInfo.t = hit->t;
        io_state->emitterHitInfo.position = hit->position;
        io_state->emitterHitInfo.normal = hit->normal;
        io_state->emitterHitInfo.radialFalloff = facing;
        io_state->emitterHitInfo.attenuation = 1.0;
        io_state->emitterHitInfo.radiance =
            runtime_disney_v2_transport_3d_peak(emissive_r * emissive_strength * facing,
                                                emissive_g * emissive_strength * facing,
                                                emissive_b * emissive_strength * facing);
    }
    if (out_contribution_r) *out_contribution_r = contribution_r;
    if (out_contribution_g) *out_contribution_g = contribution_g;
    if (out_contribution_b) *out_contribution_b = contribution_b;
    return true;
}

bool RuntimeDisneyV2_3D_EvaluateEmissiveAreaLightSample(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissiveDirect3DResult* out_area_sample) {
    if (out_area_sample) {
        memset(out_area_sample, 0, sizeof(*out_area_sample));
    }
    if (!scene || !hit || !out_area_sample) {
        return false;
    }
    if (RuntimeEmissiveDirect3D_ShadeHit(scene, hit, sampling, out_area_sample)) {
        return true;
    }
    return out_area_sample->candidateCount > 0 ||
           out_area_sample->selectedCandidateCount > 0 ||
           out_area_sample->visibilityRayCount > 0 ||
           out_area_sample->fullScanFallbackCount > 0;
}

bool RuntimeDisneyV2_3D_ShouldEvaluateEmissiveAreaLightSample(
    const RuntimeScene3D* scene,
    bool recursive_channel,
    RuntimeDisneyV2_3DResult* io_result) {
    int candidate_count = 0;
    int triangle_count = 0;
    bool skip = false;

    if (!scene) {
        return false;
    }

    if (scene->emissiveLightSet.valid) {
        candidate_count = scene->emissiveLightSet.candidateCount;
    } else if (scene->capabilities.valid) {
        candidate_count = scene->capabilities.emissiveLightCandidateCount;
    }
    triangle_count = scene->triangleMesh.triangleCount;

    if (io_result) {
        if (candidate_count > io_result->emissiveAreaCandidateCount) {
            io_result->emissiveAreaCandidateCount = candidate_count;
        }
        if (kRuntimeDisneyV2EmissiveAreaRecursiveCandidateCap >
            io_result->emissiveAreaRecursiveCandidateCap) {
            io_result->emissiveAreaRecursiveCandidateCap =
                kRuntimeDisneyV2EmissiveAreaRecursiveCandidateCap;
        }
        if (kRuntimeDisneyV2EmissiveAreaRecursiveTriangleCap >
            io_result->emissiveAreaRecursiveTriangleCap) {
            io_result->emissiveAreaRecursiveTriangleCap =
                kRuntimeDisneyV2EmissiveAreaRecursiveTriangleCap;
        }
    }

    if (!recursive_channel) {
        return true;
    }
    if (candidate_count <= 0) {
        return false;
    }
    if (candidate_count > kRuntimeDisneyV2EmissiveAreaRecursiveCandidateCap) {
        skip = true;
        if (io_result) {
            io_result->emissiveAreaRecursiveCandidateCapSkipCount += 1;
        }
    }
    if (triangle_count > kRuntimeDisneyV2EmissiveAreaRecursiveTriangleCap) {
        skip = true;
        if (io_result) {
            io_result->emissiveAreaRecursiveTriangleCapSkipCount += 1;
        }
    }
    if (skip) {
        if (io_result) {
            io_result->emissiveAreaRecursivePolicySkipCount += 1;
        }
        return false;
    }
    return true;
}

bool RuntimeDisneyV2_3D_AccumulateEmissiveAreaLightSample(
    const RuntimeEmissiveDirect3DResult* area_sample,
    double throughput_r,
    double throughput_g,
    double throughput_b,
    double mis_weight_light,
    int vertex_index,
    bool recursive_channel,
    RuntimeDisneyV2_3DResult* io_result) {
    double contribution_r = 0.0;
    double contribution_g = 0.0;
    double contribution_b = 0.0;

    if (!area_sample || !io_result || vertex_index < 0 ||
        vertex_index >= RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY) {
        return false;
    }

    io_result->emissiveAreaSampledTriangleCount += area_sample->sampledTriangleCount;
    io_result->emissiveAreaContributingTriangleCount +=
        area_sample->contributingTriangleCount;
    if (area_sample->selectedCandidateCount > 0 ||
        area_sample->sampledTriangleCount > 0 ||
        area_sample->visibilityRayCount > 0 ||
        area_sample->fullScanFallbackCount > 0) {
        io_result->emissiveAreaLightSampleCount += 1;
        if (recursive_channel) {
            io_result->emissiveAreaRecursiveSampleCount += 1;
        } else {
            io_result->emissiveAreaPrimarySampleCount += 1;
        }
    }
    if (area_sample->candidateCount > io_result->emissiveAreaCandidateCount) {
        io_result->emissiveAreaCandidateCount = area_sample->candidateCount;
    }
    io_result->emissiveAreaSelectedCandidateCount += area_sample->selectedCandidateCount;
    io_result->emissiveAreaVisibilityRayCount += area_sample->visibilityRayCount;
    io_result->emissiveAreaFullScanFallbackCount += area_sample->fullScanFallbackCount;

    if (area_sample->contributingTriangleCount <= 0 || !(mis_weight_light > 0.0)) {
        return false;
    }

    contribution_r = throughput_r * area_sample->directRadianceR * mis_weight_light;
    contribution_g = throughput_g * area_sample->directRadianceG * mis_weight_light;
    contribution_b = throughput_b * area_sample->directRadianceB * mis_weight_light;
    if (!(runtime_disney_v2_transport_3d_luma(contribution_r,
                                              contribution_g,
                                              contribution_b) > 1e-12)) {
        return false;
    }

    io_result->emissiveAreaRadianceR += contribution_r;
    io_result->emissiveAreaRadianceG += contribution_g;
    io_result->emissiveAreaRadianceB += contribution_b;

    io_result->lightSampleContributionR[vertex_index] += contribution_r;
    io_result->lightSampleContributionG[vertex_index] += contribution_g;
    io_result->lightSampleContributionB[vertex_index] += contribution_b;
    io_result->lightSampleContributionTotalR += contribution_r;
    io_result->lightSampleContributionTotalG += contribution_g;
    io_result->lightSampleContributionTotalB += contribution_b;
    io_result->lightSampleContributionCount += 1;

    if (recursive_channel) {
        io_result->recursiveBsdfRadianceR += contribution_r;
        io_result->recursiveBsdfRadianceG += contribution_g;
        io_result->recursiveBsdfRadianceB += contribution_b;
    } else {
        io_result->stochasticDirectRadianceR += contribution_r;
        io_result->stochasticDirectRadianceG += contribution_g;
        io_result->stochasticDirectRadianceB += contribution_b;
    }
    return true;
}
