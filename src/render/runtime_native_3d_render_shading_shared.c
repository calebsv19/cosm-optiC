#include "render/runtime_native_3d_render_shading_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "render/runtime_caustic_photon_direct_consumer_3d.h"
#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/runtime_volume_3d_integrate.h"

RuntimeNative3DPrimaryTrace runtime_native_3d_render_trace_primary(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y) {
    RuntimeNative3DPrimaryTrace trace = {0};
    RuntimeLightEmitterHit3DResult emitter_hit = {0};

    if (!scene || !projector) return trace;

    RuntimeDirectLight3D_TracePrimaryHit(scene,
                                         projector,
                                         pixel_x,
                                         pixel_y,
                                         &trace.primary);
    if (trace.primary.hit) {
        trace.payloadResolved =
            RuntimeMaterialPayload3D_ResolveFromHit(&trace.primary.hitInfo, &trace.payload);
        if (trace.payloadResolved) {
            (void)RuntimeMaterialPayload3D_ApplyShadingNormal(
                &trace.payload, &trace.primary.hitInfo);
        }
    }
    if (RuntimeLightEmitter3D_IntersectRay(scene,
                                           &trace.primary.primaryRay,
                                           projector->nearPlane,
                                           HUGE_VAL,
                                           &emitter_hit) &&
        (!trace.primary.hit || emitter_hit.t < trace.primary.hitInfo.t)) {
        const RuntimeVisibility3DTransmittance transmittance =
            RuntimeVolume3D_TransmittanceAlongRayRGB(&scene->volume,
                                                     &trace.primary.primaryRay,
                                                     projector->nearPlane,
                                                     emitter_hit.t);
        emitter_hit.radiance *= transmittance.luma;
        trace.emitterHit = emitter_hit;
        trace.emitterWins = true;
    }

    return trace;
}

void runtime_native_3d_render_record_disney_v2_emissive_area_stats(
    RuntimeNative3DRenderStats* stats,
    const RuntimeDisneyV2_3DResult* result) {
    if (!stats || !result) return;
    if (result->emissiveAreaCandidateCount > stats->emissiveAreaCandidateCount) {
        stats->emissiveAreaCandidateCount = result->emissiveAreaCandidateCount;
    }
    stats->emissiveAreaSelectedCandidateCount +=
        result->emissiveAreaSelectedCandidateCount;
    stats->emissiveAreaVisibilityRayCount += result->emissiveAreaVisibilityRayCount;
    stats->emissiveAreaPrimarySampleCount += result->emissiveAreaPrimarySampleCount;
    stats->emissiveAreaRecursiveSampleCount += result->emissiveAreaRecursiveSampleCount;
    stats->emissiveAreaRecursivePolicySkipCount +=
        result->emissiveAreaRecursivePolicySkipCount;
    stats->emissiveAreaRecursiveCandidateCapSkipCount +=
        result->emissiveAreaRecursiveCandidateCapSkipCount;
    stats->emissiveAreaRecursiveTriangleCapSkipCount +=
        result->emissiveAreaRecursiveTriangleCapSkipCount;
    if (result->emissiveAreaRecursiveCandidateCap >
        stats->emissiveAreaRecursiveCandidateCap) {
        stats->emissiveAreaRecursiveCandidateCap =
            result->emissiveAreaRecursiveCandidateCap;
    }
    if (result->emissiveAreaRecursiveTriangleCap >
        stats->emissiveAreaRecursiveTriangleCap) {
        stats->emissiveAreaRecursiveTriangleCap =
            result->emissiveAreaRecursiveTriangleCap;
    }
    stats->emissiveAreaFullScanFallbackCount += result->emissiveAreaFullScanFallbackCount;
}

void runtime_native_3d_render_record_disney_v2_mirror_stats(
    RuntimeNative3DRenderStats* stats,
    const RuntimeDisneyV2_3DResult* result,
    int pixel_x,
    int pixel_y,
    double ambient_before_r,
    double ambient_before_g,
    double ambient_before_b,
    double ambient_after_r,
    double ambient_after_g,
    double ambient_after_b) {
    int i = 0;
    if (!stats || !result) return;

    if (result->mirrorDominance >= 0.75) {
    if (result->pathPolicy.requestedSpecularDepth >
        stats->mirrorFidelityRequestedSpecularDepth) {
        stats->mirrorFidelityRequestedSpecularDepth =
            result->pathPolicy.requestedSpecularDepth;
    }
    if (result->pathPolicy.specularDepth > stats->mirrorFidelityEffectiveSpecularDepth) {
        stats->mirrorFidelityEffectiveSpecularDepth = result->pathPolicy.specularDepth;
    }
    if (result->specularReflectionRoughRequestedSampleCount >
        stats->mirrorFidelityRequestedRoughSampleCount) {
        stats->mirrorFidelityRequestedRoughSampleCount =
            result->specularReflectionRoughRequestedSampleCount;
    }
    if (result->specularReflectionRoughEffectiveSampleCount >
        stats->mirrorFidelityEffectiveRoughSampleCount) {
        stats->mirrorFidelityEffectiveRoughSampleCount =
            result->specularReflectionRoughEffectiveSampleCount;
    }
    if ((int)result->specularReflectionRoughSampleReductionReason >
        stats->mirrorFidelityRoughSampleReductionReason) {
        stats->mirrorFidelityRoughSampleReductionReason =
            (int)result->specularReflectionRoughSampleReductionReason;
    }
    stats->mirrorFidelityDepthRayCount[1] += result->specularReflectionRayCount;
    stats->mirrorFidelityDepthGeometryHitCount[1] +=
        result->specularReflectionGeometryHitCount;
    stats->mirrorFidelityDepthEmitterHitCount[1] +=
        result->specularReflectionEmitterHitCount;
    stats->mirrorFidelityDepthContributingHitCount[1] +=
        result->specularReflectionContributingHitCount;
    stats->mirrorFidelityDepthNoHitTerminationCount[1] +=
        result->specularReflectionNoHitCount;
    if (result->specularReflectionLocalPathVertexEvaluated) {
        stats->mirrorFidelityLocalPathVertexEvaluatedCount += 1;
        stats->mirrorFidelityLocalPathVertexRadianceR +=
            result->specularReflectionLocalPathVertexRadianceR;
        stats->mirrorFidelityLocalPathVertexRadianceG +=
            result->specularReflectionLocalPathVertexRadianceG;
        stats->mirrorFidelityLocalPathVertexRadianceB +=
            result->specularReflectionLocalPathVertexRadianceB;
    }
    stats->mirrorFidelityRecursiveRadianceR +=
        result->specularReflectionRecursiveRadianceR;
    stats->mirrorFidelityRecursiveRadianceG +=
        result->specularReflectionRecursiveRadianceG;
    stats->mirrorFidelityRecursiveRadianceB +=
        result->specularReflectionRecursiveRadianceB;
    stats->mirrorFidelityEnvironmentMissContributionCount +=
        result->specularReflectionEnvironmentMissContributionCount;
    stats->mirrorFidelityEnvironmentMissRadianceR +=
        result->specularReflectionEnvironmentRadianceR;
    stats->mirrorFidelityEnvironmentMissRadianceG +=
        result->specularReflectionEnvironmentRadianceG;
    stats->mirrorFidelityEnvironmentMissRadianceB +=
        result->specularReflectionEnvironmentRadianceB;
    stats->mirrorFidelityRecursiveEnvironmentMissContributionCount +=
        result->specularReflectionRecursiveEnvironmentMissContributionCount;
    stats->mirrorFidelityRecursiveEnvironmentMissRadianceR +=
        result->specularReflectionRecursiveEnvironmentRadianceR;
    stats->mirrorFidelityRecursiveEnvironmentMissRadianceG +=
        result->specularReflectionRecursiveEnvironmentRadianceG;
    stats->mirrorFidelityRecursiveEnvironmentMissRadianceB +=
        result->specularReflectionRecursiveEnvironmentRadianceB;
    for (i = 0; i < RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY; ++i) {
        const RuntimeDisneyV2_3DPathState* state =
            &result->specularReflectionRecursiveStates[i];
        const RuntimeDisneyV2_3DLoopTerminationReason reason =
            result->specularReflectionRecursiveTerminationReasons[i];
        const int depth = state->depth;
        if (!state->valid || depth < 2 ||
            depth >= RUNTIME_NATIVE_3D_MIRROR_FIDELITY_DEPTH_CAPACITY) {
            continue;
        }
        stats->mirrorFidelityDepthRayCount[depth] += 1;
        if (state->hit && state->emitterWins) {
            stats->mirrorFidelityDepthEmitterHitCount[depth] += 1;
        } else if (state->hit) {
            stats->mirrorFidelityDepthGeometryHitCount[depth] += 1;
        }
        if (result->specularReflectionRecursiveContributionR[i] > 1e-9 ||
            result->specularReflectionRecursiveContributionG[i] > 1e-9 ||
            result->specularReflectionRecursiveContributionB[i] > 1e-9) {
            stats->mirrorFidelityDepthContributingHitCount[depth] += 1;
        }
        if (reason == RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_MAX_DEPTH) {
            stats->mirrorFidelityDepthPolicyTerminationCount[depth] += 1;
        } else if (reason == RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_ROULETTE) {
            stats->mirrorFidelityDepthRouletteTerminationCount[depth] += 1;
        } else if (reason == RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NO_HIT) {
            stats->mirrorFidelityDepthNoHitTerminationCount[depth] += 1;
        }
    }
    if (result->specularReflectionProbeValid) {
        const HitInfo3D* probe = &result->specularReflectionProbeHitInfo;
        const double local_min = fmin(result->specularReflectionLocalPathVertexRadianceR,
                                      fmin(result->specularReflectionLocalPathVertexRadianceG,
                                           result->specularReflectionLocalPathVertexRadianceB));
        const double local_max = fmax(result->specularReflectionLocalPathVertexRadianceR,
                                      fmax(result->specularReflectionLocalPathVertexRadianceG,
                                           result->specularReflectionLocalPathVertexRadianceB));
        const double local_chroma = local_max - local_min;
        const bool same_normal_class =
            result->specularReflectionProbeHasVertexNormals ==
            stats->mirrorFidelityProbeHasVertexNormals;
        const bool lower_identity =
            probe->triangleIndex < stats->mirrorFidelityProbeTriangleId ||
            (probe->triangleIndex == stats->mirrorFidelityProbeTriangleId &&
             strcmp(probe->source.objectId, stats->mirrorFidelityProbeObjectId) < 0);
        const bool replace_probe =
            !stats->mirrorFidelityProbeValid ||
            (result->specularReflectionProbeHasVertexNormals &&
             !stats->mirrorFidelityProbeHasVertexNormals) ||
            (same_normal_class && lower_identity);
        if (result->specularReflectionProbeHasVertexNormals) {
            stats->mirrorFidelityVertexInterpolatedHitCount += 1;
        } else {
            stats->mirrorFidelityFlatFallbackHitCount += 1;
        }
        if (replace_probe) {
            stats->mirrorFidelityProbeValid = true;
            stats->mirrorFidelityProbeHasVertexNormals =
                result->specularReflectionProbeHasVertexNormals;
            stats->mirrorFidelityProbeTriangleId = probe->triangleIndex;
            stats->mirrorFidelityProbeMaterialId =
                result->specularReflectionProbePayload.materialId;
            stats->mirrorFidelityProbePathDepth =
                result->specularReflectionProbePathDepth;
            stats->mirrorFidelityProbeLocalRadianceR =
                result->specularReflectionLocalPathVertexRadianceR;
            stats->mirrorFidelityProbeLocalRadianceG =
                result->specularReflectionLocalPathVertexRadianceG;
            stats->mirrorFidelityProbeLocalRadianceB =
                result->specularReflectionLocalPathVertexRadianceB;
            stats->mirrorFidelityProbeRecursiveRadianceR =
                result->specularReflectionRecursiveRadianceR;
            stats->mirrorFidelityProbeRecursiveRadianceG =
                result->specularReflectionRecursiveRadianceG;
            stats->mirrorFidelityProbeRecursiveRadianceB =
                result->specularReflectionRecursiveRadianceB;
            snprintf(stats->mirrorFidelityProbeObjectId,
                     sizeof(stats->mirrorFidelityProbeObjectId),
                     "%s",
                     probe->source.objectId);
        }
        {
            const bool replace_radiance_probe =
                !stats->mirrorFidelityRadianceProbeValid ||
                local_chroma > stats->mirrorFidelityRadianceProbeSelectionChroma ||
                (local_chroma == stats->mirrorFidelityRadianceProbeSelectionChroma &&
                 (pixel_y < stats->mirrorFidelityRadianceProbePixelY ||
                  (pixel_y == stats->mirrorFidelityRadianceProbePixelY &&
                   pixel_x < stats->mirrorFidelityRadianceProbePixelX)));
            if (replace_radiance_probe) {
                double classified_r = 0.0;
                double classified_g = 0.0;
                double classified_b = 0.0;
                stats->mirrorFidelityRadianceProbeValid = true;
                stats->mirrorFidelityRadianceProbeHasVertexNormals =
                    result->specularReflectionProbeHasVertexNormals;
                stats->mirrorFidelityRadianceProbePixelX = pixel_x;
                stats->mirrorFidelityRadianceProbePixelY = pixel_y;
                stats->mirrorFidelityRadianceProbeTriangleId = probe->triangleIndex;
                stats->mirrorFidelityRadianceProbeMaterialId =
                    result->specularReflectionProbePayload.materialId;
                stats->mirrorFidelityRadianceProbePathDepth =
                    result->specularReflectionProbePathDepth;
                stats->mirrorFidelityRadianceProbeSelectionChroma = local_chroma;
                stats->mirrorFidelityRadianceProbeDominance = result->mirrorDominance;
                stats->mirrorFidelityRadianceProbeBaseAttenuation =
                    result->mirrorBaseAttenuation;
                stats->mirrorFidelityRadianceProbeDirectBeforeR =
                    result->mirrorDirectRadianceBeforeAttenuationR;
                stats->mirrorFidelityRadianceProbeDirectBeforeG =
                    result->mirrorDirectRadianceBeforeAttenuationG;
                stats->mirrorFidelityRadianceProbeDirectBeforeB =
                    result->mirrorDirectRadianceBeforeAttenuationB;
                stats->mirrorFidelityRadianceProbeDirectAfterR =
                    result->mirrorDirectRadianceAfterAttenuationR;
                stats->mirrorFidelityRadianceProbeDirectAfterG =
                    result->mirrorDirectRadianceAfterAttenuationG;
                stats->mirrorFidelityRadianceProbeDirectAfterB =
                    result->mirrorDirectRadianceAfterAttenuationB;
                stats->mirrorFidelityRadianceProbeLocalDiffuseBeforeR =
                    result->mirrorLocalDiffuseRadianceBeforeAttenuationR;
                stats->mirrorFidelityRadianceProbeLocalDiffuseBeforeG =
                    result->mirrorLocalDiffuseRadianceBeforeAttenuationG;
                stats->mirrorFidelityRadianceProbeLocalDiffuseBeforeB =
                    result->mirrorLocalDiffuseRadianceBeforeAttenuationB;
                stats->mirrorFidelityRadianceProbeLocalDiffuseAfterR =
                    result->mirrorLocalDiffuseRadianceAfterAttenuationR;
                stats->mirrorFidelityRadianceProbeLocalDiffuseAfterG =
                    result->mirrorLocalDiffuseRadianceAfterAttenuationG;
                stats->mirrorFidelityRadianceProbeLocalDiffuseAfterB =
                    result->mirrorLocalDiffuseRadianceAfterAttenuationB;
                stats->mirrorFidelityRadianceProbeLocalSpecularBeforeR =
                    result->mirrorLocalSpecularRadianceBeforeAttenuationR;
                stats->mirrorFidelityRadianceProbeLocalSpecularBeforeG =
                    result->mirrorLocalSpecularRadianceBeforeAttenuationG;
                stats->mirrorFidelityRadianceProbeLocalSpecularBeforeB =
                    result->mirrorLocalSpecularRadianceBeforeAttenuationB;
                stats->mirrorFidelityRadianceProbeLocalSpecularAfterR =
                    result->mirrorLocalSpecularRadianceAfterAttenuationR;
                stats->mirrorFidelityRadianceProbeLocalSpecularAfterG =
                    result->mirrorLocalSpecularRadianceAfterAttenuationG;
                stats->mirrorFidelityRadianceProbeLocalSpecularAfterB =
                    result->mirrorLocalSpecularRadianceAfterAttenuationB;
                stats->mirrorFidelityRadianceProbeAmbientBeforeR = ambient_before_r;
                stats->mirrorFidelityRadianceProbeAmbientBeforeG = ambient_before_g;
                stats->mirrorFidelityRadianceProbeAmbientBeforeB = ambient_before_b;
                stats->mirrorFidelityRadianceProbeAmbientAfterR = ambient_after_r;
                stats->mirrorFidelityRadianceProbeAmbientAfterG = ambient_after_g;
                stats->mirrorFidelityRadianceProbeAmbientAfterB = ambient_after_b;
                stats->mirrorFidelityRadianceProbeEmissionR = result->emissionRadianceR;
                stats->mirrorFidelityRadianceProbeEmissionG = result->emissionRadianceG;
                stats->mirrorFidelityRadianceProbeEmissionB = result->emissionRadianceB;
                stats->mirrorFidelityRadianceProbeTransmissionR =
                    result->transmissionRadianceR + result->primaryTransmissionRadianceR;
                stats->mirrorFidelityRadianceProbeTransmissionG =
                    result->transmissionRadianceG + result->primaryTransmissionRadianceG;
                stats->mirrorFidelityRadianceProbeTransmissionB =
                    result->transmissionRadianceB + result->primaryTransmissionRadianceB;
                stats->mirrorFidelityRadianceProbeStochasticDirectR =
                    result->stochasticDirectRadianceR;
                stats->mirrorFidelityRadianceProbeStochasticDirectG =
                    result->stochasticDirectRadianceG;
                stats->mirrorFidelityRadianceProbeStochasticDirectB =
                    result->stochasticDirectRadianceB;
                stats->mirrorFidelityRadianceProbeStochasticBsdfR =
                    result->stochasticBsdfRadianceR;
                stats->mirrorFidelityRadianceProbeStochasticBsdfG =
                    result->stochasticBsdfRadianceG;
                stats->mirrorFidelityRadianceProbeStochasticBsdfB =
                    result->stochasticBsdfRadianceB;
                stats->mirrorFidelityRadianceProbeRecursiveDirectR =
                    result->recursiveDirectRadianceR;
                stats->mirrorFidelityRadianceProbeRecursiveDirectG =
                    result->recursiveDirectRadianceG;
                stats->mirrorFidelityRadianceProbeRecursiveDirectB =
                    result->recursiveDirectRadianceB;
                stats->mirrorFidelityRadianceProbeRecursiveBsdfR =
                    result->recursiveBsdfRadianceR;
                stats->mirrorFidelityRadianceProbeRecursiveBsdfG =
                    result->recursiveBsdfRadianceG;
                stats->mirrorFidelityRadianceProbeRecursiveBsdfB =
                    result->recursiveBsdfRadianceB;
                stats->mirrorFidelityRadianceProbeLocalRadianceR =
                    result->specularReflectionLocalPathVertexRadianceR;
                stats->mirrorFidelityRadianceProbeLocalRadianceG =
                    result->specularReflectionLocalPathVertexRadianceG;
                stats->mirrorFidelityRadianceProbeLocalRadianceB =
                    result->specularReflectionLocalPathVertexRadianceB;
                stats->mirrorFidelityRadianceProbeRecursiveRadianceR =
                    result->specularReflectionRecursiveRadianceR;
                stats->mirrorFidelityRadianceProbeRecursiveRadianceG =
                    result->specularReflectionRecursiveRadianceG;
                stats->mirrorFidelityRadianceProbeRecursiveRadianceB =
                    result->specularReflectionRecursiveRadianceB;
                stats->mirrorFidelityRadianceProbeReflectionRadianceR =
                    result->specularReflectionRadianceR;
                stats->mirrorFidelityRadianceProbeReflectionRadianceG =
                    result->specularReflectionRadianceG;
                stats->mirrorFidelityRadianceProbeReflectionRadianceB =
                    result->specularReflectionRadianceB;
                stats->mirrorFidelityRadianceProbeComposedRadianceR = result->radianceR;
                stats->mirrorFidelityRadianceProbeComposedRadianceG = result->radianceG;
                stats->mirrorFidelityRadianceProbeComposedRadianceB = result->radianceB;
                classified_r = stats->mirrorFidelityRadianceProbeLocalDiffuseAfterR +
                               stats->mirrorFidelityRadianceProbeLocalSpecularAfterR +
                               stats->mirrorFidelityRadianceProbeReflectionRadianceR +
                               stats->mirrorFidelityRadianceProbeEmissionR +
                               stats->mirrorFidelityRadianceProbeTransmissionR +
                               stats->mirrorFidelityRadianceProbeStochasticDirectR +
                               stats->mirrorFidelityRadianceProbeStochasticBsdfR +
                               stats->mirrorFidelityRadianceProbeRecursiveDirectR +
                               stats->mirrorFidelityRadianceProbeRecursiveBsdfR + ambient_after_r;
                classified_g = stats->mirrorFidelityRadianceProbeLocalDiffuseAfterG +
                               stats->mirrorFidelityRadianceProbeLocalSpecularAfterG +
                               stats->mirrorFidelityRadianceProbeReflectionRadianceG +
                               stats->mirrorFidelityRadianceProbeEmissionG +
                               stats->mirrorFidelityRadianceProbeTransmissionG +
                               stats->mirrorFidelityRadianceProbeStochasticDirectG +
                               stats->mirrorFidelityRadianceProbeStochasticBsdfG +
                               stats->mirrorFidelityRadianceProbeRecursiveDirectG +
                               stats->mirrorFidelityRadianceProbeRecursiveBsdfG + ambient_after_g;
                classified_b = stats->mirrorFidelityRadianceProbeLocalDiffuseAfterB +
                               stats->mirrorFidelityRadianceProbeLocalSpecularAfterB +
                               stats->mirrorFidelityRadianceProbeReflectionRadianceB +
                               stats->mirrorFidelityRadianceProbeEmissionB +
                               stats->mirrorFidelityRadianceProbeTransmissionB +
                               stats->mirrorFidelityRadianceProbeStochasticDirectB +
                               stats->mirrorFidelityRadianceProbeStochasticBsdfB +
                               stats->mirrorFidelityRadianceProbeRecursiveDirectB +
                               stats->mirrorFidelityRadianceProbeRecursiveBsdfB + ambient_after_b;
                stats->mirrorFidelityRadianceProbeUnclassifiedR =
                    result->radianceR - classified_r;
                stats->mirrorFidelityRadianceProbeUnclassifiedG =
                    result->radianceG - classified_g;
                stats->mirrorFidelityRadianceProbeUnclassifiedB =
                    result->radianceB - classified_b;
                snprintf(stats->mirrorFidelityRadianceProbeObjectId,
                         sizeof(stats->mirrorFidelityRadianceProbeObjectId),
                         "%s",
                         probe->source.objectId);
            }
        }
        if (strcmp(probe->source.objectId, "smooth_subject") == 0) {
            const unsigned tone_r = (unsigned)TonemapCurveToByteWithFloor(
                (float)result->radianceR, 0u);
            const unsigned tone_g = (unsigned)TonemapCurveToByteWithFloor(
                (float)result->radianceG, 0u);
            const unsigned tone_b = (unsigned)TonemapCurveToByteWithFloor(
                (float)result->radianceB, 0u);
            const unsigned tone_min = tone_r < tone_g
                                          ? (tone_r < tone_b ? tone_r : tone_b)
                                          : (tone_g < tone_b ? tone_g : tone_b);
            const unsigned tone_max = tone_r > tone_g
                                          ? (tone_r > tone_b ? tone_r : tone_b)
                                          : (tone_g > tone_b ? tone_g : tone_b);
            const bool low_chroma = tone_max - tone_min < 25u;
            const bool replace_neutral_probe =
                low_chroma &&
                (!stats->mirrorFidelityNeutralProbeValid ||
                 (double)tone_min > stats->mirrorFidelityNeutralProbeSelectionLuma ||
                 ((double)tone_min == stats->mirrorFidelityNeutralProbeSelectionLuma &&
                  (pixel_y < stats->mirrorFidelityNeutralProbePixelY ||
                   (pixel_y == stats->mirrorFidelityNeutralProbePixelY &&
                    pixel_x < stats->mirrorFidelityNeutralProbePixelX))));
            if (replace_neutral_probe) {
                stats->mirrorFidelityNeutralProbeValid = true;
                stats->mirrorFidelityNeutralProbePixelX = pixel_x;
                stats->mirrorFidelityNeutralProbePixelY = pixel_y;
                stats->mirrorFidelityNeutralProbeTriangleId = probe->triangleIndex;
                stats->mirrorFidelityNeutralProbeMaterialId =
                    result->specularReflectionProbePayload.materialId;
                stats->mirrorFidelityNeutralProbeSelectionLuma = (double)tone_min;
                stats->mirrorFidelityNeutralProbeDominance = result->mirrorDominance;
                stats->mirrorFidelityNeutralProbeBaseAttenuation =
                    result->mirrorBaseAttenuation;
                stats->mirrorFidelityNeutralProbeDirectBeforeR =
                    result->mirrorDirectRadianceBeforeAttenuationR;
                stats->mirrorFidelityNeutralProbeDirectBeforeG =
                    result->mirrorDirectRadianceBeforeAttenuationG;
                stats->mirrorFidelityNeutralProbeDirectBeforeB =
                    result->mirrorDirectRadianceBeforeAttenuationB;
                stats->mirrorFidelityNeutralProbeDirectAfterR =
                    result->mirrorDirectRadianceAfterAttenuationR;
                stats->mirrorFidelityNeutralProbeDirectAfterG =
                    result->mirrorDirectRadianceAfterAttenuationG;
                stats->mirrorFidelityNeutralProbeDirectAfterB =
                    result->mirrorDirectRadianceAfterAttenuationB;
                stats->mirrorFidelityNeutralProbeLocalDiffuseBeforeR =
                    result->mirrorLocalDiffuseRadianceBeforeAttenuationR;
                stats->mirrorFidelityNeutralProbeLocalDiffuseBeforeG =
                    result->mirrorLocalDiffuseRadianceBeforeAttenuationG;
                stats->mirrorFidelityNeutralProbeLocalDiffuseBeforeB =
                    result->mirrorLocalDiffuseRadianceBeforeAttenuationB;
                stats->mirrorFidelityNeutralProbeLocalDiffuseAfterR =
                    result->mirrorLocalDiffuseRadianceAfterAttenuationR;
                stats->mirrorFidelityNeutralProbeLocalDiffuseAfterG =
                    result->mirrorLocalDiffuseRadianceAfterAttenuationG;
                stats->mirrorFidelityNeutralProbeLocalDiffuseAfterB =
                    result->mirrorLocalDiffuseRadianceAfterAttenuationB;
                stats->mirrorFidelityNeutralProbeLocalSpecularBeforeR =
                    result->mirrorLocalSpecularRadianceBeforeAttenuationR;
                stats->mirrorFidelityNeutralProbeLocalSpecularBeforeG =
                    result->mirrorLocalSpecularRadianceBeforeAttenuationG;
                stats->mirrorFidelityNeutralProbeLocalSpecularBeforeB =
                    result->mirrorLocalSpecularRadianceBeforeAttenuationB;
                stats->mirrorFidelityNeutralProbeLocalSpecularAfterR =
                    result->mirrorLocalSpecularRadianceAfterAttenuationR;
                stats->mirrorFidelityNeutralProbeLocalSpecularAfterG =
                    result->mirrorLocalSpecularRadianceAfterAttenuationG;
                stats->mirrorFidelityNeutralProbeLocalSpecularAfterB =
                    result->mirrorLocalSpecularRadianceAfterAttenuationB;
                stats->mirrorFidelityNeutralProbeAmbientBeforeR = ambient_before_r;
                stats->mirrorFidelityNeutralProbeAmbientBeforeG = ambient_before_g;
                stats->mirrorFidelityNeutralProbeAmbientBeforeB = ambient_before_b;
                stats->mirrorFidelityNeutralProbeAmbientAfterR = ambient_after_r;
                stats->mirrorFidelityNeutralProbeAmbientAfterG = ambient_after_g;
                stats->mirrorFidelityNeutralProbeAmbientAfterB = ambient_after_b;
                stats->mirrorFidelityNeutralProbeStochasticDirectR =
                    result->stochasticDirectRadianceR;
                stats->mirrorFidelityNeutralProbeStochasticDirectG =
                    result->stochasticDirectRadianceG;
                stats->mirrorFidelityNeutralProbeStochasticDirectB =
                    result->stochasticDirectRadianceB;
                stats->mirrorFidelityNeutralProbeStochasticBsdfR =
                    result->stochasticBsdfRadianceR;
                stats->mirrorFidelityNeutralProbeStochasticBsdfG =
                    result->stochasticBsdfRadianceG;
                stats->mirrorFidelityNeutralProbeStochasticBsdfB =
                    result->stochasticBsdfRadianceB;
                stats->mirrorFidelityNeutralProbeRecursiveDirectR =
                    result->recursiveDirectRadianceR;
                stats->mirrorFidelityNeutralProbeRecursiveDirectG =
                    result->recursiveDirectRadianceG;
                stats->mirrorFidelityNeutralProbeRecursiveDirectB =
                    result->recursiveDirectRadianceB;
                stats->mirrorFidelityNeutralProbeRecursiveBsdfR =
                    result->recursiveBsdfRadianceR;
                stats->mirrorFidelityNeutralProbeRecursiveBsdfG =
                    result->recursiveBsdfRadianceG;
                stats->mirrorFidelityNeutralProbeRecursiveBsdfB =
                    result->recursiveBsdfRadianceB;
                stats->mirrorFidelityNeutralProbeLocalRadianceR =
                    result->specularReflectionLocalPathVertexRadianceR;
                stats->mirrorFidelityNeutralProbeLocalRadianceG =
                    result->specularReflectionLocalPathVertexRadianceG;
                stats->mirrorFidelityNeutralProbeLocalRadianceB =
                    result->specularReflectionLocalPathVertexRadianceB;
                stats->mirrorFidelityNeutralProbeReflectionRadianceR =
                    result->specularReflectionRadianceR;
                stats->mirrorFidelityNeutralProbeReflectionRadianceG =
                    result->specularReflectionRadianceG;
                stats->mirrorFidelityNeutralProbeReflectionRadianceB =
                    result->specularReflectionRadianceB;
                stats->mirrorFidelityNeutralProbeComposedRadianceR = result->radianceR;
                stats->mirrorFidelityNeutralProbeComposedRadianceG = result->radianceG;
                stats->mirrorFidelityNeutralProbeComposedRadianceB = result->radianceB;
                snprintf(stats->mirrorFidelityNeutralProbeObjectId,
                         sizeof(stats->mirrorFidelityNeutralProbeObjectId),
                         "%s",
                         probe->source.objectId);
            }
        }
    }
    }

    if (result->mirrorDominance > stats->maxMirrorDominance) {
        stats->maxMirrorDominance = result->mirrorDominance;
    }
    if (result->mirrorDominance >= 0.75) {
        stats->mirrorDominantPixelCount += 1;
    }
    if (result->mirrorBaseRadianceBeforeAttenuation > 1e-9 &&
        result->mirrorBaseRadianceAfterAttenuation <
            result->mirrorBaseRadianceBeforeAttenuation * 0.75) {
        stats->mirrorBaseAttenuatedPixelCount += 1;
    }
    if (result->specularReflectionHitCount > 0) {
        stats->mirrorReflectionHitPixelCount += 1;
    }
    if (result->specularReflectionEmitterHitCount > 0) {
        stats->mirrorEmitterReflectionPixelCount += 1;
    }
    if (result->specularReflectionGeometryHitCount > 0) {
        stats->mirrorGeometryReflectionPixelCount += 1;
    }
    if (result->specularReflectionRadiance > stats->maxMirrorSpecularReflectionRadiance) {
        stats->maxMirrorSpecularReflectionRadiance = result->specularReflectionRadiance;
    }
    if (result->mirrorBaseRadianceBeforeAttenuation >
        stats->maxMirrorBaseRadianceBeforeAttenuation) {
        stats->maxMirrorBaseRadianceBeforeAttenuation =
            result->mirrorBaseRadianceBeforeAttenuation;
    }
    if (result->mirrorBaseRadianceAfterAttenuation >
        stats->maxMirrorBaseRadianceAfterAttenuation) {
        stats->maxMirrorBaseRadianceAfterAttenuation =
            result->mirrorBaseRadianceAfterAttenuation;
    }
    stats->totalMirrorSpecularReflectionRadiance += result->specularReflectionRadiance;
    stats->totalMirrorBaseRadianceBeforeAttenuation +=
        result->mirrorBaseRadianceBeforeAttenuation;
    stats->totalMirrorBaseRadianceAfterAttenuation +=
        result->mirrorBaseRadianceAfterAttenuation;
}

void runtime_native_3d_render_apply_surface_caustic_cache(
    RuntimeCausticSurfaceCache3D* surface_cache,
    const HitInfo3D* hit,
    double* io_r,
    double* io_g,
    double* io_b,
    double* io_luma,
    bool* io_visible,
    RuntimeNative3DRenderStats* stats) {
    Vec3 radiance = vec3(0.0, 0.0, 0.0);
    double luma = 0.0;

    if (!surface_cache || !hit || !io_r || !io_g || !io_b || !io_luma || !stats) {
        return;
    }
    stats->causticSurfaceCacheSampleLookupCount += 1;
    if (!RuntimeCausticSurfaceCache3D_SampleAtHit(surface_cache, hit, &radiance)) {
        return;
    }
    luma = fmax(fmax(radiance.x, radiance.y), radiance.z);
    if (!(luma > 0.0)) return;
    *io_r += radiance.x;
    *io_g += radiance.y;
    *io_b += radiance.z;
    *io_luma = fmax(fmax(*io_r, *io_g), *io_b);
    if (io_visible) *io_visible = true;
    stats->causticSurfaceCacheSampleContributingCount += 1;
    stats->totalCausticSurfaceRadianceR += radiance.x;
    stats->totalCausticSurfaceRadianceG += radiance.y;
    stats->totalCausticSurfaceRadianceB += radiance.z;
    if (luma > stats->maxCausticSurfaceCacheRadiance) {
        stats->maxCausticSurfaceCacheRadiance = luma;
    }
}

void runtime_native_3d_render_apply_transmitted_surface_caustic_cache(
    RuntimeCausticSurfaceCache3D* surface_cache,
    const RuntimeDisneyV2_3DResult* result,
    double* io_r,
    double* io_g,
    double* io_b,
    double* io_luma,
    bool* io_visible,
    RuntimeNative3DRenderStats* stats) {
    Vec3 radiance = vec3(0.0, 0.0, 0.0);
    double throughput_r = 0.0;
    double throughput_g = 0.0;
    double throughput_b = 0.0;
    double luma = 0.0;

    if (!surface_cache || !result || !io_r || !io_g || !io_b || !io_luma || !stats) {
        return;
    }
    if (!result->primaryTransmissionContinued ||
        !result->primaryTransmissionPathState.valid ||
        !result->primaryTransmissionPathState.hit) {
        return;
    }
    if (result->primaryTransmissionCausticDirectMapSampled) {
        radiance = vec3(
            result->primaryTransmissionCausticRadianceR,
            result->primaryTransmissionCausticRadianceG,
            result->primaryTransmissionCausticRadianceB);
        stats->causticSurfaceCacheSampleLookupCount +=
            result->primaryTransmissionCausticLookupCount;
        luma = fmax(fmax(radiance.x, radiance.y), radiance.z);
        if (!(luma > 0.0)) return;
        *io_r += radiance.x;
        *io_g += radiance.y;
        *io_b += radiance.z;
        *io_luma = fmax(fmax(*io_r, *io_g), *io_b);
        if (io_visible) *io_visible = true;
        stats->causticSurfaceCacheSampleContributingCount +=
            result->primaryTransmissionCausticContributingSampleCount;
        stats->totalCausticSurfaceRadianceR += radiance.x;
        stats->totalCausticSurfaceRadianceG += radiance.y;
        stats->totalCausticSurfaceRadianceB += radiance.z;
        if (luma > stats->maxCausticSurfaceCacheRadiance) {
            stats->maxCausticSurfaceCacheRadiance = luma;
        }
        return;
    }
    stats->causticSurfaceCacheSampleLookupCount += 1;
    if (RuntimeCausticPhotonDirectConsumer3D_Active()) {
        if (!RuntimeCausticPhotonDirectConsumer3D_SampleSurface(
                &result->primaryTransmissionPathState.hitInfo,
                &radiance,
                NULL)) {
            return;
        }
    } else if (!RuntimeCausticSurfaceCache3D_SampleAtHit(
                   surface_cache,
                   &result->primaryTransmissionPathState.hitInfo,
                   &radiance)) {
        return;
    }
    throughput_r = fmax(result->primaryTransmissionCameraThroughputR,
                        result->primaryTransmissionPathState.throughputR);
    throughput_g = fmax(result->primaryTransmissionCameraThroughputG,
                        result->primaryTransmissionPathState.throughputG);
    throughput_b = fmax(result->primaryTransmissionCameraThroughputB,
                        result->primaryTransmissionPathState.throughputB);
    radiance.x *= fmin(fmax(throughput_r, 0.0), 1.0);
    radiance.y *= fmin(fmax(throughput_g, 0.0), 1.0);
    radiance.z *= fmin(fmax(throughput_b, 0.0), 1.0);
    luma = fmax(fmax(radiance.x, radiance.y), radiance.z);
    if (!(luma > 0.0)) return;
    *io_r += radiance.x;
    *io_g += radiance.y;
    *io_b += radiance.z;
    *io_luma = fmax(fmax(*io_r, *io_g), *io_b);
    if (io_visible) *io_visible = true;
    stats->causticSurfaceCacheSampleContributingCount += 1;
    stats->totalCausticSurfaceRadianceR += radiance.x;
    stats->totalCausticSurfaceRadianceG += radiance.y;
    stats->totalCausticSurfaceRadianceB += radiance.z;
    if (luma > stats->maxCausticSurfaceCacheRadiance) {
        stats->maxCausticSurfaceCacheRadiance = luma;
    }
}
