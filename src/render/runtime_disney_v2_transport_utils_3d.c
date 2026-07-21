#include "render/runtime_disney_v2_transport_internal_3d.h"

#include <math.h>
#include <stdint.h>

double runtime_disney_v2_transport_3d_clamp(double value,
                                            double min_value,
                                            double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

double runtime_disney_v2_transport_3d_clamp01(double value) {
    return runtime_disney_v2_transport_3d_clamp(value, 0.0, 1.0);
}

double runtime_disney_v2_transport_3d_luma(double r, double g, double b) {
    return (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
}

double runtime_disney_v2_transport_3d_peak(double r, double g, double b) {
    double peak = r;
    if (g > peak) peak = g;
    if (b > peak) peak = b;
    return peak;
}

void runtime_disney_v2_transport_3d_balance_mis(double light_pdf,
                                                double bsdf_pdf,
                                                double* out_light,
                                                double* out_bsdf) {
    RuntimeDisneyV2_3DMisWeights weights = {0};

    (void)RuntimeDisneyV2_3D_ResolvePowerHeuristicMIS(light_pdf, bsdf_pdf, &weights);
    if (out_light) *out_light = runtime_disney_v2_transport_3d_clamp01(weights.lightWeight);
    if (out_bsdf) *out_bsdf = runtime_disney_v2_transport_3d_clamp01(weights.bsdfWeight);
}

RuntimeDisneyV2_3DMisBranch runtime_disney_v2_transport_3d_make_mis_branch(
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

void runtime_disney_v2_transport_3d_record_mis_vertex(
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

void runtime_disney_v2_transport_3d_record_mis_branch_vertex(
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

void runtime_disney_v2_transport_3d_record_bsdf_sample_contribution(
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

uint32_t runtime_disney_v2_transport_3d_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint32_t runtime_disney_v2_transport_3d_seed_from_hit(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling) {
    uint32_t sx = 0U;
    uint32_t sy = 0U;
    uint32_t sz = 0U;
    uint32_t surface_seed = 0U;
    uint32_t sequence = sampling ? sampling->sampleSequence : 1U;

    if (!hit) {
        return runtime_disney_v2_transport_3d_hash_u32(sequence ^ 0x6d2b79f5U);
    }
    sx = (uint32_t)(fabs(hit->position.x) * 4096.0);
    sy = (uint32_t)(fabs(hit->position.y) * 4096.0);
    sz = (uint32_t)(fabs(hit->position.z) * 4096.0);
    /* Preserve the historical triangle-zero stream without keying valid hits by tessellation. */
    surface_seed = hit->triangleIndex >= 0 ? 83492791U : 0U;
    return runtime_disney_v2_transport_3d_hash_u32(
        sx ^ (sy * 73856093U) ^ (sz * 19349663U) ^
        surface_seed ^
        runtime_disney_v2_transport_3d_hash_u32(sequence ^ 0x9e3779b9U));
}

double runtime_disney_v2_transport_3d_roulette_sample(
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

RuntimePathDepthPolicy3DLobe runtime_disney_v2_transport_3d_policy_lobe(
    RuntimeDisneyV2_3DDominantLobe lobe) {
    if (lobe == RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR) {
        return RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_SPECULAR;
    }
    if (lobe == RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION) {
        return RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION;
    }
    return RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_DIFFUSE;
}

Vec3 runtime_disney_v2_transport_3d_default_tangent(Vec3 normal) {
    Vec3 guide = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3_cross(guide, normal);
    if (vec3_length(tangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
    }
    return vec3_normalize(tangent);
}

void runtime_disney_v2_transport_3d_build_basis(Vec3 normal,
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

Vec3 runtime_disney_v2_transport_3d_reflect(Vec3 incident_dir, Vec3 normal) {
    const double ndoti = vec3_dot(normal, incident_dir);
    return vec3_normalize(vec3_sub(incident_dir, vec3_scale(normal, 2.0 * ndoti)));
}
