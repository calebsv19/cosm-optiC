#include "render/runtime_disney_v2_transport_3d.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "render/runtime_disney_v2_estimator_3d.h"
#include "render/runtime_disney_v2_transmission_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_path_depth_policy_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double kRuntimeDisneyV2Transport3DEpsilon = 1e-4;
static const double kRuntimeDisneyV2Transport3DMaxDistance = 48.0;
static const double kRuntimeDisneyV2Transport3DNegligibleLuma = 1e-9;
static const int kRuntimeDisneyV2EmissiveAreaRecursiveCandidateCap = 16;
static const int kRuntimeDisneyV2EmissiveAreaRecursiveTriangleCap = 8192;

typedef struct {
    RuntimeDisneyV2_3DDominantLobe lobe;
    RuntimePathDepthPolicy3DLobe policyLobe;
    Vec3 direction;
    double throughputR;
    double throughputG;
    double throughputB;
    double pdf;
    double cosTheta;
    double lightPdf;
    double directBsdfPdf;
    double misWeightLight;
    double misWeightBsdf;
    RuntimeDisneyV2_3DMisBranch finiteLightMis;
    RuntimeDisneyV2_3DMisBranch emissiveAreaMis;
} RuntimeDisneyV2Transport3DVertexSample;

static double runtime_disney_v2_transport_3d_clamp(double value,
                                                   double min_value,
                                                   double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_disney_v2_transport_3d_clamp01(double value) {
    return runtime_disney_v2_transport_3d_clamp(value, 0.0, 1.0);
}

static double runtime_disney_v2_transport_3d_luma(double r, double g, double b) {
    return (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
}

static double runtime_disney_v2_transport_3d_peak(double r, double g, double b) {
    double peak = r;
    if (g > peak) peak = g;
    if (b > peak) peak = b;
    return peak;
}

static void runtime_disney_v2_transport_3d_balance_mis(double light_pdf,
                                                       double bsdf_pdf,
                                                       double* out_light,
                                                       double* out_bsdf) {
    RuntimeDisneyV2_3DMisWeights weights = {0};

    (void)RuntimeDisneyV2_3D_ResolvePowerHeuristicMIS(light_pdf, bsdf_pdf, &weights);
    if (out_light) *out_light = runtime_disney_v2_transport_3d_clamp01(weights.lightWeight);
    if (out_bsdf) *out_bsdf = runtime_disney_v2_transport_3d_clamp01(weights.bsdfWeight);
}

static RuntimeDisneyV2_3DMisBranch runtime_disney_v2_transport_3d_make_mis_branch(
    double light_pdf,
    double bsdf_pdf) {
    RuntimeDisneyV2_3DMisWeights weights = {0};
    RuntimeDisneyV2_3DMisBranch branch = {0};

    (void)RuntimeDisneyV2_3D_ResolvePowerHeuristicMIS(light_pdf, bsdf_pdf, &weights);
    branch.lightPdf = weights.lightPdf;
    branch.bsdfPdf = weights.bsdfPdf;
    branch.weightLight = runtime_disney_v2_transport_3d_clamp01(weights.lightWeight);
    branch.weightBsdf = runtime_disney_v2_transport_3d_clamp01(weights.bsdfWeight);
    return branch;
}

static void runtime_disney_v2_transport_3d_record_mis_vertex(
    RuntimeDisneyV2_3DResult* io_result,
    int vertex_index) {
    bool was_unrecorded = false;

    if (!io_result || vertex_index < 0 ||
        vertex_index >= RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY) {
        return;
    }
    was_unrecorded =
        vertex_index >= io_result->misVertexCount ||
        io_result->misVertexLightPdf[vertex_index] +
                io_result->misVertexBsdfPdf[vertex_index] <=
            1e-12;
    io_result->misVertexLightPdf[vertex_index] = io_result->lightSamplePdf;
    io_result->misVertexBsdfPdf[vertex_index] = io_result->bsdfSamplePdf;
    io_result->misVertexWeightLight[vertex_index] = io_result->misWeightLight;
    io_result->misVertexWeightBsdf[vertex_index] = io_result->misWeightBsdf;
    io_result->misHeuristicPower = 2.0;
    if (was_unrecorded) {
        io_result->misPowerHeuristicCount += 1;
    }
    if (io_result->misVertexCount < vertex_index + 1) {
        io_result->misVertexCount = vertex_index + 1;
    }
}

static void runtime_disney_v2_transport_3d_record_mis_branch_vertex(
    RuntimeDisneyV2_3DResult* io_result,
    int vertex_index,
    RuntimeDisneyV2_3DMisBranch finite_light_branch,
    RuntimeDisneyV2_3DMisBranch emissive_area_branch) {
    if (!io_result || vertex_index < 0 ||
        vertex_index >= RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY) {
        return;
    }

    io_result->misVertexFiniteLight[vertex_index] = finite_light_branch;
    io_result->misVertexEmissiveArea[vertex_index] = emissive_area_branch;
    if (finite_light_branch.lightPdf + finite_light_branch.bsdfPdf > 1e-12 &&
        io_result->finiteLightMisVertexCount < vertex_index + 1) {
        io_result->finiteLightMisVertexCount = vertex_index + 1;
    }
    if (emissive_area_branch.lightPdf + emissive_area_branch.bsdfPdf > 1e-12 &&
        io_result->emissiveAreaMisVertexCount < vertex_index + 1) {
        io_result->emissiveAreaMisVertexCount = vertex_index + 1;
    }
}

static void runtime_disney_v2_transport_3d_record_bsdf_sample_contribution(
    RuntimeDisneyV2_3DResult* io_result,
    int vertex_index,
    double r,
    double g,
    double b,
    RuntimeDisneyV2_3DEmitterKind emitter_kind) {
    if (!io_result || vertex_index < 0 ||
        vertex_index >= RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY) {
        return;
    }
    io_result->bsdfSampleContributionR[vertex_index] += r;
    io_result->bsdfSampleContributionG[vertex_index] += g;
    io_result->bsdfSampleContributionB[vertex_index] += b;
    io_result->bsdfSampleContributionTotalR += r;
    io_result->bsdfSampleContributionTotalG += g;
    io_result->bsdfSampleContributionTotalB += b;
    if (emitter_kind != RUNTIME_DISNEY_V2_3D_EMITTER_NONE) {
        const RuntimeDisneyV2_3DEmitterKind previous_emitter_kind =
            io_result->misVertexEmitterKind[vertex_index];
        io_result->misVertexEmitterKind[vertex_index] = emitter_kind;
        if (previous_emitter_kind != emitter_kind) {
            if (emitter_kind == RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT) {
                io_result->finiteLightEmitterHitCount += 1;
            } else if (emitter_kind == RUNTIME_DISNEY_V2_3D_EMITTER_EMISSIVE_MATERIAL) {
                io_result->emissiveMaterialHitCount += 1;
            }
        }
    }
    if (runtime_disney_v2_transport_3d_luma(r, g, b) > 1e-9) {
        io_result->bsdfSampleContributionCount += 1;
    }
}

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

static uint32_t runtime_disney_v2_transport_3d_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint32_t runtime_disney_v2_transport_3d_seed_from_hit(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling) {
    uint32_t sx = 0U;
    uint32_t sy = 0U;
    uint32_t sz = 0U;
    uint32_t sequence = sampling ? sampling->sampleSequence : 1U;

    if (!hit) {
        return runtime_disney_v2_transport_3d_hash_u32(sequence ^ 0x6d2b79f5U);
    }
    sx = (uint32_t)(fabs(hit->position.x) * 4096.0);
    sy = (uint32_t)(fabs(hit->position.y) * 4096.0);
    sz = (uint32_t)(fabs(hit->position.z) * 4096.0);
    return runtime_disney_v2_transport_3d_hash_u32(
        sx ^ (sy * 73856093U) ^ (sz * 19349663U) ^
        ((uint32_t)(hit->triangleIndex + 1) * 83492791U) ^
        runtime_disney_v2_transport_3d_hash_u32(sequence ^ 0x9e3779b9U));
}

static double runtime_disney_v2_transport_3d_roulette_sample(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    int depth) {
    uint32_t seed = runtime_disney_v2_transport_3d_seed_from_hit(hit, sampling);
    double u = 0.5;
    double v = 0.5;

    RuntimeNative3DSampling_Stratified2D(sampling,
                                         seed ^ 0xa511e9b3U,
                                         1,
                                         0,
                                         (uint32_t)(2048 + depth),
                                         &u,
                                         &v);
    (void)v;
    return runtime_disney_v2_transport_3d_clamp01(u);
}

static RuntimePathDepthPolicy3DLobe runtime_disney_v2_transport_3d_policy_lobe(
    RuntimeDisneyV2_3DDominantLobe lobe) {
    if (lobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR) {
        return RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_SPECULAR;
    }
    if (lobe == RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION) {
        return RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION;
    }
    return RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_DIFFUSE;
}

static Vec3 runtime_disney_v2_transport_3d_default_tangent(Vec3 normal) {
    Vec3 guide = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3_cross(guide, normal);
    if (vec3_length(tangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
    }
    return vec3_normalize(tangent);
}

static void runtime_disney_v2_transport_3d_build_basis(Vec3 normal,
                                                       Vec3* out_tangent,
                                                       Vec3* out_bitangent) {
    Vec3 tangent = runtime_disney_v2_transport_3d_default_tangent(normal);
    Vec3 bitangent = vec3_normalize(vec3_cross(normal, tangent));

    if (vec3_length(bitangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
        bitangent = vec3(0.0, 0.0, 1.0);
    }
    if (out_tangent) *out_tangent = tangent;
    if (out_bitangent) *out_bitangent = bitangent;
}

static Vec3 runtime_disney_v2_transport_3d_reflect(Vec3 incident_dir, Vec3 normal) {
    const double ndoti = vec3_dot(normal, incident_dir);
    return vec3_normalize(vec3_sub(incident_dir, vec3_scale(normal, 2.0 * ndoti)));
}

static RuntimeDisneyV2_3DDominantLobe runtime_disney_v2_transport_3d_choose_lobe(
    const RuntimePrincipledBSDF3D* principled,
    const RuntimeNative3DSamplingContext* sampling,
    const HitInfo3D* hit,
    int depth,
    double* out_diffuse_probability,
    double* out_specular_probability,
    double* out_transmission_probability) {
    double diffuse = 1.0;
    double specular = 0.0;
    double transmission = 0.0;
    double total = 0.0;
    double u = 0.5;
    double v = 0.5;
    uint32_t seed = runtime_disney_v2_transport_3d_seed_from_hit(hit, sampling);

    if (principled && principled->valid) {
        transmission = runtime_disney_v2_transport_3d_clamp01(principled->transmissionWeight);
        diffuse = runtime_disney_v2_transport_3d_clamp01(
            principled->diffuseWeight * (1.0 - transmission));
        specular = runtime_disney_v2_transport_3d_clamp01(principled->specularWeight);
    }
    total = diffuse + specular + transmission;
    if (!(total > 1e-9)) {
        diffuse = 1.0;
        specular = 0.0;
        transmission = 0.0;
        total = 1.0;
    }
    diffuse /= total;
    specular /= total;
    transmission /= total;

    if (out_diffuse_probability) *out_diffuse_probability = diffuse;
    if (out_specular_probability) *out_specular_probability = specular;
    if (out_transmission_probability) *out_transmission_probability = transmission;

    RuntimeNative3DSampling_Stratified2D(sampling,
                                         seed ^ (0x74dcb303U + (uint32_t)(depth * 97)),
                                         1,
                                         0,
                                         (uint32_t)(4096 + depth),
                                         &u,
                                         &v);
    (void)v;
    u = runtime_disney_v2_transport_3d_clamp01(u);
    if (u <= diffuse) return RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE;
    if (u <= diffuse + specular) return RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR;
    return RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION;
}

static Vec3 runtime_disney_v2_transport_3d_sample_diffuse_direction(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    int depth,
    double* out_pdf,
    double* out_cos_theta) {
    Vec3 tangent = vec3(1.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 1.0);
    Vec3 normal = hit ? hit->normal : vec3(0.0, 1.0, 0.0);
    double u = 0.5;
    double v = 0.5;
    double phi = 0.0;
    double radius = 0.0;
    double local_x = 0.0;
    double local_y = 0.0;
    double local_z = 1.0;
    Vec3 direction = normal;
    uint32_t seed = runtime_disney_v2_transport_3d_seed_from_hit(hit, sampling);

    runtime_disney_v2_transport_3d_build_basis(normal, &tangent, &bitangent);
    RuntimeNative3DSampling_Stratified2D(sampling,
                                         seed ^ (0x2c1b3c6dU + (uint32_t)(depth * 131)),
                                         1,
                                         0,
                                         (uint32_t)(8192 + depth),
                                         &u,
                                         &v);
    phi = 2.0 * M_PI * u;
    radius = sqrt(runtime_disney_v2_transport_3d_clamp01(v));
    local_x = radius * cos(phi);
    local_y = radius * sin(phi);
    local_z = sqrt(fmax(0.0, 1.0 - v));
    direction = vec3_normalize(vec3_add(vec3_add(vec3_scale(tangent, local_x),
                                                vec3_scale(bitangent, local_y)),
                                       vec3_scale(normal, local_z)));
    if (out_cos_theta) {
        *out_cos_theta = runtime_disney_v2_transport_3d_clamp01(vec3_dot(normal, direction));
    }
    if (out_pdf) {
        *out_pdf = fmax((out_cos_theta ? *out_cos_theta : local_z) / M_PI, 1e-9);
    }
    return direction;
}

static Vec3 runtime_disney_v2_transport_3d_sample_specular_direction(
    const HitInfo3D* hit,
    Vec3 incoming_dir,
    const RuntimeNative3DSamplingContext* sampling,
    const RuntimePrincipledBSDF3D* principled,
    int depth,
    double* out_pdf,
    double* out_cos_theta) {
    Vec3 tangent = vec3(1.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 1.0);
    Vec3 normal = hit ? hit->normal : vec3(0.0, 1.0, 0.0);
    Vec3 reflection = runtime_disney_v2_transport_3d_reflect(vec3_normalize(incoming_dir),
                                                             normal);
    Vec3 jitter = reflection;
    double u = 0.5;
    double v = 0.5;
    double phi = 0.0;
    double radius = 0.0;
    double roughness = principled ? principled->roughness : 0.5;
    double blend = 0.0;
    uint32_t seed = runtime_disney_v2_transport_3d_seed_from_hit(hit, sampling);

    runtime_disney_v2_transport_3d_build_basis(normal, &tangent, &bitangent);
    RuntimeNative3DSampling_Stratified2D(sampling,
                                         seed ^ (0xa136aaadU + (uint32_t)(depth * 193)),
                                         1,
                                         0,
                                         (uint32_t)(12288 + depth),
                                         &u,
                                         &v);
    phi = 2.0 * M_PI * u;
    radius = sqrt(runtime_disney_v2_transport_3d_clamp01(v));
    jitter = vec3_normalize(vec3_add(vec3_add(vec3_scale(tangent, radius * cos(phi)),
                                             vec3_scale(bitangent, radius * sin(phi))),
                                    vec3_scale(reflection, 1.0)));
    blend = runtime_disney_v2_transport_3d_clamp(roughness * 0.35, 0.0, 0.35);
    reflection = vec3_normalize(vec3_add(vec3_scale(reflection, 1.0 - blend),
                                         vec3_scale(jitter, blend)));
    if (vec3_dot(reflection, normal) <= 1e-6) {
        reflection = normal;
    }
    if (out_cos_theta) {
        *out_cos_theta = runtime_disney_v2_transport_3d_clamp01(vec3_dot(normal, reflection));
    }
    if (out_pdf) {
        *out_pdf = fmax(RuntimePrincipledBSDF3D_GGXHalfVectorPdf(
                            principled,
                            sqrt(runtime_disney_v2_transport_3d_clamp01(
                                out_cos_theta ? *out_cos_theta : vec3_dot(normal, reflection))),
                            runtime_disney_v2_transport_3d_clamp01(
                                fabs(vec3_dot(reflection, vec3_scale(vec3_normalize(incoming_dir),
                                                                     -1.0))))),
                        1e-6);
    }
    return reflection;
}

static bool runtime_disney_v2_transport_3d_sample_vertex(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 incoming_dir,
    int depth,
    bool has_emissive_area_direct,
    const RuntimeEmissiveDirect3DResult* emissive_area_direct,
    RuntimeDisneyV2Transport3DVertexSample* out_sample) {
    RuntimeDisneyV2Transport3DVertexSample sample = {0};
    RuntimeDisneyV2_3DTransmissionSample transmission_sample = {0};
    Vec3 normal = hit ? hit->normal : vec3(0.0, 1.0, 0.0);
    Vec3 light_dir = normal;
    double light_distance = 0.0;
    double diffuse_probability = 1.0;
    double specular_probability = 0.0;
    double transmission_probability = 0.0;
    double finite_direct_bsdf_pdf = 0.0;
    double area_direct_bsdf_pdf = 0.0;

    if (!hit || !out_sample) return false;
    memset(out_sample, 0, sizeof(*out_sample));

    sample.lobe = runtime_disney_v2_transport_3d_choose_lobe(principled,
                                                             sampling,
                                                             hit,
                                                             depth,
                                                             &diffuse_probability,
                                                             &specular_probability,
                                                             &transmission_probability);
    sample.policyLobe = runtime_disney_v2_transport_3d_policy_lobe(sample.lobe);
    if (scene && scene->hasLight) {
        light_dir = vec3_sub(scene->light.position, hit->position);
        light_distance = vec3_length(light_dir);
        if (light_distance > 1e-9) {
            light_dir = vec3_scale(light_dir, 1.0 / light_distance);
        } else {
            light_dir = normal;
        }
    }

    if (sample.lobe == RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION) {
        Vec3 view_dir = vec3_scale(vec3_normalize(incoming_dir), -1.0);
        if (RuntimeDisneyV2_3D_SampleTransmission(payload,
                                                  principled,
                                                  hit,
                                                  view_dir,
                                                  transmission_probability,
                                                  &transmission_sample)) {
            sample.direction = transmission_sample.direction;
            sample.pdf = transmission_sample.pdf;
            sample.cosTheta = 1.0;
            sample.throughputR = transmission_sample.throughputR;
            sample.throughputG = transmission_sample.throughputG;
            sample.throughputB = transmission_sample.throughputB;
        } else {
            sample.direction = light_dir;
            sample.cosTheta = runtime_disney_v2_transport_3d_clamp01(
                fabs(vec3_dot(normal, sample.direction)));
            sample.pdf = fmax(transmission_probability, 1e-6);
            sample.throughputR = (principled ? principled->baseColorR : 1.0) *
                                 fmax(transmission_probability, 1e-6);
            sample.throughputG = (principled ? principled->baseColorG : 1.0) *
                                 fmax(transmission_probability, 1e-6);
            sample.throughputB = (principled ? principled->baseColorB : 1.0) *
                                 fmax(transmission_probability, 1e-6);
        }
    } else if (sample.lobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR) {
        sample.direction = runtime_disney_v2_transport_3d_sample_specular_direction(hit,
                                                                                    incoming_dir,
                                                                                    sampling,
                                                                                    principled,
                                                                                    depth,
                                                                                    &sample.pdf,
                                                                                    &sample.cosTheta);
        if (scene && scene->hasLight && fabs(vec3_dot(light_dir, normal)) > 1e-6) {
            sample.direction = light_dir;
            sample.cosTheta =
                runtime_disney_v2_transport_3d_clamp01(fabs(vec3_dot(normal, sample.direction)));
        }
        sample.pdf = fmax(sample.pdf, 1e-6);
        sample.throughputR = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->specularF0R : 0.04) *
                fmax(specular_probability, 1e-6) *
                fmax(sample.cosTheta, 0.15) / sample.pdf,
            0.0,
            2.0);
        sample.throughputG = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->specularF0G : 0.04) *
                fmax(specular_probability, 1e-6) *
                fmax(sample.cosTheta, 0.15) / sample.pdf,
            0.0,
            2.0);
        sample.throughputB = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->specularF0B : 0.04) *
                fmax(specular_probability, 1e-6) *
                fmax(sample.cosTheta, 0.15) / sample.pdf,
            0.0,
            2.0);
    } else {
        sample.lobe = RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE;
        sample.policyLobe = RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_DIFFUSE;
        sample.direction = runtime_disney_v2_transport_3d_sample_diffuse_direction(hit,
                                                                                   sampling,
                                                                                   depth,
                                                                                   &sample.pdf,
                                                                                   &sample.cosTheta);
        if (scene && scene->hasLight && fabs(vec3_dot(light_dir, normal)) > 1e-6) {
            sample.direction = light_dir;
            sample.cosTheta =
                runtime_disney_v2_transport_3d_clamp01(fabs(vec3_dot(normal, sample.direction)));
            sample.pdf = fmax(sample.cosTheta / M_PI, 1e-9);
        }
        sample.throughputR = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->baseColorR : 1.0) *
                fmax(diffuse_probability, 1e-6) * sample.cosTheta /
                fmax(sample.pdf * M_PI, 1e-6),
            0.0,
            2.0);
        sample.throughputG = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->baseColorG : 1.0) *
                fmax(diffuse_probability, 1e-6) * sample.cosTheta /
                fmax(sample.pdf * M_PI, 1e-6),
            0.0,
            2.0);
        sample.throughputB = runtime_disney_v2_transport_3d_clamp(
            (principled ? principled->baseColorB : 1.0) *
                fmax(diffuse_probability, 1e-6) * sample.cosTheta /
                fmax(sample.pdf * M_PI, 1e-6),
            0.0,
            2.0);
    }

    if (scene && scene->hasLight) {
        finite_direct_bsdf_pdf =
            RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(principled,
                                                     hit,
                                                     incoming_dir,
                                                     light_dir,
                                                     transmission_probability > 1e-9);
    }
    if (has_emissive_area_direct &&
        emissive_area_direct &&
        emissive_area_direct->lightPdf > 0.0 &&
        vec3_length(emissive_area_direct->sampleDirection) > 1e-9) {
        area_direct_bsdf_pdf =
            RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(principled,
                                                     hit,
                                                     incoming_dir,
                                                     emissive_area_direct->sampleDirection,
                                                     transmission_probability > 1e-9);
    }
    sample.finiteLightMis =
        runtime_disney_v2_transport_3d_make_mis_branch(
            RuntimeDisneyV2_3D_EstimateFiniteLightPdfForHit(scene, hit),
            finite_direct_bsdf_pdf);
    sample.emissiveAreaMis =
        runtime_disney_v2_transport_3d_make_mis_branch(
            has_emissive_area_direct && emissive_area_direct
                ? emissive_area_direct->lightPdf
                : 0.0,
            area_direct_bsdf_pdf);
    sample.lightPdf =
        RuntimeDisneyV2_3D_EstimateDirectLightPdfForHitWithAreaPdf(
            scene,
            hit,
            has_emissive_area_direct,
            emissive_area_direct ? emissive_area_direct->lightPdf : 0.0);
    sample.directBsdfPdf = fmax(finite_direct_bsdf_pdf, area_direct_bsdf_pdf);
    runtime_disney_v2_transport_3d_balance_mis(sample.lightPdf,
                                               sample.directBsdfPdf,
                                               &sample.misWeightLight,
                                               &sample.misWeightBsdf);
    *out_sample = sample;
    return sample.pdf > 1e-9 &&
           runtime_disney_v2_transport_3d_peak(sample.throughputR,
                                               sample.throughputG,
                                               sample.throughputB) > 1e-12 &&
           vec3_length(sample.direction) > 0.0;
}

static void runtime_disney_v2_transport_3d_note_termination(
    RuntimeDisneyV2_3DResult* io_result,
    RuntimeDisneyV2_3DLoopTerminationReason reason) {
    if (!io_result) return;
    io_result->recursiveLoopTerminationReason = reason;
    if (io_result->recursiveLoopVertexCount > 0 &&
        (reason == RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_MAX_DEPTH ||
         reason == RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NEGLIGIBLE_THROUGHPUT)) {
        io_result->recursiveLoopTerminationReasons[
            io_result->recursiveLoopVertexCount - 1] = reason;
    }
    if (reason == RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_MAX_DEPTH) {
        io_result->pathDepthLimitReached = true;
        io_result->recursiveLoopPolicyTerminationCount += 1;
    } else if (reason == RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_ROULETTE) {
        io_result->rouletteTerminated = true;
        io_result->recursiveLoopRouletteTerminationCount += 1;
    } else if (reason == RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NO_HIT) {
        io_result->recursiveLoopNoHitTerminationCount += 1;
    } else if (reason == RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NEGLIGIBLE_THROUGHPUT) {
        io_result->recursiveLoopNegligibleTerminationCount += 1;
    }
}

static RuntimeDisneyV2_3DPathState* runtime_disney_v2_transport_3d_append_state(
    RuntimeDisneyV2_3DResult* io_result,
    const RuntimeDisneyV2_3DPathState* state,
    const RuntimePrincipledBSDF3D* principled,
    RuntimeDisneyV2_3DLoopTerminationReason termination_reason,
    double contribution_r,
    double contribution_g,
    double contribution_b) {
    RuntimeDisneyV2_3DPathState* slot = NULL;
    int index = 0;
    if (!io_result || !state ||
        io_result->recursiveLoopVertexCount >=
            RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY) {
        return NULL;
    }
    index = io_result->recursiveLoopVertexCount;
    slot = &io_result->recursiveLoopStates[index];
    *slot = *state;
    io_result->recursiveLoopVertexCount += 1;
    if (principled) {
        io_result->recursiveLoopPrincipled[index] = *principled;
    }
    io_result->recursiveLoopTerminationReasons[index] = termination_reason;
    io_result->recursiveLoopContributionR[index] = contribution_r;
    io_result->recursiveLoopContributionG[index] = contribution_g;
    io_result->recursiveLoopContributionB[index] = contribution_b;
    if (state->depth == 2 && !io_result->recursivePathState.valid) {
        io_result->recursivePathState = *state;
    }
    return slot;
}

bool RuntimeDisneyV2_3D_ApplyRecursivePathLoopFromDirection(
    const RuntimeScene3D* scene,
    const HitInfo3D* start_hit,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 incoming_dir,
    int start_depth,
    double parent_throughput_r,
    double parent_throughput_g,
    double parent_throughput_b,
    RuntimeDisneyV2_3DResult* io_result) {
    HitInfo3D current_hit = {0};
    double throughput_r = parent_throughput_r;
    double throughput_g = parent_throughput_g;
    double throughput_b = parent_throughput_b;
    int depth = start_depth;
    bool contributed = false;

    if (!scene || !start_hit || !io_result) {
        return false;
    }

    current_hit = *start_hit;
    if (depth < 1) {
        depth = 1;
    }
    if (!(vec3_length(incoming_dir) > 1e-9)) {
        incoming_dir = vec3(0.0, 1.0, 0.0);
    }
    if (io_result->pathState.valid &&
        io_result->pathState.sampledLobe != RUNTIME_DISNEY_V2_3D_LOBE_NONE &&
        !RuntimePathDepthPolicy3D_AllowsDepth(
            &io_result->pathPolicy,
            runtime_disney_v2_transport_3d_policy_lobe(io_result->pathState.sampledLobe),
            depth)) {
        runtime_disney_v2_transport_3d_note_termination(
            io_result,
            RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_MAX_DEPTH);
        return false;
    }

    while (depth < start_depth + RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY) {
        RuntimeDisneyV2_3DPathState state = {0};
        RuntimeLightEmitterTrace3DResult trace = {0};
        RuntimeMaterialPayload3D payload = {0};
        RuntimePrincipledBSDF3D principled = {0};
        RuntimeDisneyV2Transport3DVertexSample vertex_sample = {0};
        RuntimeEmissiveDirect3DResult emissive_area_direct = {0};
        double area_throughput_r = throughput_r;
        double area_throughput_g = throughput_g;
        double area_throughput_b = throughput_b;
        double survival = 1.0;
        double throughput_luma = 0.0;
        int vertex_index = depth - 1;
        bool has_emissive_area_direct = false;
        bool payload_resolved = false;

        payload_resolved = RuntimeMaterialPayload3D_ResolveFromHit(&current_hit, &payload) &&
                           payload.valid;
        principled = payload_resolved ? RuntimePrincipledBSDF3D_FromMaterialPayload(&payload)
                                      : RuntimePrincipledBSDF3D_Default();
        if (RuntimeDisneyV2_3D_ShouldEvaluateEmissiveAreaLightSample(scene,
                                                                     true,
                                                                     io_result)) {
            has_emissive_area_direct =
                RuntimeDisneyV2_3D_EvaluateEmissiveAreaLightSample(scene,
                                                                   &current_hit,
                                                                   sampling,
                                                                   &emissive_area_direct);
        }
        if (!runtime_disney_v2_transport_3d_sample_vertex(scene,
                                                          &current_hit,
                                                          payload_resolved ? &payload : NULL,
                                                          &principled,
                                                          sampling,
                                                          incoming_dir,
                                                          depth,
                                                          has_emissive_area_direct,
                                                          &emissive_area_direct,
                                                          &vertex_sample)) {
            runtime_disney_v2_transport_3d_note_termination(
                io_result,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NEGLIGIBLE_THROUGHPUT);
            break;
        }

        if (!RuntimePathDepthPolicy3D_AllowsDepth(&io_result->pathPolicy,
                                                  vertex_sample.policyLobe,
                                                  depth)) {
            runtime_disney_v2_transport_3d_note_termination(
                io_result,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_MAX_DEPTH);
            break;
        }

        area_throughput_r = throughput_r;
        area_throughput_g = throughput_g;
        area_throughput_b = throughput_b;
        throughput_r *= vertex_sample.throughputR;
        throughput_g *= vertex_sample.throughputG;
        throughput_b *= vertex_sample.throughputB;
        throughput_luma = runtime_disney_v2_transport_3d_luma(throughput_r,
                                                              throughput_g,
                                                              throughput_b);

        io_result->rouletteThroughputLuma = throughput_luma;
        io_result->rouletteSample =
            runtime_disney_v2_transport_3d_roulette_sample(&current_hit, sampling, depth);
        io_result->rouletteSurvivalProbability =
            RuntimePathDepthPolicy3D_SurvivalProbability(&io_result->pathPolicy,
                                                         depth,
                                                         throughput_luma);
        io_result->rouletteEvaluated =
            io_result->pathPolicy.rouletteThreshold > 0.0 &&
            depth >= io_result->pathPolicy.minDepthBeforeRoulette &&
            io_result->rouletteSurvivalProbability < 1.0;
        if (RuntimePathDepthPolicy3D_ShouldTerminate(&io_result->pathPolicy,
                                                     depth,
                                                     throughput_luma,
                                                     io_result->rouletteSample,
                                                     &survival)) {
            io_result->rouletteSurvivalProbability = survival;
            runtime_disney_v2_transport_3d_note_termination(
                io_result,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_ROULETTE);
            break;
        }
        if (!(throughput_luma > kRuntimeDisneyV2Transport3DNegligibleLuma)) {
            runtime_disney_v2_transport_3d_note_termination(
                io_result,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NEGLIGIBLE_THROUGHPUT);
            break;
        }

        state.valid = true;
        state.depth = depth;
        state.sampledLobe = vertex_sample.lobe;
        state.throughputR = throughput_r / survival;
        state.throughputG = throughput_g / survival;
        state.throughputB = throughput_b / survival;
        state.pdf = vertex_sample.pdf;
        state.ray = RuntimeRay3D_MakeOffset(current_hit.position,
                                            current_hit.normal,
                                            vertex_sample.direction,
                                            kRuntimeDisneyV2Transport3DEpsilon);
        throughput_r = state.throughputR;
        throughput_g = state.throughputG;
        throughput_b = state.throughputB;
        io_result->bsdfSamplePdf = vertex_sample.directBsdfPdf;
        io_result->lightSamplePdf = vertex_sample.lightPdf;
        io_result->misWeightLight = vertex_sample.misWeightLight;
        io_result->misWeightBsdf = vertex_sample.misWeightBsdf;
        io_result->finiteLightMis = vertex_sample.finiteLightMis;
        io_result->emissiveAreaMis = vertex_sample.emissiveAreaMis;
        io_result->sampledLobeMaxDepth =
            RuntimePathDepthPolicy3D_MaxDepthForLobe(&io_result->pathPolicy,
                                                     vertex_sample.policyLobe);
        runtime_disney_v2_transport_3d_record_mis_branch_vertex(
            io_result,
            vertex_index,
            vertex_sample.finiteLightMis,
            vertex_sample.emissiveAreaMis);

        if (has_emissive_area_direct &&
            RuntimeDisneyV2_3D_AccumulateEmissiveAreaLightSample(
                &emissive_area_direct,
                area_throughput_r,
                area_throughput_g,
                area_throughput_b,
                vertex_sample.emissiveAreaMis.weightLight,
                vertex_index,
                true,
                io_result)) {
            runtime_disney_v2_transport_3d_record_mis_vertex(io_result, vertex_index);
            contributed = true;
        }

        io_result->secondaryRayCount += 1;
        io_result->recursiveLoopRayCount += 1;
        RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
            RUNTIME_RENDER_TRACE_COST_RAY_DISNEY_RECURSIVE,
            depth);
        if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                                   &state.ray,
                                                   kRuntimeDisneyV2Transport3DEpsilon,
                                                   kRuntimeDisneyV2Transport3DMaxDistance,
                                                   &trace)) {
            (void)runtime_disney_v2_transport_3d_append_state(
                io_result,
                &state,
                &principled,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NO_HIT,
                0.0,
                0.0,
                0.0);
            runtime_disney_v2_transport_3d_note_termination(
                io_result,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NO_HIT);
            break;
        }

        if (trace.geometryHit) {
            RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&trace.geometryHitInfo);
            state.hit = true;
            state.hitInfo = trace.geometryHitInfo;
            io_result->secondaryHitCount += 1;
            io_result->recursiveLoopGeometryHitCount += 1;
        }

        if (trace.emitterWins) {
            const double emitter_radiance = trace.emitterHitInfo.radiance;
            const double contribution_r =
                state.throughputR * emitter_radiance * io_result->misWeightBsdf;
            const double contribution_g =
                state.throughputG * emitter_radiance * io_result->misWeightBsdf;
            const double contribution_b =
                state.throughputB * emitter_radiance * io_result->misWeightBsdf;
            state.emitterHit = trace.emitterHit;
            state.emitterWins = true;
            state.emitterHitInfo = trace.emitterHitInfo;
            io_result->recursiveLoopEmitterHitCount += 1;
            io_result->pathDepth = depth;
            runtime_disney_v2_transport_3d_record_mis_vertex(io_result, vertex_index);
            io_result->recursiveBsdfRadianceR += contribution_r;
            io_result->recursiveBsdfRadianceG += contribution_g;
            io_result->recursiveBsdfRadianceB += contribution_b;
            runtime_disney_v2_transport_3d_record_bsdf_sample_contribution(
                io_result,
                vertex_index,
                contribution_r,
                contribution_g,
                contribution_b,
                RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT);
            io_result->recursiveLoopContributingHitCount += 1;
            io_result->secondaryContributingHitCount += 1;
            (void)runtime_disney_v2_transport_3d_append_state(
                io_result,
                &state,
                &principled,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER,
                contribution_r,
                contribution_g,
                contribution_b);
            io_result->recursiveLoopTerminationReason =
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER;
            contributed = true;
            break;
        }

        if (trace.geometryHit) {
            RuntimeMaterialPayload3D emitter_payload = {0};
            RuntimePrincipledBSDF3D emitter_principled = {0};
            double contribution_r = 0.0;
            double contribution_g = 0.0;
            double contribution_b = 0.0;

            if (RuntimeMaterialPayload3D_ResolveFromHit(&trace.geometryHitInfo,
                                                        &emitter_payload) &&
                emitter_payload.valid) {
                emitter_principled =
                    RuntimePrincipledBSDF3D_FromMaterialPayload(&emitter_payload);
                if (RuntimeDisneyV2_3D_AccumulateEmissiveMaterialHit(
                        &trace.geometryHitInfo,
                        &emitter_payload,
                        &emitter_principled,
                        &state.ray,
                        state.throughputR,
                        state.throughputG,
                        state.throughputB,
                        io_result->misWeightBsdf,
                        vertex_index,
                        true,
                        &state,
                        &contribution_r,
                        &contribution_g,
                        &contribution_b,
                        io_result)) {
                    io_result->recursiveLoopEmitterHitCount += 1;
                    io_result->recursiveLoopContributingHitCount += 1;
                    io_result->secondaryContributingHitCount += 1;
                    io_result->pathDepth = depth;
                    runtime_disney_v2_transport_3d_record_mis_vertex(io_result, vertex_index);
                    (void)runtime_disney_v2_transport_3d_append_state(
                        io_result,
                        &state,
                        &principled,
                        RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER,
                        contribution_r,
                        contribution_g,
                        contribution_b);
                    io_result->recursiveLoopTerminationReason =
                        RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER;
                    contributed = true;
                    break;
                }
            }
        }

        (void)runtime_disney_v2_transport_3d_append_state(
            io_result,
            &state,
            &principled,
            RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NONE,
            0.0,
            0.0,
            0.0);
        if (!trace.geometryHit) {
            runtime_disney_v2_transport_3d_note_termination(
                io_result,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NO_HIT);
            break;
        }

        incoming_dir = state.ray.direction;
        current_hit = trace.geometryHitInfo;
        io_result->pathDepth = depth;
        depth += 1;
    }

    return contributed;
}

bool RuntimeDisneyV2_3D_ApplyRecursivePathLoop(
    const RuntimeScene3D* scene,
    const HitInfo3D* start_hit,
    const RuntimeNative3DSamplingContext* sampling,
    double parent_throughput_r,
    double parent_throughput_g,
    double parent_throughput_b,
    RuntimeDisneyV2_3DResult* io_result) {
    Vec3 incoming_dir = vec3(0.0, 1.0, 0.0);

    if (io_result && io_result->pathState.valid &&
        vec3_length(io_result->pathState.ray.direction) > 0.0) {
        incoming_dir = io_result->pathState.ray.direction;
    }
    return RuntimeDisneyV2_3D_ApplyRecursivePathLoopFromDirection(scene,
                                                                  start_hit,
                                                                  sampling,
                                                                  incoming_dir,
                                                                  2,
                                                                  parent_throughput_r,
                                                                  parent_throughput_g,
                                                                  parent_throughput_b,
                                                                  io_result);
}

static Ray3D runtime_disney_v2_transport_3d_make_rough_reflection_ray(
    const HitInfo3D* source_hit,
    const Ray3D* base_ray,
    const RuntimeNative3DSamplingContext* sampling,
    double roughness,
    int sample_count,
    int sample_index) {
    Vec3 axis = base_ray ? vec3_normalize(base_ray->direction) : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3(1.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 1.0);
    Vec3 direction = axis;
    double u = 0.5;
    double v = 0.5;
    double phi = 0.0;
    double radius = 0.0;
    double cone = runtime_disney_v2_transport_3d_clamp(roughness * 0.45, 0.0, 0.45);
    uint32_t seed = runtime_disney_v2_transport_3d_seed_from_hit(source_hit, sampling);

    if (!(vec3_length(axis) > 1e-9)) {
        axis = source_hit ? source_hit->normal : vec3(0.0, 1.0, 0.0);
    }
    runtime_disney_v2_transport_3d_build_basis(axis, &tangent, &bitangent);
    RuntimeNative3DSampling_Stratified2D(sampling,
                                         seed ^ 0x47b6137dU,
                                         sample_count,
                                         sample_index,
                                         24576U,
                                         &u,
                                         &v);
    phi = 2.0 * M_PI * u;
    radius = sqrt(runtime_disney_v2_transport_3d_clamp01(v)) * cone;
    direction = vec3_normalize(vec3_add(vec3_add(vec3_scale(tangent,
                                                            radius * cos(phi)),
                                                 vec3_scale(bitangent,
                                                            radius * sin(phi))),
                                        vec3_scale(axis, 1.0)));
    if (source_hit && vec3_dot(direction, source_hit->normal) <= 1e-6) {
        direction = axis;
    }
    return RuntimeRay3D_MakeOffset(source_hit ? source_hit->position : vec3(0.0, 0.0, 0.0),
                                   source_hit ? source_hit->normal : vec3(0.0, 1.0, 0.0),
                                   direction,
                                   kRuntimeDisneyV2Transport3DEpsilon);
}

static int runtime_disney_v2_transport_3d_resolve_rough_reflection_sample_count(
    const RuntimeScene3D* scene,
    double roughness) {
    int sample_count = RuntimeDisneyV2_3D_RoughReflectionEstimatorSampleCount(roughness);
    int triangle_count = 0;

    if (!scene || sample_count <= 1) {
        return sample_count;
    }
    triangle_count = scene->triangleMesh.triangleCount;
    if (triangle_count > 100000) {
        return 1;
    }
    if (triangle_count > 512 && sample_count > 2) {
        return 2;
    }
    return sample_count;
}

static void runtime_disney_v2_transport_3d_merge_reflection_loop(
    RuntimeDisneyV2_3DResult* io_result,
    const RuntimeDisneyV2_3DResult* loop_result,
    bool rough_sample_used) {
    int copy_count = 0;
    int dst = 0;
    int i = 0;

    if (!io_result || !loop_result) return;

    dst = io_result->specularReflectionRecursiveVertexCount;
    if (dst < RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY) {
        copy_count = loop_result->recursiveLoopVertexCount;
        if (copy_count > RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY - dst) {
            copy_count = RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY - dst;
        }
        for (i = 0; i < copy_count; ++i) {
            const int target = dst + i;
            io_result->specularReflectionRecursiveStates[target] =
                loop_result->recursiveLoopStates[i];
            io_result->specularReflectionRecursivePrincipled[target] =
                loop_result->recursiveLoopPrincipled[i];
            io_result->specularReflectionRecursiveTerminationReasons[target] =
                loop_result->recursiveLoopTerminationReasons[i];
            io_result->specularReflectionRecursiveContributionR[target] =
                loop_result->recursiveLoopContributionR[i];
            io_result->specularReflectionRecursiveContributionG[target] =
                loop_result->recursiveLoopContributionG[i];
            io_result->specularReflectionRecursiveContributionB[target] =
                loop_result->recursiveLoopContributionB[i];
        }
        io_result->specularReflectionRecursiveVertexCount += copy_count;
    }

    io_result->specularReflectionRecursiveRayCount += loop_result->recursiveLoopRayCount;
    io_result->specularReflectionRecursiveGeometryHitCount +=
        loop_result->recursiveLoopGeometryHitCount;
    io_result->specularReflectionRecursiveEmitterHitCount +=
        loop_result->recursiveLoopEmitterHitCount;
    io_result->specularReflectionRecursiveContributingHitCount +=
        loop_result->recursiveLoopContributingHitCount;
    io_result->specularReflectionRecursivePolicyTerminationCount +=
        loop_result->recursiveLoopPolicyTerminationCount;
    io_result->specularReflectionRecursiveRouletteTerminationCount +=
        loop_result->recursiveLoopRouletteTerminationCount;
    io_result->specularReflectionRecursiveNoHitTerminationCount +=
        loop_result->recursiveLoopNoHitTerminationCount;

    io_result->specularReflectionRecursiveRadianceR += loop_result->recursiveBsdfRadianceR;
    io_result->specularReflectionRecursiveRadianceG += loop_result->recursiveBsdfRadianceG;
    io_result->specularReflectionRecursiveRadianceB += loop_result->recursiveBsdfRadianceB;
    io_result->specularReflectionRadianceR += loop_result->recursiveBsdfRadianceR;
    io_result->specularReflectionRadianceG += loop_result->recursiveBsdfRadianceG;
    io_result->specularReflectionRadianceB += loop_result->recursiveBsdfRadianceB;
    io_result->recursiveBsdfRadianceR += loop_result->recursiveBsdfRadianceR;
    io_result->recursiveBsdfRadianceG += loop_result->recursiveBsdfRadianceG;
    io_result->recursiveBsdfRadianceB += loop_result->recursiveBsdfRadianceB;

    io_result->emissiveAreaRadianceR += loop_result->emissiveAreaRadianceR;
    io_result->emissiveAreaRadianceG += loop_result->emissiveAreaRadianceG;
    io_result->emissiveAreaRadianceB += loop_result->emissiveAreaRadianceB;
    io_result->emissiveAreaSampledTriangleCount +=
        loop_result->emissiveAreaSampledTriangleCount;
    io_result->emissiveAreaContributingTriangleCount +=
        loop_result->emissiveAreaContributingTriangleCount;
    io_result->emissiveAreaLightSampleCount += loop_result->emissiveAreaLightSampleCount;
    if (loop_result->emissiveAreaCandidateCount > io_result->emissiveAreaCandidateCount) {
        io_result->emissiveAreaCandidateCount = loop_result->emissiveAreaCandidateCount;
    }
    io_result->emissiveAreaSelectedCandidateCount +=
        loop_result->emissiveAreaSelectedCandidateCount;
    io_result->emissiveAreaVisibilityRayCount += loop_result->emissiveAreaVisibilityRayCount;
    io_result->emissiveAreaPrimarySampleCount += loop_result->emissiveAreaPrimarySampleCount;
    io_result->emissiveAreaRecursiveSampleCount +=
        loop_result->emissiveAreaRecursiveSampleCount;
    io_result->emissiveAreaRecursivePolicySkipCount +=
        loop_result->emissiveAreaRecursivePolicySkipCount;
    io_result->emissiveAreaRecursiveCandidateCapSkipCount +=
        loop_result->emissiveAreaRecursiveCandidateCapSkipCount;
    io_result->emissiveAreaRecursiveTriangleCapSkipCount +=
        loop_result->emissiveAreaRecursiveTriangleCapSkipCount;
    if (loop_result->emissiveAreaRecursiveCandidateCap >
        io_result->emissiveAreaRecursiveCandidateCap) {
        io_result->emissiveAreaRecursiveCandidateCap =
            loop_result->emissiveAreaRecursiveCandidateCap;
    }
    if (loop_result->emissiveAreaRecursiveTriangleCap >
        io_result->emissiveAreaRecursiveTriangleCap) {
        io_result->emissiveAreaRecursiveTriangleCap =
            loop_result->emissiveAreaRecursiveTriangleCap;
    }
    io_result->emissiveAreaFullScanFallbackCount +=
        loop_result->emissiveAreaFullScanFallbackCount;

    io_result->lightSampleContributionTotalR +=
        loop_result->lightSampleContributionTotalR;
    io_result->lightSampleContributionTotalG +=
        loop_result->lightSampleContributionTotalG;
    io_result->lightSampleContributionTotalB +=
        loop_result->lightSampleContributionTotalB;
    io_result->lightSampleContributionCount += loop_result->lightSampleContributionCount;
    for (i = 0; i < RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY; ++i) {
        io_result->lightSampleContributionR[i] += loop_result->lightSampleContributionR[i];
        io_result->lightSampleContributionG[i] += loop_result->lightSampleContributionG[i];
        io_result->lightSampleContributionB[i] += loop_result->lightSampleContributionB[i];
    }

    io_result->bsdfSampleContributionTotalR += loop_result->bsdfSampleContributionTotalR;
    io_result->bsdfSampleContributionTotalG += loop_result->bsdfSampleContributionTotalG;
    io_result->bsdfSampleContributionTotalB += loop_result->bsdfSampleContributionTotalB;
    io_result->bsdfSampleContributionCount += loop_result->bsdfSampleContributionCount;
    for (i = 0; i < RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY; ++i) {
        const RuntimeDisneyV2_3DEmitterKind emitter_kind =
            loop_result->misVertexEmitterKind[i];
        if (emitter_kind == RUNTIME_DISNEY_V2_3D_EMITTER_NONE ||
            io_result->misVertexEmitterKind[i] == emitter_kind) {
            continue;
        }
        io_result->misVertexEmitterKind[i] = emitter_kind;
        if (emitter_kind == RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT) {
            io_result->finiteLightEmitterHitCount += 1;
        } else if (emitter_kind == RUNTIME_DISNEY_V2_3D_EMITTER_EMISSIVE_MATERIAL) {
            io_result->emissiveMaterialHitCount += 1;
        }
    }

    if (rough_sample_used) {
        io_result->specularReflectionRoughContributionR += loop_result->recursiveBsdfRadianceR;
        io_result->specularReflectionRoughContributionG += loop_result->recursiveBsdfRadianceG;
        io_result->specularReflectionRoughContributionB += loop_result->recursiveBsdfRadianceB;
    }
}

static bool runtime_disney_v2_transport_3d_apply_reflection_loop(
    const RuntimeScene3D* scene,
    const RuntimeSpecularReflection3DResult* selected,
    const RuntimeNative3DSamplingContext* sampling,
    double sample_weight,
    bool rough_sample_used,
    RuntimeDisneyV2_3DResult* io_result) {
    RuntimeDisneyV2_3DResult loop_result = {0};
    double parent_r = 0.0;
    double parent_g = 0.0;
    double parent_b = 0.0;

    if (!scene || !selected || !io_result || !selected->geometryHit ||
        selected->hitInfo.triangleIndex < 0) {
        return false;
    }

    parent_r = selected->weight * selected->tintR * sample_weight;
    parent_g = selected->weight * selected->tintG * sample_weight;
    parent_b = selected->weight * selected->tintB * sample_weight;
    if (!(runtime_disney_v2_transport_3d_peak(parent_r, parent_g, parent_b) > 1e-9)) {
        return false;
    }

    loop_result.pathPolicy = io_result->pathPolicy;
    loop_result.pathPolicyResolved = io_result->pathPolicyResolved;
    loop_result.pathState.valid = true;
    loop_result.pathState.sampledLobe = RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR;
    loop_result.pathState.ray = selected->ray;
    (void)RuntimeDisneyV2_3D_ApplyRecursivePathLoopFromDirection(scene,
                                                                  &selected->hitInfo,
                                                                  sampling,
                                                                  selected->ray.direction,
                                                                  2,
                                                                  parent_r,
                                                                  parent_g,
                                                                  parent_b,
                                                                  &loop_result);
    runtime_disney_v2_transport_3d_merge_reflection_loop(io_result,
                                                         &loop_result,
                                                         rough_sample_used);
    if (rough_sample_used && loop_result.recursiveLoopContributingHitCount > 0) {
        io_result->specularReflectionRoughContributingSampleCount += 1;
    }
    return loop_result.recursiveLoopContributingHitCount > 0 ||
           loop_result.recursiveLoopPolicyTerminationCount > 0 ||
           loop_result.recursiveLoopRouletteTerminationCount > 0 ||
           loop_result.recursiveLoopNoHitTerminationCount > 0;
}

bool RuntimeDisneyV2_3D_ApplySpecularReflectionRecursion(
    const RuntimeScene3D* scene,
    const HitInfo3D* source_hit,
    const RuntimeSpecularReflection3DResult* reflection,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDisneyV2_3DResult* io_result) {
    RuntimeSpecularReflection3DResult selected = {0};
    RuntimeLightEmitterTrace3DResult rough_trace = {0};
    Ray3D rough_ray = {0};
    double roughness = 0.0;
    int rough_sample_count = 0;
    int rough_hit_count = 0;
    bool applied = false;

    if (!scene || !source_hit || !reflection || !io_result || !reflection->traced ||
        !reflection->geometryHit || reflection->hitInfo.triangleIndex < 0 ||
        reflection->hitInfo.triangleIndex == source_hit->triangleIndex) {
        return false;
    }

    selected = *reflection;
    roughness = runtime_disney_v2_transport_3d_clamp(io_result->payload.bsdf.roughness,
                                                     0.0,
                                                     1.0);
    rough_sample_count =
        runtime_disney_v2_transport_3d_resolve_rough_reflection_sample_count(scene, roughness);
    if (rough_sample_count > 0) {
        int sample_index = 0;
        io_result->specularReflectionRoughSampleCount += rough_sample_count;
        io_result->specularReflectionRoughness = roughness;
        for (sample_index = 0; sample_index < rough_sample_count; ++sample_index) {
            selected = *reflection;
            rough_ray = runtime_disney_v2_transport_3d_make_rough_reflection_ray(
                source_hit,
                &reflection->ray,
                sampling,
                roughness,
                rough_sample_count,
                sample_index);
            RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
                RUNTIME_RENDER_TRACE_COST_RAY_REFLECTION_SPECULAR,
                2);
            if (RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                                      &rough_ray,
                                                      kRuntimeDisneyV2Transport3DEpsilon,
                                                      kRuntimeDisneyV2Transport3DMaxDistance,
                                                      &rough_trace) &&
                rough_trace.geometryHit &&
                rough_trace.geometryHitInfo.triangleIndex >= 0 &&
                rough_trace.geometryHitInfo.triangleIndex != source_hit->triangleIndex) {
                RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(
                    &rough_trace.geometryHitInfo);
                selected.ray = rough_ray;
                selected.geometryHit = true;
                selected.emitterHit = rough_trace.emitterHit;
                selected.emitterWins = rough_trace.emitterWins;
                selected.hitInfo = rough_trace.geometryHitInfo;
                selected.emitterHitInfo = rough_trace.emitterHitInfo;
                io_result->specularReflectionRoughHitCount += 1;
                rough_hit_count += 1;
                applied |= runtime_disney_v2_transport_3d_apply_reflection_loop(
                    scene,
                    &selected,
                    sampling,
                    RuntimeDisneyV2_3D_EstimatorSampleWeight(rough_sample_count),
                    true,
                    io_result);
            } else {
                io_result->specularReflectionRoughNoHitCount += 1;
            }
        }
    }
    if (rough_sample_count == 0 || rough_hit_count == 0) {
        applied |= runtime_disney_v2_transport_3d_apply_reflection_loop(scene,
                                                                        reflection,
                                                                        sampling,
                                                                        1.0,
                                                                        false,
                                                                        io_result);
    }
    return applied;
}
