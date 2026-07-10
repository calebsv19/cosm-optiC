#include "render/runtime_disney_v2_transmission_internal_3d.h"

#include <math.h>
#include <stdlib.h>

#include "app/animation.h"
#include "material/material.h"

bool runtime_disney_v2_3d_payload_has_transparent_alpha(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled) {
    if (payload && payload->valid && payload->transparency > 1e-6) {
        return true;
    }
    return principled && principled->valid && principled->opacity < 0.999;
}

static int runtime_disney_v2_3d_resolve_primary_transmission_sample_count(void) {
    int value = animSettings.transmissionSamples3D;

    if (value < RUNTIME_3D_TRANSMISSION_SAMPLES_MIN) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    }
    if (value > RUNTIME_3D_TRANSMISSION_SAMPLES_MAX) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MAX;
    }
    return value;
}

int runtime_disney_v2_3d_resolve_transmission_sample_count(
    bool allow_recursive_receiver_shade) {
    int value = runtime_disney_v2_3d_resolve_primary_transmission_sample_count();
    const char* reflected_cap_text = NULL;
    char* reflected_cap_end = NULL;
    long reflected_cap = 0;

    if (allow_recursive_receiver_shade) {
        return value;
    }

    reflected_cap_text =
        getenv("RAY_TRACING_DISNEY_V2_REFLECTED_TRANSMISSION_SAMPLE_CAP");
    if (!reflected_cap_text || !reflected_cap_text[0]) {
        return value;
    }
    reflected_cap = strtol(reflected_cap_text, &reflected_cap_end, 10);
    if (reflected_cap_end == reflected_cap_text || reflected_cap <= 0) {
        return value;
    }
    if (reflected_cap < value) {
        value = (int)reflected_cap;
    }
    if (value < 1) {
        value = 1;
    }
    return value;
}

static bool runtime_disney_v2_3d_reflected_first_subpass_no_hit_reuse_probe_enabled(void) {
    const char* value =
        getenv("RAY_TRACING_DISNEY_V2_REFLECTED_FIRST_SUBPASS_NO_HIT_REUSE_PROBE");
    return !value || value[0] == '\0' || value[0] != '0';
}

bool runtime_disney_v2_3d_can_reuse_reflected_first_subpass_no_hit(
    RuntimeRenderTraceCostTransmissionSource3D ledger_source,
    RuntimeRenderTraceCostTransmissionPixelStability3D ledger_pixel_stability,
    int sample_index,
    RuntimeRenderTraceCostTransmissionTermination3D termination) {
    return runtime_disney_v2_3d_reflected_first_subpass_no_hit_reuse_probe_enabled() &&
           ledger_source == RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED &&
           ledger_pixel_stability ==
               RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_FIRST_SUBPASS &&
           sample_index == 0 &&
           termination == RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_NO_HIT;
}

static uint32_t runtime_disney_v2_3d_transmission_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint32_t runtime_disney_v2_3d_transmission_seed_from_hit(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling) {
    uint32_t sequence = sampling ? sampling->sampleSequence : 1U;
    uint32_t sx = 0U;
    uint32_t sy = 0U;
    uint32_t sz = 0U;

    if (!hit) return runtime_disney_v2_3d_transmission_hash_u32(sequence);
    sx = (uint32_t)(fabs(hit->position.x) * 4096.0);
    sy = (uint32_t)(fabs(hit->position.y) * 4096.0);
    sz = (uint32_t)(fabs(hit->position.z) * 4096.0);
    return runtime_disney_v2_3d_transmission_hash_u32(
        sx ^ (sy * 73856093U) ^ (sz * 19349663U) ^
        ((uint32_t)(hit->sceneObjectIndex + 1) * 83492791U) ^
        ((uint32_t)(hit->triangleIndex + 1) * 2654435761U) ^
        runtime_disney_v2_3d_transmission_hash_u32(sequence ^ 0x9e3779b9U));
}

static Vec3 runtime_disney_v2_3d_transmission_default_tangent(Vec3 normal) {
    Vec3 guide = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3_cross(guide, normal);

    if (vec3_length(tangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
    }
    return vec3_normalize(tangent);
}

static void runtime_disney_v2_3d_transmission_build_basis(Vec3 normal,
                                                          Vec3* out_tangent,
                                                          Vec3* out_bitangent) {
    Vec3 tangent = runtime_disney_v2_3d_transmission_default_tangent(normal);
    Vec3 bitangent = vec3_normalize(vec3_cross(normal, tangent));

    if (vec3_length(bitangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
        bitangent = vec3(0.0, 0.0, 1.0);
    }
    if (out_tangent) *out_tangent = tangent;
    if (out_bitangent) *out_bitangent = bitangent;
}

Vec3 runtime_disney_v2_3d_roughen_transmission_direction(
    Vec3 direction,
    double roughness,
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t base_seed,
    int sample_count,
    int sample_index) {
    Vec3 tangent = {0};
    Vec3 bitangent = {0};
    double u = 0.5;
    double v = 0.5;
    double angle = 0.0;
    double radius = 0.0;
    double cone = 0.0;
    Vec3 roughened = {0};

    direction = vec3_normalize(direction);
    roughness = runtime_disney_v2_transmission_3d_clamp01(roughness);
    if (!(roughness > 1e-6) || sample_count <= 1) {
        return direction;
    }

    RuntimeNative3DSampling_Stratified2D(sampling,
                                         base_seed,
                                         sample_count,
                                         sample_index,
                                         0x44563211U,
                                         &u,
                                         &v);
    runtime_disney_v2_3d_transmission_build_basis(direction, &tangent, &bitangent);
    angle = 6.28318530717958647692 * u;
    cone = roughness * kRuntimeDisneyV2_3DPrimaryTransmissionRoughConeScale;
    radius = sqrt(runtime_disney_v2_transmission_3d_clamp01(v)) * cone;
    roughened = vec3_add(direction,
                         vec3_add(vec3_scale(tangent, cos(angle) * radius),
                                  vec3_scale(bitangent, sin(angle) * radius)));
    if (vec3_length(roughened) <= 1e-9) {
        return direction;
    }
    return vec3_normalize(roughened);
}

bool runtime_disney_v2_3d_payload_is_transparent(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled) {
    if (!payload || !payload->valid || !principled || !principled->valid) {
        return false;
    }
    return payload->transparency > 1e-6 || principled->transmissionWeight > 1e-6 ||
           principled->opacity < 0.999;
}

static double runtime_disney_v2_3d_transparent_surface_weight(
    const RuntimePrincipledBSDF3D* principled) {
    double weight = 0.0;
    if (!principled || !principled->valid) {
        return 0.0;
    }
    weight = runtime_disney_v2_transmission_3d_clamp01(principled->transmissionWeight);
    if (weight <= 1e-6) {
        weight = runtime_disney_v2_transmission_3d_clamp01(1.0 - principled->opacity);
    }
    return runtime_disney_v2_transmission_3d_clamp(weight, 0.05, 1.0);
}

static double runtime_disney_v2_3d_transparent_layer_visible_weight(
    const RuntimePrincipledBSDF3D* principled) {
    double transmission_weight = runtime_disney_v2_3d_transparent_surface_weight(principled);
    return runtime_disney_v2_transmission_3d_clamp01(1.0 - transmission_weight);
}

bool runtime_disney_v2_3d_policy_is_physical_transmission(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled) {
    if (!runtime_disney_v2_3d_payload_has_transparent_alpha(payload, principled)) {
        return false;
    }
    if (!payload || !payload->valid) {
        return false;
    }
    if (payload->materialId == MATERIAL_PRESET_TRANSPARENT) {
        return true;
    }
    if (payload->thinWalled) {
        return true;
    }
    if (payload->opticalIor > 1.0001) {
        return true;
    }
    if (fabs(payload->absorptionDistance - 1.0) > 1e-6) {
        return true;
    }
    return false;
}

RuntimeDisneyV2_3DTransparentPolicy runtime_disney_v2_3d_resolve_transparent_policy(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled,
    double segment_distance) {
    RuntimeDisneyV2_3DTransparentPolicy policy = {0};
    double absorption_distance = 1.0;
    double distance_ratio = 1.0;

    policy.transmissionWeight = runtime_disney_v2_3d_transparent_surface_weight(principled);
    policy.visibleWeight = runtime_disney_v2_3d_transparent_layer_visible_weight(principled);
    policy.tintR = principled ? runtime_disney_v2_transmission_3d_clamp01(principled->baseColorR)
                              : 1.0;
    policy.tintG = principled ? runtime_disney_v2_transmission_3d_clamp01(principled->baseColorG)
                              : 1.0;
    policy.tintB = principled ? runtime_disney_v2_transmission_3d_clamp01(principled->baseColorB)
                              : 1.0;
    policy.thinWalled = payload ? payload->thinWalled : false;
    policy.physicalTransmission =
        runtime_disney_v2_3d_policy_is_physical_transmission(payload, principled);
    policy.alphaOnly =
        runtime_disney_v2_3d_payload_has_transparent_alpha(payload, principled) &&
        !policy.physicalTransmission;
    if (!payload || payload->thinWalled || policy.alphaOnly) {
        return policy;
    }

    absorption_distance =
        fmax(payload->absorptionDistance, kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon);
    distance_ratio = fmax(segment_distance, 0.0) / absorption_distance;
    policy.tintR = pow(fmax(policy.tintR, kRuntimeDisneyV2_3DMinimumTransmittance),
                       distance_ratio);
    policy.tintG = pow(fmax(policy.tintG, kRuntimeDisneyV2_3DMinimumTransmittance),
                       distance_ratio);
    policy.tintB = pow(fmax(policy.tintB, kRuntimeDisneyV2_3DMinimumTransmittance),
                       distance_ratio);
    return policy;
}

RuntimeRenderTraceCostTransmissionSurfaceKind3D
runtime_disney_v2_3d_transmission_surface_kind(
    const RuntimeDisneyV2_3DTransparentPolicy* transparent_policy) {
    if (!transparent_policy) {
        return RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_UNKNOWN;
    }
    if (transparent_policy->alphaOnly) {
        return RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_ALPHA_ONLY;
    }
    if (transparent_policy->thinWalled) {
        return RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_THIN_WALLED;
    }
    if (transparent_policy->physicalTransmission) {
        return RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_SOLID_PHYSICAL;
    }
    return RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SURFACE_SOLID_NONPHYSICAL;
}

RuntimeRenderTraceCostTransmissionScreenRegion3D
runtime_disney_v2_3d_transmission_screen_region(const RuntimeDisneyV2_3DResult* result) {
    if (!result || !result->tracePixelContextResolved ||
        result->tracePixelWidth <= 0 || result->tracePixelHeight <= 0) {
        return RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_UNKNOWN;
    }
    const bool right = result->tracePixelX >= (result->tracePixelWidth / 2);
    const bool bottom = result->tracePixelY >= (result->tracePixelHeight / 2);
    if (bottom) {
        return right ? RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_BOTTOM_RIGHT
                     : RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_BOTTOM_LEFT;
    }
    return right ? RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_TOP_RIGHT
                 : RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_TOP_LEFT;
}

RuntimeRenderTraceCostTransmissionPixelStability3D
runtime_disney_v2_3d_transmission_pixel_stability(
    const RuntimeNative3DSamplingContext* sampling) {
    if (!sampling || sampling->temporalSubpassCount <= 1u) {
        return RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_UNKNOWN;
    }
    if (sampling->temporalSubpassIndex == 0u) {
        return RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_FIRST_SUBPASS;
    }
    if (sampling->temporalSubpassIndex + 1u >= sampling->temporalSubpassCount) {
        return RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_LATE_SUBPASS;
    }
    return RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_EARLY_SUBPASS;
}
