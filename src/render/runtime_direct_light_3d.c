#include "render/runtime_direct_light_3d.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "config/config_manager.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_volume_3d_integrate.h"

static const double kRuntimeDirectLight3DTopFillIntensityScale = 0.08;
static const double kRuntimeDirectLight3DTopFillIntensityMax = 0.75;
static const double kRuntimeDirectLight3DContributionEpsilon = 1e-8;
#define RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_POPULATION_COUNT 16
#define RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_ACTIVE_COUNT 8
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

static double runtime_direct_light_3d_peak(double r, double g, double b) {
    double peak = r;
    if (g > peak) peak = g;
    if (b > peak) peak = b;
    return peak;
}

static double runtime_direct_light_3d_clamp(double value,
                                            double min_value,
                                            double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool runtime_direct_light_3d_sample_contributes(double light_intensity,
                                                       double attenuation,
                                                       double ndotl) {
    return light_intensity * attenuation * ndotl > kRuntimeDirectLight3DContributionEpsilon;
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

static int runtime_direct_light_3d_area_light_decision_count(const RuntimeLight3D* light) {
    int sample_count = runtime_direct_light_3d_area_light_sample_count(light);
    if (sample_count < RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_DECISION_COUNT) {
        return sample_count;
    }
    return RUNTIME_DIRECT_LIGHT_3D_AREA_LIGHT_DECISION_COUNT;
}

static bool runtime_direct_light_3d_area_light_can_stop_adaptive(
    int evaluated_count,
    int decision_count,
    int max_count,
    int clear_visible_count,
    int clear_blocked_count) {
    if (decision_count <= 0 || decision_count >= max_count) return false;
    if (evaluated_count != decision_count) return false;
    return clear_visible_count == decision_count || clear_blocked_count == decision_count;
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

static void runtime_direct_light_3d_apply_transmittance(
    const RuntimeVisibility3DTransmittance* transmittance,
    RuntimeDirectLight3DResult* io_result) {
    if (!transmittance || !io_result) return;

    io_result->radianceR *= transmittance->r;
    io_result->radianceG *= transmittance->g;
    io_result->radianceB *= transmittance->b;
    io_result->radiance = runtime_direct_light_3d_peak(io_result->radianceR,
                                                       io_result->radianceG,
                                                       io_result->radianceB);
    if (!(transmittance->luma > 1e-9)) {
        io_result->visible = false;
    }
}

static double runtime_direct_light_3d_top_fill_radiance(const RuntimeScene3D* scene,
                                                        const HitInfo3D* hit) {
    double up_facing = 0.0;
    double top_fill_intensity = 0.0;

    if (!scene || !hit || scene->environment.lightMode != ENVIRONMENT_LIGHT_MODE_TOP_FILL) {
        return 0.0;
    }

    up_facing = runtime_direct_light_3d_clamp(hit->normal.z, 0.0, 1.0);
    if (!(up_facing > 0.0)) return 0.0;

    top_fill_intensity =
        runtime_direct_light_3d_clamp(scene->environment.topFillIntensity *
                                          kRuntimeDirectLight3DTopFillIntensityScale,
                                      0.0,
                                      kRuntimeDirectLight3DTopFillIntensityMax);
    return top_fill_intensity * up_facing;
}

static void runtime_direct_light_3d_resolve_hit_tint(const HitInfo3D* hit,
                                                     const RuntimeMaterialPayload3D* resolved_payload,
                                                     double* out_r,
                                                     double* out_g,
                                                     double* out_b) {
    RuntimeMaterialPayload3D payload = {0};
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;

    if (resolved_payload && resolved_payload->valid) {
        r = resolved_payload->baseColorR;
        g = resolved_payload->baseColorG;
        b = resolved_payload->baseColorB;
    } else if (hit &&
        hit->sceneObjectIndex >= 0 &&
        hit->sceneObjectIndex < sceneSettings.objectCount &&
        RuntimeMaterialPayload3D_ResolveFromHit(hit, &payload) &&
        payload.valid) {
        r = payload.baseColorR;
        g = payload.baseColorG;
        b = payload.baseColorB;
    }

    if (out_r) *out_r = r;
    if (out_g) *out_g = g;
    if (out_b) *out_b = b;
}

bool RuntimeDirectLight3D_TracePrimaryHit(const RuntimeScene3D* scene,
                                          const RuntimeCameraProjector3D* projector,
                                          double pixel_x,
                                          double pixel_y,
                                          RuntimePrimaryHit3DResult* out_result) {
    RuntimePrimaryHit3DResult result = {0};

    if (!scene || !projector || !out_result) return false;

    result.primaryRay = RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
    result.primaryTransmittance = RuntimeVisibility3D_UnitTransmittance();
    if (!RuntimeRay3D_TraceSceneFirstHit(scene,
                                         &result.primaryRay,
                                         projector->nearPlane,
                                         HUGE_VAL,
                                         &result.hitInfo)) {
        *out_result = result;
        return false;
    }

    result.hit = true;
    result.primaryTransmittance =
        RuntimeVolume3D_TransmittanceAlongRayRGB(&scene->volume,
                                                 &result.primaryRay,
                                                 projector->nearPlane,
                                                 result.hitInfo.t);
    *out_result = result;
    return true;
}

bool RuntimeDirectLight3D_ShadeHit(const RuntimeScene3D* scene,
                                   const HitInfo3D* hit,
                                   const RuntimeNative3DSamplingContext* sampling,
                                   RuntimeDirectLight3DResult* out_result) {
    return RuntimeDirectLight3D_ShadeHitWithPayload(scene, hit, NULL, sampling, out_result);
}

bool RuntimeDirectLight3D_ShadeHitWithPayload(const RuntimeScene3D* scene,
                                              const HitInfo3D* hit,
                                              const RuntimeMaterialPayload3D* payload,
                                              const RuntimeNative3DSamplingContext* sampling,
                                              RuntimeDirectLight3DResult* out_result) {
    RuntimeDirectLight3DResult result = {0};
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double light_distance =
        0.0;
    [[fisics::dim(length)]] [[fisics::unit(meter)]] double length_epsilon = 1e-9;
    double attenuation = 0.0;
    RuntimeVisibility3DTransmittance transmittance = {0};
    double tint_r = 1.0;
    double tint_g = 1.0;
    double tint_b = 1.0;
    double light_r = 0.0;
    double light_g = 0.0;
    double light_b = 0.0;
    double top_fill = 0.0;
    bool any_light_sample_visible = false;
    int light_sample_count = 0;
    int light_sample_decision_count = 0;
    int light_sample_evaluated_count = 0;
    int clear_visible_sample_count = 0;
    int clear_blocked_sample_count = 0;

    if (!scene || !hit || !out_result) return false;
    if (!scene->hasLight) return false;
    if (hit->triangleIndex < 0) return false;

    result.hit = true;
    result.hitInfo = *hit;
    runtime_direct_light_3d_resolve_hit_tint(hit, payload, &tint_r, &tint_g, &tint_b);
    top_fill = runtime_direct_light_3d_top_fill_radiance(scene, hit);
    to_light = vec3_sub(scene->light.position, hit->position);
    light_distance = vec3_length(to_light);
    result.lightDistance = light_distance;
    if (light_distance <= length_epsilon) {
        result.visible = true;
        result.ndotl = 1.0;
        result.attenuation = 1.0;
        result.radiance = scene->light.intensity;
        result.radianceR = result.radiance * tint_r;
        result.radianceG = result.radiance * tint_g;
        result.radianceB = result.radiance * tint_b;
        *out_result = result;
        return true;
    }

    to_light = vec3_scale(to_light, 1.0 / light_distance);
    result.ndotl = fmax(0.0, vec3_dot(hit->normal, to_light));
    attenuation = runtime_direct_light_3d_attenuation(&scene->light, light_distance);
    result.attenuation = attenuation;
    light_sample_count = runtime_direct_light_3d_area_light_sample_count(&scene->light);
    light_sample_decision_count =
        runtime_direct_light_3d_area_light_decision_count(&scene->light);
    if (result.ndotl > 0.0 && light_sample_count > 0) {
        for (int i = 0; i < light_sample_count; ++i) {
            Vec3 sample_position = runtime_direct_light_3d_sample_light_position(&scene->light,
                                                                                 to_light,
                                                                                 hit,
                                                                                 i,
                                                                                 sampling);
            Vec3 sample_to_light = vec3_sub(sample_position, hit->position);
            [[fisics::dim(length)]] [[fisics::unit(meter)]] double sample_distance =
                vec3_length(sample_to_light);
            double sample_attenuation = 0.0;
            double sample_ndotl = 0.0;
            Vec3 sample_dir = vec3(0.0, 0.0, 0.0);

            light_sample_evaluated_count += 1;
            if (!(sample_distance > length_epsilon)) {
                clear_blocked_sample_count += 1;
                if (runtime_direct_light_3d_area_light_can_stop_adaptive(
                        light_sample_evaluated_count,
                        light_sample_decision_count,
                        light_sample_count,
                        clear_visible_sample_count,
                        clear_blocked_sample_count)) {
                    break;
                }
                continue;
            }
            sample_dir = vec3_scale(sample_to_light, 1.0 / sample_distance);
            sample_ndotl = fmax(0.0, vec3_dot(hit->normal, sample_dir));
            if (!(sample_ndotl > 0.0)) {
                clear_blocked_sample_count += 1;
                if (runtime_direct_light_3d_area_light_can_stop_adaptive(
                        light_sample_evaluated_count,
                        light_sample_decision_count,
                        light_sample_count,
                        clear_visible_sample_count,
                        clear_blocked_sample_count)) {
                    break;
                }
                continue;
            }

            sample_attenuation =
                runtime_direct_light_3d_attenuation(&scene->light, sample_distance);
            if (!runtime_direct_light_3d_sample_contributes(scene->light.intensity,
                                                            sample_attenuation,
                                                            sample_ndotl)) {
                clear_blocked_sample_count += 1;
                if (runtime_direct_light_3d_area_light_can_stop_adaptive(
                        light_sample_evaluated_count,
                        light_sample_decision_count,
                        light_sample_count,
                        clear_visible_sample_count,
                        clear_blocked_sample_count)) {
                    break;
                }
                continue;
            }

            transmittance = RuntimeVisibility3D_TransmittanceFromHitToPointRGB(scene,
                                                                                hit,
                                                                                sample_position,
                                                                                -1,
                                                                                -1);
            if (transmittance.luma > 1e-6) {
                any_light_sample_visible = true;
            }
            if (transmittance.luma > 0.999) {
                clear_visible_sample_count += 1;
            } else if (!(transmittance.luma > 1e-6)) {
                clear_blocked_sample_count += 1;
            }
            light_r += scene->light.intensity * sample_attenuation * sample_ndotl * transmittance.r;
            light_g += scene->light.intensity * sample_attenuation * sample_ndotl * transmittance.g;
            light_b += scene->light.intensity * sample_attenuation * sample_ndotl * transmittance.b;
            if (runtime_direct_light_3d_area_light_can_stop_adaptive(
                    light_sample_evaluated_count,
                    light_sample_decision_count,
                    light_sample_count,
                    clear_visible_sample_count,
                    clear_blocked_sample_count)) {
                break;
            }
        }
        if (light_sample_evaluated_count <= 0) {
            light_sample_evaluated_count = light_sample_count;
        }
        light_r /= (double)light_sample_evaluated_count;
        light_g /= (double)light_sample_evaluated_count;
        light_b /= (double)light_sample_evaluated_count;
    }
    result.visible = any_light_sample_visible || top_fill > 1e-6;
    result.radianceR = (light_r + top_fill) * tint_r;
    result.radianceG = (light_g + top_fill) * tint_g;
    result.radianceB = (light_b + top_fill) * tint_b;
    result.radiance = runtime_direct_light_3d_peak(result.radianceR,
                                                   result.radianceG,
                                                   result.radianceB);

    *out_result = result;
    return true;
}

bool RuntimeDirectLight3D_ShadePixel(const RuntimeScene3D* scene,
                                     const RuntimeCameraProjector3D* projector,
                                     double pixel_x,
                                     double pixel_y,
                                     const RuntimeNative3DSamplingContext* sampling,
                                     RuntimeDirectLight3DResult* out_result) {
    RuntimeDirectLight3DResult result = {0};
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

    return RuntimeDirectLight3D_ShadePrimaryHit(scene, &primary_hit, sampling, out_result);
}

bool RuntimeDirectLight3D_ShadePrimaryHit(const RuntimeScene3D* scene,
                                          const RuntimePrimaryHit3DResult* primary_hit,
                                          const RuntimeNative3DSamplingContext* sampling,
                                          RuntimeDirectLight3DResult* out_result) {
    return RuntimeDirectLight3D_ShadePrimaryHitWithPayload(scene,
                                                           primary_hit,
                                                           NULL,
                                                           sampling,
                                                           out_result);
}

bool RuntimeDirectLight3D_ShadePrimaryHitWithPayload(
    const RuntimeScene3D* scene,
    const RuntimePrimaryHit3DResult* primary_hit,
    const RuntimeMaterialPayload3D* payload,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeDirectLight3DResult* out_result) {
    RuntimeDirectLight3DResult result = {0};

    if (!scene || !primary_hit || !out_result) return false;
    if (!primary_hit->hit) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }

    if (!RuntimeDirectLight3D_ShadeHitWithPayload(scene,
                                                  &primary_hit->hitInfo,
                                                  payload,
                                                  sampling,
                                                  &result)) {
        result.primaryRay = primary_hit->primaryRay;
        *out_result = result;
        return false;
    }

    runtime_direct_light_3d_apply_transmittance(&primary_hit->primaryTransmittance, &result);
    result.primaryRay = primary_hit->primaryRay;
    *out_result = result;
    return true;
}
