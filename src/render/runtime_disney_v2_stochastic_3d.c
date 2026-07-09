#include "render/runtime_disney_v2_internal_3d.h"

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
    RuntimeDisneyV2_3DMisWeights weights = {0};

    (void)RuntimeDisneyV2_3D_ResolvePowerHeuristicMIS(light_pdf, bsdf_pdf, &weights);
    if (out_light_weight) *out_light_weight = weights.lightWeight;
    if (out_bsdf_weight) *out_bsdf_weight = weights.bsdfWeight;
}

static RuntimeDisneyV2_3DMisBranch runtime_disney_v2_3d_make_mis_branch(
    double light_pdf,
    double bsdf_pdf) {
    RuntimeDisneyV2_3DMisWeights weights = {0};
    RuntimeDisneyV2_3DMisBranch branch = {0};

    (void)RuntimeDisneyV2_3D_ResolvePowerHeuristicMIS(light_pdf, bsdf_pdf, &weights);
    branch.lightPdf = weights.lightPdf;
    branch.bsdfPdf = weights.bsdfPdf;
    branch.weightLight = weights.lightWeight;
    branch.weightBsdf = weights.bsdfWeight;
    return branch;
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
    io_result->misHeuristicPower = 2.0;
    io_result->misPowerHeuristicCount += 1;
    if (io_result->misVertexCount < vertex_index + 1) {
        io_result->misVertexCount = vertex_index + 1;
    }
}

static void runtime_disney_v2_3d_record_mis_branch_vertex(
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

Vec3 runtime_disney_v2_3d_default_view_dir(const HitInfo3D* hit) {
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

void runtime_disney_v2_3d_apply_stochastic_transport(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    Vec3 view_dir,
    RuntimeDisneyV2_3DResult* io_result) {
    RuntimeLightEmitterTrace3DResult trace = {0};
    RuntimeDirectLight3DResult secondary_direct = {0};
    RuntimeDisneyV2_3DTransmissionSample transmission_sample = {0};
    RuntimeEmissiveDirect3DResult emissive_area_direct = {0};
    Vec3 sample_dir = vec3(0.0, 1.0, 0.0);
    double lobe_sample = 0.5;
    double sample_pdf = 0.0;
    double cos_theta = 0.0;
    double throughput_r = 0.0;
    double throughput_g = 0.0;
    double throughput_b = 0.0;
    double finite_direct_bsdf_pdf = 0.0;
    double area_direct_bsdf_pdf = 0.0;
    RuntimeDisneyV2_3DMisBranch finite_light_branch = {0};
    RuntimeDisneyV2_3DMisBranch emissive_area_branch = {0};
    bool has_emissive_area_direct = false;

    if (!scene || !hit || !io_result || !io_result->principled.valid) return;
    if (RuntimeDisneyV2_3D_ShouldEvaluateEmissiveAreaLightSample(scene,
                                                                 false,
                                                                 io_result)) {
        has_emissive_area_direct =
            RuntimeDisneyV2_3D_EvaluateEmissiveAreaLightSample(scene,
                                                               hit,
                                                               sampling,
                                                               &emissive_area_direct);
    }

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

    finite_direct_bsdf_pdf =
        RuntimeDisneyV2_3D_EstimateDirectBsdfPdfForSceneLight(scene,
                                                              hit,
                                                              &io_result->principled,
                                                              vec3_scale(view_dir, -1.0),
                                                              io_result->transmissionProbability >
                                                                  1e-9);
    if (has_emissive_area_direct &&
        emissive_area_direct.lightPdf > 0.0 &&
        vec3_length(emissive_area_direct.sampleDirection) > 1e-9) {
        area_direct_bsdf_pdf =
            RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&io_result->principled,
                                                     hit,
                                                     vec3_scale(view_dir, -1.0),
                                                     emissive_area_direct.sampleDirection,
                                                     io_result->transmissionProbability >
                                                         1e-9);
    }
    finite_light_branch =
        runtime_disney_v2_3d_make_mis_branch(
            RuntimeDisneyV2_3D_EstimateFiniteLightPdfForHit(scene, hit),
            finite_direct_bsdf_pdf);
    emissive_area_branch =
        runtime_disney_v2_3d_make_mis_branch(
            has_emissive_area_direct ? emissive_area_direct.lightPdf : 0.0,
            area_direct_bsdf_pdf);
    io_result->finiteLightMis = finite_light_branch;
    io_result->emissiveAreaMis = emissive_area_branch;
    io_result->bsdfSamplePdf = fmax(finite_direct_bsdf_pdf, area_direct_bsdf_pdf);
    io_result->lightSamplePdf =
        RuntimeDisneyV2_3D_EstimateDirectLightPdfForHitWithAreaPdf(
            scene,
            hit,
            has_emissive_area_direct,
            emissive_area_direct.lightPdf);
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
    runtime_disney_v2_3d_record_mis_branch_vertex(io_result,
                                                  0,
                                                  finite_light_branch,
                                                  emissive_area_branch);

    io_result->stochasticDirectRadianceR =
        io_result->directRadianceR * finite_light_branch.weightLight;
    io_result->stochasticDirectRadianceG =
        io_result->directRadianceG * finite_light_branch.weightLight;
    io_result->stochasticDirectRadianceB =
        io_result->directRadianceB * finite_light_branch.weightLight;
    runtime_disney_v2_3d_record_light_sample_contribution(
        io_result,
        0,
        io_result->stochasticDirectRadianceR,
        io_result->stochasticDirectRadianceG,
        io_result->stochasticDirectRadianceB);
    if (has_emissive_area_direct) {
        (void)RuntimeDisneyV2_3D_AccumulateEmissiveAreaLightSample(&emissive_area_direct,
                                                                   1.0,
                                                                   1.0,
                                                                   1.0,
                                                                   emissive_area_branch.weightLight,
                                                                   0,
                                                                   false,
                                                                   io_result);
    }

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
                                                       RUNTIME_DISNEY_V2_3D_EPSILON);
    io_result->secondaryRayCount = 1;
    RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
        RUNTIME_RENDER_TRACE_COST_RAY_DISNEY_RECURSIVE,
        1);
    if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                               &io_result->pathState.ray,
                                               RUNTIME_DISNEY_V2_3D_EPSILON,
                                               RUNTIME_DISNEY_V2_3D_MAX_DISTANCE,
                                               &trace)) {
        return;
    }
    if (trace.geometryHit) {
        RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(&trace.geometryHitInfo);
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
        bool secondary_material_emitter = false;

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
            secondary_material_emitter = RuntimeDisneyV2_3D_AccumulateEmissiveMaterialHit(
                &trace.geometryHitInfo,
                &secondary_payload,
                &secondary_principled,
                &io_result->pathState.ray,
                throughput_r,
                throughput_g,
                throughput_b,
                io_result->misWeightBsdf,
                1,
                false,
                &io_result->pathState,
                NULL,
                NULL,
                NULL,
                io_result);
        }
        if (!secondary_material_emitter &&
            (secondary_payload_resolved
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
                scene && scene->hasLight && scene->light.radius > 1e-9
                    ? RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT
                    : RUNTIME_DISNEY_V2_3D_EMITTER_NONE);
        }
        if (!secondary_material_emitter) {
            RuntimeDisneyV2_3D_ApplyRecursivePathLoop(scene,
                                                      &trace.geometryHitInfo,
                                                      sampling,
                                                      throughput_r,
                                                      throughput_g,
                                                      throughput_b,
                                                      io_result);
        }
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
