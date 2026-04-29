#include "render/runtime_emissive_direct_3d.h"

#include <math.h>

#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_visibility_3d.h"

static const double kRuntimeEmissiveDirect3DEpsilon = 1e-4;
static const double kRuntimeEmissiveDirect3DEnergyScale = 1.25;

static double runtime_emissive_direct_3d_clamp(double value,
                                               double min_value,
                                               double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_emissive_direct_3d_distance_decay(double distance) {
    return 1.0 / (1.0 + distance * distance);
}

static uint32_t runtime_emissive_direct_3d_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint32_t runtime_emissive_direct_3d_seed_from_hit(
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
    return runtime_emissive_direct_3d_hash_u32(
        sx ^ (sy * 73856093U) ^ (sz * 19349663U) ^
        ((uint32_t)(hit->triangleIndex + 1) * 83492791U) ^
        runtime_emissive_direct_3d_hash_u32(sequence ^ 0x27d4eb2dU));
}

static double runtime_emissive_direct_3d_triangle_area(const RuntimeTriangle3D* triangle) {
    Vec3 edge_a = vec3(0.0, 0.0, 0.0);
    Vec3 edge_b = vec3(0.0, 0.0, 0.0);
    Vec3 cross = vec3(0.0, 0.0, 0.0);

    if (!triangle) return 0.0;

    edge_a = vec3_sub(triangle->p1, triangle->p0);
    edge_b = vec3_sub(triangle->p2, triangle->p0);
    cross = vec3_cross(edge_a, edge_b);
    return 0.5 * vec3_length(cross);
}

static Vec3 runtime_emissive_direct_3d_sample_triangle_point(
    const RuntimeTriangle3D* triangle,
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t base_seed,
    int triangle_index) {
    double u = 0.0;
    double v = 0.0;

    if (!triangle) return vec3(0.0, 0.0, 0.0);

    RuntimeNative3DSampling_Stratified2D(sampling,
                                         base_seed ^ (uint32_t)(triangle_index * 2654435761U),
                                         1,
                                         0,
                                         (uint32_t)triangle_index,
                                         &u,
                                         &v);
    if ((u + v) > 1.0) {
        u = 1.0 - u;
        v = 1.0 - v;
    }

    return vec3_add(triangle->p0,
                    vec3_add(vec3_scale(vec3_sub(triangle->p1, triangle->p0), u),
                             vec3_scale(vec3_sub(triangle->p2, triangle->p0), v)));
}

bool RuntimeEmissiveDirect3D_ShadeHit(const RuntimeScene3D* scene,
                                      const HitInfo3D* hit,
                                      const RuntimeNative3DSamplingContext* sampling,
                                      RuntimeEmissiveDirect3DResult* out_result) {
    RuntimeEmissiveDirect3DResult result = {0};
    uint32_t base_seed = 0U;

    if (!scene || !hit || !out_result) return false;
    if (!scene->triangleMesh.triangles || scene->triangleMesh.triangleCount <= 0) {
        *out_result = result;
        return false;
    }

    base_seed = runtime_emissive_direct_3d_seed_from_hit(hit, sampling);
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
        RuntimeMaterialPayload3D payload = {0};
        Vec3 sample_point = vec3(0.0, 0.0, 0.0);
        Vec3 to_emitter = vec3(0.0, 0.0, 0.0);
        Vec3 light_dir = vec3(0.0, 0.0, 0.0);
        double distance = 0.0;
        double receiver_facing = 0.0;
        double emitter_facing = 0.0;
        double transmittance = 0.0;
        double area = 0.0;
        double sample_energy = 0.0;
        double sample_energy_r = 0.0;
        double sample_energy_g = 0.0;
        double sample_energy_b = 0.0;

        if (triangle->sceneObjectIndex < 0 ||
            triangle->sceneObjectIndex == hit->sceneObjectIndex) {
            continue;
        }
        if (!RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(triangle->sceneObjectIndex, &payload) ||
            !(payload.emissive > 0.0)) {
            continue;
        }

        sample_point = runtime_emissive_direct_3d_sample_triangle_point(triangle,
                                                                        sampling,
                                                                        base_seed,
                                                                        i);
        to_emitter = vec3_sub(sample_point, hit->position);
        distance = vec3_length(to_emitter);
        if (!(distance > kRuntimeEmissiveDirect3DEpsilon)) {
            continue;
        }

        light_dir = vec3_scale(to_emitter, 1.0 / distance);
        receiver_facing = fmax(0.0, vec3_dot(hit->normal, light_dir));
        emitter_facing = fmax(0.0, vec3_dot(triangle->normal, vec3_scale(light_dir, -1.0)));
        if (!(receiver_facing > 0.0) || !(emitter_facing > 0.0)) {
            continue;
        }

        result.sampledTriangleCount += 1;
        transmittance = RuntimeVisibility3D_TransmittanceFromHitToPoint(scene,
                                                                        hit,
                                                                        sample_point,
                                                                        triangle->sceneObjectIndex,
                                                                        i);
        if (!(transmittance > 1e-6)) {
            continue;
        }

        area = runtime_emissive_direct_3d_triangle_area(triangle);
        sample_energy = payload.emissive *
                        receiver_facing *
                        emitter_facing *
                        transmittance *
                        runtime_emissive_direct_3d_distance_decay(distance) *
                        runtime_emissive_direct_3d_clamp(area, 0.25, 4.0) *
                        kRuntimeEmissiveDirect3DEnergyScale;
        if (!(sample_energy > 0.0)) {
            continue;
        }
        sample_energy_r = sample_energy * payload.baseColorR;
        sample_energy_g = sample_energy * payload.baseColorG;
        sample_energy_b = sample_energy * payload.baseColorB;

        result.contributingTriangleCount += 1;
        result.directRadiance += sample_energy;
        result.directRadianceR += sample_energy_r;
        result.directRadianceG += sample_energy_g;
        result.directRadianceB += sample_energy_b;
    }

    *out_result = result;
    return result.contributingTriangleCount > 0;
}
