#include "render/runtime_render_trace_cost_ledger_3d.h"

#include "material/material.h"
#include "render/runtime_material_payload_3d.h"

#include <stdlib.h>
#include <string.h>

static RuntimeRenderTraceCostLedger3D gRuntimeRenderTraceCostLedger3D;

const char* RuntimeRenderTraceCostRayClass3DLabel(RuntimeRenderTraceCostRayClass3D ray_class) {
    switch (ray_class) {
        case RUNTIME_RENDER_TRACE_COST_RAY_PRIMARY:
            return "primary";
        case RUNTIME_RENDER_TRACE_COST_RAY_DIRECT_LIGHT_VISIBILITY:
            return "direct_light_visibility";
        case RUNTIME_RENDER_TRACE_COST_RAY_TRANSMISSION:
            return "transmission";
        case RUNTIME_RENDER_TRACE_COST_RAY_REFLECTION_SPECULAR:
            return "reflection_specular";
        case RUNTIME_RENDER_TRACE_COST_RAY_DIFFUSE_SECONDARY:
            return "diffuse_secondary";
        case RUNTIME_RENDER_TRACE_COST_RAY_DISNEY_RECURSIVE:
            return "disney_recursive";
        case RUNTIME_RENDER_TRACE_COST_RAY_CAUSTIC:
            return "caustic";
        case RUNTIME_RENDER_TRACE_COST_RAY_EMISSIVE_AREA:
            return "emissive_area";
        case RUNTIME_RENDER_TRACE_COST_RAY_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostMaterialFamily3DLabel(
    RuntimeRenderTraceCostMaterialFamily3D family) {
    switch (family) {
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_OPAQUE:
            return "primitive_opaque";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_OPAQUE:
            return "runtime_mesh_opaque";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_TRANSPARENT:
            return "primitive_transparent";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_TRANSPARENT:
            return "runtime_mesh_transparent";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_MIRROR_SPECULAR:
            return "mirror_specular";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_METAL_GLOSSY:
            return "metal_glossy";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_EMISSIVE:
            return "emissive";
        case RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostPathDepthBucket3DLabel(
    RuntimeRenderTraceCostPathDepthBucket3D bucket) {
    switch (bucket) {
        case RUNTIME_RENDER_TRACE_COST_DEPTH_0:
            return "depth_0";
        case RUNTIME_RENDER_TRACE_COST_DEPTH_1:
            return "depth_1";
        case RUNTIME_RENDER_TRACE_COST_DEPTH_2:
            return "depth_2";
        case RUNTIME_RENDER_TRACE_COST_DEPTH_3_PLUS:
            return "depth_3_plus";
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostDirectLightCaller3DLabel(
    RuntimeRenderTraceCostDirectLightCaller3D caller) {
    switch (caller) {
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_PRIMARY_HIT:
            return "primary_hit";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_SHADED_HIT:
            return "shaded_hit";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_LIGHT_SET:
            return "light_set";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostDirectLightSourceKind3DLabel(
    RuntimeRenderTraceCostDirectLightSourceKind3D kind) {
    switch (kind) {
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_POINT:
            return "point";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_SPHERE:
            return "sphere";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_DISK:
            return "disk";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_RECT:
            return "rect";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_MESH_EMISSIVE:
            return "mesh_emissive";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostDirectLightSourceOrigin3DLabel(
    RuntimeRenderTraceCostDirectLightSourceOrigin3D origin) {
    switch (origin) {
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COMPAT_SCENE_LIGHT:
            return "compat_scene_light";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_AUTHORED_LIGHT:
            return "authored_light";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_MATERIAL_EMITTER:
            return "material_emitter";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostDirectLightEmissionProfile3DLabel(
    RuntimeRenderTraceCostDirectLightEmissionProfile3D profile) {
    switch (profile) {
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_OMNI:
            return "omni";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_ONE_SIDED:
            return "one_sided";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_TWO_SIDED:
            return "two_sided";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostDirectLightOutcome3DLabel(
    RuntimeRenderTraceCostDirectLightOutcome3D outcome) {
    switch (outcome) {
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_NO_VISIBILITY_TRACE:
            return "no_visibility_trace";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_CLEAR_VISIBLE:
            return "clear_visible";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_CLEAR_BLOCKED:
            return "clear_blocked";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_STABLE_PARTIAL:
            return "stable_partial";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_MIXED_PARTIAL:
            return "mixed_partial";
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostDirectLightStopReason3DLabel(
    RuntimeRenderTraceCostDirectLightStopReason3D reason) {
    switch (reason) {
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_FULL_SAMPLE_COUNT:
            return "full_sample_count";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_ALL_CLEAR:
            return "all_clear";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_ALL_BLOCKED:
            return "all_blocked";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_STABLE_PARTIAL:
            return "stable_partial";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_LOW_IMPORTANCE:
            return "low_importance";
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostDirectLightSampleBucket3DLabel(
    RuntimeRenderTraceCostDirectLightSampleBucket3D bucket) {
    switch (bucket) {
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_ZERO:
            return "zero";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_DECISION:
            return "decision_count";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_PARTIAL_ESCALATION:
            return "partial_escalation";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_FULL:
            return "full_count";
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostDirectLightDistanceBucket3DLabel(
    RuntimeRenderTraceCostDirectLightDistanceBucket3D bucket) {
    switch (bucket) {
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_NEAR:
            return "near";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_MID:
            return "mid";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_FAR:
            return "far";
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostDirectLightImportanceBucket3DLabel(
    RuntimeRenderTraceCostDirectLightImportanceBucket3D bucket) {
    switch (bucket) {
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_ZERO:
            return "zero";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_LOW:
            return "low";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_MEDIUM:
            return "medium";
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_HIGH:
            return "high";
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostTransmissionSource3DLabel(
    RuntimeRenderTraceCostTransmissionSource3D source) {
    switch (source) {
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_PRIMARY:
            return "primary";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED:
            return "reflected";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostTransmissionSurfaceKind3DLabel(
    RuntimeRenderTraceCostTransmissionSurfaceKind3D kind) {
    switch (kind) {
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_ALPHA_ONLY:
            return "alpha_only";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_THIN_WALLED:
            return "thin_walled";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_SOLID_PHYSICAL:
            return "solid_physical";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_SOLID_NONPHYSICAL:
            return "solid_nonphysical";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_OPAQUE_RECEIVER:
            return "opaque_receiver";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostTransmissionTermination3DLabel(
    RuntimeRenderTraceCostTransmissionTermination3D termination) {
    switch (termination) {
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_RECEIVER_HIT:
            return "receiver_hit";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_NO_HIT:
            return "no_hit";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_DEPTH_LIMIT:
            return "depth_limit";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_SKIP_LIMIT:
            return "skip_limit";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_NO_CONTRIBUTION:
            return "no_contribution";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_POLICY_REJECT:
            return "policy_reject";
        case RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_UNKNOWN:
        default:
            return "unknown";
    }
}

const char* RuntimeRenderTraceCostThroughputBucket3DLabel(
    RuntimeRenderTraceCostThroughputBucket3D bucket) {
    switch (bucket) {
        case RUNTIME_RENDER_TRACE_COST_THROUGHPUT_ZERO:
            return "zero";
        case RUNTIME_RENDER_TRACE_COST_THROUGHPUT_TINY:
            return "tiny";
        case RUNTIME_RENDER_TRACE_COST_THROUGHPUT_LOW:
            return "low";
        case RUNTIME_RENDER_TRACE_COST_THROUGHPUT_MEDIUM:
            return "medium";
        case RUNTIME_RENDER_TRACE_COST_THROUGHPUT_HIGH:
            return "high";
        default:
            return "unknown";
    }
}

void RuntimeRenderTraceCostLedger3D_SetEnabled(bool enabled) {
    gRuntimeRenderTraceCostLedger3D.enabled = enabled;
}

void RuntimeRenderTraceCostLedger3D_SetEnabledFromEnvironment(void) {
    const char* value = getenv("RAY_TRACING_RENDER_TRACE_COST_LEDGER");
    RuntimeRenderTraceCostLedger3D_SetEnabled(value && value[0] != '\0' && value[0] != '0');
}

bool RuntimeRenderTraceCostLedger3D_IsEnabled(void) {
    return gRuntimeRenderTraceCostLedger3D.enabled;
}

void RuntimeRenderTraceCostLedger3D_Reset(void) {
    const bool enabled = gRuntimeRenderTraceCostLedger3D.enabled;
    memset(&gRuntimeRenderTraceCostLedger3D, 0, sizeof(gRuntimeRenderTraceCostLedger3D));
    gRuntimeRenderTraceCostLedger3D.enabled = enabled;
}

void RuntimeRenderTraceCostLedger3D_RecordRay(RuntimeRenderTraceCostRayClass3D ray_class) {
    RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(ray_class, 0);
}

static RuntimeRenderTraceCostPathDepthBucket3D runtime_render_trace_cost_depth_bucket(
    int path_depth) {
    if (path_depth <= 0) return RUNTIME_RENDER_TRACE_COST_DEPTH_0;
    if (path_depth == 1) return RUNTIME_RENDER_TRACE_COST_DEPTH_1;
    if (path_depth == 2) return RUNTIME_RENDER_TRACE_COST_DEPTH_2;
    return RUNTIME_RENDER_TRACE_COST_DEPTH_3_PLUS;
}

void RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
    RuntimeRenderTraceCostRayClass3D ray_class,
    int path_depth) {
    RuntimeRenderTraceCostPathDepthBucket3D depth_bucket =
        runtime_render_trace_cost_depth_bucket(path_depth);
    if (!gRuntimeRenderTraceCostLedger3D.enabled) return;
    if (ray_class < 0 || ray_class >= RUNTIME_RENDER_TRACE_COST_RAY_CLASS_COUNT) {
        ray_class = RUNTIME_RENDER_TRACE_COST_RAY_UNKNOWN;
    }
    gRuntimeRenderTraceCostLedger3D.totalRays += 1u;
    gRuntimeRenderTraceCostLedger3D.rayClassCounts[ray_class] += 1u;
    gRuntimeRenderTraceCostLedger3D.pathDepthCounts[depth_bucket] += 1u;
    gRuntimeRenderTraceCostLedger3D.rayClassDepthCounts[ray_class][depth_bucket] += 1u;
}

static bool runtime_render_trace_cost_hit_is_runtime_mesh(const HitInfo3D* hit) {
    return hit && hit->source.kind == RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
}

static RuntimeRenderTraceCostMaterialFamily3D
runtime_render_trace_cost_material_family_from_hit(const HitInfo3D* hit,
                                                   const RuntimeMaterialPayload3D* payload) {
    const bool runtime_mesh = runtime_render_trace_cost_hit_is_runtime_mesh(hit);
    if (!hit || !payload || !payload->valid) {
        return RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN;
    }
    if (payload->emissive > 0.0 || payload->bsdf.emissive > 0.0 ||
        payload->materialId == MATERIAL_PRESET_EMISSIVE) {
        return RUNTIME_RENDER_TRACE_COST_MATERIAL_EMISSIVE;
    }
    if (payload->materialId == MATERIAL_PRESET_MIRROR ||
        payload->bsdf.reflectivity >= 0.85) {
        return RUNTIME_RENDER_TRACE_COST_MATERIAL_MIRROR_SPECULAR;
    }
    if (payload->materialId == MATERIAL_PRESET_ROUGH_METAL ||
        payload->materialId == MATERIAL_PRESET_GLOSSY) {
        return RUNTIME_RENDER_TRACE_COST_MATERIAL_METAL_GLOSSY;
    }
    if (payload->transparency > 0.0 ||
        payload->materialId == MATERIAL_PRESET_TRANSPARENT) {
        return runtime_mesh ? RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_TRANSPARENT
                            : RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_TRANSPARENT;
    }
    return runtime_mesh ? RUNTIME_RENDER_TRACE_COST_MATERIAL_RUNTIME_MESH_OPAQUE
                        : RUNTIME_RENDER_TRACE_COST_MATERIAL_PRIMITIVE_OPAQUE;
}

void RuntimeRenderTraceCostLedger3D_RecordHitMaterialFamily(const HitInfo3D* hit) {
    RuntimeMaterialPayload3D payload = {0};
    RuntimeRenderTraceCostMaterialFamily3D family =
        RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN;
    if (!gRuntimeRenderTraceCostLedger3D.enabled) return;
    if (RuntimeMaterialPayload3D_ResolveFromHit(hit, &payload)) {
        family = runtime_render_trace_cost_material_family_from_hit(hit, &payload);
    }
    if (family < 0 || family >= RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT) {
        family = RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN;
    }
    gRuntimeRenderTraceCostLedger3D.materialFamilyCounts[family] += 1u;
}

static RuntimeRenderTraceCostDirectLightSampleBucket3D
runtime_render_trace_cost_direct_light_sample_bucket(int evaluated_count,
                                                     int decision_count,
                                                     int max_count) {
    if (evaluated_count <= 0) return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_ZERO;
    if (decision_count > 0 && evaluated_count <= decision_count) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_DECISION;
    }
    if (max_count > 0 && evaluated_count >= max_count) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_FULL;
    }
    return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_PARTIAL_ESCALATION;
}

static RuntimeRenderTraceCostDirectLightDistanceBucket3D
runtime_render_trace_cost_direct_light_distance_bucket(double light_distance) {
    if (light_distance <= 2.0) return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_NEAR;
    if (light_distance <= 8.0) return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_MID;
    return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_FAR;
}

static RuntimeRenderTraceCostDirectLightImportanceBucket3D
runtime_render_trace_cost_direct_light_importance_bucket(double contribution_peak) {
    if (!(contribution_peak > 1.0e-8)) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_ZERO;
    }
    if (contribution_peak <= 0.01) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_LOW;
    }
    if (contribution_peak <= 0.1) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_MEDIUM;
    }
    return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_HIGH;
}

static RuntimeRenderTraceCostThroughputBucket3D
runtime_render_trace_cost_throughput_bucket(double peak) {
    if (!(peak > 1.0e-9)) return RUNTIME_RENDER_TRACE_COST_THROUGHPUT_ZERO;
    if (peak <= 1.0e-4) return RUNTIME_RENDER_TRACE_COST_THROUGHPUT_TINY;
    if (peak <= 1.0e-2) return RUNTIME_RENDER_TRACE_COST_THROUGHPUT_LOW;
    if (peak <= 1.0e-1) return RUNTIME_RENDER_TRACE_COST_THROUGHPUT_MEDIUM;
    return RUNTIME_RENDER_TRACE_COST_THROUGHPUT_HIGH;
}

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
    double transmittance_luma_max) {
    RuntimeRenderTraceCostDirectLightVisibilityPolicy3D* policy =
        &gRuntimeRenderTraceCostLedger3D.directLightVisibilityPolicy;
    RuntimeRenderTraceCostDirectLightSampleBucket3D sample_bucket =
        runtime_render_trace_cost_direct_light_sample_bucket(light_sample_evaluated_count,
                                                             light_sample_decision_count,
                                                             light_sample_count);
    RuntimeRenderTraceCostDirectLightDistanceBucket3D distance_bucket =
        runtime_render_trace_cost_direct_light_distance_bucket(light_distance);
    RuntimeRenderTraceCostDirectLightImportanceBucket3D importance_bucket =
        runtime_render_trace_cost_direct_light_importance_bucket(contribution_peak);
    if (!gRuntimeRenderTraceCostLedger3D.enabled) return;
    if (caller < 0 || caller >= RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_COUNT) {
        caller = RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_UNKNOWN;
    }
    if (source_kind < 0 ||
        source_kind >= RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_KIND_COUNT) {
        source_kind = RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_UNKNOWN;
    }
    if (source_origin < 0 ||
        source_origin >= RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COUNT) {
        source_origin = RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_UNKNOWN;
    }
    if (emission_profile < 0 ||
        emission_profile >= RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_COUNT) {
        emission_profile = RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_UNKNOWN;
    }
    if (outcome < 0 || outcome >= RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_COUNT) {
        outcome = RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_NO_VISIBILITY_TRACE;
    }
    if (stop_reason < 0 || stop_reason >= RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_COUNT) {
        stop_reason = RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_FULL_SAMPLE_COUNT;
    }
    policy->sourceEvaluations += 1u;
    policy->evaluatedSamples += light_sample_evaluated_count > 0
                                    ? (uint64_t)light_sample_evaluated_count
                                    : 0u;
    policy->visibilityTraces += visibility_trace_count > 0
                                    ? (uint64_t)visibility_trace_count
                                    : 0u;
    policy->callerCounts[caller] += 1u;
    policy->sourceKindCounts[source_kind] += 1u;
    policy->sourceOriginCounts[source_origin] += 1u;
    policy->emissionProfileCounts[emission_profile] += 1u;
    policy->outcomeCounts[outcome] += 1u;
    policy->stopReasonCounts[stop_reason] += 1u;
    policy->sampleBucketCounts[sample_bucket] += 1u;
    policy->distanceBucketCounts[distance_bucket] += 1u;
    policy->importanceBucketCounts[importance_bucket] += 1u;
    policy->sourceKindOutcomeCounts[source_kind][outcome] += 1u;
    policy->sourceKindStopReasonCounts[source_kind][stop_reason] += 1u;
    if (source_kind == RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_RECT &&
        source_origin == RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_MATERIAL_EMITTER &&
        emission_profile == RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_ONE_SIDED) {
        policy->materialEmitterRectEvaluations += 1u;
        policy->materialEmitterRectEvaluatedSamples += light_sample_evaluated_count > 0
                                                           ? (uint64_t)light_sample_evaluated_count
                                                           : 0u;
        policy->materialEmitterRectVisibilityTraces += visibility_trace_count > 0
                                                           ? (uint64_t)visibility_trace_count
                                                           : 0u;
        policy->materialEmitterRectDistanceCounts[distance_bucket] += 1u;
        policy->materialEmitterRectImportanceCounts[importance_bucket] += 1u;
        policy->materialEmitterRectDistanceImportanceCounts[distance_bucket]
                                                         [importance_bucket] += 1u;
        policy->materialEmitterRectEvaluatedSamplesByDistance[distance_bucket] +=
            light_sample_evaluated_count > 0 ? (uint64_t)light_sample_evaluated_count : 0u;
        policy->materialEmitterRectVisibilityTracesByDistance[distance_bucket] +=
            visibility_trace_count > 0 ? (uint64_t)visibility_trace_count : 0u;
        policy->materialEmitterRectEvaluatedSamplesByImportance[importance_bucket] +=
            light_sample_evaluated_count > 0 ? (uint64_t)light_sample_evaluated_count : 0u;
        policy->materialEmitterRectVisibilityTracesByImportance[importance_bucket] +=
            visibility_trace_count > 0 ? (uint64_t)visibility_trace_count : 0u;
    }
    if (visibility_trace_count > 0 &&
        transmittance_luma_min <= transmittance_luma_max) {
        const double span = transmittance_luma_max - transmittance_luma_min;
        if (policy->lumaRangeCount == 0u) {
            policy->lumaMinObserved = transmittance_luma_min;
            policy->lumaMaxObserved = transmittance_luma_max;
        } else {
            if (transmittance_luma_min < policy->lumaMinObserved) {
                policy->lumaMinObserved = transmittance_luma_min;
            }
            if (transmittance_luma_max > policy->lumaMaxObserved) {
                policy->lumaMaxObserved = transmittance_luma_max;
            }
        }
        policy->lumaRangeCount += 1u;
        policy->lumaSpanSum += span;
        if (span > policy->lumaSpanMax) {
            policy->lumaSpanMax = span;
        }
    }
}

void RuntimeRenderTraceCostLedger3D_RecordTransmissionPathEvaluation(
    RuntimeRenderTraceCostTransmissionSource3D source,
    int requested_sample_count) {
    RuntimeRenderTraceCostTransmissionPathPolicy3D* policy =
        &gRuntimeRenderTraceCostLedger3D.transmissionPathPolicy;
    if (!gRuntimeRenderTraceCostLedger3D.enabled) return;
    if (source < 0 || source >= RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT) {
        source = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_UNKNOWN;
    }
    policy->pathEvaluations += 1u;
    policy->sourceCounts[source] += 1u;
    if (requested_sample_count > 0) {
        policy->requestedSamples += (uint64_t)requested_sample_count;
        policy->sourceSampleCounts[source] += (uint64_t)requested_sample_count;
    }
}

void RuntimeRenderTraceCostLedger3D_RecordTransmissionRayAtDepth(
    RuntimeRenderTraceCostTransmissionSource3D source,
    int path_depth) {
    RuntimeRenderTraceCostTransmissionPathPolicy3D* policy =
        &gRuntimeRenderTraceCostLedger3D.transmissionPathPolicy;
    RuntimeRenderTraceCostPathDepthBucket3D depth_bucket =
        runtime_render_trace_cost_depth_bucket(path_depth);
    (void)source;
    RuntimeRenderTraceCostLedger3D_RecordRayAtDepth(
        RUNTIME_RENDER_TRACE_COST_RAY_TRANSMISSION,
        path_depth);
    if (!gRuntimeRenderTraceCostLedger3D.enabled) return;
    policy->rayTraces += 1u;
    policy->rayDepthCounts[depth_bucket] += 1u;
}

void RuntimeRenderTraceCostLedger3D_RecordTransmissionSurface(
    RuntimeRenderTraceCostTransmissionSource3D source,
    RuntimeRenderTraceCostTransmissionSurfaceKind3D surface_kind,
    const HitInfo3D* hit) {
    RuntimeRenderTraceCostTransmissionPathPolicy3D* policy =
        &gRuntimeRenderTraceCostLedger3D.transmissionPathPolicy;
    RuntimeMaterialPayload3D payload = {0};
    RuntimeRenderTraceCostMaterialFamily3D family =
        RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN;
    (void)source;
    if (!gRuntimeRenderTraceCostLedger3D.enabled) return;
    if (surface_kind < 0 ||
        surface_kind >= RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_COUNT) {
        surface_kind = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_UNKNOWN;
    }
    if (RuntimeMaterialPayload3D_ResolveFromHit(hit, &payload)) {
        family = runtime_render_trace_cost_material_family_from_hit(hit, &payload);
    }
    if (family < 0 || family >= RUNTIME_RENDER_TRACE_COST_MATERIAL_COUNT) {
        family = RUNTIME_RENDER_TRACE_COST_MATERIAL_UNKNOWN;
    }
    policy->hitSurfaces += 1u;
    policy->surfaceKindCounts[surface_kind] += 1u;
    policy->surfaceKindMaterialCounts[surface_kind][family] += 1u;
    if (surface_kind == RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_OPAQUE_RECEIVER) {
        policy->receiverHits += 1u;
        policy->receiverMaterialCounts[family] += 1u;
    } else {
        policy->transparentSurfaceHits += 1u;
        policy->transparentSurfaceMaterialCounts[family] += 1u;
    }
}

void RuntimeRenderTraceCostLedger3D_RecordTransmissionSample(
    RuntimeRenderTraceCostTransmissionSource3D source,
    RuntimeRenderTraceCostTransmissionTermination3D termination,
    int terminal_depth,
    int ray_trace_count,
    int transparent_surface_count,
    bool receiver_found,
    double throughput_peak,
    double contribution_peak) {
    RuntimeRenderTraceCostTransmissionPathPolicy3D* policy =
        &gRuntimeRenderTraceCostLedger3D.transmissionPathPolicy;
    RuntimeRenderTraceCostPathDepthBucket3D depth_bucket =
        runtime_render_trace_cost_depth_bucket(terminal_depth);
    RuntimeRenderTraceCostThroughputBucket3D throughput_bucket =
        runtime_render_trace_cost_throughput_bucket(throughput_peak);
    RuntimeRenderTraceCostThroughputBucket3D contribution_bucket =
        runtime_render_trace_cost_throughput_bucket(contribution_peak);
    if (!gRuntimeRenderTraceCostLedger3D.enabled) return;
    if (source < 0 || source >= RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_COUNT) {
        source = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_UNKNOWN;
    }
    if (termination < 0 ||
        termination >= RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_COUNT) {
        termination = RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_UNKNOWN;
    }
    policy->sampleEvaluations += 1u;
    if (contribution_peak > 1.0e-9) {
        policy->contributingSamples += 1u;
    }
    if (receiver_found) {
        policy->receiverSamples += 1u;
    }
    policy->terminationCounts[termination] += 1u;
    policy->sourceTerminationCounts[source][termination] += 1u;
    policy->terminalDepthCounts[depth_bucket] += 1u;
    policy->throughputBucketCounts[throughput_bucket] += 1u;
    policy->contributionBucketCounts[contribution_bucket] += 1u;
    if (ray_trace_count > 0) {
        policy->totalRayTracesPerSample += (uint64_t)ray_trace_count;
        if ((uint64_t)ray_trace_count > policy->maxRayTracesInSample) {
            policy->maxRayTracesInSample = (uint64_t)ray_trace_count;
        }
    }
    if (transparent_surface_count > 0) {
        policy->totalTransparentSurfacesPerSample += (uint64_t)transparent_surface_count;
        if ((uint64_t)transparent_surface_count > policy->maxTransparentSurfacesInSample) {
            policy->maxTransparentSurfacesInSample = (uint64_t)transparent_surface_count;
        }
    }
}

void RuntimeRenderTraceCostLedger3D_Snapshot(RuntimeRenderTraceCostLedger3D* out_ledger) {
    if (!out_ledger) return;
    *out_ledger = gRuntimeRenderTraceCostLedger3D;
}
