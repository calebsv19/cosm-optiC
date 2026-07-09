#ifndef RENDER_RUNTIME_RENDER_TRACE_COST_LEDGER_3D_H
#define RENDER_RUNTIME_RENDER_TRACE_COST_LEDGER_3D_H

#include <stdbool.h>
#include <stdint.h>

#include "render/runtime_ray_3d.h"

typedef enum RuntimeRenderTraceCostRayClass3D {
    RUNTIME_RENDER_TRACE_COST_RAY_PRIMARY = 0,
    RUNTIME_RENDER_TRACE_COST_RAY_DIRECT_LIGHT_VISIBILITY = 1,
    RUNTIME_RENDER_TRACE_COST_RAY_TRANSMISSION = 2,
    RUNTIME_RENDER_TRACE_COST_RAY_REFLECTION_SPECULAR = 3,
    RUNTIME_RENDER_TRACE_COST_RAY_DIFFUSE_SECONDARY = 4,
    RUNTIME_RENDER_TRACE_COST_RAY_DISNEY_RECURSIVE = 5,
    RUNTIME_RENDER_TRACE_COST_RAY_CAUSTIC = 6,
    RUNTIME_RENDER_TRACE_COST_RAY_EMISSIVE_AREA = 7,
    RUNTIME_RENDER_TRACE_COST_RAY_UNKNOWN = 8,
    RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT = 9
} RuntimeRenderTraceCostRayClass3D;

typedef enum RuntimeRenderTraceCostMaterialFamily3D {
    RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_OPAQUE = 1,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_OPAQUE = 2,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_TRANSPARENT = 3,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_TRANSPARENT = 4,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_MIRROR_SPECULAR = 5,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_METAL_GLOSSY = 6,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_EMISSIVE = 7,
    RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT = 8
} RuntimeRenderTraceCostMaterialFamily3D;

typedef enum RuntimeRenderTraceCostPathDepthBucket3D {
    RUNTIME_RENDER_TRACE_COST_DEPTH_0 = 0,
    RUNTIME_RENDER_TRACE_COST_DEPTH_1 = 1,
    RUNTIME_RENDER_TRACE_COST_DEPTH_2 = 2,
    RUNTIME_RENDER_TRACE_COST_DEPTH_3_PLUS = 3,
    RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT = 4
} RuntimeRenderTraceCostPathDepthBucket3D;

typedef enum RuntimeRenderTraceCostDirectLightCaller3D {
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_PRIMARY_HIT = 1,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_SHADED_HIT = 2,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_LIGHT_SET = 3,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_COUNT = 4
} RuntimeRenderTraceCostDirectLightCaller3D;

typedef enum RuntimeRenderTraceCostDirectLightSourceKind3D {
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_POINT = 1,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_SPHERE = 2,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_DISK = 3,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_RECT = 4,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_MESH_EMISSIVE = 5,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT = 6
} RuntimeRenderTraceCostDirectLightSourceKind3D;

typedef enum RuntimeRenderTraceCostDirectLightSourceOrigin3D {
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COMPAT_SCENE_LIGHT = 1,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_AUTHORED_LIGHT = 2,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_MATERIAL_EMITTER = 3,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COUNT = 4
} RuntimeRenderTraceCostDirectLightSourceOrigin3D;

typedef enum RuntimeRenderTraceCostDirectLightEmissionProfile3D {
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_OMNI = 1,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_ONE_SIDED = 2,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_TWO_SIDED = 3,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_COUNT = 4
} RuntimeRenderTraceCostDirectLightEmissionProfile3D;

typedef enum RuntimeRenderTraceCostDirectLightOutcome3D {
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_NO_VISIBILITY_TRACE = 0,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_CLEAR_VISIBLE = 1,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_CLEAR_BLOCKED = 2,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_STABLE_PARTIAL = 3,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_MIXED_PARTIAL = 4,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT = 5
} RuntimeRenderTraceCostDirectLightOutcome3D;

typedef enum RuntimeRenderTraceCostDirectLightStopReason3D {
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_FULL_SAMPLE_COUNT = 0,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_ALL_CLEAR = 1,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_ALL_BLOCKED = 2,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_STABLE_PARTIAL = 3,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_LOW_IMPORTANCE = 4,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT = 5
} RuntimeRenderTraceCostDirectLightStopReason3D;

typedef enum RuntimeRenderTraceCostDirectLightSampleBucket3D {
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_ZERO = 0,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_DECISION = 1,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_PARTIAL_ESCALATION = 2,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_FULL = 3,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_COUNT = 4
} RuntimeRenderTraceCostDirectLightSampleBucket3D;

typedef enum RuntimeRenderTraceCostDirectLightDistanceBucket3D {
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_NEAR = 0,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_MID = 1,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_FAR = 2,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT = 3
} RuntimeRenderTraceCostDirectLightDistanceBucket3D;

typedef enum RuntimeRenderTraceCostDirectLightImportanceBucket3D {
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_ZERO = 0,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_LOW = 1,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_MEDIUM = 2,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_HIGH = 3,
    RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT = 4
} RuntimeRenderTraceCostDirectLightImportanceBucket3D;

typedef enum RuntimeRenderTraceCostTransmissionSource3D {
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_PRIMARY = 1,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED = 2,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT = 3
} RuntimeRenderTraceCostTransmissionSource3D;

typedef enum RuntimeRenderTraceCostTransmissionSurfaceKind3D {
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_ALPHA_ONLY = 1,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_THIN_WALLED = 2,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_SOLID_PHYSICAL = 3,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_SOLID_NONPHYSICAL = 4,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_OPAQUE_RECEIVER = 5,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_COUNT = 6
} RuntimeRenderTraceCostTransmissionSurfaceKind3D;

typedef enum RuntimeRenderTraceCostTransmissionTermination3D {
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_RECEIVER_HIT = 1,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_NO_HIT = 2,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_DEPTH_LIMIT = 3,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_SKIP_LIMIT = 4,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_NO_CONTRIBUTION = 5,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_POLICY_REJECT = 6,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_COUNT = 7
} RuntimeRenderTraceCostTransmissionTermination3D;

typedef enum RuntimeRenderTraceCostTransmissionSampleIndexBucket3D {
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_FIRST = 0,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_SECOND = 1,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_THIRD = 2,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_FOURTH = 3,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_LATER = 4,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT = 5
} RuntimeRenderTraceCostTransmissionSampleIndexBucket3D;

typedef enum RuntimeRenderTraceCostTransmissionAlignmentBucket3D {
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_AXIAL = 0,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_NARROW = 1,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_MEDIUM = 2,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_WIDE = 3,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT = 4
} RuntimeRenderTraceCostTransmissionAlignmentBucket3D;

typedef enum RuntimeRenderTraceCostTransmissionScreenRegion3D {
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_TOP_LEFT = 1,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_TOP_RIGHT = 2,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_BOTTOM_LEFT = 3,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_BOTTOM_RIGHT = 4,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT = 5
} RuntimeRenderTraceCostTransmissionScreenRegion3D;

typedef enum RuntimeRenderTraceCostTransmissionPixelStability3D {
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_UNKNOWN = 0,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_FIRST_SUBPASS = 1,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_EARLY_SUBPASS = 2,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_LATE_SUBPASS = 3,
    RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT = 4
} RuntimeRenderTraceCostTransmissionPixelStability3D;

typedef enum RuntimeRenderTraceCostThroughputBucket3D {
    RUNTIME_RENDER_TRACE_COST_THROUGHPUT_ZERO = 0,
    RUNTIME_RENDER_TRACE_COST_THROUGHPUT_TINY = 1,
    RUNTIME_RENDER_TRACE_COST_THROUGHPUT_LOW = 2,
    RUNTIME_RENDER_TRACE_COST_THROUGHPUT_MEDIUM = 3,
    RUNTIME_RENDER_TRACE_COST_THROUGHPUT_HIGH = 4,
    RUNTIME_RENDER_TRACE_COST_THROUGHPUT_COUNT = 5
} RuntimeRenderTraceCostThroughputBucket3D;

typedef struct RuntimeRenderTraceCostDirectLightVisibilityPolicy3D {
    uint64_t sourceEvaluations;
    uint64_t evaluatedSamples;
    uint64_t visibilityTraces;
    uint64_t lumaRangeCount;
    double lumaMinObserved;
    double lumaMaxObserved;
    double lumaSpanSum;
    double lumaSpanMax;
    uint64_t callerCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_COUNT];
    uint64_t sourceKindCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT];
    uint64_t sourceOriginCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COUNT];
    uint64_t emissionProfileCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_COUNT];
    uint64_t outcomeCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT];
    uint64_t stopReasonCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT];
    uint64_t sampleBucketCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_COUNT];
    uint64_t distanceBucketCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT];
    uint64_t importanceBucketCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT];
    uint64_t evaluatedSamplesBySourceKind
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT];
    uint64_t visibilityTracesBySourceKind
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT];
    uint64_t evaluatedSamplesBySourceOrigin
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COUNT];
    uint64_t visibilityTracesBySourceOrigin
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COUNT];
    uint64_t evaluatedSamplesByEmissionProfile
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_COUNT];
    uint64_t visibilityTracesByEmissionProfile
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_COUNT];
    uint64_t evaluatedSamplesByOutcome
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT];
    uint64_t visibilityTracesByOutcome
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT];
    uint64_t evaluatedSamplesByStopReason
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT];
    uint64_t visibilityTracesByStopReason
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT];
    uint64_t evaluatedSamplesBySampleBucket
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_COUNT];
    uint64_t visibilityTracesBySampleBucket
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_COUNT];
    uint64_t evaluatedSamplesByDistance
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT];
    uint64_t visibilityTracesByDistance
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT];
    uint64_t evaluatedSamplesByImportance
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT];
    uint64_t visibilityTracesByImportance
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT];
    uint64_t distanceImportanceCounts
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT];
    uint64_t evaluatedSamplesByDistanceImportance
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT];
    uint64_t visibilityTracesByDistanceImportance
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT];
    uint64_t materialEmitterRectEvaluations;
    uint64_t materialEmitterRectEvaluatedSamples;
    uint64_t materialEmitterRectVisibilityTraces;
    uint64_t materialEmitterRectDistanceCounts
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT];
    uint64_t materialEmitterRectImportanceCounts
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT];
    uint64_t materialEmitterRectDistanceImportanceCounts
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT];
    uint64_t materialEmitterRectEvaluatedSamplesByDistance
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT];
    uint64_t materialEmitterRectVisibilityTracesByDistance
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_COUNT];
    uint64_t materialEmitterRectEvaluatedSamplesByImportance
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT];
    uint64_t materialEmitterRectVisibilityTracesByImportance
        [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_COUNT];
    uint64_t sourceKindOutcomeCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT]
                                    [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT];
    uint64_t sourceKindStopReasonCounts[RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT]
                                       [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT];
} RuntimeRenderTraceCostDirectLightVisibilityPolicy3D;

typedef struct RuntimeRenderTraceCostTransmissionPathPolicy3D {
    uint64_t pathEvaluations;
    uint64_t requestedSamples;
    uint64_t sampleEvaluations;
    uint64_t contributingSamples;
    uint64_t receiverSamples;
    uint64_t rayTraces;
    uint64_t hitSurfaces;
    uint64_t transparentSurfaceHits;
    uint64_t receiverHits;
    uint64_t sourceCounts[RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT];
    uint64_t sourceSampleCounts[RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT];
    uint64_t sampleIndexCounts
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT];
    uint64_t sourceSampleIndexCounts
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT];
    uint64_t contributingSamplesByIndex
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT];
    uint64_t receiverSamplesByIndex
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT];
    uint64_t noHitSamplesByIndex
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT];
    uint64_t zeroContributionSamplesByIndex
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT];
    uint64_t sourceContributingSamplesByIndex
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT];
    uint64_t sourceReceiverSamplesByIndex
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT];
    uint64_t sourceNoHitSamplesByIndex
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT];
    uint64_t sourceZeroContributionSamplesByIndex
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_COUNT];
    uint64_t alignmentCounts
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT];
    uint64_t sourceAlignmentCounts
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT];
    uint64_t contributingSamplesByAlignment
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT];
    uint64_t receiverSamplesByAlignment
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT];
    uint64_t noHitSamplesByAlignment
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT];
    uint64_t zeroContributionSamplesByAlignment
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT];
    uint64_t sourceContributingSamplesByAlignment
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT];
    uint64_t sourceReceiverSamplesByAlignment
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT];
    uint64_t sourceNoHitSamplesByAlignment
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT];
    uint64_t sourceZeroContributionSamplesByAlignment
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_COUNT];
    uint64_t screenRegionCounts
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT];
    uint64_t sourceScreenRegionCounts
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT];
    uint64_t contributingSamplesByScreenRegion
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT];
    uint64_t receiverSamplesByScreenRegion
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT];
    uint64_t noHitSamplesByScreenRegion
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT];
    uint64_t zeroContributionSamplesByScreenRegion
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT];
    uint64_t sourceContributingSamplesByScreenRegion
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT];
    uint64_t sourceReceiverSamplesByScreenRegion
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT];
    uint64_t sourceNoHitSamplesByScreenRegion
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT];
    uint64_t sourceZeroContributionSamplesByScreenRegion
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_COUNT];
    uint64_t pixelStabilityCounts
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT];
    uint64_t sourcePixelStabilityCounts
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT];
    uint64_t contributingSamplesByPixelStability
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT];
    uint64_t receiverSamplesByPixelStability
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT];
    uint64_t noHitSamplesByPixelStability
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT];
    uint64_t zeroContributionSamplesByPixelStability
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT];
    uint64_t sourceContributingSamplesByPixelStability
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT];
    uint64_t sourceReceiverSamplesByPixelStability
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT];
    uint64_t sourceNoHitSamplesByPixelStability
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT];
    uint64_t sourceZeroContributionSamplesByPixelStability
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
        [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_COUNT];
    uint64_t terminationCounts[RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_COUNT];
    uint64_t sourceTerminationCounts[RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT]
                                   [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_COUNT];
    uint64_t surfaceKindCounts[RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_COUNT];
    uint64_t surfaceKindMaterialCounts[RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_COUNT]
                                      [RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT];
    uint64_t transparentSurfaceMaterialCounts[RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT];
    uint64_t receiverMaterialCounts[RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT];
    uint64_t terminalDepthCounts[RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT];
    uint64_t rayDepthCounts[RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT];
    uint64_t throughputBucketCounts[RUNTIME_RENDER_TRACE_COST_THROUGHPUT_COUNT];
    uint64_t contributionBucketCounts[RUNTIME_RENDER_TRACE_COST_THROUGHPUT_COUNT];
    uint64_t totalTransparentSurfacesPerSample;
    uint64_t maxTransparentSurfacesInSample;
    uint64_t totalRayTracesPerSample;
    uint64_t maxRayTracesInSample;
} RuntimeRenderTraceCostTransmissionPathPolicy3D;

typedef struct RuntimeRenderTraceCostLedger3D {
    bool enabled;
    uint64_t totalRays;
    uint64_t rayClassCounts[RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT];
    uint64_t pathDepthCounts[RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT];
    uint64_t rayClassDepthCounts[RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT]
                                [RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT];
    uint64_t materialFamilyCounts[RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT];
    RuntimeRenderTraceCostDirectLightVisibilityPolicy3D directLightVisibilityPolicy;
    RuntimeRenderTraceCostTransmissionPathPolicy3D transmissionPathPolicy;
} RuntimeRenderTraceCostLedger3D;

const char* RuntimeRenderTraceCostRayClass3DLabel(RuntimeRenderTraceCostRayClass3D ray_class);
const char* RuntimeRenderTraceCostMaterialFamily3DLabel(
    RuntimeRenderTraceCostMaterialFamily3D family);
const char* RuntimeRenderTraceCostPathDepthBucket3DLabel(
    RuntimeRenderTraceCostPathDepthBucket3D bucket);
const char* RuntimeRenderTraceCostDirectLightCaller3DLabel(
    RuntimeRenderTraceCostDirectLightCaller3D caller);
const char* RuntimeRenderTraceCostDirectLightSourceKind3DLabel(
    RuntimeRenderTraceCostDirectLightSourceKind3D kind);
const char* RuntimeRenderTraceCostDirectLightSourceOrigin3DLabel(
    RuntimeRenderTraceCostDirectLightSourceOrigin3D origin);
const char* RuntimeRenderTraceCostDirectLightEmissionProfile3DLabel(
    RuntimeRenderTraceCostDirectLightEmissionProfile3D profile);
const char* RuntimeRenderTraceCostDirectLightOutcome3DLabel(
    RuntimeRenderTraceCostDirectLightOutcome3D outcome);
const char* RuntimeRenderTraceCostDirectLightStopReason3DLabel(
    RuntimeRenderTraceCostDirectLightStopReason3D reason);
const char* RuntimeRenderTraceCostDirectLightSampleBucket3DLabel(
    RuntimeRenderTraceCostDirectLightSampleBucket3D bucket);
const char* RuntimeRenderTraceCostDirectLightDistanceBucket3DLabel(
    RuntimeRenderTraceCostDirectLightDistanceBucket3D bucket);
const char* RuntimeRenderTraceCostDirectLightImportanceBucket3DLabel(
    RuntimeRenderTraceCostDirectLightImportanceBucket3D bucket);
const char* RuntimeRenderTraceCostTransmissionSource3DLabel(
    RuntimeRenderTraceCostTransmissionSource3D source);
const char* RuntimeRenderTraceCostTransmissionSurfaceKind3DLabel(
    RuntimeRenderTraceCostTransmissionSurfaceKind3D kind);
const char* RuntimeRenderTraceCostTransmissionTermination3DLabel(
    RuntimeRenderTraceCostTransmissionTermination3D termination);
const char* RuntimeRenderTraceCostTransmissionSampleIndexBucket3DLabel(
    RuntimeRenderTraceCostTransmissionSampleIndexBucket3D bucket);
const char* RuntimeRenderTraceCostTransmissionAlignmentBucket3DLabel(
    RuntimeRenderTraceCostTransmissionAlignmentBucket3D bucket);
const char* RuntimeRenderTraceCostTransmissionScreenRegion3DLabel(
    RuntimeRenderTraceCostTransmissionScreenRegion3D region);
const char* RuntimeRenderTraceCostTransmissionPixelStability3DLabel(
    RuntimeRenderTraceCostTransmissionPixelStability3D bucket);
const char* RuntimeRenderTraceCostThroughputBucket3DLabel(
    RuntimeRenderTraceCostThroughputBucket3D bucket);
void RuntimeRenderTraceCostLedger3D_SetEnabled(bool enabled);
void RuntimeRenderTraceCostLedger3D_SetEnabledFromEnvironment(void);
bool RuntimeRenderTraceCostLedger3D_IsEnabled(void);
void RuntimeRenderTraceCostLedger3D_Reset(void);
void RuntimeRenderTraceCostLedger3D_RecordRay(RuntimeRenderTraceCostRayClass3D ray_class);
void RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
    RuntimeRenderTraceCostRayClass3D ray_class,
    int path_depth);
void RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(const HitInfo3D* hit);
void RuntimeRenderTraceCostLedger3D_RecordDirectLightVisibilityPolicy(
    RuntimeRenderTraceCostDirectLightCaller3D caller,
    RuntimeRenderTraceCostDirectLightSourceKind3D source_kind,
    RuntimeRenderTraceCostDirectLightSourceOrigin3D source_origin,
    RuntimeRenderTraceCostDirectLightEmissionProfile3D emission_profile,
    RuntimeRenderTraceCostDirectLightOutcome3D outcome,
    RuntimeRenderTraceCostDirectLightStopReason3D stop_reason,
    int light_sample_count,
    int light_sample_decision_count,
    int light_sample_evaluated_count,
    int visibility_trace_count,
    double light_distance,
    double contribution_peak,
    double transmittance_luma_min,
    double transmittance_luma_max);
void RuntimeRenderTraceCostLedger3D_RecordTransmissionPathEvaluation(
    RuntimeRenderTraceCostTransmissionSource3D source,
    int requested_sample_count);
void RuntimeRenderTraceCostLedger3D_RecordTransmissionRayAtDepth(
    RuntimeRenderTraceCostTransmissionSource3D source,
    int path_depth);
void RuntimeRenderTraceCostLedger3D_RecordTransmissionSurface(
    RuntimeRenderTraceCostTransmissionSource3D source,
    RuntimeRenderTraceCostTransmissionSurfaceKind3D surface_kind,
    const HitInfo3D* hit);
void RuntimeRenderTraceCostLedger3D_RecordTransmissionSample(
    RuntimeRenderTraceCostTransmissionSource3D source,
    RuntimeRenderTraceCostTransmissionTermination3D termination,
    int sample_index,
    double direction_alignment,
    RuntimeRenderTraceCostTransmissionScreenRegion3D screen_region,
    RuntimeRenderTraceCostTransmissionPixelStability3D pixel_stability,
    int terminal_depth,
    int ray_trace_count,
    int transparent_surface_count,
    bool receiver_found,
    double throughput_peak,
    double contribution_peak);
void RuntimeRenderTraceCostLedger3D_Snapshot(RuntimeRenderTraceCostLedger3D* out_ledger);

#endif
