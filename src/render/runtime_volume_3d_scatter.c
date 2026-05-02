#include "render/runtime_volume_3d_scatter.h"

#include <math.h>

#include "render/runtime_visibility_3d.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_sampling.h"

static const double kRuntimeVolume3DScatterMinimumStep = 1e-4;
static const double kRuntimeVolume3DScatterPhaseIsotropic = 0.07957747154594767; /* 1 / (4*pi) */
static const double kRuntimeVolume3DScatterStrength = 0.12;
static const double kRuntimeVolume3DScatterAnisotropy = 0.55;

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
                                                          double light_distance) {
    double falloff = 1.0;
    double normalized = 0.0;

    if (!light) return 0.0;

    falloff = light->falloffDistance;
    if (!(falloff > 0.0)) {
        falloff = 1.0;
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
    double t_min,
    double t_max) {
    RuntimeVolume3DScatterResult result = {0};
    double t_enter = 0.0;
    double t_exit = 0.0;
    double step = 0.0;
    double camera_transmittance = 1.0;

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

    step = runtime_volume_3d_scatter_clamp(scene->volume.grid.voxelSize * 0.5,
                                           kRuntimeVolume3DScatterMinimumStep,
                                           t_exit - t_enter);
    for (double t = t_enter; t < t_exit; t += step) {
        const double next_t = fmin(t + step, t_exit);
        const double segment_length = next_t - t;
        const double sample_t = 0.5 * (t + next_t);
        const Vec3 sample_position =
            vec3_add(ray->origin, vec3_scale(ray->direction, sample_t));
        const double density =
            fmax((double)RuntimeVolume3D_SampleDensityAtPosition(&scene->volume, sample_position),
                 0.0);

        if (density > 0.0) {
            Vec3 to_light = vec3_sub(scene->light.position, sample_position);
            const double light_distance = vec3_length(to_light);
            const double attenuation =
                runtime_volume_3d_scatter_light_attenuation(&scene->light, light_distance);
            RuntimeVisibility3DTransmittance light_transmittance =
                RuntimeVisibility3D_UnitTransmittance();
            double source_term = 0.0;

            if (light_distance > 1e-9) {
                const Vec3 to_light_dir = vec3_scale(to_light, 1.0 / light_distance);
                const Vec3 view_to_camera_dir = vec3_scale(ray->direction, -1.0);
                const Vec3 incoming_light_dir = vec3_scale(to_light_dir, -1.0);
                const double scatter_probability = 1.0 - exp(-density * segment_length);
                const double phase =
                    runtime_volume_3d_scatter_phase_henyey_greenstein(
                        vec3_dot(view_to_camera_dir, incoming_light_dir),
                        kRuntimeVolume3DScatterAnisotropy);

                source_term = scatter_probability * scene->light.intensity * attenuation *
                              kRuntimeVolume3DScatterStrength * phase;
                light_transmittance = RuntimeVisibility3D_TransmittanceToLightRGB(
                    scene, sample_position, to_light_dir, scene->light.position);
            }

            if (source_term > 0.0 && light_transmittance.luma > 1e-9) {
                result.radianceR += camera_transmittance * light_transmittance.r * source_term;
                result.radianceG += camera_transmittance * light_transmittance.g * source_term;
                result.radianceB += camera_transmittance * light_transmittance.b * source_term;
                result.sampleCount += 1;
            }

            camera_transmittance *= exp(-density * segment_length);
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
