#include "render/runtime_disney_v2_transport_internal_3d.h"

#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
                                            HitInfo3D_OffsetNormal(&current_hit),
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
                                   source_hit ? HitInfo3D_OffsetNormal(source_hit)
                                              : vec3(0.0, 1.0, 0.0),
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
    (void)RuntimeDisneyV2_3D_ApplyReflectedTransmissionContinuation(scene,
                                                                    &selected->hitInfo,
                                                                    selected->ray,
                                                                    sampling,
                                                                    parent_r,
                                                                    parent_g,
                                                                    parent_b,
                                                                    io_result);
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
