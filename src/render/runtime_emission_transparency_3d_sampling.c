#include "render/runtime_emission_transparency_3d_internal.h"

#include <math.h>

#include "render/runtime_native_3d_sampling.h"

const double kRuntimeEmissionTransparency3DEpsilon = 1e-4;
const double kRuntimeEmissionTransparency3DMaxDistance = 8.0;
const double kRuntimeEmissionTransparency3DEnergyScale = 0.75;
const double kRuntimeEmissionTransparency3DTransmissionMaxDistance = 32.0;
const int kRuntimeEmissionTransparency3DMaxTransmissionSurfaceSkips = 16;
const double kRuntimeEmissionTransparency3DTransmissionConeBase = 0.015;
const double kRuntimeEmissionTransparency3DTransmissionConeScale = 0.12;

int runtime_emission_transparency_3d_resolve_secondary_sample_count(void) {
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

int runtime_emission_transparency_3d_resolve_transmission_sample_count(void) {
    int value = animSettings.transmissionSamples3D;

    if (value < RUNTIME_3D_TRANSMISSION_SAMPLES_MIN) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    }
    if (value > RUNTIME_3D_TRANSMISSION_SAMPLES_MAX) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MAX;
    }
    return value;
}

double runtime_emission_transparency_3d_clamp(double value,
                                              double min_value,
                                              double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

void runtime_emission_transparency_3d_resolve_payload_tint(
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

Vec3 runtime_emission_transparency_3d_default_tangent(Vec3 normal) {
    Vec3 guide = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3_cross(guide, normal);
    if (vec3_length(tangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
    }
    return vec3_normalize(tangent);
}

void runtime_emission_transparency_3d_build_basis(Vec3 normal,
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

double runtime_emission_transparency_3d_distance_decay(double distance) {
    return 1.0 / (1.0 + distance * distance);
}

uint32_t runtime_emission_transparency_3d_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint32_t runtime_emission_transparency_3d_seed_from_hit(
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

Vec3 runtime_emission_transparency_3d_sample_direction(const HitInfo3D* hit,
                                                       Vec3 normal,
                                                       Vec3 tangent,
                                                       Vec3 bitangent,
                                                       const RuntimeNative3DSamplingContext* sampling,
                                                       int sample_count,
                                                       int sample_index) {
    uint32_t base_seed = runtime_emission_transparency_3d_seed_from_hit(hit, sampling);
    double u = 0.5;
    double v = 0.5;
    double phi = 2.0 * M_PI * u;
    double radius = sqrt(v);
    double local_x = radius * cos(phi);
    double local_y = radius * sin(phi);
    double local_z = sqrt(fmax(0.0, 1.0 - v));

    RuntimeNative3DSampling_Stratified2D(sampling,
                                         base_seed,
                                         sample_count,
                                         sample_index,
                                         0u,
                                         &u,
                                         &v);
    phi = 2.0 * M_PI * u;
    radius = sqrt(v);
    local_x = radius * cos(phi);
    local_y = radius * sin(phi);
    local_z = sqrt(fmax(0.0, 1.0 - v));

    Vec3 world_dir = vec3_add(vec3_add(vec3_scale(tangent, local_x),
                                       vec3_scale(bitangent, local_y)),
                              vec3_scale(normal, local_z));
    return vec3_normalize(world_dir);
}

double runtime_emission_transparency_3d_transmission_cone_radius(
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

void runtime_emission_transparency_3d_resolve_mix_weights(
    const RuntimeMaterialPayload3D* payload,
    const RuntimeDielectricTransport3D* dielectric,
    double* out_base_front_weight,
    double* out_reflection_weight,
    double* out_transmission_weight) {
    double transparency = 0.0;
    double base_front_weight = 1.0;
    double reflection_weight = 0.0;
    double transmission_weight = 0.0;

    if (!payload) return;

    transparency = runtime_emission_transparency_3d_clamp(payload->transparency, 0.0, 1.0);
    base_front_weight = 1.0 - transparency;
    if (dielectric) {
        reflection_weight = transparency *
                            runtime_emission_transparency_3d_clamp(dielectric->fresnel, 0.0, 1.0);
        transmission_weight = transparency - reflection_weight;
        if (dielectric->totalInternalReflection) {
            reflection_weight += transmission_weight;
            transmission_weight = 0.0;
        }
    } else {
        transmission_weight = transparency;
    }
    reflection_weight = runtime_emission_transparency_3d_clamp(reflection_weight, 0.0, 1.0);
    transmission_weight = runtime_emission_transparency_3d_clamp(transmission_weight, 0.0, 1.0);

    if (out_base_front_weight) *out_base_front_weight = base_front_weight;
    if (out_reflection_weight) *out_reflection_weight = reflection_weight;
    if (out_transmission_weight) *out_transmission_weight = transmission_weight;
}

Vec3 runtime_emission_transparency_3d_sample_transmission_direction(
    const HitInfo3D* hit,
    Vec3 base_dir,
    Vec3 tangent,
    Vec3 bitangent,
    double cone_radius,
    const RuntimeNative3DSamplingContext* sampling,
    int sample_count,
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

    RuntimeNative3DSampling_Stratified2D(sampling,
                                         base_seed ^ 0x51f15e1dU,
                                         sample_count,
                                         sample_index,
                                         17u,
                                         &u,
                                         &v);
    phi = 2.0 * M_PI * u;
    radius = cone_radius * sqrt(v);
    offset_x = cos(phi) * radius;
    offset_y = sin(phi) * radius;
    jittered = vec3_add(base_dir,
                        vec3_add(vec3_scale(tangent, offset_x),
                                 vec3_scale(bitangent, offset_y)));
    return vec3_normalize(jittered);
}
