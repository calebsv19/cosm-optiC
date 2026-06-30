#include "render/runtime_volume_3d_scatter.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "render/runtime_visibility_3d.h"
#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_sampling.h"

static const double kRuntimeVolume3DScatterMinimumStep = 1e-4;
static const double kRuntimeVolume3DScatterPhaseIsotropic = 0.07957747154594767; /* 1 / (4*pi) */
static const double kRuntimeVolume3DScatterStrength = 0.12;
static const double kRuntimeVolume3DScatterAnisotropy = 0.55;
static double gRuntimeVolume3DScatterStrengthGain = 1.0;
static double gRuntimeVolume3DScatterStepScale = 1.0;
static double gRuntimeVolume3DScatterTintR = 1.0;
static double gRuntimeVolume3DScatterTintG = 1.0;
static double gRuntimeVolume3DScatterTintB = 1.0;

static double runtime_volume_3d_scatter_zero_length(void) {
    double zero [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    return zero;
}

static double runtime_volume_3d_scatter_unit_length(void) {
    double unit_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = 1.0;
    return unit_length;
}

static double runtime_volume_3d_scatter_length_epsilon(void) {
    double epsilon [[fisics::dim(length)]] [[fisics::unit(meter)]] = 1e-9;
    return epsilon;
}

static double runtime_volume_3d_scatter_minimum_step(void) {
    double minimum_step [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        kRuntimeVolume3DScatterMinimumStep;
    return minimum_step;
}

static double runtime_volume_3d_scatter_clamp(double value,
                                              double min_value,
                                              double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_volume_3d_scatter_peak(double r, double g, double b) {
    double peak = r;
    if (g > peak) peak = g;
    if (b > peak) peak = b;
    return peak;
}

static bool runtime_volume_3d_scatter_has_temporal_jitter(
    const RuntimeNative3DSamplingContext* sampling) {
    return sampling && sampling->temporalSubpassCount > 1u;
}

static void runtime_volume_3d_scatter_build_basis(Vec3 normal,
                                                  Vec3* out_tangent,
                                                  Vec3* out_bitangent) {
    Vec3 guide = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 tangent = vec3_cross(guide, normal);
    Vec3 bitangent = vec3(0.0, 0.0, 0.0);
    double epsilon = runtime_volume_3d_scatter_length_epsilon();

    if (vec3_length(tangent) <= epsilon) {
        tangent = vec3(1.0, 0.0, 0.0);
    }
    tangent = vec3_normalize(tangent);
    bitangent = vec3_normalize(vec3_cross(normal, tangent));
    if (vec3_length(bitangent) <= epsilon) {
        tangent = vec3(1.0, 0.0, 0.0);
        bitangent = vec3(0.0, 0.0, 1.0);
    }
    if (out_tangent) *out_tangent = tangent;
    if (out_bitangent) *out_bitangent = bitangent;
}

static void runtime_volume_3d_scatter_map_square_to_disk(double u,
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

static Vec3 runtime_volume_3d_scatter_resolve_light_position(
    const RuntimeLight3D* light,
    const Vec3* sample_position,
    const RuntimeNative3DSamplingContext* sampling) {
    Vec3 to_light = vec3(0.0, 0.0, 0.0);
    Vec3 light_dir = vec3(0.0, 0.0, 0.0);
    Vec3 tangent = vec3(0.0, 0.0, 0.0);
    Vec3 bitangent = vec3(0.0, 0.0, 0.0);
    double u = 0.5;
    double v = 0.5;
    double disk_x = 0.0;
    double disk_y = 0.0;
    double radius [[fisics::dim(length)]] [[fisics::unit(meter)]] = runtime_volume_3d_scatter_zero_length();
    uint32_t base_seed = 0u;
    double epsilon = runtime_volume_3d_scatter_length_epsilon();

    if (!light) return vec3(0.0, 0.0, 0.0);
    if (!sample_position || !(light->radius > epsilon) ||
        !runtime_volume_3d_scatter_has_temporal_jitter(sampling)) {
        return light->position;
    }

    to_light = vec3_sub(light->position, *sample_position);
    if (vec3_length(to_light) <= epsilon) {
        return light->position;
    }
    light_dir = vec3_normalize(to_light);
    runtime_volume_3d_scatter_build_basis(light_dir, &tangent, &bitangent);
    base_seed = (uint32_t)(fabs(sample_position->x) * 4096.0) ^
                ((uint32_t)(fabs(sample_position->y) * 2048.0) << 1u) ^
                ((uint32_t)(fabs(sample_position->z) * 1024.0) << 2u) ^
                0x9e3779b9u;
    RuntimeNative3DSampling_Stratified2D(sampling, base_seed, 1, 0, 31u, &u, &v);
    runtime_volume_3d_scatter_map_square_to_disk(u, v, &disk_x, &disk_y);
    radius = light->radius * 0.7071067811865476;
    return vec3_add(light->position,
                    vec3_add(vec3_scale(tangent, disk_x * radius),
                             vec3_scale(bitangent, disk_y * radius)));
}

void RuntimeVolume3DScatter_ResetTuning(void) {
    gRuntimeVolume3DScatterStrengthGain = 1.0;
    gRuntimeVolume3DScatterStepScale = 1.0;
    gRuntimeVolume3DScatterTintR = 1.0;
    gRuntimeVolume3DScatterTintG = 1.0;
    gRuntimeVolume3DScatterTintB = 1.0;
}

void RuntimeVolume3DScatter_SetStrengthGain(double gain) {
    if (!(gain > 0.0) || !isfinite(gain)) {
        gRuntimeVolume3DScatterStrengthGain = 1.0;
        return;
    }
    gRuntimeVolume3DScatterStrengthGain = gain;
}

void RuntimeVolume3DScatter_SetStepScale(double step_scale) {
    if (!(step_scale > 0.0) || !isfinite(step_scale)) {
        gRuntimeVolume3DScatterStepScale = 1.0;
        return;
    }
    gRuntimeVolume3DScatterStepScale = step_scale;
}

void RuntimeVolume3DScatter_SetTint(double r, double g, double b) {
    if (!isfinite(r) || r < 0.0) r = 1.0;
    if (!isfinite(g) || g < 0.0) g = 1.0;
    if (!isfinite(b) || b < 0.0) b = 1.0;
    gRuntimeVolume3DScatterTintR = r;
    gRuntimeVolume3DScatterTintG = g;
    gRuntimeVolume3DScatterTintB = b;
}

static double runtime_volume_3d_scatter_phase_henyey_greenstein(double cos_theta,
                                                                double anisotropy) {
    const double g = runtime_volume_3d_scatter_clamp(anisotropy, -0.95, 0.95);
    const double g2 = g * g;
    const double clamped_cos = runtime_volume_3d_scatter_clamp(cos_theta, -1.0, 1.0);
    const double denom = 1.0 + g2 - (2.0 * g * clamped_cos);

    if (!(denom > 1e-9)) {
        return kRuntimeVolume3DScatterPhaseIsotropic;
    }

    return (1.0 - g2) / (4.0 * M_PI * pow(denom, 1.5));
}

static double runtime_volume_3d_scatter_light_attenuation(const RuntimeLight3D* light,
                                                          double light_distance [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    double falloff [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        runtime_volume_3d_scatter_unit_length();
    double normalized = 0.0;
    double zero_length = runtime_volume_3d_scatter_zero_length();

    if (!light) return 0.0;

    falloff = light->falloffDistance;
    if (!(falloff > zero_length)) {
        falloff = runtime_volume_3d_scatter_unit_length();
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

RuntimeVolume3DScatterResult RuntimeVolume3D_AccumulateSingleScatterAlongRayRGB(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]],
    const RuntimeNative3DSamplingContext* sampling) {
    RuntimeVolume3DScatterResult result = {0};
    double t_enter [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_min;
    double t_exit [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_max;
    double step [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        runtime_volume_3d_scatter_zero_length();
    double camera_transmittance = 1.0;
    double zero_length = runtime_volume_3d_scatter_zero_length();
    double epsilon = runtime_volume_3d_scatter_length_epsilon();

    if (!scene || !ray || !scene->hasLight) {
        return result;
    }
    if (!(scene->light.intensity > 0.0) ||
        !RuntimeVolume3D_HasActiveExtinction(&scene->volume)) {
        return result;
    }
    if (!RuntimeVolume3D_ClipRayToBounds(&scene->volume,
                                         ray,
                                         t_min,
                                         t_max,
                                         &t_enter,
                                         &t_exit)) {
        return result;
    }

    step = runtime_volume_3d_scatter_clamp(scene->volume.grid.voxelSize * 0.5 *
                                               gRuntimeVolume3DScatterStepScale,
                                           runtime_volume_3d_scatter_minimum_step(),
                                           t_exit - t_enter);
    if (!(step > zero_length)) {
        step = runtime_volume_3d_scatter_unit_length();
    }
    for (double t [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_enter;
         t < t_exit;
         t += step) {
        const double next_t [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            fmin(t + step, t_exit);
        const double segment_length [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            next_t - t;
        const double sample_t [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            0.5 * (t + next_t);
        const Vec3 sample_position =
            vec3_add(ray->origin, vec3_scale(ray->direction, sample_t));
        const double raw_density =
            (double)RuntimeVolume3D_SampleDensityAtPosition(&scene->volume, sample_position);
        const double density = RuntimeVolume3DMaterial_RemapDensity(raw_density);
        const double extinction_density = RuntimeVolume3DMaterial_ExtinctionDensity(raw_density);

        if (density > 0.0) {
            const Vec3 light_position =
                runtime_volume_3d_scatter_resolve_light_position(&scene->light,
                                                                 &sample_position,
                                                                 sampling);
            Vec3 to_light = vec3_sub(light_position, sample_position);
            const double light_distance [[fisics::dim(length)]] [[fisics::unit(meter)]] =
                vec3_length(to_light);
            const double attenuation =
                runtime_volume_3d_scatter_light_attenuation(&scene->light, light_distance);
            RuntimeVisibility3DTransmittance light_transmittance =
                RuntimeVisibility3D_UnitTransmittance();
            double source_term = 0.0;

            if (light_distance > epsilon) {
                const Vec3 to_light_dir = vec3_scale(to_light, 1.0 / light_distance);
                const Vec3 view_to_camera_dir = vec3_scale(ray->direction, -1.0);
                const Vec3 incoming_light_dir = vec3_scale(to_light_dir, -1.0);
                const double scatter_probability = 1.0 - exp(-density * segment_length);
                const double phase =
                    runtime_volume_3d_scatter_phase_henyey_greenstein(
                        vec3_dot(view_to_camera_dir, incoming_light_dir),
                        kRuntimeVolume3DScatterAnisotropy);

                source_term = scatter_probability * scene->light.intensity * attenuation *
                              kRuntimeVolume3DScatterStrength * gRuntimeVolume3DScatterStrengthGain *
                              phase;
                light_transmittance = RuntimeVisibility3D_TransmittanceToLightRGB(
                    scene, sample_position, to_light_dir, light_position);
            }

            if (source_term > 0.0 && light_transmittance.luma > 1e-9) {
                result.radianceR += camera_transmittance * light_transmittance.r * source_term *
                                    gRuntimeVolume3DScatterTintR;
                result.radianceG += camera_transmittance * light_transmittance.g * source_term *
                                    gRuntimeVolume3DScatterTintG;
                result.radianceB += camera_transmittance * light_transmittance.b * source_term *
                                    gRuntimeVolume3DScatterTintB;
                result.sampleCount += 1;
            }

            camera_transmittance *= exp(-extinction_density * segment_length);
            if (!(camera_transmittance > 1e-9)) {
                break;
            }
        }
    }

    result.radiance = runtime_volume_3d_scatter_peak(result.radianceR,
                                                     result.radianceG,
                                                     result.radianceB);
    result.active = result.radiance > 0.0;
    return result;
}

RuntimeVolume3DScatterResult RuntimeVolume3D_AccumulateDensityDebugAlongRayRGB(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    RuntimeVolume3DScatterResult result = {0};
    double t_enter [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_min;
    double t_exit [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_max;
    double step [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        runtime_volume_3d_scatter_zero_length();
    double density_integral = 0.0;
    double zero_length = runtime_volume_3d_scatter_zero_length();

    if (!scene || !ray || !RuntimeVolume3D_HasActiveExtinction(&scene->volume)) {
        return result;
    }
    if (!RuntimeVolume3D_ClipRayToBounds(&scene->volume,
                                         ray,
                                         t_min,
                                         t_max,
                                         &t_enter,
                                         &t_exit)) {
        return result;
    }

    step = runtime_volume_3d_scatter_clamp(scene->volume.grid.voxelSize * 0.5 *
                                               gRuntimeVolume3DScatterStepScale,
                                           runtime_volume_3d_scatter_minimum_step(),
                                           t_exit - t_enter);
    if (!(step > zero_length)) {
        step = runtime_volume_3d_scatter_unit_length();
    }

    for (double t [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_enter;
         t < t_exit;
         t += step) {
        const double next_t [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            fmin(t + step, t_exit);
        const double segment_length [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            next_t - t;
        const double sample_t [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            0.5 * (t + next_t);
        const Vec3 sample_position =
            vec3_add(ray->origin, vec3_scale(ray->direction, sample_t));
        const double raw_density =
            (double)RuntimeVolume3D_SampleDensityAtPosition(&scene->volume, sample_position);
        const double density = RuntimeVolume3DMaterial_RemapDensity(raw_density);

        if (density > 0.0) {
            density_integral += density * segment_length;
            result.sampleCount += 1;
        }
    }

    result.radiance = 1.0 - exp(-density_integral * gRuntimeVolume3DScatterStrengthGain);
    result.radianceR = result.radiance * gRuntimeVolume3DScatterTintR;
    result.radianceG = result.radiance * gRuntimeVolume3DScatterTintG;
    result.radianceB = result.radiance * gRuntimeVolume3DScatterTintB;
    result.radiance = runtime_volume_3d_scatter_peak(result.radianceR,
                                                     result.radianceG,
                                                     result.radianceB);
    result.active = result.radiance > 0.0;
    return result;
}
