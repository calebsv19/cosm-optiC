#include "render/runtime_disney_v2_internal_3d.h"

void runtime_disney_v2_3d_apply_transmittance(
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
    io_result->mirrorBaseRadianceBeforeAttenuation *= transmittance->luma;
    io_result->mirrorBaseRadianceAfterAttenuation *= transmittance->luma;
    io_result->specularReflectionRadianceR *= transmittance->r;
    io_result->specularReflectionRadianceG *= transmittance->g;
    io_result->specularReflectionRadianceB *= transmittance->b;
    io_result->specularReflectionRecursiveRadianceR *= transmittance->r;
    io_result->specularReflectionRecursiveRadianceG *= transmittance->g;
    io_result->specularReflectionRecursiveRadianceB *= transmittance->b;
    io_result->specularReflectionRoughContributionR *= transmittance->r;
    io_result->specularReflectionRoughContributionG *= transmittance->g;
    io_result->specularReflectionRoughContributionB *= transmittance->b;
    io_result->emissionRadianceR *= transmittance->r;
    io_result->emissionRadianceG *= transmittance->g;
    io_result->emissionRadianceB *= transmittance->b;
    io_result->emissiveAreaRadianceR *= transmittance->r;
    io_result->emissiveAreaRadianceG *= transmittance->g;
    io_result->emissiveAreaRadianceB *= transmittance->b;
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
        io_result->specularReflectionRecursiveContributionR[i] *= transmittance->r;
        io_result->specularReflectionRecursiveContributionG[i] *= transmittance->g;
        io_result->specularReflectionRecursiveContributionB[i] *= transmittance->b;
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

void runtime_disney_v2_3d_refresh_peaks(RuntimeDisneyV2_3DResult* result) {
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
    result->specularReflectionRadiance =
        runtime_disney_v2_3d_peak(result->specularReflectionRadianceR,
                                  result->specularReflectionRadianceG,
                                  result->specularReflectionRadianceB);
    result->specularReflectionRecursiveRadiance =
        runtime_disney_v2_3d_peak(result->specularReflectionRecursiveRadianceR,
                                  result->specularReflectionRecursiveRadianceG,
                                  result->specularReflectionRecursiveRadianceB);
    result->specularReflectionRoughContribution =
        runtime_disney_v2_3d_peak(result->specularReflectionRoughContributionR,
                                  result->specularReflectionRoughContributionG,
                                  result->specularReflectionRoughContributionB);
    result->emissionRadiance = runtime_disney_v2_3d_peak(result->emissionRadianceR,
                                                         result->emissionRadianceG,
                                                         result->emissionRadianceB);
    result->emissiveAreaRadiance =
        runtime_disney_v2_3d_peak(result->emissiveAreaRadianceR,
                                  result->emissiveAreaRadianceG,
                                  result->emissiveAreaRadianceB);
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
