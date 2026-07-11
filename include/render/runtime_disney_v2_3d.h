#ifndef RENDER_RUNTIME_DISNEY_V2_3D_H
#define RENDER_RUNTIME_DISNEY_V2_3D_H

#include <stdbool.h>

#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_path_depth_policy_3d.h"
#include "render/runtime_principled_bsdf_3d.h"

typedef enum {
    RUNTIME_DISNEY_V2_3D_LOBE_NONE = 0,
    RUNTIME_DISNEY_V2_3D_LOBE_DIFFUSE = 1,
    RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR = 2,
    RUNTIME_DISNEY_V2_3D_LOBE_EMISSION = 3,
    RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION = 4
} RuntimeDisneyV2_3DDominantLobe;

typedef struct {
    bool valid;
    bool hit;
    bool emitterHit;
    bool emitterWins;
    int depth;
    RuntimeDisneyV2_3DDominantLobe sampledLobe;
    Ray3D ray;
    HitInfo3D hitInfo;
    RuntimeLightEmitterHit3DResult emitterHitInfo;
    double throughputR;
    double throughputG;
    double throughputB;
    double pdf;
} RuntimeDisneyV2_3DPathState;

#define RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY 8

typedef enum {
    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NONE = 0,
    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NO_HIT = 1,
    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_EMITTER = 2,
    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_MAX_DEPTH = 3,
    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_ROULETTE = 4,
    RUNTIME_DISNEY_V2_3D_LOOP_TERMINATION_NEGLIGIBLE_THROUGHPUT = 5
} RuntimeDisneyV2_3DLoopTerminationReason;

typedef enum {
    RUNTIME_DISNEY_V2_3D_EMITTER_NONE = 0,
    RUNTIME_DISNEY_V2_3D_EMITTER_FINITE_LIGHT = 1,
    RUNTIME_DISNEY_V2_3D_EMITTER_EMISSIVE_MATERIAL = 2
} RuntimeDisneyV2_3DEmitterKind;

typedef struct {
    double lightPdf;
    double bsdfPdf;
    double weightLight;
    double weightBsdf;
} RuntimeDisneyV2_3DMisBranch;

typedef struct {
    bool hit;
    bool visible;
    bool payloadResolved;
    bool tracePixelContextResolved;
    int tracePixelX;
    int tracePixelY;
    int tracePixelWidth;
    int tracePixelHeight;
    Ray3D primaryRay;
    HitInfo3D hitInfo;
    RuntimeMaterialPayload3D payload;
    RuntimePrincipledBSDF3D principled;
    double diffuseProbability;
    double specularProbability;
    double transmissionProbability;
    double ndotl;
    double ndotv;
    double fresnelWeight;
    double diffuseBsdfCos;
    double specularBsdfCos;
    double specularHalfPdf;
    double directRadiance;
    double directRadianceR;
    double directRadianceG;
    double directRadianceB;
    double diffuseRadiance;
    double diffuseRadianceR;
    double diffuseRadianceG;
    double diffuseRadianceB;
    double specularRadiance;
    double specularRadianceR;
    double specularRadianceG;
    double specularRadianceB;
    double mirrorDominance;
    double mirrorBaseAttenuation;
    double mirrorBaseRadianceBeforeAttenuation;
    double mirrorBaseRadianceAfterAttenuation;
    double specularReflectionRadiance;
    double specularReflectionRadianceR;
    double specularReflectionRadianceG;
    double specularReflectionRadianceB;
    int specularReflectionRayCount;
    int specularReflectionHitCount;
    int specularReflectionGeometryHitCount;
    int specularReflectionEmitterHitCount;
    int specularReflectionNoHitCount;
    int specularReflectionContributingHitCount;
    double specularReflectionRecursiveRadiance;
    double specularReflectionRecursiveRadianceR;
    double specularReflectionRecursiveRadianceG;
    double specularReflectionRecursiveRadianceB;
    double specularReflectionRoughContribution;
    double specularReflectionRoughContributionR;
    double specularReflectionRoughContributionG;
    double specularReflectionRoughContributionB;
    double specularReflectionRoughness;
    int specularReflectionRecursiveRayCount;
    int specularReflectionRecursiveVertexCount;
    int specularReflectionRecursiveGeometryHitCount;
    int specularReflectionRecursiveEmitterHitCount;
    int specularReflectionRecursiveContributingHitCount;
    int specularReflectionRecursivePolicyTerminationCount;
    int specularReflectionRecursiveRouletteTerminationCount;
    int specularReflectionRecursiveNoHitTerminationCount;
    int specularReflectionRoughSampleCount;
    int specularReflectionRoughHitCount;
    int specularReflectionRoughNoHitCount;
    int specularReflectionRoughContributingSampleCount;
    double emissionRadiance;
    double emissionRadianceR;
    double emissionRadianceG;
    double emissionRadianceB;
    double emissiveAreaRadiance;
    double emissiveAreaRadianceR;
    double emissiveAreaRadianceG;
    double emissiveAreaRadianceB;
    int emissiveAreaSampledTriangleCount;
    int emissiveAreaContributingTriangleCount;
    int emissiveAreaLightSampleCount;
    int emissiveAreaCandidateCount;
    int emissiveAreaSelectedCandidateCount;
    int emissiveAreaVisibilityRayCount;
    int emissiveAreaPrimarySampleCount;
    int emissiveAreaRecursiveSampleCount;
    int emissiveAreaRecursivePolicySkipCount;
    int emissiveAreaRecursiveCandidateCapSkipCount;
    int emissiveAreaRecursiveTriangleCapSkipCount;
    int emissiveAreaRecursiveCandidateCap;
    int emissiveAreaRecursiveTriangleCap;
    int emissiveAreaFullScanFallbackCount;
    double transmissionRadiance;
    double transmissionRadianceR;
    double transmissionRadianceG;
    double transmissionRadianceB;
    double primaryTransmissionRadiance;
    double primaryTransmissionRadianceR;
    double primaryTransmissionRadianceG;
    double primaryTransmissionRadianceB;
    double primaryTransmissionSurfaceWeight;
    double primaryTransmissionBlendWeight;
    double primaryTransmissionFrontDiffuseWeight;
    double primaryTransmissionFrontSpecularWeight;
    double primaryTransmissionCameraThroughputR;
    double primaryTransmissionCameraThroughputG;
    double primaryTransmissionCameraThroughputB;
    double primaryTransmissionReceiverRadiance;
    double primaryTransmissionReceiverRadianceR;
    double primaryTransmissionReceiverRadianceG;
    double primaryTransmissionReceiverRadianceB;
    double primaryTransmissionTransparentLayerRadiance;
    double primaryTransmissionTransparentLayerRadianceR;
    double primaryTransmissionTransparentLayerRadianceG;
    double primaryTransmissionTransparentLayerRadianceB;
    double primaryTransmissionInteriorReturnRadiance;
    double primaryTransmissionInteriorReturnRadianceR;
    double primaryTransmissionInteriorReturnRadianceG;
    double primaryTransmissionInteriorReturnRadianceB;
    int primaryTransmissionSampleCount;
    int primaryTransmissionContributingSampleCount;
    int primaryTransmissionReceiverSampleCount;
    int primaryTransmissionReceiverShadeCount;
    int primaryTransmissionTransparentLayerShadeCount;
    int primaryTransmissionInteriorReturnSampleCount;
    int primaryTransmissionInteriorReturnSurfaceCount;
    int primaryTransmissionTransparentSurfaceCount;
    int primaryTransmissionThinWalledSurfaceCount;
    int primaryTransmissionSolidSurfaceCount;
    int primaryTransmissionPhysicalSurfaceCount;
    int primaryTransmissionAlphaOnlySurfaceCount;
    int primaryTransmissionMediumStackDepth;
    int primaryTransmissionMaxMediumStackDepth;
    int primaryTransmissionMediumEntryCount;
    int primaryTransmissionMediumExitCount;
    int primaryTransmissionMediumMismatchCount;
    int primaryTransmissionDepthLimitCount;
    double stochasticDirectRadiance;
    double stochasticDirectRadianceR;
    double stochasticDirectRadianceG;
    double stochasticDirectRadianceB;
    double stochasticBsdfRadiance;
    double stochasticBsdfRadianceR;
    double stochasticBsdfRadianceG;
    double stochasticBsdfRadianceB;
    double recursiveBsdfRadiance;
    double recursiveBsdfRadianceR;
    double recursiveBsdfRadianceG;
    double recursiveBsdfRadianceB;
    double radiance;
    double radianceR;
    double radianceG;
    double radianceB;
    RuntimeDisneyV2_3DPathState pathState;
    RuntimeDisneyV2_3DPathState primaryTransmissionPathState;
    RuntimeDisneyV2_3DPathState recursivePathState;
    bool secondaryPayloadResolved;
    RuntimeMaterialPayload3D secondaryPayload;
    RuntimePrincipledBSDF3D secondaryPrincipled;
    double secondaryMaterialResponseR;
    double secondaryMaterialResponseG;
    double secondaryMaterialResponseB;
    double secondaryVertexThroughputR;
    double secondaryVertexThroughputG;
    double secondaryVertexThroughputB;
    RuntimeDisneyV2_3DPathState recursiveLoopStates[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    RuntimePrincipledBSDF3D recursiveLoopPrincipled[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    RuntimeDisneyV2_3DLoopTerminationReason recursiveLoopTerminationReasons[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double recursiveLoopContributionR[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double recursiveLoopContributionG[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double recursiveLoopContributionB[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    RuntimeDisneyV2_3DPathState specularReflectionRecursiveStates[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    RuntimePrincipledBSDF3D specularReflectionRecursivePrincipled[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    RuntimeDisneyV2_3DLoopTerminationReason specularReflectionRecursiveTerminationReasons[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double specularReflectionRecursiveContributionR[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double specularReflectionRecursiveContributionG[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double specularReflectionRecursiveContributionB[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double misVertexLightPdf[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double misVertexBsdfPdf[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double misVertexWeightLight[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double misVertexWeightBsdf[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    RuntimeDisneyV2_3DMisBranch misVertexFiniteLight[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    RuntimeDisneyV2_3DMisBranch misVertexEmissiveArea[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double lightSampleContributionR[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double lightSampleContributionG[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double lightSampleContributionB[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double bsdfSampleContributionR[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double bsdfSampleContributionG[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double bsdfSampleContributionB[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    RuntimeDisneyV2_3DEmitterKind misVertexEmitterKind[
        RUNTIME_DISNEY_V2_3D_RECURSIVE_LOOP_STATE_CAPACITY];
    double lightSampleContribution;
    double lightSampleContributionTotalR;
    double lightSampleContributionTotalG;
    double lightSampleContributionTotalB;
    double bsdfSampleContribution;
    double bsdfSampleContributionTotalR;
    double bsdfSampleContributionTotalG;
    double bsdfSampleContributionTotalB;
    double radianceWithoutLightSamples;
    double radianceWithoutLightSamplesR;
    double radianceWithoutLightSamplesG;
    double radianceWithoutLightSamplesB;
    double radianceWithoutBsdfSamples;
    double radianceWithoutBsdfSamplesR;
    double radianceWithoutBsdfSamplesG;
    double radianceWithoutBsdfSamplesB;
    RuntimeDisneyV2_3DLoopTerminationReason recursiveLoopTerminationReason;
    int recursiveLoopVertexCount;
    int misVertexCount;
    int finiteLightMisVertexCount;
    int emissiveAreaMisVertexCount;
    int recursiveLoopRayCount;
    int recursiveLoopGeometryHitCount;
    int recursiveLoopEmitterHitCount;
    int recursiveLoopContributingHitCount;
    int recursiveLoopPolicyTerminationCount;
    int recursiveLoopRouletteTerminationCount;
    int recursiveLoopNoHitTerminationCount;
    int recursiveLoopNegligibleTerminationCount;
    RuntimePathDepthPolicy3D pathPolicy;
    RuntimeDisneyV2_3DDominantLobe sampledLobe;
    double lightSamplePdf;
    double bsdfSamplePdf;
    double misWeightLight;
    double misWeightBsdf;
    double misHeuristicPower;
    RuntimeDisneyV2_3DMisBranch finiteLightMis;
    RuntimeDisneyV2_3DMisBranch emissiveAreaMis;
    int misPowerHeuristicCount;
    double rouletteThroughputLuma;
    double rouletteSample;
    double rouletteSurvivalProbability;
    int sampledLobeMaxDepth;
    int pathDepth;
    int directSampleCount;
    int directVisibilityOutcomeNoTraceCount;
    int directVisibilityOutcomeClearVisibleCount;
    int directVisibilityOutcomeClearBlockedCount;
    int directVisibilityOutcomeStablePartialCount;
    int directVisibilityOutcomeMixedPartialCount;
    int bsdfSampleCount;
    int primaryTransmissionRayCount;
    int primaryTransmissionHitCount;
    int secondaryRayCount;
    int secondaryHitCount;
    int secondaryContributingHitCount;
    int lightSampleContributionCount;
    int bsdfSampleContributionCount;
    int finiteLightEmitterHitCount;
    int emissiveMaterialHitCount;
    bool pathPolicyResolved;
    bool pathDepthLimitReached;
    bool primaryTransmissionContinued;
    bool rouletteEvaluated;
    bool rouletteTerminated;
} RuntimeDisneyV2_3DResult;

typedef struct {
    bool valid;
    bool hit;
    bool payloadResolved;
    bool hasDirectSignal;
    bool hasDiffuseSignal;
    bool hasSpecularSignal;
    bool hasEmissionSignal;
    bool hasTransmissionSignal;
    bool routeProofReady;
    RuntimeDisneyV2_3DDominantLobe dominantLobe;
    double lobeProbabilitySum;
    double radianceEnergy;
    double specularToDiffuseRatio;
    double transmissionWeight;
    double emissionStrength;
} RuntimeDisneyV2_3DDiagnostics;

bool RuntimeDisneyV2_3D_BuildDiagnostics(const RuntimeDisneyV2_3DResult* result,
                                         RuntimeDisneyV2_3DDiagnostics* out_diagnostics);

bool RuntimeDisneyV2_3D_ShadeHit(const RuntimeScene3D* scene,
                                 const HitInfo3D* hit,
                                 const RuntimeNative3DSamplingContext* sampling,
                                 RuntimeDisneyV2_3DResult* out_result);

bool RuntimeDisneyV2_3D_ShadeHitWithTraceContext(const RuntimeScene3D* scene,
                                                 const HitInfo3D* hit,
                                                 const RuntimeNative3DSamplingContext* sampling,
                                                 int pixel_x,
                                                 int pixel_y,
                                                 int width,
                                                 int height,
                                                 RuntimeDisneyV2_3DResult* out_result);

bool RuntimeDisneyV2_3D_ShadePrimaryHit(const RuntimeScene3D* scene,
                                        const RuntimePrimaryHit3DResult* primary_hit,
                                        const RuntimeNative3DSamplingContext* sampling,
                                        RuntimeDisneyV2_3DResult* out_result);

bool RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDisneyV2_3DResult* out_result);

bool RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    int pixel_x,
    int pixel_y,
    int width,
    int height,
    RuntimeDisneyV2_3DResult* out_result);

bool RuntimeDisneyV2_3D_ShadePixel(const RuntimeScene3D* scene,
                                   const RuntimeCameraProjector3D* projector,
                                   double pixel_x,
                                   double pixel_y,
                                   const RuntimeNative3DSamplingContext* sampling,
                                   RuntimeDisneyV2_3DResult* out_result);

#endif
