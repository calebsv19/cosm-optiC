#include "render/runtime_diffuse_bounce_3d.h"

#include <math.h>
#include <stdint.h>

static const double kRuntimeDiffuseBounce3DEpsilon = 1e-4;
static const double kRuntimeDiffuseBounce3DMaxDistance = 8.0;
static const double kRuntimeDiffuseBounce3DEnergyScale = 0.75;
static const int kRuntimeDiffuseBounce3DSampleCols = 6;
static const int kRuntimeDiffuseBounce3DSampleRows = 4;
static const int kRuntimeDiffuseBounce3DSampleCount =
    kRuntimeDiffuseBounce3DSampleCols * kRuntimeDiffuseBounce3DSampleRows;

static Vec3 runtime_diffuse_bounce_3d_default_tangent(Vec3 normal) {
    Vec3 guide = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3_cross(guide, normal);
    if (vec3_length(tangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
    }
    return vec3_normalize(tangent);
}

static void runtime_diffuse_bounce_3d_build_basis(Vec3 normal,
                                                  Vec3* out_tangent,
                                                  Vec3* out_bitangent) {
    Vec3 tangent = runtime_diffuse_bounce_3d_default_tangent(normal);
    Vec3 bitangent = vec3_normalize(vec3_cross(normal, tangent));

    if (vec3_length(bitangent) <= 1e-9) {
        tangent = vec3(1.0, 0.0, 0.0);
        bitangent = vec3(0.0, 0.0, 1.0);
    }

    if (out_tangent) *out_tangent = tangent;
    if (out_bitangent) *out_bitangent = bitangent;
}

static double runtime_diffuse_bounce_3d_distance_decay(double distance) {
    return 1.0 / (1.0 + distance * distance);
}

static uint32_t runtime_diffuse_bounce_3d_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint32_t runtime_diffuse_bounce_3d_seed_from_hit(const HitInfo3D* hit) {
    uint32_t sx = 0U;
    uint32_t sy = 0U;
    uint32_t sz = 0U;
    if (!hit) return 0U;
    sx = (uint32_t)(fabs(hit->position.x) * 4096.0);
    sy = (uint32_t)(fabs(hit->position.y) * 4096.0);
    sz = (uint32_t)(fabs(hit->position.z) * 4096.0);
    return runtime_diffuse_bounce_3d_hash_u32(
        sx ^ (sy * 73856093U) ^ (sz * 19349663U) ^ ((uint32_t)(hit->triangleIndex + 1) * 83492791U));
}

static double runtime_diffuse_bounce_3d_hash01(uint32_t base_seed, uint32_t salt) {
    uint32_t bits = runtime_diffuse_bounce_3d_hash_u32(base_seed ^ salt);
    return (double)bits / 4294967295.0;
}

static Vec3 runtime_diffuse_bounce_3d_sample_direction(const HitInfo3D* hit,
                                                       Vec3 normal,
                                                       Vec3 tangent,
                                                       Vec3 bitangent,
                                                       int sample_index) {
    uint32_t base_seed = runtime_diffuse_bounce_3d_seed_from_hit(hit);
    int col = sample_index % kRuntimeDiffuseBounce3DSampleCols;
    int row = sample_index / kRuntimeDiffuseBounce3DSampleCols;
    double jitter_u =
        runtime_diffuse_bounce_3d_hash01(base_seed, (uint32_t)(sample_index * 2 + 1));
    double jitter_v =
        runtime_diffuse_bounce_3d_hash01(base_seed, (uint32_t)(sample_index * 2 + 2));
    double u = ((double)col + jitter_u) / (double)kRuntimeDiffuseBounce3DSampleCols;
    double v = ((double)row + jitter_v) / (double)kRuntimeDiffuseBounce3DSampleRows;
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

bool RuntimeDiffuseBounce3D_ShadeHit(const RuntimeScene3D* scene,
                                     const HitInfo3D* hit,
                                     RuntimeDiffuseBounce3DResult* out_result) {
    RuntimeDiffuseBounce3DResult result = {0};
    RuntimeDirectLight3DResult direct_result = {0};
    Vec3 tangent = vec3(0.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 0.0);
    double accumulated = 0.0;

    if (!scene || !hit || !out_result) return false;
    if (!scene->hasLight) return false;

    if (!RuntimeDirectLight3D_ShadeHit(scene, hit, &direct_result)) {
        return false;
    }

    result.hit = direct_result.hit;
    result.visible = direct_result.visible;
    result.hitInfo = direct_result.hitInfo;
    result.directRadiance = direct_result.radiance;

    runtime_diffuse_bounce_3d_build_basis(hit->normal, &tangent, &bitangent);
    result.secondaryRayCount = kRuntimeDiffuseBounce3DSampleCount;

    for (int i = 0; i < kRuntimeDiffuseBounce3DSampleCount; ++i) {
        HitInfo3D secondary_hit = {0};
        RuntimeDirectLight3DResult secondary_direct = {0};
        Vec3 sample_dir = runtime_diffuse_bounce_3d_sample_direction(hit,
                                                                     hit->normal,
                                                                     tangent,
                                                                     bitangent,
                                                                     i);
        Ray3D bounce_ray = RuntimeRay3D_MakeOffset(hit->position,
                                                   hit->normal,
                                                   sample_dir,
                                                   kRuntimeDiffuseBounce3DEpsilon);
        double secondary_facing = 0.0;
        double segment_distance = 0.0;
        double sample_energy = 0.0;

        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &bounce_ray,
                                             kRuntimeDiffuseBounce3DEpsilon,
                                             kRuntimeDiffuseBounce3DMaxDistance,
                                             &secondary_hit)) {
            continue;
        }
        result.secondaryHitCount += 1;
        if (secondary_hit.triangleIndex == hit->triangleIndex) {
            continue;
        }
        if (!RuntimeDirectLight3D_ShadeHit(scene, &secondary_hit, &secondary_direct)) {
            continue;
        }
        if (!(secondary_direct.radiance > 0.0)) {
            continue;
        }
        result.secondaryContributingHitCount += 1;

        secondary_facing = fabs(vec3_dot(secondary_hit.normal, vec3_scale(sample_dir, -1.0)));
        segment_distance = vec3_length(vec3_sub(secondary_hit.position, hit->position));
        /*
         * The hemisphere sampler is already cosine-weighted around the first-hit
         * normal, so multiplying by the first-hit cosine again would
         * systematically under-light indirect bounce.
         */
        sample_energy = secondary_direct.radiance *
                        secondary_facing *
                        runtime_diffuse_bounce_3d_distance_decay(segment_distance) *
                        kRuntimeDiffuseBounce3DEnergyScale;
        accumulated += sample_energy;
    }

    result.bounceRadiance = fmin(accumulated / (double)kRuntimeDiffuseBounce3DSampleCount,
                                 fmax(scene->light.intensity * 0.35, 0.0));
    result.radiance = result.directRadiance + result.bounceRadiance;
    *out_result = result;
    return true;
}

bool RuntimeDiffuseBounce3D_ShadePixel(const RuntimeScene3D* scene,
                                       const RuntimeCameraProjector3D* projector,
                                       double pixel_x,
                                       double pixel_y,
                                       RuntimeDiffuseBounce3DResult* out_result) {
    RuntimeDiffuseBounce3DResult result = {0};
    RuntimePrimaryHit3DResult primary_hit = {0};

    if (!scene || !projector || !out_result) return false;
    if (!scene->hasLight) return false;

    if (!RuntimeDirectLight3D_TracePrimaryHit(scene,
                                              projector,
                                              pixel_x,
                                              pixel_y,
                                              &primary_hit)) {
        result.primaryRay = primary_hit.primaryRay;
        *out_result = result;
        return false;
    }

    if (!RuntimeDiffuseBounce3D_ShadeHit(scene, &primary_hit.hitInfo, &result)) {
        result.primaryRay = primary_hit.primaryRay;
        *out_result = result;
        return false;
    }

    result.primaryRay = primary_hit.primaryRay;
    *out_result = result;
    return true;
}
