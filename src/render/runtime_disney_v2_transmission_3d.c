#include "render/runtime_disney_v2_transmission_3d.h"

#include <math.h>
#include <stdint.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_ray_3d.h"

enum {
    RUNTIME_DISNEY_V2_3D_PRIMARY_TRANSMISSION_SKIP_CAP = 4,
    RUNTIME_DISNEY_V2_3D_PRIMARY_TRANSMISSION_DEPTH_CAP = 8
};

static const double kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon = 1e-4;
static const double kRuntimeDisneyV2_3DPrimaryTransmissionMaxDistance = 1.0e6;
static const double kRuntimeDisneyV2_3DPrimaryTransmissionRoughConeScale = 0.18;

static double runtime_disney_v2_transmission_3d_clamp(double value,
                                                      double min_value,
                                                      double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_disney_v2_transmission_3d_clamp01(double value) {
    return runtime_disney_v2_transmission_3d_clamp(value, 0.0, 1.0);
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

static uint32_t runtime_disney_v2_3d_transmission_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint32_t runtime_disney_v2_3d_transmission_seed_from_hit(
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

static Vec3 runtime_disney_v2_3d_roughen_transmission_direction(
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

static bool runtime_disney_v2_3d_payload_is_transparent(
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

bool RuntimeDisneyV2_3D_SampleTransmission(
    const RuntimeMaterialPayload3D* payload,
    const RuntimePrincipledBSDF3D* principled,
    const HitInfo3D* hit,
    Vec3 view_dir,
    double transmission_probability,
    RuntimeDisneyV2_3DTransmissionSample* out_sample) {
    RuntimeDisneyV2_3DTransmissionSample sample = {0};
    double transmission_weight = 0.0;
    double fresnel_transmission = 0.0;

    if (!payload || !principled || !principled->valid || !hit || !out_sample) return false;
    if (!(transmission_probability > 1e-9) || !(principled->transmissionWeight > 1e-9)) {
        *out_sample = sample;
        return false;
    }
    if (!RuntimeDielectricTransport3D_Resolve(payload,
                                              hit->normal,
                                              vec3_scale(vec3_normalize(view_dir), -1.0),
                                              &sample.dielectric) ||
        !sample.dielectric.hasRefraction) {
        *out_sample = sample;
        return false;
    }

    fresnel_transmission = 1.0 -
                           runtime_disney_v2_transmission_3d_clamp01(sample.dielectric.fresnel);
    transmission_weight =
        runtime_disney_v2_transmission_3d_clamp01(principled->transmissionWeight) *
        runtime_disney_v2_transmission_3d_clamp(fresnel_transmission, 0.05, 1.0);

    sample.direction = vec3_normalize(sample.dielectric.refractionDir);
    sample.pdf = fmax(transmission_probability, 1e-6);
    sample.throughputR =
        runtime_disney_v2_transmission_3d_clamp(principled->baseColorR *
                                                    transmission_weight / sample.pdf,
                                                0.0,
                                                2.0);
    sample.throughputG =
        runtime_disney_v2_transmission_3d_clamp(principled->baseColorG *
                                                    transmission_weight / sample.pdf,
                                                0.0,
                                                2.0);
    sample.throughputB =
        runtime_disney_v2_transmission_3d_clamp(principled->baseColorB *
                                                    transmission_weight / sample.pdf,
                                                0.0,
                                                2.0);
    sample.valid = vec3_length(sample.direction) > 1e-9 &&
                   (sample.throughputR > 1e-9 ||
                    sample.throughputG > 1e-9 ||
                    sample.throughputB > 1e-9);
    *out_sample = sample;
    return sample.valid;
}

static bool runtime_disney_v2_3d_trace_transmission_next_hit(
    const RuntimeScene3D* scene,
    const HitInfo3D* source_hit,
    Vec3 direction,
    HitInfo3D* out_hit,
    Ray3D* out_ray,
    int* out_ray_count) {
    Ray3D ray = {0};
    HitInfo3D hit = {0};
    double remaining_distance = kRuntimeDisneyV2_3DPrimaryTransmissionMaxDistance;
    int ray_count = 0;

    if (out_ray_count) *out_ray_count = 0;
    if (!scene || !source_hit || !out_hit || !out_ray) return false;
    HitInfo3D_Reset(out_hit);
    ray = RuntimeRay3D_MakeOffset(source_hit->position,
                                  source_hit->normal,
                                  direction,
                                  kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon);
    for (int skip_count = 0;
         skip_count <= RUNTIME_DISNEY_V2_3D_PRIMARY_TRANSMISSION_SKIP_CAP &&
         remaining_distance > kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon;
         ++skip_count) {
        HitInfo3D_Reset(&hit);
        ray_count += 1;
        if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                             &ray,
                                             kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon,
                                             remaining_distance,
                                             &hit)) {
            if (out_ray_count) *out_ray_count = ray_count;
            *out_ray = ray;
            return false;
        }
        if (hit.sceneObjectIndex != source_hit->sceneObjectIndex ||
            hit.triangleIndex != source_hit->triangleIndex) {
            *out_hit = hit;
            *out_ray = ray;
            if (out_ray_count) *out_ray_count = ray_count;
            return true;
        }
        remaining_distance -= hit.t;
        ray = RuntimeRay3D_MakeOffset(hit.position,
                                      hit.normal,
                                      direction,
                                      kRuntimeDisneyV2_3DPrimaryTransmissionEpsilon);
    }
    if (out_ray_count) *out_ray_count = ray_count;
    *out_ray = ray;
    return false;
}

static bool runtime_disney_v2_3d_trace_primary_transmission_receiver(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeNative3DSamplingContext* sampling,
    const RuntimeDisneyV2_3DTransmissionSample* initial_sample,
    Vec3 sample_direction,
    double blend_weight,
    RuntimeDisneyV2_3DResult* io_result,
    RuntimeDirectLight3DResult* out_direct,
    HitInfo3D* out_hit,
    Ray3D* out_ray,
    double* out_throughput_r,
    double* out_throughput_g,
    double* out_throughput_b,
    int* out_depth) {
    HitInfo3D source_hit = {0};
    HitInfo3D hit = {0};
    Ray3D ray = {0};
    RuntimeMaterialPayload3D payload = {0};
    RuntimePrincipledBSDF3D principled = {0};
    RuntimeDirectLight3DResult direct = {0};
    RuntimeDisneyV2_3DResult receiver_result = {0};
    Vec3 direction = sample_direction;
    double throughput_r = initial_sample ? initial_sample->throughputR : 0.0;
    double throughput_g = initial_sample ? initial_sample->throughputG : 0.0;
    double throughput_b = initial_sample ? initial_sample->throughputB : 0.0;
    int depth = 1;

    if (!scene || !primary_hit || !primary_hit->hit || !initial_sample || !io_result ||
        !out_direct || !out_hit || !out_ray || !out_throughput_r || !out_throughput_g ||
        !out_throughput_b || !out_depth) {
        return false;
    }

    source_hit = primary_hit->hitInfo;
    for (depth = 1; depth <= RUNTIME_DISNEY_V2_3D_PRIMARY_TRANSMISSION_DEPTH_CAP; ++depth) {
        int ray_count = 0;

        if (!RuntimePathDepthPolicy3D_AllowsDepth(&io_result->pathPolicy,
                                                  RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION,
                                                  depth)) {
            io_result->primaryTransmissionDepthLimitCount += 1;
            return false;
        }
        if (!runtime_disney_v2_3d_trace_transmission_next_hit(scene,
                                                              &source_hit,
                                                              direction,
                                                              &hit,
                                                              &ray,
                                                              &ray_count)) {
            io_result->primaryTransmissionRayCount += ray_count;
            return false;
        }
        io_result->primaryTransmissionRayCount += ray_count;
        io_result->primaryTransmissionHitCount += 1;

        if (!RuntimeMaterialPayload3D_ResolveFromHit(&hit, &payload)) {
            return false;
        }
        principled = RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
        if (!runtime_disney_v2_3d_payload_is_transparent(&payload, &principled)) {
            if (RuntimeDisneyV2_3D_ShadeHit(scene, &hit, sampling, &receiver_result) &&
                receiver_result.visible) {
                direct.visible = true;
                direct.radianceR = receiver_result.radianceR;
                direct.radianceG = receiver_result.radianceG;
                direct.radianceB = receiver_result.radianceB;
                direct.radiance = receiver_result.radiance;
                io_result->primaryTransmissionReceiverShadeCount += 1;
                io_result->primaryTransmissionReceiverRadianceR += receiver_result.radianceR;
                io_result->primaryTransmissionReceiverRadianceG += receiver_result.radianceG;
                io_result->primaryTransmissionReceiverRadianceB += receiver_result.radianceB;
            } else {
                if (!RuntimeDirectLight3D_ShadeHitWithPayload(scene,
                                                              &hit,
                                                              &payload,
                                                              sampling,
                                                              &direct)) {
                    return false;
                }
            }
            *out_direct = direct;
            *out_hit = hit;
            *out_ray = ray;
            *out_throughput_r = throughput_r * blend_weight;
            *out_throughput_g = throughput_g * blend_weight;
            *out_throughput_b = throughput_b * blend_weight;
            *out_depth = depth;
            return true;
        }

        io_result->primaryTransmissionTransparentSurfaceCount += 1;
        throughput_r *= principled.baseColorR *
                        runtime_disney_v2_3d_transparent_surface_weight(&principled);
        throughput_g *= principled.baseColorG *
                        runtime_disney_v2_3d_transparent_surface_weight(&principled);
        throughput_b *= principled.baseColorB *
                        runtime_disney_v2_3d_transparent_surface_weight(&principled);

        if (!RuntimePathDepthPolicy3D_AllowsDepth(&io_result->pathPolicy,
                                                  RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION,
                                                  depth + 1)) {
            io_result->primaryTransmissionDepthLimitCount += 1;
            return false;
        }
        source_hit = hit;
    }

    io_result->primaryTransmissionDepthLimitCount += 1;
    return false;
}

bool RuntimeDisneyV2_3D_ApplyPrimaryTransmissionContinuation(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDisneyV2_3DResult* io_result) {
    RuntimeDisneyV2_3DTransmissionSample sample = {0};
    RuntimeDisneyV2_3DTransmissionSample sample_path = {0};
    HitInfo3D continuation_hit = {0};
    RuntimeDirectLight3DResult continuation_direct = {0};
    Ray3D continuation_ray = {0};
    Vec3 view_dir = vec3(0.0, 0.0, 1.0);
    double fresnel_transmission = 0.0;
    double blend_weight = 0.0;
    double front_diffuse_weight = 1.0;
    double front_specular_weight = 1.0;
    double accum_r = 0.0;
    double accum_g = 0.0;
    double accum_b = 0.0;
    double accum_throughput_r = 0.0;
    double accum_throughput_g = 0.0;
    double accum_throughput_b = 0.0;
    double path_throughput_r = 0.0;
    double path_throughput_g = 0.0;
    double path_throughput_b = 0.0;
    double best_throughput_r = 0.0;
    double best_throughput_g = 0.0;
    double best_throughput_b = 0.0;
    int sample_count = 1;
    int contributing_sample_count = 0;
    int receiver_depth = 0;
    int best_depth = 0;
    uint32_t sample_seed = 0U;
    HitInfo3D best_hit = {0};
    Ray3D best_ray = {0};

    if (!scene || !primary_hit || !primary_hit->hit || !io_result ||
        !io_result->payloadResolved || !io_result->principled.valid) {
        return false;
    }
    if (!(io_result->transmissionProbability > 1e-9) ||
        !(io_result->principled.transmissionWeight > 1e-9)) {
        return false;
    }
    if (!io_result->pathPolicyResolved ||
        !RuntimePathDepthPolicy3D_AllowsDepth(&io_result->pathPolicy,
                                              RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION,
                                              1)) {
        return false;
    }

    view_dir = vec3_scale(primary_hit->primaryRay.direction, -1.0);
    if (!RuntimeDisneyV2_3D_SampleTransmission(&io_result->payload,
                                               &io_result->principled,
                                               &primary_hit->hitInfo,
                                               view_dir,
                                               io_result->transmissionProbability,
                                               &sample)) {
        return false;
    }

    fresnel_transmission =
        1.0 - runtime_disney_v2_transmission_3d_clamp01(sample.dielectric.fresnel);
    blend_weight =
        runtime_disney_v2_transmission_3d_clamp01(io_result->principled.transmissionWeight) *
        runtime_disney_v2_transmission_3d_clamp(fresnel_transmission, 0.05, 1.0);
    front_diffuse_weight =
        runtime_disney_v2_transmission_3d_clamp((1.0 - blend_weight) *
                                                    (1.0 - blend_weight),
                                                0.015,
                                                1.0);
    front_specular_weight =
        runtime_disney_v2_transmission_3d_clamp(front_diffuse_weight +
                                                    (sample.dielectric.fresnel * 0.80),
                                                0.05,
                                                0.65);
    sample_count = runtime_disney_v2_3d_resolve_primary_transmission_sample_count();
    sample_seed = runtime_disney_v2_3d_transmission_seed_from_hit(&primary_hit->hitInfo,
                                                                  sampling);
    io_result->primaryTransmissionSampleCount = sample_count;

    for (int sample_index = 0; sample_index < sample_count; ++sample_index) {
        Vec3 sample_direction = runtime_disney_v2_3d_roughen_transmission_direction(
            sample.direction,
            io_result->principled.roughness,
            sampling,
            sample_seed,
            sample_count,
            sample_index);

        sample_path = sample;
        sample_path.direction = sample_direction;
        continuation_direct = (RuntimeDirectLight3DResult){0};
        continuation_hit = (HitInfo3D){0};
        continuation_ray = (Ray3D){0};
        path_throughput_r = 0.0;
        path_throughput_g = 0.0;
        path_throughput_b = 0.0;
        receiver_depth = 0;
        if (!runtime_disney_v2_3d_trace_primary_transmission_receiver(scene,
                                                                      primary_hit,
                                                                      sampling,
                                                                      &sample_path,
                                                                      sample_direction,
                                                                      blend_weight,
                                                                      io_result,
                                                                      &continuation_direct,
                                                                      &continuation_hit,
                                                                      &continuation_ray,
                                                                      &path_throughput_r,
                                                                      &path_throughput_g,
                                                                      &path_throughput_b,
                                                                      &receiver_depth)) {
            continue;
        }
        accum_r += continuation_direct.radianceR * path_throughput_r;
        accum_g += continuation_direct.radianceG * path_throughput_g;
        accum_b += continuation_direct.radianceB * path_throughput_b;
        accum_throughput_r += path_throughput_r;
        accum_throughput_g += path_throughput_g;
        accum_throughput_b += path_throughput_b;
        contributing_sample_count += 1;
        best_depth = receiver_depth;
        best_hit = continuation_hit;
        best_ray = continuation_ray;
        best_throughput_r = path_throughput_r;
        best_throughput_g = path_throughput_g;
        best_throughput_b = path_throughput_b;
    }

    if (contributing_sample_count <= 0) {
        return false;
    }

    io_result->directRadianceR *= front_diffuse_weight;
    io_result->directRadianceG *= front_diffuse_weight;
    io_result->directRadianceB *= front_diffuse_weight;
    io_result->diffuseRadianceR *= front_diffuse_weight;
    io_result->diffuseRadianceG *= front_diffuse_weight;
    io_result->diffuseRadianceB *= front_diffuse_weight;
    io_result->specularRadianceR *= front_specular_weight;
    io_result->specularRadianceG *= front_specular_weight;
    io_result->specularRadianceB *= front_specular_weight;
    io_result->transmissionRadianceR *= front_diffuse_weight;
    io_result->transmissionRadianceG *= front_diffuse_weight;
    io_result->transmissionRadianceB *= front_diffuse_weight;
    io_result->stochasticDirectRadianceR *= front_diffuse_weight;
    io_result->stochasticDirectRadianceG *= front_diffuse_weight;
    io_result->stochasticDirectRadianceB *= front_diffuse_weight;
    io_result->stochasticBsdfRadianceR *= front_diffuse_weight;
    io_result->stochasticBsdfRadianceG *= front_diffuse_weight;
    io_result->stochasticBsdfRadianceB *= front_diffuse_weight;
    io_result->recursiveBsdfRadianceR *= front_diffuse_weight;
    io_result->recursiveBsdfRadianceG *= front_diffuse_weight;
    io_result->recursiveBsdfRadianceB *= front_diffuse_weight;

    io_result->primaryTransmissionRadianceR = accum_r / (double)contributing_sample_count;
    io_result->primaryTransmissionRadianceG = accum_g / (double)contributing_sample_count;
    io_result->primaryTransmissionRadianceB = accum_b / (double)contributing_sample_count;
    io_result->primaryTransmissionRadiance =
        fmax(io_result->primaryTransmissionRadianceR,
             fmax(io_result->primaryTransmissionRadianceG,
                  io_result->primaryTransmissionRadianceB));
    io_result->primaryTransmissionSurfaceWeight = front_diffuse_weight;
    io_result->primaryTransmissionBlendWeight = blend_weight;
    if (io_result->primaryTransmissionReceiverShadeCount > 0) {
        double receiver_count = (double)io_result->primaryTransmissionReceiverShadeCount;
        io_result->primaryTransmissionReceiverRadianceR /= receiver_count;
        io_result->primaryTransmissionReceiverRadianceG /= receiver_count;
        io_result->primaryTransmissionReceiverRadianceB /= receiver_count;
        io_result->primaryTransmissionReceiverRadiance =
            fmax(io_result->primaryTransmissionReceiverRadianceR,
                 fmax(io_result->primaryTransmissionReceiverRadianceG,
                      io_result->primaryTransmissionReceiverRadianceB));
    }
    io_result->primaryTransmissionFrontDiffuseWeight = front_diffuse_weight;
    io_result->primaryTransmissionFrontSpecularWeight = front_specular_weight;
    io_result->primaryTransmissionCameraThroughputR =
        accum_throughput_r / (double)contributing_sample_count;
    io_result->primaryTransmissionCameraThroughputG =
        accum_throughput_g / (double)contributing_sample_count;
    io_result->primaryTransmissionCameraThroughputB =
        accum_throughput_b / (double)contributing_sample_count;
    io_result->primaryTransmissionReceiverSampleCount = contributing_sample_count;
    io_result->primaryTransmissionContinued = true;
    io_result->primaryTransmissionPathState.valid = true;
    io_result->primaryTransmissionPathState.hit = true;
    io_result->primaryTransmissionPathState.depth = best_depth;
    io_result->primaryTransmissionPathState.sampledLobe =
        RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION;
    io_result->primaryTransmissionPathState.ray = best_ray;
    io_result->primaryTransmissionPathState.hitInfo = best_hit;
    io_result->primaryTransmissionPathState.throughputR = best_throughput_r;
    io_result->primaryTransmissionPathState.throughputG = best_throughput_g;
    io_result->primaryTransmissionPathState.throughputB = best_throughput_b;
    io_result->primaryTransmissionPathState.pdf = sample.pdf;
    return true;
}
