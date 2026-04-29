#include "render/runtime_diffuse_bounce_3d.h"

#include <math.h>
#include <stdint.h>

#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_sampling.h"

static const double kRuntimeDiffuseBounce3DEpsilon = 1e-4;
static const double kRuntimeDiffuseBounce3DMaxDistance = 8.0;
static const double kRuntimeDiffuseBounce3DEnergyScale = 0.75;
static const int kRuntimeDiffuseBounce3DMinDepthBeforeRoulette = 2;

static int runtime_diffuse_bounce_3d_resolve_sample_count(void) {
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

static int runtime_diffuse_bounce_3d_resolve_bounce_depth(void) {
    int value = animSettings.bounceDepth3D;

    if (value < RUNTIME_3D_BOUNCE_DEPTH_MIN) {
        value = RUNTIME_3D_BOUNCE_DEPTH_DEFAULT;
    }
    if (value > RUNTIME_3D_BOUNCE_DEPTH_MAX) {
        value = RUNTIME_3D_BOUNCE_DEPTH_MAX;
    }
    return value;
}

static double runtime_diffuse_bounce_3d_resolve_roulette_threshold(void) {
    double value = animSettings.rouletteThreshold3D;

    if (value < RUNTIME_3D_ROULETTE_THRESHOLD_MIN) {
        value = RUNTIME_3D_ROULETTE_THRESHOLD_MIN;
    }
    if (value > RUNTIME_3D_ROULETTE_THRESHOLD_MAX) {
        value = RUNTIME_3D_ROULETTE_THRESHOLD_MAX;
    }
    return value;
}

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

static double runtime_diffuse_bounce_3d_clamp(double value,
                                              double min_value,
                                              double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_diffuse_bounce_3d_peak(double r, double g, double b) {
    double peak = r;
    if (g > peak) peak = g;
    if (b > peak) peak = b;
    return peak;
}

static double runtime_diffuse_bounce_3d_luma(double r, double g, double b) {
    return (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
}

static uint32_t runtime_diffuse_bounce_3d_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint32_t runtime_diffuse_bounce_3d_seed_from_hit(
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
    return runtime_diffuse_bounce_3d_hash_u32(
        sx ^ (sy * 73856093U) ^ (sz * 19349663U) ^
        ((uint32_t)(hit->triangleIndex + 1) * 83492791U) ^
        runtime_diffuse_bounce_3d_hash_u32(sequence ^ 0x9e3779b9U));
}

static Vec3 runtime_diffuse_bounce_3d_sample_direction(const HitInfo3D* hit,
                                                       Vec3 normal,
                                                       Vec3 tangent,
                                                       Vec3 bitangent,
                                                       const RuntimeNative3DSamplingContext* sampling,
                                                       int sample_count,
                                                       int sample_index,
                                                       uint32_t sample_dimension) {
    uint32_t base_seed = runtime_diffuse_bounce_3d_seed_from_hit(hit, sampling);
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
                                         sample_dimension,
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

static void runtime_diffuse_bounce_3d_resolve_surface_reflectance(
    const HitInfo3D* hit,
    double* out_r,
    double* out_g,
    double* out_b) {
    RuntimeMaterialPayload3D payload = {0};
    double reflectance_scale = 1.0;
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;

    if (hit &&
        RuntimeMaterialPayload3D_ResolveFromHit(hit, &payload) &&
        payload.valid) {
        reflectance_scale = runtime_diffuse_bounce_3d_clamp(
            payload.bsdf.albedo * fmax(payload.bsdf.diffuseWeight, 0.15),
            0.05,
            1.0);
        r = runtime_diffuse_bounce_3d_clamp(payload.baseColorR * reflectance_scale, 0.0, 1.0);
        g = runtime_diffuse_bounce_3d_clamp(payload.baseColorG * reflectance_scale, 0.0, 1.0);
        b = runtime_diffuse_bounce_3d_clamp(payload.baseColorB * reflectance_scale, 0.0, 1.0);
    }

    if (out_r) *out_r = r;
    if (out_g) *out_g = g;
    if (out_b) *out_b = b;
}

static double runtime_diffuse_bounce_3d_resolve_roulette_sample(
    const HitInfo3D* hit,
    const RuntimeNative3DSamplingContext* sampling,
    int sample_count,
    int sample_index,
    int depth) {
    uint32_t base_seed = runtime_diffuse_bounce_3d_seed_from_hit(hit, sampling);
    double u = 0.5;
    double v = 0.5;

    RuntimeNative3DSampling_Stratified2D(sampling,
                                         base_seed ^ 0xa511e9b3u,
                                         sample_count,
                                         sample_index,
                                         (uint32_t)(1024 + depth),
                                         &u,
                                         &v);
    (void)v;
    return runtime_diffuse_bounce_3d_clamp(u, 0.0, 1.0);
}

static void runtime_diffuse_bounce_3d_trace_path(
    const RuntimeScene3D* scene,
    const HitInfo3D* root_hit,
    const RuntimeNative3DSamplingContext* sampling,
    int sample_count,
    int sample_index,
    RuntimeDiffuseBounce3DResult* io_result,
    double* io_r,
    double* io_g,
    double* io_b) {
    HitInfo3D current_hit = {0};
    double throughput_r = 1.0;
    double throughput_g = 1.0;
    double throughput_b = 1.0;
    const int max_depth = runtime_diffuse_bounce_3d_resolve_bounce_depth();
    const double roulette_threshold = runtime_diffuse_bounce_3d_resolve_roulette_threshold();

    if (!scene || !root_hit || !io_result || !io_r || !io_g || !io_b) return;

    current_hit = *root_hit;
    for (int depth = 1; depth <= max_depth; ++depth) {
        HitInfo3D next_hit = {0};
        RuntimeDirectLight3DResult next_direct = {0};
        Vec3 tangent = vec3(0.0, 0.0, 0.0);
        Vec3 bitangent = vec3(0.0, 0.0, 0.0);
        Vec3 sample_dir = vec3(0.0, 0.0, 1.0);
        Ray3D bounce_ray = {0};
        double parent_r = 1.0;
        double parent_g = 1.0;
        double parent_b = 1.0;
        double segment_distance = 0.0;
        double secondary_facing = 0.0;
        double hop_scale = 0.0;
        double path_r = 0.0;
        double path_g = 0.0;
        double path_b = 0.0;
        double contribution_peak = 0.0;

        runtime_diffuse_bounce_3d_build_basis(current_hit.normal, &tangent, &bitangent);
        sample_dir = runtime_diffuse_bounce_3d_sample_direction(&current_hit,
                                                                current_hit.normal,
                                                                tangent,
                                                                bitangent,
                                                                sampling,
                                                                sample_count,
                                                                sample_index,
                                                                (uint32_t)(depth - 1));
        bounce_ray = RuntimeRay3D_MakeOffset(current_hit.position,
                                             current_hit.normal,
                                             sample_dir,
                                             kRuntimeDiffuseBounce3DEpsilon);
        io_result->secondaryRayCount += 1;
        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &bounce_ray,
                                             kRuntimeDiffuseBounce3DEpsilon,
                                             kRuntimeDiffuseBounce3DMaxDistance,
                                             &next_hit)) {
            break;
        }

        io_result->secondaryHitCount += 1;
        if (next_hit.triangleIndex == current_hit.triangleIndex) {
            break;
        }

        runtime_diffuse_bounce_3d_resolve_surface_reflectance(&current_hit,
                                                              &parent_r,
                                                              &parent_g,
                                                              &parent_b);
        segment_distance = vec3_length(vec3_sub(next_hit.position, current_hit.position));
        secondary_facing = fabs(vec3_dot(next_hit.normal, vec3_scale(sample_dir, -1.0)));
        hop_scale = secondary_facing *
                    runtime_diffuse_bounce_3d_distance_decay(segment_distance) *
                    kRuntimeDiffuseBounce3DEnergyScale;
        path_r = runtime_diffuse_bounce_3d_clamp(throughput_r * parent_r * hop_scale, 0.0, 4.0);
        path_g = runtime_diffuse_bounce_3d_clamp(throughput_g * parent_g * hop_scale, 0.0, 4.0);
        path_b = runtime_diffuse_bounce_3d_clamp(throughput_b * parent_b * hop_scale, 0.0, 4.0);
        if (!(runtime_diffuse_bounce_3d_peak(path_r, path_g, path_b) > 1e-9)) {
            break;
        }

        if (RuntimeDirectLight3D_ShadeHit(scene, &next_hit, &next_direct)) {
            const double contributed_r = path_r * next_direct.radianceR;
            const double contributed_g = path_g * next_direct.radianceG;
            const double contributed_b = path_b * next_direct.radianceB;
            *io_r += contributed_r;
            *io_g += contributed_g;
            *io_b += contributed_b;
            contribution_peak = runtime_diffuse_bounce_3d_peak(contributed_r,
                                                               contributed_g,
                                                               contributed_b);
        }
        if (contribution_peak > 1e-9) {
            io_result->secondaryContributingHitCount += 1;
        }

        throughput_r = path_r;
        throughput_g = path_g;
        throughput_b = path_b;
        if (roulette_threshold > 0.0 && depth >= kRuntimeDiffuseBounce3DMinDepthBeforeRoulette) {
            const double throughput_luma =
                runtime_diffuse_bounce_3d_luma(throughput_r, throughput_g, throughput_b);
            if (throughput_luma < roulette_threshold) {
                const double survival =
                    runtime_diffuse_bounce_3d_clamp(throughput_luma / roulette_threshold,
                                                    1e-6,
                                                    1.0);
                const double roulette_sample =
                    runtime_diffuse_bounce_3d_resolve_roulette_sample(&next_hit,
                                                                      sampling,
                                                                      sample_count,
                                                                      sample_index,
                                                                      depth);
                if (roulette_sample > survival) {
                    break;
                }
                throughput_r =
                    runtime_diffuse_bounce_3d_clamp(throughput_r / survival, 0.0, 4.0);
                throughput_g =
                    runtime_diffuse_bounce_3d_clamp(throughput_g / survival, 0.0, 4.0);
                throughput_b =
                    runtime_diffuse_bounce_3d_clamp(throughput_b / survival, 0.0, 4.0);
            }
        }

        current_hit = next_hit;
    }
}

bool RuntimeDiffuseBounce3D_ShadeHit(const RuntimeScene3D* scene,
                                     const HitInfo3D* hit,
                                     const RuntimeNative3DSamplingContext* sampling,
                                     RuntimeDiffuseBounce3DResult* out_result) {
    RuntimeDiffuseBounce3DResult result = {0};
    RuntimeDirectLight3DResult direct_result = {0};
    double accumulated_r = 0.0;
    double accumulated_g = 0.0;
    double accumulated_b = 0.0;
    double bounce_limit = 0.0;
    int sample_count = 0;

    if (!scene || !hit || !out_result) return false;
    if (!scene->hasLight) return false;

    if (!RuntimeDirectLight3D_ShadeHit(scene, hit, &direct_result)) {
        return false;
    }

    result.hit = direct_result.hit;
    result.visible = direct_result.visible;
    result.hitInfo = direct_result.hitInfo;
    result.directRadiance = direct_result.radiance;
    result.directRadianceR = direct_result.radianceR;
    result.directRadianceG = direct_result.radianceG;
    result.directRadianceB = direct_result.radianceB;

    sample_count = runtime_diffuse_bounce_3d_resolve_sample_count();

    for (int i = 0; i < sample_count; ++i) {
        runtime_diffuse_bounce_3d_trace_path(scene,
                                             hit,
                                             sampling,
                                             sample_count,
                                             i,
                                             &result,
                                             &accumulated_r,
                                             &accumulated_g,
                                             &accumulated_b);
    }

    bounce_limit = fmax(scene->light.intensity * 0.35, 0.0);
    result.bounceRadianceR = runtime_diffuse_bounce_3d_clamp(
        accumulated_r / (double)sample_count,
        0.0,
        bounce_limit);
    result.bounceRadianceG = runtime_diffuse_bounce_3d_clamp(
        accumulated_g / (double)sample_count,
        0.0,
        bounce_limit);
    result.bounceRadianceB = runtime_diffuse_bounce_3d_clamp(
        accumulated_b / (double)sample_count,
        0.0,
        bounce_limit);
    result.bounceRadiance = runtime_diffuse_bounce_3d_peak(result.bounceRadianceR,
                                                           result.bounceRadianceG,
                                                           result.bounceRadianceB);
    result.radianceR = result.directRadianceR + result.bounceRadianceR;
    result.radianceG = result.directRadianceG + result.bounceRadianceG;
    result.radianceB = result.directRadianceB + result.bounceRadianceB;
    result.radiance = runtime_diffuse_bounce_3d_peak(result.radianceR,
                                                     result.radianceG,
                                                     result.radianceB);
    result.visible = result.visible || result.bounceRadiance > 0.0;
    *out_result = result;
    return true;
}

bool RuntimeDiffuseBounce3D_ShadePixel(const RuntimeScene3D* scene,
                                       const RuntimeCameraProjector3D* projector,
                                       double pixel_x,
                                       double pixel_y,
                                       const RuntimeNative3DSamplingContext* sampling,
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

    if (!RuntimeDiffuseBounce3D_ShadeHit(scene, &primary_hit.hitInfo, sampling, &result)) {
        result.primaryRay = primary_hit.primaryRay;
        *out_result = result;
        return false;
    }

    result.primaryRay = primary_hit.primaryRay;
    *out_result = result;
    return true;
}
