#include "render/runtime_disney_v2_3d.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_disney_v2_transport_3d.h"
#include "render/runtime_disney_v2_transmission_3d.h"
#include "render/runtime_path_depth_policy_3d.h"
#include "render/runtime_ray_3d.h"

static const double kRuntimeDisneyV2_3DEpsilon = 1e-4;
static const double kRuntimeDisneyV2_3DMaxDistance = 48.0;

static double runtime_disney_v2_3d_clamp(double value,
                                         double min_value,
                                         double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_disney_v2_3d_clamp01(double value) {
    return runtime_disney_v2_3d_clamp(value, 0.0, 1.0);
}

static double runtime_disney_v2_3d_peak(double r, double g, double b) {
    double peak = r;
    if (g > peak) peak = g;
    if (b > peak) peak = b;
    return peak;
}

static uint32_t runtime_disney_v2_3d_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint32_t runtime_disney_v2_3d_seed_from_hit(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling) {
    uint32_t sx = 0U;
    uint32_t sy = 0U;
    uint32_t sz = 0U;
    uint32_t sequence = sampling ? sampling->sampleSequence : 1U;

    if (!hit) return runtime_disney_v2_3d_hash_u32(sequence ^ 0x6d2b79f5U);
    sx = (uint32_t)(fabs(hit->position.x) * 4096.0);
    sy = (uint32_t)(fabs(hit->position.y) * 4096.0);
    sz = (uint32_t)(fabs(hit->position.z) * 4096.0);
    return runtime_disney_v2_3d_hash_u32(
        sx ^ (sy * 73856093U) ^ (sz * 19349663U) ^
        ((uint32_t)(hit->triangleIndex + 1) * 83492791U) ^
        runtime_disney_v2_3d_hash_u32(sequence ^ 0x9e3779b9U));
}

static double runtime_disney_v2_3d_luma(double r, double g, double b) {
    return (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
}

static void runtime_disney_v2_3d_balance_mis(double light_pdf,
                                             double bsdf_pdf,
                                             double* out_light_weight,
                                             double* out_bsdf_weight) {
    const double pdf_sum = light_pdf + bsdf_pdf;
    double light_weight = 0.0;
    double bsdf_weight = 0.0;

    if (pdf_sum > 1e-9) {
        light_weight = light_pdf / pdf_sum;
        bsdf_weight = bsdf_pdf / pdf_sum;
    }
    if (out_light_weight) *out_light_weight = light_weight;
    if (out_bsdf_weight) *out_bsdf_weight = bsdf_weight;
}

static void runtime_disney_v2_3d_record_mis_vertex(RuntimeDisneyV2_3DResult* io_result,
                                                   int vertex_index,
                                                   double light_pdf,
                                                   double bsdf_pdf,
                                                   double light_weight,
                                                   double bsdf_weight) {
    if (!io_result || vertex_index < 0 ||
        vertex_index >= RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY) {
        return;
    }
    io_result->misVertexLightPdf[vertex_index] = light_pdf;
    io_result->misVertexBsdfPdf[vertex_index] = bsdf_pdf;
    io_result->misVertexWeightLight[vertex_index] = light_weight;
    io_result->misVertexWeightBsdf[vertex_index] = bsdf_weight;
    if (io_result->misVertexCount < vertex_index + 1) {
        io_result->misVertexCount = vertex_index + 1;
    }
}

static void runtime_disney_v2_3d_record_light_sample_contribution(
    RuntimeDisneyV2_3DResult* io_result,
    int vertex_index,
    double r,
    double g,
    double b) {
    if (!io_result || vertex_index < 0 ||
        vertex_index >= RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY) {
        return;
    }
    io_result->lightSampleContributionR[vertex_index] += r;
    io_result->lightSampleContributionG[vertex_index] += g;
    io_result->lightSampleContributionB[vertex_index] += b;
    io_result->lightSampleContributionTotalR += r;
    io_result->lightSampleContributionTotalG += g;
    io_result->lightSampleContributionTotalB += b;
    if (runtime_disney_v2_3d_luma(r, g, b) > 1e-9) {
        io_result->lightSampleContributionCount += 1;
    }
}

static void runtime_disney_v2_3d_record_bsdf_sample_contribution(
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
        io_result->misVertexEmitterKind[vertex_index] = emitter_kind;
        if (emitter_kind == RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT) {
            io_result->finiteLightEmitterHitCount += 1;
        } else if (emitter_kind == RUNTIME_DISNEY_V2_3D_EMITTER_EMISSIVE_MATERIAL) {
            io_result->emissiveMaterialHitCount += 1;
        }
    }
    if (runtime_disney_v2_3d_luma(r, g, b) > 1e-9) {
        io_result->bsdfSampleContributionCount += 1;
    }
}

static void runtime_disney_v2_3d_secondary_response(
    const RuntimePrincipledBSDF3D* principled,
    double* out_r,
    double* out_g,
    double* out_b) {
    double lobe_scale = 1.0;
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;

    if (principled && principled->valid) {
        lobe_scale = runtime_disney_v2_3d_clamp(0.65 +
                                                    (principled->diffuseWeight * 0.35) +
                                                    (principled->specularWeight * 0.20) +
                                                    (principled->transmissionWeight * 0.15),
                                                0.25,
                                                1.5);
        r = runtime_disney_v2_3d_clamp((0.15 + (principled->baseColorR * 0.85)) *
                                           lobe_scale,
                                       0.05,
                                       2.0);
        g = runtime_disney_v2_3d_clamp((0.15 + (principled->baseColorG * 0.85)) *
                                           lobe_scale,
                                       0.05,
                                       2.0);
        b = runtime_disney_v2_3d_clamp((0.15 + (principled->baseColorB * 0.85)) *
                                           lobe_scale,
                                       0.05,
                                       2.0);
    }

    if (out_r) *out_r = r;
    if (out_g) *out_g = g;
    if (out_b) *out_b = b;
}

static RuntimePathDepthPolicy3DLobe runtime_disney_v2_3d_policy_lobe(
    RuntimeDisneyV2_3DDominantLobe lobe) {
    if (lobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR) {
        return RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_SPECULAR;
    }
    if (lobe == RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION) {
        return RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION;
    }
    return RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_DIFFUSE;
}

static Vec3 runtime_disney_v2_3d_default_view_dir(const HitInfo3D* hit) {
    if (!hit) return vec3(0.0, 0.0, 1.0);
    return vec3_normalize(hit->normal);
}

static Vec3 runtime_disney_v2_3d_default_tangent(Vec3 normal) {
    Vec3 guide = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3_cross(guide, normal);
    if (vec3_length(tangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
    }
    return vec3_normalize(tangent);
}

static void runtime_disney_v2_3d_build_basis(Vec3 normal,
                                             Vec3* out_tangent,
                                             Vec3* out_bitangent) {
    Vec3 tangent = runtime_disney_v2_3d_default_tangent(normal);
    Vec3 bitangent = vec3_normalize(vec3_cross(normal, tangent));

    if (vec3_length(bitangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
        bitangent = vec3(0.0, 0.0, 1.0);
    }
    if (out_tangent) *out_tangent = tangent;
    if (out_bitangent) *out_bitangent = bitangent;
}

static Vec3 runtime_disney_v2_3d_reflect(Vec3 incident_dir, Vec3 normal) {
    const double ndoti = vec3_dot(normal, incident_dir);
    return vec3_normalize(vec3_sub(incident_dir, vec3_scale(normal, 2.0 * ndoti)));
}

static RuntimeDisneyV2_3DDominantLobe runtime_disney_v2_3d_choose_lobe(
    const RuntimeDisneyV2_3DResult* result,
    const RuntimeNative3DSamplingContext* sampling,
    double* out_sample) {
    double u = 0.5;
    double v = 0.5;
    double total = 0.0;
    uint32_t seed = 0U;

    if (!result || !out_sample) return RUNTIME_DISNEY_V2_3D_LOBE_NONE;
    seed = runtime_disney_v2_3d_seed_from_hit(&result->hitInfo, sampling);
    RuntimeNative3DSampling_Stratified2D(sampling, seed ^ 0xd1b54a35U, 1, 0, 61U, &u, &v);
    (void)v;
    *out_sample = runtime_disney_v2_3d_clamp01(u);
    total = result->diffuseProbability +
            result->specularProbability +
            result->transmissionProbability;
    if (!(total > 1e-9)) return RUNTIME_DISNEY_V2_3D_LOBE_NONE;

    if (*out_sample <= result->diffuseProbability / total) {
        return RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE;
    }
    if (*out_sample <=
        (result->diffuseProbability + result->specularProbability) / total) {
        return RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR;
    }
    return RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION;
}

static Vec3 runtime_disney_v2_3d_sample_diffuse_direction(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    double* out_pdf,
    double* out_cos_theta) {
    Vec3 tangent = vec3(1.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 1.0);
    double u = 0.5;
    double v = 0.5;
    double phi = 0.0;
    double radius = 0.0;
    double local_x = 0.0;
    double local_y = 0.0;
    double local_z = 1.0;
    Vec3 direction = vec3(0.0, 1.0, 0.0);
    uint32_t seed = runtime_disney_v2_3d_seed_from_hit(hit, sampling);

    runtime_disney_v2_3d_build_basis(hit ? hit->normal : vec3(0.0, 1.0, 0.0),
                                     &tangent,
                                     &bitangent);
    RuntimeNative3DSampling_Stratified2D(sampling, seed ^ 0x94d049bbU, 1, 0, 71U, &u, &v);
    phi = 2.0 * M_PI * u;
    radius = sqrt(runtime_disney_v2_3d_clamp01(v));
    local_x = radius * cos(phi);
    local_y = radius * sin(phi);
    local_z = sqrt(fmax(0.0, 1.0 - v));
    direction = vec3_normalize(vec3_add(vec3_add(vec3_scale(tangent, local_x),
                                                vec3_scale(bitangent, local_y)),
                                       vec3_scale(hit ? hit->normal : vec3(0.0, 1.0, 0.0),
                                                  local_z)));
    if (out_cos_theta) {
        *out_cos_theta = runtime_disney_v2_3d_clamp01(
            vec3_dot(hit ? hit->normal : vec3(0.0, 1.0, 0.0), direction));
    }
    if (out_pdf) {
        *out_pdf = fmax((out_cos_theta ? *out_cos_theta : local_z) / M_PI, 1e-9);
    }
    return direction;
}

static Vec3 runtime_disney_v2_3d_sample_specular_direction(
    const HitInfo3D* hit,
    Vec3 view_dir,
    const RuntimeNative3DSamplingContext* sampling,
    const RuntimePrincipledBSDF3D* principled,
    double* out_pdf,
    double* out_cos_theta) {
    Vec3 tangent = vec3(1.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 1.0);
    Vec3 reflection = vec3(0.0, 1.0, 0.0);
    Vec3 jitter = vec3(0.0, 1.0, 0.0);
    Vec3 normal = hit ? hit->normal : vec3(0.0, 1.0, 0.0);
    double u = 0.5;
    double v = 0.5;
    double phi = 0.0;
    double radius = 0.0;
    double roughness = principled ? principled->roughness : 0.5;
    double blend = 0.0;
    uint32_t seed = runtime_disney_v2_3d_seed_from_hit(hit, sampling);

    runtime_disney_v2_3d_build_basis(normal, &tangent, &bitangent);
    reflection = runtime_disney_v2_3d_reflect(vec3_scale(vec3_normalize(view_dir), -1.0),
                                              normal);
    RuntimeNative3DSampling_Stratified2D(sampling, seed ^ 0x369dea0fU, 1, 0, 73U, &u, &v);
    phi = 2.0 * M_PI * u;
    radius = sqrt(runtime_disney_v2_3d_clamp01(v));
    jitter = vec3_normalize(vec3_add(vec3_add(vec3_scale(tangent, radius * cos(phi)),
                                             vec3_scale(bitangent, radius * sin(phi))),
                                    vec3_scale(reflection, 1.0)));
    blend = runtime_disney_v2_3d_clamp(roughness * 0.35, 0.0, 0.35);
    reflection = vec3_normalize(vec3_add(vec3_scale(reflection, 1.0 - blend),
                                         vec3_scale(jitter, blend)));
    if (vec3_dot(reflection, normal) <= 1e-6) {
        reflection = normal;
    }
    if (out_cos_theta) {
        *out_cos_theta = runtime_disney_v2_3d_clamp01(vec3_dot(normal, reflection));
    }
    if (out_pdf) {
        *out_pdf = fmax(RuntimePrincipledBSDF3D_GGXHalfVectorPdf(
                            principled,
                            sqrt(runtime_disney_v2_3d_clamp01(*out_cos_theta)),
                            runtime_disney_v2_3d_clamp01(vec3_dot(reflection,
                                                                  vec3_normalize(view_dir)))),
                        1e-6);
    }
    return reflection;
}

static bool runtime_disney_v2_3d_resolve_payload(
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    RuntimeMaterialPayload3D* out_payload) {
    if (!out_payload) return false;
    memset(out_payload, 0, sizeof(*out_payload));
    if (payload && payload->valid) {
        *out_payload = *payload;
        return true;
    }
    if (!hit) {
        return false;
    }
    return RuntimeMaterialPayload3D_ResolveFromHit(hit, out_payload) && out_payload->valid;
}

static void runtime_disney_v2_3d_apply_transmittance(
    const RuntimeVisibility3DTransmittance* transmittance,
    RuntimeDisneyV2_3DResult* io_result) {
    int i = 0;
    if (!transmittance || !io_result) return;

    io_result->directRadianceR *= transmittance->r;
    io_result->directRadianceG *= transmittance->g;
    io_result->directRadianceB *= transmittance->b;
    io_result->diffuseRadianceR *= transmittance->r;
    io_result->diffuseRadianceG *= transmittance->g;
    io_result->diffuseRadianceB *= transmittance->b;
    io_result->specularRadianceR *= transmittance->r;
    io_result->specularRadianceG *= transmittance->g;
    io_result->specularRadianceB *= transmittance->b;
    io_result->emissionRadianceR *= transmittance->r;
    io_result->emissionRadianceG *= transmittance->g;
    io_result->emissionRadianceB *= transmittance->b;
    io_result->transmissionRadianceR *= transmittance->r;
    io_result->transmissionRadianceG *= transmittance->g;
    io_result->transmissionRadianceB *= transmittance->b;
    io_result->stochasticDirectRadianceR *= transmittance->r;
    io_result->stochasticDirectRadianceG *= transmittance->g;
    io_result->stochasticDirectRadianceB *= transmittance->b;
    io_result->stochasticBsdfRadianceR *= transmittance->r;
    io_result->stochasticBsdfRadianceG *= transmittance->g;
    io_result->stochasticBsdfRadianceB *= transmittance->b;
    io_result->recursiveBsdfRadianceR *= transmittance->r;
    io_result->recursiveBsdfRadianceG *= transmittance->g;
    io_result->recursiveBsdfRadianceB *= transmittance->b;
    io_result->pathState.throughputR *= transmittance->r;
    io_result->pathState.throughputG *= transmittance->g;
    io_result->pathState.throughputB *= transmittance->b;
    io_result->recursivePathState.throughputR *= transmittance->r;
    io_result->recursivePathState.throughputG *= transmittance->g;
    io_result->recursivePathState.throughputB *= transmittance->b;
    for (i = 0; i < RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY; ++i) {
        io_result->lightSampleContributionR[i] *= transmittance->r;
        io_result->lightSampleContributionG[i] *= transmittance->g;
        io_result->lightSampleContributionB[i] *= transmittance->b;
        io_result->bsdfSampleContributionR[i] *= transmittance->r;
        io_result->bsdfSampleContributionG[i] *= transmittance->g;
        io_result->bsdfSampleContributionB[i] *= transmittance->b;
        io_result->recursiveLoopContributionR[i] *= transmittance->r;
        io_result->recursiveLoopContributionG[i] *= transmittance->g;
        io_result->recursiveLoopContributionB[i] *= transmittance->b;
    }
    io_result->lightSampleContributionTotalR *= transmittance->r;
    io_result->lightSampleContributionTotalG *= transmittance->g;
    io_result->lightSampleContributionTotalB *= transmittance->b;
    io_result->bsdfSampleContributionTotalR *= transmittance->r;
    io_result->bsdfSampleContributionTotalG *= transmittance->g;
    io_result->bsdfSampleContributionTotalB *= transmittance->b;
    if (!(transmittance->luma > 1e-9)) {
        io_result->visible = false;
    }
}

static void runtime_disney_v2_3d_refresh_peaks(RuntimeDisneyV2_3DResult* result) {
    if (!result) return;
    result->directRadiance = runtime_disney_v2_3d_peak(result->directRadianceR,
                                                       result->directRadianceG,
                                                       result->directRadianceB);
    result->diffuseRadiance = runtime_disney_v2_3d_peak(result->diffuseRadianceR,
                                                        result->diffuseRadianceG,
                                                        result->diffuseRadianceB);
    result->specularRadiance = runtime_disney_v2_3d_peak(result->specularRadianceR,
                                                         result->specularRadianceG,
                                                         result->specularRadianceB);
    result->emissionRadiance = runtime_disney_v2_3d_peak(result->emissionRadianceR,
                                                         result->emissionRadianceG,
                                                         result->emissionRadianceB);
    result->transmissionRadiance = runtime_disney_v2_3d_peak(result->transmissionRadianceR,
                                                             result->transmissionRadianceG,
                                                             result->transmissionRadianceB);
    result->primaryTransmissionRadiance =
        runtime_disney_v2_3d_peak(result->primaryTransmissionRadianceR,
                                  result->primaryTransmissionRadianceG,
                                  result->primaryTransmissionRadianceB);
    result->radianceR = result->diffuseRadianceR +
                        result->specularRadianceR +
                        result->emissionRadianceR +
                        result->transmissionRadianceR +
                        result->primaryTransmissionRadianceR +
                        result->stochasticDirectRadianceR +
                        result->stochasticBsdfRadianceR +
                        result->recursiveBsdfRadianceR;
    result->radianceG = result->diffuseRadianceG +
                        result->specularRadianceG +
                        result->emissionRadianceG +
                        result->transmissionRadianceG +
                        result->primaryTransmissionRadianceG +
                        result->stochasticDirectRadianceG +
                        result->stochasticBsdfRadianceG +
                        result->recursiveBsdfRadianceG;
    result->radianceB = result->diffuseRadianceB +
                        result->specularRadianceB +
                        result->emissionRadianceB +
                        result->transmissionRadianceB +
                        result->primaryTransmissionRadianceB +
                        result->stochasticDirectRadianceB +
                        result->stochasticBsdfRadianceB +
                        result->recursiveBsdfRadianceB;
    result->stochasticDirectRadiance =
        runtime_disney_v2_3d_peak(result->stochasticDirectRadianceR,
                                  result->stochasticDirectRadianceG,
                                  result->stochasticDirectRadianceB);
    result->stochasticBsdfRadiance =
        runtime_disney_v2_3d_peak(result->stochasticBsdfRadianceR,
                                  result->stochasticBsdfRadianceG,
                                  result->stochasticBsdfRadianceB);
    result->recursiveBsdfRadiance =
        runtime_disney_v2_3d_peak(result->recursiveBsdfRadianceR,
                                  result->recursiveBsdfRadianceG,
                                  result->recursiveBsdfRadianceB);
    result->lightSampleContribution =
        runtime_disney_v2_3d_peak(result->lightSampleContributionTotalR,
                                  result->lightSampleContributionTotalG,
                                  result->lightSampleContributionTotalB);
    result->bsdfSampleContribution =
        runtime_disney_v2_3d_peak(result->bsdfSampleContributionTotalR,
                                  result->bsdfSampleContributionTotalG,
                                  result->bsdfSampleContributionTotalB);
    result->radianceWithoutLightSamplesR =
        result->radianceR - result->lightSampleContributionTotalR;
    result->radianceWithoutLightSamplesG =
        result->radianceG - result->lightSampleContributionTotalG;
    result->radianceWithoutLightSamplesB =
        result->radianceB - result->lightSampleContributionTotalB;
    result->radianceWithoutBsdfSamplesR =
        result->radianceR - result->bsdfSampleContributionTotalR;
    result->radianceWithoutBsdfSamplesG =
        result->radianceG - result->bsdfSampleContributionTotalG;
    result->radianceWithoutBsdfSamplesB =
        result->radianceB - result->bsdfSampleContributionTotalB;
    result->radianceWithoutLightSamples =
        runtime_disney_v2_3d_peak(result->radianceWithoutLightSamplesR,
                                  result->radianceWithoutLightSamplesG,
                                  result->radianceWithoutLightSamplesB);
    result->radianceWithoutBsdfSamples =
        runtime_disney_v2_3d_peak(result->radianceWithoutBsdfSamplesR,
                                  result->radianceWithoutBsdfSamplesG,
                                  result->radianceWithoutBsdfSamplesB);
    result->radiance = runtime_disney_v2_3d_peak(result->radianceR,
                                                 result->radianceG,
                                                 result->radianceB);
    if (result->radiance > 1e-9) {
        result->visible = true;
    }
}

bool RuntimeDisneyV2_3D_BuildDiagnostics(const RuntimeDisneyV2_3DResult* result,
                                         RuntimeDisneyV2_3DDiagnostics* out_diagnostics) {
    RuntimeDisneyV2_3DDiagnostics diagnostics = {0};
    double dominant = 0.0;

    if (!result || !out_diagnostics) return false;

    diagnostics.hit = result->hit;
    diagnostics.payloadResolved = result->payloadResolved;
    diagnostics.valid = result->hit && result->payloadResolved && result->principled.valid;
    diagnostics.hasDirectSignal = result->directRadiance > 1e-9;
    diagnostics.hasDiffuseSignal = result->diffuseRadiance > 1e-9;
    diagnostics.hasSpecularSignal = result->specularRadiance > 1e-9;
    diagnostics.hasEmissionSignal = result->emissionRadiance > 1e-9;
    diagnostics.hasTransmissionSignal = result->transmissionRadiance > 1e-9;
    diagnostics.lobeProbabilitySum =
        result->diffuseProbability + result->specularProbability + result->transmissionProbability;
    diagnostics.radianceEnergy =
        result->diffuseRadiance + result->specularRadiance +
        result->emissionRadiance + result->transmissionRadiance;
    diagnostics.specularToDiffuseRatio =
        result->specularRadiance / fmax(result->diffuseRadiance, 1e-9);
    diagnostics.transmissionWeight = result->principled.transmissionWeight;
    diagnostics.emissionStrength = result->principled.emissiveStrength;

    diagnostics.dominantLobe = RUNTIME_DISNEY_V2_3D_LOBE_NONE;
    dominant = 1e-9;
    if (result->diffuseRadiance > dominant) {
        diagnostics.dominantLobe = RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE;
        dominant = result->diffuseRadiance;
    }
    if (result->specularRadiance > dominant) {
        diagnostics.dominantLobe = RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR;
        dominant = result->specularRadiance;
    }
    if (result->emissionRadiance > dominant) {
        diagnostics.dominantLobe = RUNTIME_DISNEY_V2_3D_LOBE_EMISSION;
        dominant = result->emissionRadiance;
    }
    if (result->transmissionRadiance > dominant) {
        diagnostics.dominantLobe = RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION;
    }

    diagnostics.routeProofReady =
        diagnostics.valid &&
        diagnostics.hasDirectSignal &&
        diagnostics.lobeProbabilitySum > 0.0 &&
        diagnostics.radianceEnergy > 0.0;

    *out_diagnostics = diagnostics;
    return diagnostics.valid;
}

static void runtime_disney_v2_3d_apply_stochastic_transport(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 view_dir,
    RuntimeDisneyV2_3DResult* io_result) {
    RuntimeLightEmitterTrace3DResult trace = {0};
    RuntimeDirectLight3DResult secondary_direct = {0};
    RuntimeDisneyV2_3DTransmissionSample transmission_sample = {0};
    Vec3 sample_dir = vec3(0.0, 1.0, 0.0);
    double lobe_sample = 0.5;
    double sample_pdf = 0.0;
    double cos_theta = 0.0;
    double throughput_r = 0.0;
    double throughput_g = 0.0;
    double throughput_b = 0.0;

    if (!scene || !hit || !io_result || !io_result->principled.valid) return;

    io_result->sampledLobe = runtime_disney_v2_3d_choose_lobe(io_result,
                                                              sampling,
                                                              &lobe_sample);
    if (io_result->sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_NONE) {
        io_result->sampledLobe = io_result->specularProbability > io_result->diffuseProbability
                                     ? RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR
                                     : RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE;
    }
    io_result->sampledLobeMaxDepth =
        RuntimePathDepthPolicy3D_MaxDepthForLobe(
            &io_result->pathPolicy,
            runtime_disney_v2_3d_policy_lobe(io_result->sampledLobe));
    if (!RuntimePathDepthPolicy3D_AllowsDepth(
            &io_result->pathPolicy,
            runtime_disney_v2_3d_policy_lobe(io_result->sampledLobe),
            1)) {
        io_result->pathDepthLimitReached = true;
        return;
    }

    if (io_result->sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION) {
        if (!RuntimeDisneyV2_3D_SampleTransmission(&io_result->payload,
                                                   &io_result->principled,
                                                   hit,
                                                   view_dir,
                                                   io_result->transmissionProbability,
                                                   &transmission_sample)) {
            return;
        }
        sample_dir = transmission_sample.direction;
        sample_pdf = transmission_sample.pdf;
        cos_theta = 1.0;
        throughput_r = transmission_sample.throughputR;
        throughput_g = transmission_sample.throughputG;
        throughput_b = transmission_sample.throughputB;
    } else if (io_result->sampledLobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR) {
        sample_dir = runtime_disney_v2_3d_sample_specular_direction(hit,
                                                                    view_dir,
                                                                    sampling,
                                                                    &io_result->principled,
                                                                    &sample_pdf,
                                                                    &cos_theta);
        throughput_r = runtime_disney_v2_3d_clamp(
            io_result->principled.specularF0R * io_result->specularProbability *
                fmax(io_result->fresnelWeight, 0.15),
            0.0,
            2.0);
        throughput_g = runtime_disney_v2_3d_clamp(
            io_result->principled.specularF0G * io_result->specularProbability *
                fmax(io_result->fresnelWeight, 0.15),
            0.0,
            2.0);
        throughput_b = runtime_disney_v2_3d_clamp(
            io_result->principled.specularF0B * io_result->specularProbability *
                fmax(io_result->fresnelWeight, 0.15),
            0.0,
            2.0);
    } else {
        io_result->sampledLobe = RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE;
        sample_dir = runtime_disney_v2_3d_sample_diffuse_direction(hit,
                                                                   sampling,
                                                                   &sample_pdf,
                                                                   &cos_theta);
        throughput_r = runtime_disney_v2_3d_clamp(
            io_result->principled.baseColorR * io_result->diffuseProbability * cos_theta /
                fmax(sample_pdf * M_PI, 1e-6),
            0.0,
            2.0);
        throughput_g = runtime_disney_v2_3d_clamp(
            io_result->principled.baseColorG * io_result->diffuseProbability * cos_theta /
                fmax(sample_pdf * M_PI, 1e-6),
            0.0,
            2.0);
        throughput_b = runtime_disney_v2_3d_clamp(
            io_result->principled.baseColorB * io_result->diffuseProbability * cos_theta /
                fmax(sample_pdf * M_PI, 1e-6),
            0.0,
            2.0);
    }

    io_result->bsdfSamplePdf = sample_pdf;
    io_result->lightSamplePdf = scene->hasLight ? 1.0 : 0.0;
    runtime_disney_v2_3d_balance_mis(io_result->lightSamplePdf,
                                      io_result->bsdfSamplePdf,
                                      &io_result->misWeightLight,
                                      &io_result->misWeightBsdf);
    runtime_disney_v2_3d_record_mis_vertex(io_result,
                                           0,
                                           io_result->lightSamplePdf,
                                           io_result->bsdfSamplePdf,
                                           io_result->misWeightLight,
                                           io_result->misWeightBsdf);

    io_result->stochasticDirectRadianceR =
        io_result->directRadianceR * io_result->misWeightLight;
    io_result->stochasticDirectRadianceG =
        io_result->directRadianceG * io_result->misWeightLight;
    io_result->stochasticDirectRadianceB =
        io_result->directRadianceB * io_result->misWeightLight;
    runtime_disney_v2_3d_record_light_sample_contribution(
        io_result,
        0,
        io_result->stochasticDirectRadianceR,
        io_result->stochasticDirectRadianceG,
        io_result->stochasticDirectRadianceB);

    io_result->pathState.valid = sample_pdf > 1e-9 &&
                                 runtime_disney_v2_3d_peak(throughput_r,
                                                            throughput_g,
                                                            throughput_b) > 1e-9;
    io_result->pathState.depth = 1;
    io_result->pathState.sampledLobe = io_result->sampledLobe;
    io_result->pathState.throughputR = throughput_r;
    io_result->pathState.throughputG = throughput_g;
    io_result->pathState.throughputB = throughput_b;
    io_result->pathState.pdf = sample_pdf;
    if (!io_result->pathState.valid) return;

    io_result->bsdfSampleCount = 1;
    io_result->pathState.ray = RuntimeRay3D_MakeOffset(hit->position,
                                                       hit->normal,
                                                       sample_dir,
                                                       kRuntimeDisneyV2_3DEpsilon);
    io_result->secondaryRayCount = 1;
    if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                               &io_result->pathState.ray,
                                               kRuntimeDisneyV2_3DEpsilon,
                                               kRuntimeDisneyV2_3DMaxDistance,
                                               &trace)) {
        return;
    }

    if (trace.geometryHit) {
        RuntimeMaterialPayload3D secondary_payload = {0};
        RuntimePrincipledBSDF3D secondary_principled = {0};
        double secondary_response_r = 1.0;
        double secondary_response_g = 1.0;
        double secondary_response_b = 1.0;
        double secondary_throughput_r = throughput_r;
        double secondary_throughput_g = throughput_g;
        double secondary_throughput_b = throughput_b;
        bool secondary_payload_resolved = false;

        io_result->pathState.hit = true;
        io_result->pathState.hitInfo = trace.geometryHitInfo;
        io_result->secondaryHitCount += 1;
        secondary_payload_resolved =
            RuntimeMaterialPayload3D_ResolveFromHit(&trace.geometryHitInfo, &secondary_payload) &&
            secondary_payload.valid;
        if (secondary_payload_resolved) {
            secondary_principled = RuntimePrincipledBSDF3D_FromMaterialPayload(&secondary_payload);
            runtime_disney_v2_3d_secondary_response(&secondary_principled,
                                                    &secondary_response_r,
                                                    &secondary_response_g,
                                                    &secondary_response_b);
            secondary_throughput_r *= secondary_response_r;
            secondary_throughput_g *= secondary_response_g;
            secondary_throughput_b *= secondary_response_b;
            io_result->secondaryPayloadResolved = true;
            io_result->secondaryPayload = secondary_payload;
            io_result->secondaryPrincipled = secondary_principled;
            io_result->secondaryMaterialResponseR = secondary_response_r;
            io_result->secondaryMaterialResponseG = secondary_response_g;
            io_result->secondaryMaterialResponseB = secondary_response_b;
            io_result->secondaryVertexThroughputR = secondary_throughput_r;
            io_result->secondaryVertexThroughputG = secondary_throughput_g;
            io_result->secondaryVertexThroughputB = secondary_throughput_b;
        }
        if ((secondary_payload_resolved
                 ? RuntimeDirectLight3D_ShadeHitWithPayload(scene,
                                                            &trace.geometryHitInfo,
                                                            &secondary_payload,
                                                            sampling,
                                                            &secondary_direct)
                 : RuntimeDirectLight3D_ShadeHit(scene,
                                                 &trace.geometryHitInfo,
                                                 sampling,
                                                 &secondary_direct))) {
            const double contribution_r =
                secondary_throughput_r * secondary_direct.radianceR * io_result->misWeightBsdf;
            const double contribution_g =
                secondary_throughput_g * secondary_direct.radianceG * io_result->misWeightBsdf;
            const double contribution_b =
                secondary_throughput_b * secondary_direct.radianceB * io_result->misWeightBsdf;
            io_result->stochasticBsdfRadianceR +=
                contribution_r;
            io_result->stochasticBsdfRadianceG +=
                contribution_g;
            io_result->stochasticBsdfRadianceB +=
                contribution_b;
            runtime_disney_v2_3d_record_mis_vertex(io_result,
                                                   1,
                                                   io_result->lightSamplePdf,
                                                   io_result->bsdfSamplePdf,
                                                   io_result->misWeightLight,
                                                   io_result->misWeightBsdf);
            runtime_disney_v2_3d_record_light_sample_contribution(io_result,
                                                                  1,
                                                                  contribution_r,
                                                                  contribution_g,
                                                                  contribution_b);
            runtime_disney_v2_3d_record_bsdf_sample_contribution(
                io_result,
                1,
                contribution_r,
                contribution_g,
                contribution_b,
                RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT);
        }
        RuntimeDisneyV2_3D_ApplyRecursivePathLoop(scene,
                                                  &trace.geometryHitInfo,
                                                  sampling,
                                                  secondary_throughput_r,
                                                  secondary_throughput_g,
                                                  secondary_throughput_b,
                                                  io_result);
    }
    if (trace.emitterWins) {
        const double emitter_radiance = trace.emitterHitInfo.radiance;
        const double contribution_r = throughput_r * emitter_radiance * io_result->misWeightBsdf;
        const double contribution_g = throughput_g * emitter_radiance * io_result->misWeightBsdf;
        const double contribution_b = throughput_b * emitter_radiance * io_result->misWeightBsdf;
        io_result->pathState.emitterHit = trace.emitterHit;
        io_result->pathState.emitterWins = true;
        io_result->pathState.emitterHitInfo = trace.emitterHitInfo;
        io_result->stochasticBsdfRadianceR +=
            contribution_r;
        io_result->stochasticBsdfRadianceG +=
            contribution_g;
        io_result->stochasticBsdfRadianceB +=
            contribution_b;
        runtime_disney_v2_3d_record_bsdf_sample_contribution(
            io_result,
            0,
            contribution_r,
            contribution_g,
            contribution_b,
            RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT);
    }
    if (runtime_disney_v2_3d_luma(io_result->stochasticBsdfRadianceR,
                                  io_result->stochasticBsdfRadianceG,
                                  io_result->stochasticBsdfRadianceB) > 1e-9) {
        io_result->secondaryContributingHitCount += 1;
    }
}

static bool runtime_disney_v2_3d_shade_hit_with_payload(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 view_dir,
    RuntimeDisneyV2_3DResult* out_result) {
    RuntimeDisneyV2_3DResult result = {0};
    RuntimeDirectLight3DResult direct = {0};
    Vec3 light_dir = vec3(0.0, 0.0, 0.0);
    Vec3 half_vec = vec3(0.0, 0.0, 0.0);
    double light_distance = 0.0;
    double cos_theta_h = 0.0;
    double dot_i_h = 0.0;
    double diffuse_scale = 0.0;
    double specular_scale = 0.0;
    double transmission_scale = 0.0;

    if (!scene || !hit || !out_result) return false;
    result.hit = true;
    result.hitInfo = *hit;
    result.primaryRay.direction = vec3_scale(view_dir, -1.0);
    result.payloadResolved = runtime_disney_v2_3d_resolve_payload(hit, payload, &result.payload);
    if (!result.payloadResolved) {
        *out_result = result;
        return false;
    }

    result.principled = RuntimePrincipledBSDF3D_FromMaterialPayload(&result.payload);
    result.pathPolicy = RuntimePathDepthPolicy3D_Resolve();
    result.pathPolicyResolved = true;
    result.diffuseProbability =
        RuntimePrincipledBSDF3D_DiffuseProbability(&result.principled);
    result.specularProbability =
        RuntimePrincipledBSDF3D_SpecularProbability(&result.principled);
    result.transmissionProbability =
        runtime_disney_v2_3d_clamp01(result.principled.transmissionWeight);
    result.pathDepth = 1;

    view_dir = vec3_normalize(view_dir);
    result.ndotv = runtime_disney_v2_3d_clamp01(vec3_dot(hit->normal, view_dir));
    light_dir = vec3_sub(scene->light.position, hit->position);
    light_distance = vec3_length(light_dir);
    if (light_distance > 1e-9) {
        light_dir = vec3_scale(light_dir, 1.0 / light_distance);
    } else {
        light_dir = vec3_normalize(hit->normal);
    }
    result.ndotl = runtime_disney_v2_3d_clamp01(vec3_dot(hit->normal, light_dir));
    half_vec = vec3_normalize(vec3_add(light_dir, view_dir));
    cos_theta_h = runtime_disney_v2_3d_clamp01(vec3_dot(hit->normal, half_vec));
    dot_i_h = runtime_disney_v2_3d_clamp01(vec3_dot(light_dir, half_vec));
    result.fresnelWeight =
        RuntimePrincipledBSDF3D_FresnelSchlick(dot_i_h, result.principled.dielectricF0);
    result.diffuseBsdfCos =
        RuntimePrincipledBSDF3D_EvaluateDiffuseCos(&result.principled, result.ndotl);
    result.specularBsdfCos =
        RuntimePrincipledBSDF3D_EvaluateGGXSpecularCos(&result.principled,
                                                       result.ndotl,
                                                       result.ndotv,
                                                       cos_theta_h);
    result.specularHalfPdf =
        RuntimePrincipledBSDF3D_GGXHalfVectorPdf(&result.principled, cos_theta_h, dot_i_h);

    if (RuntimeDirectLight3D_ShadeHitWithPayload(scene, hit, &result.payload, sampling, &direct)) {
        result.directSampleCount = scene->hasLight ? 1 : 0;
        result.directRadianceR = direct.radianceR;
        result.directRadianceG = direct.radianceG;
        result.directRadianceB = direct.radianceB;
        result.visible = direct.visible;
    }

    diffuse_scale = runtime_disney_v2_3d_clamp(result.diffuseProbability *
                                                  (0.25 + result.diffuseBsdfCos),
                                              0.0,
                                              1.25);
    specular_scale = runtime_disney_v2_3d_clamp(result.specularProbability *
                                                   (result.fresnelWeight + result.specularBsdfCos),
                                               0.0,
                                               1.75);
    transmission_scale = runtime_disney_v2_3d_clamp(result.transmissionProbability *
                                                       (1.0 - result.diffuseProbability),
                                                   0.0,
                                                   1.0);

    result.diffuseRadianceR = result.directRadianceR * diffuse_scale;
    result.diffuseRadianceG = result.directRadianceG * diffuse_scale;
    result.diffuseRadianceB = result.directRadianceB * diffuse_scale;
    result.specularRadianceR = result.directRadianceR * specular_scale;
    result.specularRadianceG = result.directRadianceG * specular_scale;
    result.specularRadianceB = result.directRadianceB * specular_scale;
    result.transmissionRadianceR = result.directRadianceR * transmission_scale;
    result.transmissionRadianceG = result.directRadianceG * transmission_scale;
    result.transmissionRadianceB = result.directRadianceB * transmission_scale;
    result.emissionRadianceR = result.principled.emissiveR * result.principled.emissiveStrength;
    result.emissionRadianceG = result.principled.emissiveG * result.principled.emissiveStrength;
    result.emissionRadianceB = result.principled.emissiveB * result.principled.emissiveStrength;

    runtime_disney_v2_3d_apply_stochastic_transport(scene, hit, sampling, view_dir, &result);
    runtime_disney_v2_3d_refresh_peaks(&result);
    *out_result = result;
    return true;
}

bool RuntimeDisneyV2_3D_ShadeHit(const RuntimeScene3D* scene,
                                 const HitInfo3D* hit,
                                 const RuntimeNative3DSamplingContext* sampling,
                                 RuntimeDisneyV2_3DResult* out_result) {
    return runtime_disney_v2_3d_shade_hit_with_payload(scene,
                                                       hit,
                                                       NULL,
                                                       sampling,
                                                       runtime_disney_v2_3d_default_view_dir(hit),
                                                       out_result);
}

bool RuntimeDisneyV2_3D_ShadePrimaryHit(const RuntimeScene3D* scene,
                                        const RuntimePrimaryHit3DResult* primary_hit,
                                        const RuntimeNative3DSamplingContext* sampling,
                                        RuntimeDisneyV2_3DResult* out_result) {
    return RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(scene,
                                                         primary_hit,
                                                         NULL,
                                                         sampling,
                                                         out_result);
}

bool RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDisneyV2_3DResult* out_result) {
    RuntimeDisneyV2_3DResult result = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 1.0);

    if (!scene || !primary_hit || !out_result) return false;
    if (!primary_hit->hit) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }

    view_dir = vec3_scale(primary_hit->primaryRay.direction, -1.0);
    if (!runtime_disney_v2_3d_shade_hit_with_payload(scene,
                                                     &primary_hit->hitInfo,
                                                     payload,
                                                     sampling,
                                                     view_dir,
                                                     &result)) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }
    result.primaryRay = primary_hit->primaryRay;
    runtime_disney_v2_3d_apply_transmittance(&primary_hit->primaryTransmittance, &result);
    (void)RuntimeDisneyV2_3D_ApplyPrimaryTransmissionContinuation(scene,
                                                                  primary_hit,
                                                                  sampling,
                                                                  &result);
    runtime_disney_v2_3d_refresh_peaks(&result);
    *out_result = result;
    return true;
}

bool RuntimeDisneyV2_3D_ShadePixel(const RuntimeScene3D* scene,
                                   const RuntimeCameraProjector3D* projector,
                                   double pixel_x,
                                   double pixel_y,
                                   const RuntimeNative3DSamplingContext* sampling,
                                   RuntimeDisneyV2_3DResult* out_result) {
    RuntimeDisneyV2_3DResult result = {0};
    RuntimePrimaryHit3DResult primary_hit = {0};

    if (!scene || !projector || !out_result) return false;
    if (!RuntimeDirectLight3D_TracePrimaryHit(scene,
                                              projector,
                                              pixel_x,
                                              pixel_y,
                                              &primary_hit)) {
        result.primaryRay = primary_hit.primaryRay;
        *out_result = result;
        return false;
    }

    return RuntimeDisneyV2_3D_ShadePrimaryHit(scene, &primary_hit, sampling, out_result);
}
