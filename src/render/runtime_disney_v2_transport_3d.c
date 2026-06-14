#include "render/runtime_disney_v2_transport_3d.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_path_depth_policy_3d.h"
#include "render/runtime_ray_3d.h"

static const double kRuntimeDisneyV2Transport3DEpsilon = 1e-4;
static const double kRuntimeDisneyV2Transport3DMaxDistance = 48.0;
static const double kRuntimeDisneyV2Transport3DNegligibleLuma = 1e-9;

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

static void runtime_disney_v2_transport_3d_record_mis_vertex(
    RuntimeDisneyV2_3DResult* io_result,
    int vertex_index) {
    if (!io_result || vertex_index < 0 ||
        vertex_index >= RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY) {
        return;
    }
    io_result->misVertexLightPdf[vertex_index] = io_result->lightSamplePdf;
    io_result->misVertexBsdfPdf[vertex_index] = io_result->bsdfSamplePdf;
    io_result->misVertexWeightLight[vertex_index] = io_result->misWeightLight;
    io_result->misVertexWeightBsdf[vertex_index] = io_result->misWeightBsdf;
    if (io_result->misVertexCount < vertex_index + 1) {
        io_result->misVertexCount = vertex_index + 1;
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
        io_result->misVertexEmitterKind[vertex_index] = emitter_kind;
        if (emitter_kind == RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT) {
            io_result->finiteLightEmitterHitCount += 1;
        } else if (emitter_kind == RUNTIME_DISNEY_V2_3D_EMITTER_EMISSIVE_MATERIAL) {
            io_result->emissiveMaterialHitCount += 1;
        }
    }
    if (runtime_disney_v2_transport_3d_luma(r, g, b) > 1e-9) {
        io_result->bsdfSampleContributionCount += 1;
    }
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

static void runtime_disney_v2_transport_3d_secondary_response(
    const RuntimePrincipledBSDF3D* principled,
    double* out_r,
    double* out_g,
    double* out_b) {
    double lobe_scale = 1.0;
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;

    if (principled && principled->valid) {
        lobe_scale = runtime_disney_v2_transport_3d_clamp(
            0.65 +
                (principled->diffuseWeight * 0.35) +
                (principled->specularWeight * 0.20) +
                (principled->transmissionWeight * 0.15),
            0.25,
            1.5);
        r = runtime_disney_v2_transport_3d_clamp(
            (0.15 + (principled->baseColorR * 0.85)) * lobe_scale,
            0.05,
            2.0);
        g = runtime_disney_v2_transport_3d_clamp(
            (0.15 + (principled->baseColorG * 0.85)) * lobe_scale,
            0.05,
            2.0);
        b = runtime_disney_v2_transport_3d_clamp(
            (0.15 + (principled->baseColorB * 0.85)) * lobe_scale,
            0.05,
            2.0);
    }

    if (out_r) *out_r = r;
    if (out_g) *out_g = g;
    if (out_b) *out_b = b;
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

bool RuntimeDisneyV2_3D_ApplyRecursivePathLoop(
    const RuntimeScene3D* scene,
    const HitInfo3D* start_hit,
    const RuntimeNative3DSamplingContext* sampling,
    double parent_throughput_r,
    double parent_throughput_g,
    double parent_throughput_b,
    RuntimeDisneyV2_3DResult* io_result) {
    RuntimePathDepthPolicy3DLobe policy_lobe = RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_DIFFUSE;
    HitInfo3D current_hit = {0};
    double throughput_r = parent_throughput_r;
    double throughput_g = parent_throughput_g;
    double throughput_b = parent_throughput_b;
    int depth = 2;
    bool contributed = false;

    if (!scene || !start_hit || !io_result || !scene->hasLight ||
        !(scene->light.radius > 1e-9)) {
        return false;
    }

    policy_lobe = runtime_disney_v2_transport_3d_policy_lobe(io_result->sampledLobe);
    current_hit = *start_hit;

    while (depth <= RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY + 1) {
        RuntimeDisneyV2_3DPathState state = {0};
        RuntimeLightEmitterTrace3DResult trace = {0};
        Vec3 light_dir = vec3_sub(scene->light.position, current_hit.position);
        double light_distance = vec3_length(light_dir);
        double ndotl = 0.0;
        double survival = 1.0;
        double throughput_luma = 0.0;

        if (!RuntimePathDepthPolicy3D_AllowsDepth(&io_result->pathPolicy,
                                                  policy_lobe,
                                                  depth)) {
            runtime_disney_v2_transport_3d_note_termination(
                io_result,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_MAX_DEPTH);
            break;
        }

        if (!(light_distance > 1e-9)) {
            runtime_disney_v2_transport_3d_note_termination(
                io_result,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NO_HIT);
            break;
        }
        light_dir = vec3_scale(light_dir, 1.0 / light_distance);
        ndotl = runtime_disney_v2_transport_3d_clamp01(vec3_dot(current_hit.normal,
                                                                light_dir));
        throughput_luma = runtime_disney_v2_transport_3d_luma(throughput_r * ndotl,
                                                              throughput_g * ndotl,
                                                              throughput_b * ndotl);

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
        if (!(throughput_luma > kRuntimeDisneyV2Transport3DNegligibleLuma) ||
            !(ndotl > 1e-9)) {
            runtime_disney_v2_transport_3d_note_termination(
                io_result,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NEGLIGIBLE_THROUGHPUT);
            break;
        }

        state.valid = true;
        state.depth = depth;
        state.sampledLobe = io_result->sampledLobe;
        state.throughputR = throughput_r * ndotl / survival;
        state.throughputG = throughput_g * ndotl / survival;
        state.throughputB = throughput_b * ndotl / survival;
        state.pdf = io_result->lightSamplePdf > 0.0 ? io_result->lightSamplePdf : 1.0;
        state.ray = RuntimeRay3D_MakeOffset(current_hit.position,
                                            current_hit.normal,
                                            light_dir,
                                            kRuntimeDisneyV2Transport3DEpsilon);

        io_result->secondaryRayCount += 1;
        io_result->recursiveLoopRayCount += 1;
        if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                                   &state.ray,
                                                   kRuntimeDisneyV2Transport3DEpsilon,
                                                   kRuntimeDisneyV2Transport3DMaxDistance,
                                                   &trace)) {
            (void)runtime_disney_v2_transport_3d_append_state(
                io_result,
                &state,
                NULL,
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
            RuntimeMaterialPayload3D payload = {0};
            RuntimePrincipledBSDF3D principled = {0};
            double response_r = 1.0;
            double response_g = 1.0;
            double response_b = 1.0;

            state.hit = true;
            state.hitInfo = trace.geometryHitInfo;
            io_result->secondaryHitCount += 1;
            io_result->recursiveLoopGeometryHitCount += 1;
            if (RuntimeMaterialPayload3D_ResolveFromHit(&trace.geometryHitInfo, &payload) &&
                payload.valid) {
                principled = RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
                runtime_disney_v2_transport_3d_secondary_response(&principled,
                                                                  &response_r,
                                                                  &response_g,
                                                                  &response_b);
            }
            state.throughputR *= response_r;
            state.throughputG *= response_g;
            state.throughputB *= response_b;
            io_result->recursiveLoopPrincipled[
                io_result->recursiveLoopVertexCount] = principled;
        }

        if (trace.emitterWins) {
            const double emitter_radiance = trace.emitterHitInfo.radiance;
            const int vertex_index = depth - 1;
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
                NULL,
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER,
                contribution_r,
                contribution_g,
                contribution_b);
            io_result->recursiveLoopTerminationReason =
                RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER;
            contributed = true;
            break;
        }

        (void)runtime_disney_v2_transport_3d_append_state(
            io_result,
            &state,
            NULL,
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

        current_hit = trace.geometryHitInfo;
        throughput_r = state.throughputR;
        throughput_g = state.throughputG;
        throughput_b = state.throughputB;
        io_result->pathDepth = depth;
        depth += 1;
    }

    return contributed;
}
