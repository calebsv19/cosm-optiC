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

typedef struct RuntimeRenderTraceCostLedger3D {
    bool enabled;
    uint64_t totalRays;
    uint64_t rayClassCounts[RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT];
    uint64_t pathDepthCounts[RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT];
    uint64_t rayClassDepthCounts[RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT]
                                [RUNTIME_RENDER_TRACE_COST_DEPTH_BUCKET_COUNT];
    uint64_t materialFamilyCounts[RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT];
    RuntimeRenderTraceCostDirectLightVisibilityPolicy3D directLightVisibilityPolicy;
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
void RuntimeRenderTraceCostLedger3D_Snapshot(RuntimeRenderTraceCostLedger3D* out_ledger);

#endif
