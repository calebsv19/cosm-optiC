#include "render/runtime_direct_light_internal_3d.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "render/runtime_native_3d_sampling.h"

static const double kRuntimeDirectLight3DContributionEpsilon = 1e-8;
static const double kRuntimeDirectLight3DVisibilityClearLuma = 0.995;
static const double kRuntimeDirectLight3DVisibilityProbeStrictClearLuma = 0.9999;
static const double kRuntimeDirectLight3DVisibilityBlockedLuma = 0.005;
static const double kRuntimeDirectLight3DVisibilityStablePartialSpan = 0.02;
static const double kRuntimeDirectLight3DMaterialEmitterRectLowImportancePeak = 0.01;
#define RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_POPULATION_COUNT 16
#define RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_ACTIVE_COUNT 8
#define RUNTIME_DIRECT_LIGHT_3D_MATERIAL_EMITTER_RECT_SAMPLE_COUNT 6
#define RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_DECISION_COUNT 4

typedef struct {
    double x;
    double y;
} RuntimeDirectLight3DDiskSample;

static const RuntimeDirectLight3DDiskSample kRuntimeDirectLight3DAreaLightSamples[] = {
    {0.36, 0.0},
    {0.255, 0.255},
    {0.0, 0.36},
    {-0.255, 0.255},
    {-0.36, 0.0},
    {-0.255, -0.255},
    {0.0, -0.36},
    {0.255, -0.255},
    {0.78, 0.0},
    {0.552, 0.552},
    {0.0, 0.78},
    {-0.552, 0.552},
    {-0.78, 0.0},
    {-0.552, -0.552},
    {0.0, -0.78},
    {0.552, -0.552},
};

static void runtime_direct_light_3d_map_square_to_disk(double u,
                                                       double v,
                                                       double* out_x,
                                                       double* out_y) {
    const double a = 2.0 * u - 1.0;
    const double b = 2.0 * v - 1.0;
    double r = 0.0;
    double phi = 0.0;

    if (!out_x || !out_y) return;
    if (fabs(a) < 1e-9 && fabs(b) < 1e-9) {
        *out_x = 0.0;
        *out_y = 0.0;
        return;
    }
    if (fabs(a) > fabs(b)) {
        r = a;
        phi = (M_PI / 4.0) * (b / a);
    } else {
        r = b;
        phi = (M_PI / 2.0) - (M_PI / 4.0) * (a / b);
    }
    *out_x = r * cos(phi);
    *out_y = r * sin(phi);
}

static double runtime_direct_light_3d_attenuation(
    const RuntimeLight3D* light,
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double light_distance) {
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double zero_length = 0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double unit_length = 1.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double falloff = unit_length;
    double normalized = 0.0;
    if (!light) return 0.0;

    falloff = light->falloffDistance;
    if (!(falloff > zero_length)) {
        falloff = unit_length;
    }
    normalized = light_distance / falloff;

    switch (light->falloffMode) {
        case FORWARD_FALLOFF_MODE_LINEAR:
            return 1.0 / (1.0 + normalized);
        case FORWARD_FALLOFF_MODE_NONE:
            return 1.0;
        case FORWARD_FALLOFF_MODE_QUADRATIC:
        default:
            return 1.0 / (1.0 + normalized * normalized);
    }
}

static bool runtime_direct_light_3d_sample_contributes(double light_intensity,
                                                       double attenuation,
                                                       double ndotl) {
    return light_intensity * attenuation * ndotl > kRuntimeDirectLight3DContributionEpsilon;
}

static bool runtime_direct_light_3d_clear_visible_decision_probe_enabled(void) {
    const char* value =
        getenv("RAY_TRACING_DIRECT_LIGHT_CLEAR_VISIBLE_DECISION_SAMPLE_PROBE");
    return value && value[0] != '\0' && value[0] != '0';
}

static Vec3 runtime_direct_light_3d_default_tangent(Vec3 normal) {
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon = 1e-9;
    Vec3 guide = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3_cross(guide, normal);
    if (vec3_length(tangent) <= length_epsilon) {
        tangent = vec3(1.0, 0.0, 0.0);
    }
    return vec3_normalize(tangent);
}

static void runtime_direct_light_3d_build_basis(Vec3 normal,
                                                Vec3* out_tangent,
                                                Vec3* out_bitangent) {
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon = 1e-9;
    Vec3 tangent = runtime_direct_light_3d_default_tangent(normal);
    Vec3 bitangent = vec3_normalize(vec3_cross(normal, tangent));

    if (vec3_length(bitangent) <= length_epsilon) {
        tangent = vec3(1.0, 0.0, 0.0);
        bitangent = vec3(0.0, 0.0, 1.0);
    }

    if (out_tangent) *out_tangent = tangent;
    if (out_bitangent) *out_bitangent = bitangent;
}

static uint32_t runtime_direct_light_3d_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint32_t runtime_direct_light_3d_seed_from_light_hit(
    const RuntimeLight3D* light,
    const HitInfo3D* hit) {
    uint32_t seed = 0x6d2b79f5U;
    if (light) {
        seed ^= runtime_direct_light_3d_hash_u32((uint32_t)(light->position.x * 4096.0));
        seed ^= runtime_direct_light_3d_hash_u32((uint32_t)(light->position.y * 4096.0) ^
                                                 0x9e3779b9U);
        seed ^= runtime_direct_light_3d_hash_u32((uint32_t)(light->position.z * 4096.0) ^
                                                 0x85ebca6bU);
        seed ^= runtime_direct_light_3d_hash_u32((uint32_t)(light->radius * 4096.0) ^
                                                 0xc2b2ae35U);
    }
    if (hit) {
        seed ^= runtime_direct_light_3d_hash_u32((uint32_t)(hit->position.x * 4096.0) ^
                                                 0x27d4eb2fU);
        seed ^= runtime_direct_light_3d_hash_u32((uint32_t)(hit->position.y * 4096.0) ^
                                                 0x165667b1U);
        seed ^= runtime_direct_light_3d_hash_u32((uint32_t)(hit->position.z * 4096.0) ^
                                                 0xd3a2646cU);
        seed ^= runtime_direct_light_3d_hash_u32((uint32_t)(hit->triangleIndex + 1) *
                                                 83492791U);
        seed ^= runtime_direct_light_3d_hash_u32((uint32_t)(hit->primitiveIndex + 1) *
                                                 2654435761U);
    }
    return runtime_direct_light_3d_hash_u32(seed);
}

static int runtime_direct_light_3d_area_light_sample_count(const RuntimeLight3D* light) {
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon = 1e-9;
    if (!light || !(light->radius > length_epsilon)) {
        return 1;
    }
    return RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_ACTIVE_COUNT;
}

static bool runtime_direct_light_3d_uses_material_emitter_rect_budget(
    const RuntimeLightSource3D* source) {
    return source && source->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_RECT &&
           source->origin == RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER &&
           source->emissionProfile == RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED;
}

static int runtime_direct_light_3d_source_area_light_sample_count(
    const RuntimeLightSource3D* source,
    const RuntimeLight3D* light) {
    const int base_count = runtime_direct_light_3d_area_light_sample_count(light);
    if (base_count > RUNTIME_DIRECT_LIGHT_3D_MATERIAL_EMITTER_RECT_SAMPLE_COUNT &&
        runtime_direct_light_3d_uses_material_emitter_rect_budget(source)) {
        return RUNTIME_DIRECT_LIGHT_3D_MATERIAL_EMITTER_RECT_SAMPLE_COUNT;
    }
    return base_count;
}

static bool runtime_direct_light_3d_material_emitter_rect_can_stop_low_importance(
    const RuntimeLightSource3D* source,
    int evaluated_count,
    int decision_count,
    int max_count,
    double source_r_sum,
    double source_g_sum,
    double source_b_sum) {
    double inv_evaluated = 0.0;
    double provisional_peak = 0.0;
    if (!runtime_direct_light_3d_uses_material_emitter_rect_budget(source)) {
        return false;
    }
    if (decision_count <= 0 || evaluated_count != decision_count ||
        decision_count >= max_count) {
        return false;
    }
    inv_evaluated = 1.0 / (double)evaluated_count;
    provisional_peak = runtime_direct_light_3d_peak(source_r_sum * inv_evaluated,
                                                   source_g_sum * inv_evaluated,
                                                   source_b_sum * inv_evaluated);
    return provisional_peak <= kRuntimeDirectLight3DMaterialEmitterRectLowImportancePeak;
}

static int runtime_direct_light_3d_area_light_decision_count_for_samples(int sample_count) {
    if (sample_count < RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_DECISION_COUNT) {
        return sample_count;
    }
    return RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_DECISION_COUNT;
}

static int runtime_direct_light_3d_clear_visible_decision_count_for_samples(
    int sample_count,
    int default_decision_count,
    bool receiver_is_transparent) {
    if (!runtime_direct_light_3d_clear_visible_decision_probe_enabled()) {
        return default_decision_count;
    }
    if (receiver_is_transparent) {
        return default_decision_count;
    }
    if (sample_count <= 2) {
        return sample_count;
    }
    return 2;
}

static RuntimeRenderTraceCostDirectLightStopReason3D
runtime_direct_light_3d_area_light_stop_reason(
    int evaluated_count,
    int decision_count,
    int clear_visible_decision_count,
    int max_count,
    int clear_visible_count,
    int clear_blocked_count,
    int visibility_trace_count,
    double transmittance_luma_min,
    double transmittance_luma_max) {
    if (decision_count <= 0 || decision_count >= max_count) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_FULL_SAMPLE_COUNT;
    }
    if (clear_visible_decision_count > 0 &&
        clear_visible_decision_count < decision_count &&
        evaluated_count == clear_visible_decision_count &&
        clear_visible_count == clear_visible_decision_count &&
        visibility_trace_count == clear_visible_decision_count &&
        transmittance_luma_min >= kRuntimeDirectLight3DVisibilityProbeStrictClearLuma) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_ALL_CLEAR;
    }
    if (evaluated_count != decision_count) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_FULL_SAMPLE_COUNT;
    }
    if (clear_visible_count == decision_count || clear_blocked_count == decision_count) {
        return clear_visible_count == decision_count
                   ? RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_ALL_CLEAR
                   : RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_ALL_BLOCKED;
    }
    if (visibility_trace_count == decision_count &&
        transmittance_luma_min > kRuntimeDirectLight3DVisibilityBlockedLuma &&
        transmittance_luma_max < kRuntimeDirectLight3DVisibilityClearLuma &&
        transmittance_luma_max - transmittance_luma_min <=
            kRuntimeDirectLight3DVisibilityStablePartialSpan) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_STABLE_PARTIAL;
    }
    return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_FULL_SAMPLE_COUNT;
}

static bool runtime_direct_light_3d_area_light_update_stop_reason(
    int evaluated_count,
    int decision_count,
    int clear_visible_decision_count,
    int max_count,
    int clear_visible_count,
    int clear_blocked_count,
    int visibility_trace_count,
    double transmittance_luma_min,
    double transmittance_luma_max,
    RuntimeRenderTraceCostDirectLightStopReason3D* io_stop_reason) {
    RuntimeRenderTraceCostDirectLightStopReason3D reason =
        runtime_direct_light_3d_area_light_stop_reason(evaluated_count,
                                                       decision_count,
                                                       clear_visible_decision_count,
                                                       max_count,
                                                       clear_visible_count,
                                                       clear_blocked_count,
                                                       visibility_trace_count,
                                                       transmittance_luma_min,
                                                       transmittance_luma_max);
    if (reason == RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_FULL_SAMPLE_COUNT) {
        return false;
    }
    if (io_stop_reason) {
        *io_stop_reason = reason;
    }
    return true;
}

static int runtime_direct_light_3d_select_area_light_population_index(
    const RuntimeLight3D* light,
    const HitInfo3D* hit,
    int sample_index,
    const RuntimeNative3DSamplingContext* sampling) {
    const int population_count = RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_POPULATION_COUNT;
    uint32_t seed = runtime_direct_light_3d_seed_from_light_hit(light, hit);
    uint32_t sequence = sampling ? sampling->sampleSequence : 0u;
    int offset = 0;
    int stride = 0;

    if (sample_index < 0) sample_index = 0;
    seed = runtime_direct_light_3d_hash_u32(seed ^ runtime_direct_light_3d_hash_u32(sequence));
    offset = (int)(seed % (uint32_t)population_count);
    stride = 1 + (int)((seed >> 5) % (uint32_t)(population_count - 1));
    if ((stride & 1) == 0) {
        stride += 1;
    }
    if (stride >= population_count) {
        stride = 1;
    }
    return (offset + sample_index * stride) % population_count;
}

static Vec3 runtime_direct_light_3d_sample_light_position(const RuntimeLight3D* light,
                                                          Vec3 center_dir,
                                                          const HitInfo3D* hit,
                                                          int sample_index,
                                                          const RuntimeNative3DSamplingContext* sampling) {
    Vec3 tangent = vec3(0.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 0.0);
    RuntimeDirectLight3DDiskSample sample = {0.0, 0.0};
    int population_index = 0;
    double radius = 0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon = 1e-9;

    if (!light || !(light->radius > length_epsilon)) {
        return light ? light->position : vec3(0.0, 0.0, 0.0);
    }
    runtime_direct_light_3d_build_basis(center_dir, &tangent, &bitangent);
    population_index = runtime_direct_light_3d_select_area_light_population_index(light,
                                                                                  hit,
                                                                                  sample_index,
                                                                                  sampling);
    radius = light->radius;
    if (sampling) {
        const uint32_t base_seed = runtime_direct_light_3d_seed_from_light_hit(light, hit);
        double u = 0.5;
        double v = 0.5;
        double disk_x = 0.0;
        double disk_y = 0.0;

        RuntimeNative3DSampling_Stratified2D(sampling,
                                             base_seed,
                                             RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_POPULATION_COUNT,
                                             population_index,
                                             (uint32_t)(population_index + 17),
                                             &u,
                                             &v);
        runtime_direct_light_3d_map_square_to_disk(u, v, &disk_x, &disk_y);
        return vec3_add(light->position,
                        vec3_add(vec3_scale(tangent, disk_x * radius),
                                 vec3_scale(bitangent, disk_y * radius)));
    }
    sample = kRuntimeDirectLight3DAreaLightSamples[population_index];
    return vec3_add(light->position,
                    vec3_add(vec3_scale(tangent, sample.x * radius),
                             vec3_scale(bitangent, sample.y * radius)));
}

static Vec3 runtime_direct_light_3d_safe_axis(Vec3 axis, Vec3 fallback) {
    if (vec3_length(axis) > 1e-9) {
        return vec3_normalize(axis);
    }
    if (vec3_length(fallback) > 1e-9) {
        return vec3_normalize(fallback);
    }
    return vec3(1.0, 0.0, 0.0);
}

static Vec3 runtime_direct_light_3d_sample_rect_source_position(
    const RuntimeLightSource3D* source,
    const RuntimeLight3D* light,
    const HitInfo3D* hit,
    int sample_index,
    const RuntimeNative3DSamplingContext* sampling) {
    Vec3 axis_u = vec3(1.0, 0.0, 0.0);
    Vec3 axis_v = vec3(0.0, 0.0, 1.0);
    int population_index = 0;
    double offset_u = 0.0;
    double offset_v = 0.0;

    if (!source || !light || source->width <= 0.0 || source->height <= 0.0) {
        return light ? light->position : vec3(0.0, 0.0, 0.0);
    }

    axis_u = runtime_direct_light_3d_safe_axis(source->axisU, vec3(1.0, 0.0, 0.0));
    axis_v = runtime_direct_light_3d_safe_axis(source->axisV, vec3(0.0, 0.0, 1.0));
    population_index = runtime_direct_light_3d_select_area_light_population_index(light,
                                                                                  hit,
                                                                                  sample_index,
                                                                                  sampling);
    if (sampling) {
        const uint32_t base_seed = runtime_direct_light_3d_seed_from_light_hit(light, hit);
        double u = 0.5;
        double v = 0.5;

        RuntimeNative3DSampling_Stratified2D(sampling,
                                             base_seed,
                                             RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_POPULATION_COUNT,
                                             population_index,
                                             (uint32_t)(population_index + 31),
                                             &u,
                                             &v);
        offset_u = (u - 0.5) * source->width;
        offset_v = (v - 0.5) * source->height;
    } else {
        const RuntimeDirectLight3DDiskSample sample =
            kRuntimeDirectLight3DAreaLightSamples[population_index];
        offset_u = sample.x * source->width * 0.5;
        offset_v = sample.y * source->height * 0.5;
    }

    return vec3_add(source->position,
                    vec3_add(vec3_scale(axis_u, offset_u),
                             vec3_scale(axis_v, offset_v)));
}

static Vec3 runtime_direct_light_3d_sample_light_source_position(
    const RuntimeLightSource3D* source,
    const RuntimeLight3D* light,
    Vec3 center_dir,
    const HitInfo3D* hit,
    int sample_index,
    const RuntimeNative3DSamplingContext* sampling) {
    if (source && source->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_RECT) {
        return runtime_direct_light_3d_sample_rect_source_position(source,
                                                                  light,
                                                                  hit,
                                                                  sample_index,
                                                                  sampling);
    }
    return runtime_direct_light_3d_sample_light_position(light,
                                                        center_dir,
                                                        hit,
                                                        sample_index,
                                                        sampling);
}

static double runtime_direct_light_3d_light_source_radius(
    const RuntimeLightSource3D* source) {
    if (!source) return 0.0;
    if (source->radius > 0.0) return source->radius;
    switch (source->kind) {
        case RUNTIME_LIGHT_SOURCE_3D_KIND_DISK:
            return source->width > 0.0 ? source->width * 0.5 : 0.0;
        case RUNTIME_LIGHT_SOURCE_3D_KIND_RECT:
            if (source->width > 0.0 && source->height > 0.0) {
                return 0.5 * sqrt(source->width * source->width +
                                  source->height * source->height);
            }
            return 0.0;
        case RUNTIME_LIGHT_SOURCE_3D_KIND_POINT:
        case RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE:
        case RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE:
        default:
            return 0.0;
    }
}

static double runtime_direct_light_3d_source_facing(
    const RuntimeLightSource3D* source,
    Vec3 sample_dir_from_hit) {
    Vec3 normal = vec3(0.0, 0.0, 0.0);
    if (!source || source->kind != RUNTIME_LIGHT_SOURCE_3D_KIND_RECT) {
        return 1.0;
    }
    normal = source->normal;
    if (vec3_length(normal) <= 1e-9) {
        return 1.0;
    }
    normal = vec3_normalize(normal);
    return fmax(0.0, vec3_dot(normal, vec3_scale(sample_dir_from_hit, -1.0)));
}

static double runtime_direct_light_3d_emission_profile_weight(
    const RuntimeLightSource3D* source,
    Vec3 sample_dir_from_hit) {
    Vec3 normal = vec3(0.0, 0.0, 0.0);
    double facing = 1.0;

    if (!source || source->kind != RUNTIME_LIGHT_SOURCE_3D_KIND_RECT) {
        return 1.0;
    }
    normal = source->normal;
    if (vec3_length(normal) <= 1e-9) {
        return 1.0;
    }
    normal = vec3_normalize(normal);
    facing = vec3_dot(normal, vec3_scale(sample_dir_from_hit, -1.0));

    switch (source->emissionProfile) {
        case RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED:
            return fmax(0.0, facing);
        case RUNTIME_LIGHT_SOURCE_3D_EMISSION_TWO_SIDED:
            return fabs(facing);
        case RUNTIME_LIGHT_SOURCE_3D_EMISSION_OMNI:
        default:
            return 1.0;
    }
}

static RuntimeLight3D runtime_direct_light_3d_compat_light_from_source(
    const RuntimeLightSource3D* source) {
    RuntimeLight3D light = {0};
    if (!source) return light;
    light.position = source->position;
    light.radius = runtime_direct_light_3d_light_source_radius(source);
    light.intensity = source->intensity;
    light.falloffDistance = source->falloffDistance;
    light.falloffMode = source->falloffMode;
    return light;
}

static RuntimeRenderTraceCostDirectLightSourceKind3D
runtime_direct_light_3d_ledger_source_kind(const RuntimeLightSource3D* source) {
    if (!source) return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_UNKNOWN;
    switch (source->kind) {
        case RUNTIME_LIGHT_SOURCE_3D_KIND_POINT:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_POINT;
        case RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_SPHERE;
        case RUNTIME_LIGHT_SOURCE_3D_KIND_DISK:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_DISK;
        case RUNTIME_LIGHT_SOURCE_3D_KIND_RECT:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_RECT;
        case RUNTIME_LIGHT_SOURCE_3D_KIND_MESH_EMISSIVE:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_MESH_EMISSIVE;
        default:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_UNKNOWN;
    }
}

static RuntimeRenderTraceCostDirectLightSourceOrigin3D
runtime_direct_light_3d_ledger_source_origin(const RuntimeLightSource3D* source) {
    if (!source) return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_UNKNOWN;
    switch (source->origin) {
        case RUNTIME_LIGHT_SOURCE_3D_ORIGIN_COMPAT_SCENE_LIGHT:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_COMPAT_SCENE_LIGHT;
        case RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_AUTHORED_LIGHT;
        case RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_MATERIAL_EMITTER;
        default:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_UNKNOWN;
    }
}

static RuntimeRenderTraceCostDirectLightEmissionProfile3D
runtime_direct_light_3d_ledger_emission_profile(const RuntimeLightSource3D* source) {
    if (!source) return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_UNKNOWN;
    switch (source->emissionProfile) {
        case RUNTIME_LIGHT_SOURCE_3D_EMISSION_OMNI:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_OMNI;
        case RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_ONE_SIDED;
        case RUNTIME_LIGHT_SOURCE_3D_EMISSION_TWO_SIDED:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_TWO_SIDED;
        default:
            return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_UNKNOWN;
    }
}

static RuntimeRenderTraceCostDirectLightOutcome3D runtime_direct_light_3d_ledger_outcome(
    int clear_visible_count,
    int clear_blocked_count,
    int visibility_trace_count,
    double transmittance_luma_min,
    double transmittance_luma_max) {
    if (visibility_trace_count <= 0) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_NO_VISIBILITY_TRACE;
    }
    if (clear_visible_count == visibility_trace_count) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_CLEAR_VISIBLE;
    }
    if (clear_blocked_count == visibility_trace_count) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_CLEAR_BLOCKED;
    }
    if (transmittance_luma_min > kRuntimeDirectLight3DVisibilityBlockedLuma &&
        transmittance_luma_max < kRuntimeDirectLight3DVisibilityClearLuma &&
        transmittance_luma_max - transmittance_luma_min <=
            kRuntimeDirectLight3DVisibilityStablePartialSpan) {
        return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_STABLE_PARTIAL;
    }
    return RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_MIXED_PARTIAL;
}

static void runtime_direct_light_3d_record_result_outcome(
    RuntimeDirectLight3DResult* result,
    RuntimeRenderTraceCostDirectLightOutcome3D outcome) {
    if (!result) return;
    switch (outcome) {
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_NO_VISIBILITY_TRACE:
            result->visibilityOutcomeNoTraceCount += 1;
            break;
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_CLEAR_VISIBLE:
            result->visibilityOutcomeClearVisibleCount += 1;
            break;
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_CLEAR_BLOCKED:
            result->visibilityOutcomeClearBlockedCount += 1;
            break;
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_STABLE_PARTIAL:
            result->visibilityOutcomeStablePartialCount += 1;
            break;
        case RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_MIXED_PARTIAL:
            result->visibilityOutcomeMixedPartialCount += 1;
            break;
        default:
            break;
    }
}

void runtime_direct_light_3d_accumulate_source(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeLightSource3D* source,
    bool receiver_is_transparent,
    RuntimeRenderTraceCostDirectLightCaller3D caller,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDirectLight3DResult* io_result,
    double* io_light_r,
    double* io_light_g,
    double* io_light_b,
    bool* io_any_light_sample_visible) {
    RuntimeLight3D light = {0};
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double light_distance = 0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon = 1e-9;
    double attenuation = 0.0;
    double ndotl = 0.0;
    double source_r = 0.0;
    double source_g = 0.0;
    double source_b = 0.0;
    double source_peak = 0.0;
    bool source_visible = false;
    int light_sample_count = 0;
    int light_sample_decision_count = 0;
    int clear_visible_decision_count = 0;
    int light_sample_evaluated_count = 0;
    int clear_visible_sample_count = 0;
    int clear_blocked_sample_count = 0;
    int visibility_trace_count = 0;
    double transmittance_luma_min = 1.0e30;
    double transmittance_luma_max = -1.0e30;
    RuntimeRenderTraceCostDirectLightStopReason3D stop_reason =
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_FULL_SAMPLE_COUNT;

    if (!scene || !hit || !source || !source->enabled || !io_result ||
        !io_light_r || !io_light_g || !io_light_b || !io_any_light_sample_visible) {
        return;
    }

    light = runtime_direct_light_3d_compat_light_from_source(source);
    io_result->evaluatedLightCount += 1;
    to_light = vec3_sub(light.position, hit->position);
    light_distance = vec3_length(to_light);
    if (io_result->evaluatedLightCount == 1) {
        io_result->lightDistance = light_distance;
    }
    if (light_distance <= length_epsilon) {
        source_r = light.intensity * source->color.x;
        source_g = light.intensity * source->color.y;
        source_b = light.intensity * source->color.z;
        source_peak = runtime_direct_light_3d_peak(source_r, source_g, source_b);
        *io_light_r += source_r;
        *io_light_g += source_g;
        *io_light_b += source_b;
        io_result->visible = true;
        io_result->ndotl = 1.0;
        io_result->attenuation = 1.0;
        io_result->lightDistance = light_distance;
        if (source_peak > kRuntimeDirectLight3DContributionEpsilon) {
            io_result->contributingLightCount += 1;
            if (source_peak > io_result->peakLightContribution) {
                io_result->peakLightContribution = source_peak;
            }
        }
        return;
    }

    to_light = vec3_scale(to_light, 1.0 / light_distance);
    ndotl = fabs(vec3_dot(hit->normal, to_light));
    attenuation = runtime_direct_light_3d_attenuation(&light, light_distance);
    if (io_result->evaluatedLightCount == 1) {
        io_result->ndotl = ndotl;
        io_result->attenuation = attenuation;
    }

    light_sample_count = runtime_direct_light_3d_source_area_light_sample_count(source, &light);
    light_sample_decision_count =
        runtime_direct_light_3d_area_light_decision_count_for_samples(light_sample_count);
    clear_visible_decision_count =
        runtime_direct_light_3d_clear_visible_decision_count_for_samples(
            light_sample_count,
            light_sample_decision_count,
            receiver_is_transparent);
    if (ndotl > 0.0 && light_sample_count > 0) {
        for (int i = 0; i < light_sample_count; ++i) {
            RuntimeVisibility3DTransmittance transmittance = {0};
            int target_scene_object_index = -1;
            int target_triangle_index = -1;
            Vec3 sample_position = runtime_direct_light_3d_sample_light_source_position(source,
                                                                                       &light,
                                                                                       to_light,
                                                                                       hit,
                                                                                       i,
                                                                                       sampling);
            Vec3 sample_to_light = vec3_sub(sample_position, hit->position);
            [[fisics::dim(length)]] [[fisics::unit(meter)]] double sample_distance =
                vec3_length(sample_to_light);
            double sample_attenuation = 0.0;
            double sample_ndotl = 0.0;
            double sample_source_facing = 1.0;
            double sample_emission_weight = 1.0;
            Vec3 sample_dir = vec3(0.0, 0.0, 0.0);

            light_sample_evaluated_count += 1;
            if (!(sample_distance > length_epsilon)) {
                clear_blocked_sample_count += 1;
                if (runtime_direct_light_3d_area_light_update_stop_reason(
                        light_sample_evaluated_count,
                        light_sample_decision_count,
                        clear_visible_decision_count,
                        light_sample_count,
                        clear_visible_sample_count,
                        clear_blocked_sample_count,
                        visibility_trace_count,
                        transmittance_luma_min,
                        transmittance_luma_max,
                        &stop_reason)) {
                    break;
                }
                continue;
            }
            sample_dir = vec3_scale(sample_to_light, 1.0 / sample_distance);
            sample_ndotl = fabs(vec3_dot(hit->normal, sample_dir));
            sample_source_facing =
                runtime_direct_light_3d_source_facing(source, sample_dir);
            sample_emission_weight =
                runtime_direct_light_3d_emission_profile_weight(source, sample_dir);
            if (source->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_RECT) {
                io_result->rectSampleCount += 1;
                io_result->rectReceiverCosSum += sample_ndotl;
                io_result->rectEmitterCosSum += sample_source_facing;
                if (!(sample_source_facing > 0.0)) {
                    io_result->rectBackfaceSampleCount += 1;
                }
            }
            if (!(sample_ndotl > 0.0)) {
                clear_blocked_sample_count += 1;
                if (runtime_direct_light_3d_area_light_update_stop_reason(
                        light_sample_evaluated_count,
                        light_sample_decision_count,
                        clear_visible_decision_count,
                        light_sample_count,
                        clear_visible_sample_count,
                        clear_blocked_sample_count,
                        visibility_trace_count,
                        transmittance_luma_min,
                        transmittance_luma_max,
                        &stop_reason)) {
                    break;
                }
                continue;
            }
            if (!(sample_emission_weight > 0.0)) {
                clear_blocked_sample_count += 1;
                if (runtime_direct_light_3d_area_light_update_stop_reason(
                        light_sample_evaluated_count,
                        light_sample_decision_count,
                        clear_visible_decision_count,
                        light_sample_count,
                        clear_visible_sample_count,
                        clear_blocked_sample_count,
                        visibility_trace_count,
                        transmittance_luma_min,
                        transmittance_luma_max,
                        &stop_reason)) {
                    break;
                }
                continue;
            }

            sample_attenuation = runtime_direct_light_3d_attenuation(&light, sample_distance);
            if (!runtime_direct_light_3d_sample_contributes(light.intensity * sample_emission_weight,
                                                            sample_attenuation,
                                                            sample_ndotl)) {
                clear_blocked_sample_count += 1;
                if (runtime_direct_light_3d_area_light_update_stop_reason(
                        light_sample_evaluated_count,
                        light_sample_decision_count,
                        clear_visible_decision_count,
                        light_sample_count,
                        clear_visible_sample_count,
                        clear_blocked_sample_count,
                        visibility_trace_count,
                        transmittance_luma_min,
                        transmittance_luma_max,
                        &stop_reason)) {
                    break;
                }
                continue;
            }

            io_result->visibilityRayCount += 1;
            if (source->origin == RUNTIME_LIGHT_SOURCE_3D_ORIGIN_MATERIAL_EMITTER &&
                source->sourceSceneObjectIndex >= 0) {
                target_scene_object_index = source->sourceSceneObjectIndex;
                target_triangle_index = source->sourceTriangleIndex;
            }
            transmittance = RuntimeVisibility3D_TransmittanceFromHitToPointRGB(scene,
                                                                                hit,
                                                                                sample_position,
                                                                                target_scene_object_index,
                                                                                target_triangle_index);
            visibility_trace_count += 1;
            if (transmittance.luma < transmittance_luma_min) {
                transmittance_luma_min = transmittance.luma;
            }
            if (transmittance.luma > transmittance_luma_max) {
                transmittance_luma_max = transmittance.luma;
            }
            if (transmittance.luma > 1e-6) {
                source_visible = true;
                *io_any_light_sample_visible = true;
                if (source->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_RECT) {
                    io_result->rectVisibleSampleCount += 1;
                }
            }
            if (transmittance.luma >= kRuntimeDirectLight3DVisibilityClearLuma) {
                clear_visible_sample_count += 1;
            } else if (transmittance.luma <= kRuntimeDirectLight3DVisibilityBlockedLuma) {
                clear_blocked_sample_count += 1;
            }
            source_r += light.intensity * sample_attenuation * sample_ndotl *
                        sample_emission_weight * transmittance.r * source->color.x;
            source_g += light.intensity * sample_attenuation * sample_ndotl *
                        sample_emission_weight * transmittance.g * source->color.y;
            source_b += light.intensity * sample_attenuation * sample_ndotl *
                        sample_emission_weight * transmittance.b * source->color.z;
            if (runtime_direct_light_3d_material_emitter_rect_can_stop_low_importance(
                    source,
                    light_sample_evaluated_count,
                    light_sample_decision_count,
                    light_sample_count,
                    source_r,
                    source_g,
                    source_b)) {
                stop_reason = RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_LOW_IMPORTANCE;
                break;
            }
            if (runtime_direct_light_3d_area_light_update_stop_reason(
                    light_sample_evaluated_count,
                    light_sample_decision_count,
                    clear_visible_decision_count,
                    light_sample_count,
                    clear_visible_sample_count,
                    clear_blocked_sample_count,
                    visibility_trace_count,
                    transmittance_luma_min,
                    transmittance_luma_max,
                    &stop_reason)) {
                break;
            }
        }
        if (light_sample_evaluated_count <= 0) {
            light_sample_evaluated_count = light_sample_count;
        }
        source_r /= (double)light_sample_evaluated_count;
        source_g /= (double)light_sample_evaluated_count;
        source_b /= (double)light_sample_evaluated_count;
    }

    source_peak = runtime_direct_light_3d_peak(source_r, source_g, source_b);
    *io_light_r += source_r;
    *io_light_g += source_g;
    *io_light_b += source_b;
    if (source_visible && source_peak > kRuntimeDirectLight3DContributionEpsilon) {
        io_result->contributingLightCount += 1;
    }
    if (source_peak > io_result->peakLightContribution) {
        io_result->peakLightContribution = source_peak;
        io_result->lightDistance = light_distance;
        io_result->ndotl = ndotl;
        io_result->attenuation = attenuation;
    }
    {
        RuntimeRenderTraceCostDirectLightOutcome3D outcome =
            runtime_direct_light_3d_ledger_outcome(clear_visible_sample_count,
                                                   clear_blocked_sample_count,
                                                   visibility_trace_count,
                                                   transmittance_luma_min,
                                                   transmittance_luma_max);
        runtime_direct_light_3d_record_result_outcome(io_result, outcome);
        RuntimeRenderTraceCostLedger3D_RecordDirectLightVisibilityPolicy(
            caller,
            runtime_direct_light_3d_ledger_source_kind(source),
            runtime_direct_light_3d_ledger_source_origin(source),
            runtime_direct_light_3d_ledger_emission_profile(source),
            outcome,
            stop_reason,
            light_sample_count,
            light_sample_decision_count,
            light_sample_evaluated_count,
            visibility_trace_count,
            light_distance,
            source_peak,
            transmittance_luma_min,
            transmittance_luma_max);
    }
}
