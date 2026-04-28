#include "render/runtime_emission_transparency_3d.h"

#include <math.h>
#include <stdint.h>

#include "render/runtime_emissive_direct_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_visibility_3d.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double kRuntimeEmissionTransparency3DEpsilon = 1e-4;
static const double kRuntimeEmissionTransparency3DMaxDistance = 8.0;
static const double kRuntimeEmissionTransparency3DEnergyScale = 0.75;
static const double kRuntimeEmissionTransparency3DTransmissionMaxDistance = 32.0;
static const double kRuntimeEmissionTransparency3DMinimumFrontWeight = 0.2;
static const int kRuntimeEmissionTransparency3DMaxTransmissionSurfaceSkips = 16;
static const double kRuntimeEmissionTransparency3DTransmissionConeBase = 0.015;
static const double kRuntimeEmissionTransparency3DTransmissionConeScale = 0.12;

typedef struct {
    bool hit;
    bool emitterWins;
    RuntimeMaterialResponse3DResult materialResult;
    RuntimeMaterialPayload3D materialPayload;
    RuntimeLightEmitterHit3DResult emitterHit;
} RuntimeEmissionTransparency3DTransmissionResult;

static int runtime_emission_transparency_3d_resolve_secondary_sample_count(void) {
    int value = animSettings.secondaryDiffuseSamples3D;

    if (value < RUNTIME_3D_SECONDARY_SAMPLES_MIN) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    }
    if (value > RUNTIME_3D_SECONDARY_SAMPLES_MAX) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MAX;
    }
    value = ((value + (RUNTIME_3D_SECONDARY_SAMPLES_STEP / 2)) /
             RUNTIME_3D_SECONDARY_SAMPLES_STEP) *
            RUNTIME_3D_SECONDARY_SAMPLES_STEP;
    if (value < RUNTIME_3D_SECONDARY_SAMPLES_MIN) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MIN;
    }
    if (value > RUNTIME_3D_SECONDARY_SAMPLES_MAX) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MAX;
    }
    return value;
}

static int runtime_emission_transparency_3d_resolve_transmission_sample_count(void) {
    int value = animSettings.transmissionSamples3D;

    if (value < RUNTIME_3D_TRANSMISSION_SAMPLES_MIN) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    }
    if (value > RUNTIME_3D_TRANSMISSION_SAMPLES_MAX) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MAX;
    }
    return value;
}

static double runtime_emission_transparency_3d_clamp(double value,
                                                     double min_value,
                                                     double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void runtime_emission_transparency_3d_resolve_payload_tint(
    const RuntimeMaterialPayload3D* payload,
    double* out_r,
    double* out_g,
    double* out_b) {
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;

    if (payload && payload->valid) {
        r = payload->baseColorR;
        g = payload->baseColorG;
        b = payload->baseColorB;
    }

    if (out_r) *out_r = runtime_emission_transparency_3d_clamp(r, 0.0, 1.0);
    if (out_g) *out_g = runtime_emission_transparency_3d_clamp(g, 0.0, 1.0);
    if (out_b) *out_b = runtime_emission_transparency_3d_clamp(b, 0.0, 1.0);
}

static Vec3 runtime_emission_transparency_3d_default_tangent(Vec3 normal) {
    Vec3 guide = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3_cross(guide, normal);
    if (vec3_length(tangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
    }
    return vec3_normalize(tangent);
}

static void runtime_emission_transparency_3d_build_basis(Vec3 normal,
                                                         Vec3* out_tangent,
                                                         Vec3* out_bitangent) {
    Vec3 tangent = runtime_emission_transparency_3d_default_tangent(normal);
    Vec3 bitangent = vec3_normalize(vec3_cross(normal, tangent));

    if (vec3_length(bitangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
        bitangent = vec3(0.0, 0.0, 1.0);
    }

    if (out_tangent) *out_tangent = tangent;
    if (out_bitangent) *out_bitangent = bitangent;
}

static double runtime_emission_transparency_3d_distance_decay(double distance) {
    return 1.0 / (1.0 + distance * distance);
}

static uint32_t runtime_emission_transparency_3d_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint32_t runtime_emission_transparency_3d_seed_from_hit(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling) {
    uint32_t sx = 0U;
    uint32_t sy = 0U;
    uint32_t sz = 0U;
    uint32_t sequence = 0U;
    if (!hit) return 0U;
    if (sampling) {
        sequence = sampling->sampleSequence;
    }
    sx = (uint32_t)(fabs(hit->position.x) * 4096.0);
    sy = (uint32_t)(fabs(hit->position.y) * 4096.0);
    sz = (uint32_t)(fabs(hit->position.z) * 4096.0);
    return runtime_emission_transparency_3d_hash_u32(
        sx ^ (sy * 73856093U) ^ (sz * 19349663U) ^
        ((uint32_t)(hit->triangleIndex + 1) * 83492791U) ^
        runtime_emission_transparency_3d_hash_u32(sequence ^ 0x85ebca6bU));
}

static double runtime_emission_transparency_3d_hash01(uint32_t base_seed, uint32_t salt) {
    uint32_t bits = runtime_emission_transparency_3d_hash_u32(base_seed ^ salt);
    return (double)bits / 4294967295.0;
}

static Vec3 runtime_emission_transparency_3d_sample_direction(const HitInfo3D* hit,
                                                              Vec3 normal,
                                                              Vec3 tangent,
                                                              Vec3 bitangent,
                                                              const RuntimeNative3DSamplingContext* sampling,
                                                              int sample_count,
                                                              int sample_index) {
    uint32_t base_seed = runtime_emission_transparency_3d_seed_from_hit(hit, sampling);
    double jitter_u =
        runtime_emission_transparency_3d_hash01(base_seed, (uint32_t)(sample_index * 2 + 1));
    double jitter_v =
        runtime_emission_transparency_3d_hash01(base_seed, (uint32_t)(sample_index * 2 + 2));
    double u = jitter_u;
    double v = ((double)sample_index + jitter_v) / (double)sample_count;
    double phi = 2.0 * M_PI * u;
    double radius = sqrt(v);
    double local_x = radius * cos(phi);
    double local_y = radius * sin(phi);
    double local_z = sqrt(fmax(0.0, 1.0 - v));
    Vec3 world_dir = vec3_add(vec3_add(vec3_scale(tangent, local_x),
                                       vec3_scale(bitangent, local_y)),
                              vec3_scale(normal, local_z));
    return vec3_normalize(world_dir);
}

static double runtime_emission_transparency_3d_transmission_cone_radius(
    const RuntimeMaterialPayload3D* payload,
    double transparency) {
    double roughness = 1.0;
    double spread = 0.0;

    if (payload && payload->valid) {
        roughness = runtime_emission_transparency_3d_clamp(payload->bsdf.roughness, 0.0, 1.0);
    }
    spread = transparency * (0.2 + (0.8 * roughness));
    return kRuntimeEmissionTransparency3DTransmissionConeBase +
           (kRuntimeEmissionTransparency3DTransmissionConeScale * spread);
}

static Vec3 runtime_emission_transparency_3d_sample_transmission_direction(
    const HitInfo3D* hit,
    Vec3 base_dir,
    Vec3 tangent,
    Vec3 bitangent,
    double cone_radius,
    const RuntimeNative3DSamplingContext* sampling,
    int sample_index) {
    uint32_t base_seed = runtime_emission_transparency_3d_seed_from_hit(hit, sampling);
    double u = 0.0;
    double v = 0.0;
    double phi = 0.0;
    double radius = 0.0;
    double offset_x = 0.0;
    double offset_y = 0.0;
    Vec3 jittered = vec3(0.0, 0.0, 0.0);

    if (sample_index <= 0 || !(cone_radius > 1e-9)) {
        return vec3_normalize(base_dir);
    }

    u = runtime_emission_transparency_3d_hash01(base_seed,
                                                (uint32_t)(sample_index * 2 + 101));
    v = runtime_emission_transparency_3d_hash01(base_seed,
                                                (uint32_t)(sample_index * 2 + 102));
    phi = 2.0 * M_PI * u;
    radius = cone_radius * sqrt(v);
    offset_x = cos(phi) * radius;
    offset_y = sin(phi) * radius;
    jittered = vec3_add(base_dir,
                        vec3_add(vec3_scale(tangent, offset_x),
                                 vec3_scale(bitangent, offset_y)));
    return vec3_normalize(jittered);
}

static double runtime_emission_transparency_3d_first_hit_emissive(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    Vec3 view_dir,
    double* out_r,
    double* out_g,
    double* out_b) {
    double facing = 0.0;
    double energy_scale = 1.0;
    double radiance = 0.0;
    double tint_r = 1.0;
    double tint_g = 1.0;
    double tint_b = 1.0;

    if (out_r) *out_r = 0.0;
    if (out_g) *out_g = 0.0;
    if (out_b) *out_b = 0.0;
    if (!scene || !hit || !payload || !payload->valid || !(payload->emissive > 0.0)) {
        return 0.0;
    }

    facing = fmax(0.0, vec3_dot(hit->normal, vec3_normalize(view_dir)));
    energy_scale = runtime_emission_transparency_3d_clamp(scene->light.intensity * 0.15, 0.5, 2.0);
    radiance = payload->emissive * facing * energy_scale;
    runtime_emission_transparency_3d_resolve_payload_tint(payload, &tint_r, &tint_g, &tint_b);
    if (out_r) *out_r = radiance * tint_r;
    if (out_g) *out_g = radiance * tint_g;
    if (out_b) *out_b = radiance * tint_b;
    return radiance;
}

static double runtime_emission_transparency_3d_secondary_emissive(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* io_result,
    double* out_r,
    double* out_g,
    double* out_b) {
    Vec3 tangent = vec3(0.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 0.0);
    double accumulated = 0.0;
    double accumulated_r = 0.0;
    double accumulated_g = 0.0;
    double accumulated_b = 0.0;
    int sample_count = 0;

    if (out_r) *out_r = 0.0;
    if (out_g) *out_g = 0.0;
    if (out_b) *out_b = 0.0;
    if (!scene || !hit || !io_result) return 0.0;

    runtime_emission_transparency_3d_build_basis(hit->normal, &tangent, &bitangent);
    sample_count = runtime_emission_transparency_3d_resolve_secondary_sample_count();

    for (int i = 0; i < sample_count; ++i) {
        HitInfo3D secondary_hit = {0};
        RuntimeMaterialPayload3D secondary_payload = {0};
        Vec3 sample_dir = runtime_emission_transparency_3d_sample_direction(hit,
                                                                            hit->normal,
                                                                            tangent,
                                                                            bitangent,
                                                                            sampling,
                                                                            sample_count,
                                                                            i);
        Ray3D bounce_ray = RuntimeRay3D_MakeOffset(hit->position,
                                                   hit->normal,
                                                   sample_dir,
                                                   kRuntimeEmissionTransparency3DEpsilon);
        double secondary_facing = 0.0;
        double segment_distance = 0.0;
        double sample_energy = 0.0;
        double tint_r = 1.0;
        double tint_g = 1.0;
        double tint_b = 1.0;

        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &bounce_ray,
                                             kRuntimeEmissionTransparency3DEpsilon,
                                             kRuntimeEmissionTransparency3DMaxDistance,
                                             &secondary_hit)) {
            continue;
        }
        io_result->secondaryHitCount += 1;
        if (secondary_hit.triangleIndex == hit->triangleIndex) {
            continue;
        }
        if (!RuntimeMaterialPayload3D_ResolveFromHit(&secondary_hit, &secondary_payload)) {
            continue;
        }
        if (!(secondary_payload.emissive > 0.0)) {
            continue;
        }
        io_result->secondaryContributingHitCount += 1;

        secondary_facing = fabs(vec3_dot(secondary_hit.normal, vec3_scale(sample_dir, -1.0)));
        segment_distance = vec3_length(vec3_sub(secondary_hit.position, hit->position));
        sample_energy = secondary_payload.emissive *
                        secondary_facing *
                        runtime_emission_transparency_3d_distance_decay(segment_distance) *
                        kRuntimeEmissionTransparency3DEnergyScale;
        accumulated += sample_energy;
        runtime_emission_transparency_3d_resolve_payload_tint(&secondary_payload,
                                                              &tint_r,
                                                              &tint_g,
                                                              &tint_b);
        accumulated_r += sample_energy * tint_r;
        accumulated_g += sample_energy * tint_g;
        accumulated_b += sample_energy * tint_b;
    }

    if (out_r) *out_r = accumulated_r / (double)sample_count;
    if (out_g) *out_g = accumulated_g / (double)sample_count;
    if (out_b) *out_b = accumulated_b / (double)sample_count;
    return accumulated / (double)sample_count;
}

static bool runtime_emission_transparency_3d_trace_transmission(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    Vec3 transmission_dir,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DTransmissionResult* out_result,
    double* out_source_segment_distance) {
    Ray3D transmission_ray;
    HitInfo3D transmitted_hit = {0};
    RuntimeLightEmitterHit3DResult emitter_hit = {0};
    RuntimeEmissionTransparency3DTransmissionResult result = {0};
    double remaining_distance = kRuntimeEmissionTransparency3DTransmissionMaxDistance;
    double source_segment_distance = 0.0;
    int skip_count = 0;

    if (!scene || !hit || !out_result) return false;
    if (out_source_segment_distance) *out_source_segment_distance = 1.0;
    HitInfo3D_Reset(&transmitted_hit);

    transmission_ray = RuntimeRay3D_MakeOffset(hit->position,
                                               hit->normal,
                                               transmission_dir,
                                               kRuntimeEmissionTransparency3DEpsilon);
    while (skip_count < kRuntimeEmissionTransparency3DMaxTransmissionSurfaceSkips &&
           remaining_distance > kRuntimeEmissionTransparency3DEpsilon) {
        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &transmission_ray,
                                             kRuntimeEmissionTransparency3DEpsilon,
                                             remaining_distance,
                                             &transmitted_hit)) {
            break;
        }
        if (transmitted_hit.sceneObjectIndex != hit->sceneObjectIndex &&
            transmitted_hit.triangleIndex != hit->triangleIndex) {
            break;
        }
        source_segment_distance += transmitted_hit.t;
        remaining_distance -= transmitted_hit.t;
        transmission_ray = RuntimeRay3D_MakeOffset(transmitted_hit.position,
                                                   transmitted_hit.normal,
                                                   transmission_dir,
                                                   kRuntimeEmissionTransparency3DEpsilon);
        skip_count += 1;
        HitInfo3D_Reset(&transmitted_hit);
    }
    if (transmitted_hit.triangleIndex < 0) {
        remaining_distance = kRuntimeEmissionTransparency3DTransmissionMaxDistance;
    } else {
        remaining_distance = transmitted_hit.t;
    }

    if (RuntimeLightEmitter3D_IntersectRay(scene,
                                           &transmission_ray,
                                           kRuntimeEmissionTransparency3DEpsilon,
                                           remaining_distance,
                                           &emitter_hit) &&
        (transmitted_hit.triangleIndex < 0 || emitter_hit.t < transmitted_hit.t)) {
        result.hit = true;
        result.emitterWins = true;
        result.emitterHit = emitter_hit;
        if (out_source_segment_distance) {
            *out_source_segment_distance = fmax(source_segment_distance, 1.0);
        }
        *out_result = result;
        return true;
    }

    if (transmitted_hit.triangleIndex < 0) {
        return false;
    }
    if (!RuntimeMaterialPayload3D_ResolveFromHit(&transmitted_hit, &result.materialPayload)) {
        return false;
    }
    if (!RuntimeMaterialResponse3D_ShadeHit(scene,
                                           &transmitted_hit,
                                           sampling,
                                           &result.materialResult)) {
        return false;
    }

    result.hit = true;
    if (out_source_segment_distance) {
        *out_source_segment_distance = fmax(source_segment_distance, 1.0);
    }
    *out_result = result;
    return true;
}

static bool runtime_emission_transparency_3d_sample_transmission(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    Vec3 transmission_dir,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* io_result,
    double* out_direct,
    double* out_direct_r,
    double* out_direct_g,
    double* out_direct_b,
    double* out_bounce,
    double* out_bounce_r,
    double* out_bounce_g,
    double* out_bounce_b) {
    RuntimeEmissionTransparency3DTransmissionResult transmitted = {0};
    Vec3 tangent = vec3(0.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 0.0);
    double transparency = 0.0;
    double cone_radius = 0.0;
    double total_direct = 0.0;
    double total_direct_r = 0.0;
    double total_direct_g = 0.0;
    double total_direct_b = 0.0;
    double total_bounce = 0.0;
    double total_bounce_r = 0.0;
    double total_bounce_g = 0.0;
    double total_bounce_b = 0.0;
    int contributing_samples = 0;
    int sample_count = 0;
    double first_hit_emissive = 0.0;
    double first_hit_emissive_r = 0.0;
    double first_hit_emissive_g = 0.0;
    double first_hit_emissive_b = 0.0;
    double source_segment_distance = 1.0;

    if (out_direct) *out_direct = 0.0;
    if (out_direct_r) *out_direct_r = 0.0;
    if (out_direct_g) *out_direct_g = 0.0;
    if (out_direct_b) *out_direct_b = 0.0;
    if (out_bounce) *out_bounce = 0.0;
    if (out_bounce_r) *out_bounce_r = 0.0;
    if (out_bounce_g) *out_bounce_g = 0.0;
    if (out_bounce_b) *out_bounce_b = 0.0;
    if (!scene || !hit || !payload || !io_result || !out_direct || !out_direct_r ||
        !out_direct_g || !out_direct_b || !out_bounce || !out_bounce_r || !out_bounce_g ||
        !out_bounce_b) {
        return false;
    }

    transparency = runtime_emission_transparency_3d_clamp(payload->transparency, 0.0, 1.0);
    runtime_emission_transparency_3d_build_basis(transmission_dir, &tangent, &bitangent);
    cone_radius = runtime_emission_transparency_3d_transmission_cone_radius(payload, transparency);
    sample_count = runtime_emission_transparency_3d_resolve_transmission_sample_count();
    io_result->secondaryRayCount += sample_count;

    for (int i = 0; i < sample_count; ++i) {
        Vec3 sample_dir = runtime_emission_transparency_3d_sample_transmission_direction(hit,
                                                                                         transmission_dir,
                                                                                         tangent,
                                                                                         bitangent,
                                                                                         cone_radius,
                                                                                         sampling,
                                                                                         i);
        RuntimeVisibility3DTransmittance source_filter = RuntimeVisibility3D_UnitTransmittance();
        if (!runtime_emission_transparency_3d_trace_transmission(scene,
                                                                 hit,
                                                                 sample_dir,
                                                                 sampling,
                                                                 &transmitted,
                                                                 &source_segment_distance)) {
            continue;
        }
        RuntimeVisibility3D_ApplyTransparentPayloadAbsorption(payload,
                                                              source_segment_distance,
                                                              &source_filter);
        contributing_samples += 1;
        if (transmitted.emitterWins) {
            total_direct += transmitted.emitterHit.radiance * source_filter.luma;
            total_direct_r += transmitted.emitterHit.radiance * source_filter.r;
            total_direct_g += transmitted.emitterHit.radiance * source_filter.g;
            total_direct_b += transmitted.emitterHit.radiance * source_filter.b;
            continue;
        }

        first_hit_emissive = runtime_emission_transparency_3d_first_hit_emissive(
            scene,
            &transmitted.materialResult.hitInfo,
            &transmitted.materialPayload,
            vec3_scale(sample_dir, -1.0),
            &first_hit_emissive_r,
            &first_hit_emissive_g,
            &first_hit_emissive_b);
        total_direct += (transmitted.materialResult.directRadiance + first_hit_emissive) *
                        source_filter.luma;
        total_direct_r += (transmitted.materialResult.directRadianceR + first_hit_emissive_r) *
                          source_filter.r;
        total_direct_g += (transmitted.materialResult.directRadianceG + first_hit_emissive_g) *
                          source_filter.g;
        total_direct_b += (transmitted.materialResult.directRadianceB + first_hit_emissive_b) *
                          source_filter.b;
        total_bounce += transmitted.materialResult.bounceRadiance * source_filter.luma;
        total_bounce_r += transmitted.materialResult.bounceRadianceR * source_filter.r;
        total_bounce_g += transmitted.materialResult.bounceRadianceG * source_filter.g;
        total_bounce_b += transmitted.materialResult.bounceRadianceB * source_filter.b;
        io_result->secondaryRayCount += transmitted.materialResult.secondaryRayCount;
        io_result->secondaryHitCount += transmitted.materialResult.secondaryHitCount;
        io_result->secondaryContributingHitCount +=
            transmitted.materialResult.secondaryContributingHitCount;
    }

    if (contributing_samples <= 0) {
        return false;
    }

    *out_direct = total_direct / (double)contributing_samples;
    *out_direct_r = total_direct_r / (double)contributing_samples;
    *out_direct_g = total_direct_g / (double)contributing_samples;
    *out_direct_b = total_direct_b / (double)contributing_samples;
    *out_bounce = total_bounce / (double)contributing_samples;
    *out_bounce_r = total_bounce_r / (double)contributing_samples;
    *out_bounce_g = total_bounce_g / (double)contributing_samples;
    *out_bounce_b = total_bounce_b / (double)contributing_samples;
    return true;
}

static void runtime_emission_transparency_3d_apply_transparency(
    const RuntimeScene3D* scene,
    const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload,
    Vec3 transmission_dir,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeEmissionTransparency3DResult* io_result) {
    double transparency = 0.0;
    double front_weight = 1.0;
    double transmitted_direct = 0.0;
    double transmitted_direct_r = 0.0;
    double transmitted_direct_g = 0.0;
    double transmitted_direct_b = 0.0;
    double transmitted_bounce = 0.0;
    double transmitted_bounce_r = 0.0;
    double transmitted_bounce_g = 0.0;
    double transmitted_bounce_b = 0.0;

    if (!scene || !hit || !payload || !io_result) return;
    transparency = runtime_emission_transparency_3d_clamp(payload->transparency, 0.0, 1.0);
    if (!(transparency > 0.0)) {
        return;
    }

    front_weight = 1.0 - transparency;
    if (front_weight < kRuntimeEmissionTransparency3DMinimumFrontWeight) {
        front_weight = kRuntimeEmissionTransparency3DMinimumFrontWeight;
    }
    transparency = 1.0 - front_weight;
    (void)runtime_emission_transparency_3d_sample_transmission(scene,
                                                               hit,
                                                               payload,
                                                               transmission_dir,
                                                               sampling,
                                                               io_result,
                                                               &transmitted_direct,
                                                               &transmitted_direct_r,
                                                               &transmitted_direct_g,
                                                               &transmitted_direct_b,
                                                               &transmitted_bounce,
                                                               &transmitted_bounce_r,
                                                               &transmitted_bounce_g,
                                                               &transmitted_bounce_b);

    io_result->emissiveDirectRadiance *= front_weight;
    io_result->emissiveDirectRadianceR *= front_weight;
    io_result->emissiveDirectRadianceG *= front_weight;
    io_result->emissiveDirectRadianceB *= front_weight;
    io_result->emissiveBounceRadiance *= front_weight;
    io_result->emissiveBounceRadianceR *= front_weight;
    io_result->emissiveBounceRadianceG *= front_weight;
    io_result->emissiveBounceRadianceB *= front_weight;
    io_result->transmittedDirectRadiance = transmitted_direct * transparency;
    io_result->transmittedDirectRadianceR = transmitted_direct_r * transparency;
    io_result->transmittedDirectRadianceG = transmitted_direct_g * transparency;
    io_result->transmittedDirectRadianceB = transmitted_direct_b * transparency;
    io_result->transmittedBounceRadiance = transmitted_bounce * transparency;
    io_result->transmittedBounceRadianceR = transmitted_bounce_r * transparency;
    io_result->transmittedBounceRadianceG = transmitted_bounce_g * transparency;
    io_result->transmittedBounceRadianceB = transmitted_bounce_b * transparency;
    io_result->directRadiance = (io_result->directRadiance * front_weight) +
                                io_result->transmittedDirectRadiance;
    io_result->directRadianceR = (io_result->directRadianceR * front_weight) +
                                 io_result->transmittedDirectRadianceR;
    io_result->directRadianceG = (io_result->directRadianceG * front_weight) +
                                 io_result->transmittedDirectRadianceG;
    io_result->directRadianceB = (io_result->directRadianceB * front_weight) +
                                 io_result->transmittedDirectRadianceB;
    io_result->bounceRadiance = (io_result->bounceRadiance * front_weight) +
                                io_result->transmittedBounceRadiance;
    io_result->bounceRadianceR = (io_result->bounceRadianceR * front_weight) +
                                 io_result->transmittedBounceRadianceR;
    io_result->bounceRadianceG = (io_result->bounceRadianceG * front_weight) +
                                 io_result->transmittedBounceRadianceG;
    io_result->bounceRadianceB = (io_result->bounceRadianceB * front_weight) +
                                 io_result->transmittedBounceRadianceB;
    io_result->radiance = io_result->directRadiance + io_result->bounceRadiance;
    io_result->radianceR = io_result->directRadianceR + io_result->bounceRadianceR;
    io_result->radianceG = io_result->directRadianceG + io_result->bounceRadianceG;
    io_result->radianceB = io_result->directRadianceB + io_result->bounceRadianceB;
}

static void runtime_emission_transparency_3d_copy_material_result(
    const RuntimeMaterialResponse3DResult* source,
    const RuntimeMaterialPayload3D* payload,
    RuntimeEmissionTransparency3DResult* out_result) {
    if (!source || !payload || !out_result) return;

    out_result->hit = source->hit;
    out_result->visible = source->visible;
    out_result->payloadResolved = payload->valid;
    out_result->primaryRay = source->primaryRay;
    out_result->hitInfo = source->hitInfo;
    out_result->payload = *payload;
    out_result->directRadiance = source->directRadiance;
    out_result->directRadianceR = source->directRadianceR;
    out_result->directRadianceG = source->directRadianceG;
    out_result->directRadianceB = source->directRadianceB;
    out_result->bounceRadiance = source->bounceRadiance;
    out_result->bounceRadianceR = source->bounceRadianceR;
    out_result->bounceRadianceG = source->bounceRadianceG;
    out_result->bounceRadianceB = source->bounceRadianceB;
    out_result->emissiveDirectRadiance = 0.0;
    out_result->emissiveDirectRadianceR = 0.0;
    out_result->emissiveDirectRadianceG = 0.0;
    out_result->emissiveDirectRadianceB = 0.0;
    out_result->emissiveBounceRadiance = 0.0;
    out_result->emissiveBounceRadianceR = 0.0;
    out_result->emissiveBounceRadianceG = 0.0;
    out_result->emissiveBounceRadianceB = 0.0;
    out_result->transmittedDirectRadiance = 0.0;
    out_result->transmittedDirectRadianceR = 0.0;
    out_result->transmittedDirectRadianceG = 0.0;
    out_result->transmittedDirectRadianceB = 0.0;
    out_result->transmittedBounceRadiance = 0.0;
    out_result->transmittedBounceRadianceR = 0.0;
    out_result->transmittedBounceRadianceG = 0.0;
    out_result->transmittedBounceRadianceB = 0.0;
    out_result->radiance = source->radiance;
    out_result->radianceR = source->radianceR;
    out_result->radianceG = source->radianceG;
    out_result->radianceB = source->radianceB;
    out_result->secondaryRayCount = source->secondaryRayCount;
    out_result->secondaryHitCount = source->secondaryHitCount;
    out_result->secondaryContributingHitCount = source->secondaryContributingHitCount;
}

bool RuntimeEmissionTransparency3D_ShadeHit(const RuntimeScene3D* scene,
                                            const HitInfo3D* hit,
                                            const RuntimeNative3DSamplingContext* sampling,
                                            RuntimeEmissionTransparency3DResult* out_result) {
    RuntimeEmissionTransparency3DResult result = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 0.0);
    Vec3 transmission_dir = vec3(0.0, 0.0, 0.0);
    double emissive_direct = 0.0;
    double emissive_bounce = 0.0;
    double emissive_direct_r = 0.0;
    double emissive_direct_g = 0.0;
    double emissive_direct_b = 0.0;
    double emissive_bounce_r = 0.0;
    double emissive_bounce_g = 0.0;
    double emissive_bounce_b = 0.0;
    RuntimeEmissiveDirect3DResult emissive_direct_result = {0};
    int secondary_sample_count = 0;

    if (!scene || !hit || !out_result) return false;
    if (!RuntimeMaterialPayload3D_ResolveFromHit(hit, &result.payload)) {
        *out_result = result;
        return false;
    }
    if (!RuntimeMaterialResponse3D_ShadeHit(scene, hit, sampling, &material_result)) {
        *out_result = result;
        return false;
    }

    runtime_emission_transparency_3d_copy_material_result(&material_result,
                                                          &result.payload,
                                                          &result);
    secondary_sample_count = runtime_emission_transparency_3d_resolve_secondary_sample_count();
    result.secondaryRayCount = secondary_sample_count;
    view_dir = vec3_normalize(hit->normal);
    transmission_dir = vec3_scale(view_dir, -1.0);
    emissive_direct = runtime_emission_transparency_3d_first_hit_emissive(scene,
                                                                          hit,
                                                                          &result.payload,
                                                                          view_dir,
                                                                          &emissive_direct_r,
                                                                          &emissive_direct_g,
                                                                          &emissive_direct_b);
    emissive_bounce = runtime_emission_transparency_3d_secondary_emissive(scene,
                                                                          hit,
                                                                          sampling,
                                                                          &result,
                                                                          &emissive_bounce_r,
                                                                          &emissive_bounce_g,
                                                                          &emissive_bounce_b);
    if (RuntimeEmissiveDirect3D_ShadeHit(scene, hit, sampling, &emissive_direct_result)) {
        emissive_direct += emissive_direct_result.directRadiance;
        emissive_direct_r += emissive_direct_result.directRadianceR;
        emissive_direct_g += emissive_direct_result.directRadianceG;
        emissive_direct_b += emissive_direct_result.directRadianceB;
    }
    result.emissiveDirectRadiance = emissive_direct;
    result.emissiveDirectRadianceR = emissive_direct_r;
    result.emissiveDirectRadianceG = emissive_direct_g;
    result.emissiveDirectRadianceB = emissive_direct_b;
    result.emissiveBounceRadiance = emissive_bounce;
    result.emissiveBounceRadianceR = emissive_bounce_r;
    result.emissiveBounceRadianceG = emissive_bounce_g;
    result.emissiveBounceRadianceB = emissive_bounce_b;
    result.directRadiance += emissive_direct;
    result.directRadianceR += emissive_direct_r;
    result.directRadianceG += emissive_direct_g;
    result.directRadianceB += emissive_direct_b;
    result.bounceRadiance += emissive_bounce;
    result.bounceRadianceR += emissive_bounce_r;
    result.bounceRadianceG += emissive_bounce_g;
    result.bounceRadianceB += emissive_bounce_b;
    result.radiance = result.directRadiance + result.bounceRadiance;
    result.radianceR = result.directRadianceR + result.bounceRadianceR;
    result.radianceG = result.directRadianceG + result.bounceRadianceG;
    result.radianceB = result.directRadianceB + result.bounceRadianceB;
    runtime_emission_transparency_3d_apply_transparency(scene,
                                                        hit,
                                                        &result.payload,
                                                        transmission_dir,
                                                        sampling,
                                                        &result);
    *out_result = result;
    return true;
}

bool RuntimeEmissionTransparency3D_ShadePixel(const RuntimeScene3D* scene,
                                              const RuntimeCameraProjector3D* projector,
                                              double pixel_x,
                                              double pixel_y,
                                              const RuntimeNative3DSamplingContext* sampling,
                                              RuntimeEmissionTransparency3DResult* out_result) {
    RuntimeEmissionTransparency3DResult result = {0};
    RuntimeMaterialResponse3DResult material_result = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 0.0);
    Vec3 transmission_dir = vec3(0.0, 0.0, 0.0);
    double emissive_direct = 0.0;
    double emissive_bounce = 0.0;
    double emissive_direct_r = 0.0;
    double emissive_direct_g = 0.0;
    double emissive_direct_b = 0.0;
    double emissive_bounce_r = 0.0;
    double emissive_bounce_g = 0.0;
    double emissive_bounce_b = 0.0;
    RuntimeEmissiveDirect3DResult emissive_direct_result = {0};
    int secondary_sample_count = 0;

    if (!scene || !projector || !out_result) return false;
    if (!RuntimeMaterialResponse3D_ShadePixel(scene,
                                              projector,
                                              pixel_x,
                                              pixel_y,
                                              sampling,
                                              &material_result)) {
        result.primaryRay = material_result.primaryRay;
        *out_result = result;
        return false;
    }
    if (!RuntimeMaterialPayload3D_ResolveFromHit(&material_result.hitInfo, &result.payload)) {
        result.primaryRay = material_result.primaryRay;
        *out_result = result;
        return false;
    }

    runtime_emission_transparency_3d_copy_material_result(&material_result,
                                                          &result.payload,
                                                          &result);
    secondary_sample_count = runtime_emission_transparency_3d_resolve_secondary_sample_count();
    result.secondaryRayCount = secondary_sample_count;
    view_dir = vec3_scale(material_result.primaryRay.direction, -1.0);
    transmission_dir = material_result.primaryRay.direction;
    emissive_direct = runtime_emission_transparency_3d_first_hit_emissive(scene,
                                                                          &result.hitInfo,
                                                                          &result.payload,
                                                                          view_dir,
                                                                          &emissive_direct_r,
                                                                          &emissive_direct_g,
                                                                          &emissive_direct_b);
    emissive_bounce = runtime_emission_transparency_3d_secondary_emissive(scene,
                                                                           &result.hitInfo,
                                                                           sampling,
                                                                           &result,
                                                                           &emissive_bounce_r,
                                                                           &emissive_bounce_g,
                                                                           &emissive_bounce_b);
    if (RuntimeEmissiveDirect3D_ShadeHit(scene,
                                         &result.hitInfo,
                                         sampling,
                                         &emissive_direct_result)) {
        emissive_direct += emissive_direct_result.directRadiance;
        emissive_direct_r += emissive_direct_result.directRadianceR;
        emissive_direct_g += emissive_direct_result.directRadianceG;
        emissive_direct_b += emissive_direct_result.directRadianceB;
    }
    result.emissiveDirectRadiance = emissive_direct;
    result.emissiveDirectRadianceR = emissive_direct_r;
    result.emissiveDirectRadianceG = emissive_direct_g;
    result.emissiveDirectRadianceB = emissive_direct_b;
    result.emissiveBounceRadiance = emissive_bounce;
    result.emissiveBounceRadianceR = emissive_bounce_r;
    result.emissiveBounceRadianceG = emissive_bounce_g;
    result.emissiveBounceRadianceB = emissive_bounce_b;
    result.directRadiance += emissive_direct;
    result.directRadianceR += emissive_direct_r;
    result.directRadianceG += emissive_direct_g;
    result.directRadianceB += emissive_direct_b;
    result.bounceRadiance += emissive_bounce;
    result.bounceRadianceR += emissive_bounce_r;
    result.bounceRadianceG += emissive_bounce_g;
    result.bounceRadianceB += emissive_bounce_b;
    result.radiance = result.directRadiance + result.bounceRadiance;
    result.radianceR = result.directRadianceR + result.bounceRadianceR;
    result.radianceG = result.directRadianceG + result.bounceRadianceG;
    result.radianceB = result.directRadianceB + result.bounceRadianceB;
    runtime_emission_transparency_3d_apply_transparency(scene,
                                                        &result.hitInfo,
                                                        &result.payload,
                                                        transmission_dir,
                                                        sampling,
                                                        &result);
    *out_result = result;
    return true;
}
