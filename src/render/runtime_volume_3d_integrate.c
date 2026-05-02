#include "render/runtime_volume_3d_integrate.h"

#include <math.h>

#include "render/runtime_volume_3d_sampling.h"

static const double kRuntimeVolume3DMinimumStep = 1e-4;
static const double kRuntimeVolume3DExtinctionScale = 1.0;

static double runtime_volume_3d_integrate_clamp(double value,
                                                double min_value,
                                                double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

bool RuntimeVolume3D_HasActiveExtinction(const RuntimeVolumeAttachment3D* attachment) {
    return RuntimeVolume3D_HasSampleableDensity(attachment);
}

RuntimeVisibility3DTransmittance RuntimeVolume3D_TransmittanceAlongRayRGB(
    const RuntimeVolumeAttachment3D* attachment,
    const Ray3D* ray,
    double t_min,
    double t_max) {
    RuntimeVisibility3DTransmittance transmittance = RuntimeVisibility3D_UnitTransmittance();
    double t_enter = 0.0;
    double t_exit = 0.0;
    double step = 0.0;
    double optical_depth = 0.0;

    if (!RuntimeVolume3D_HasActiveExtinction(attachment) || !ray) {
        return transmittance;
    }
    if (!RuntimeVolume3D_ClipRayToBounds(attachment, ray, t_min, t_max, &t_enter, &t_exit)) {
        return transmittance;
    }

    step = runtime_volume_3d_integrate_clamp(attachment->grid.voxelSize,
                                             kRuntimeVolume3DMinimumStep,
                                             t_exit - t_enter);
    for (double t = t_enter; t < t_exit; t += step) {
        const double next_t = fmin(t + step, t_exit);
        const double sample_t = 0.5 * (t + next_t);
        const Vec3 sample_position =
            vec3_add(ray->origin, vec3_scale(ray->direction, sample_t));
        const double density =
            fmax((double)RuntimeVolume3D_SampleDensityAtPosition(attachment, sample_position),
                 0.0);

        optical_depth += density * (next_t - t) * kRuntimeVolume3DExtinctionScale;
    }

    if (!(optical_depth > 0.0)) {
        return transmittance;
    }

    transmittance.r = exp(-optical_depth);
    transmittance.g = transmittance.r;
    transmittance.b = transmittance.r;
    transmittance.luma = transmittance.r;
    return transmittance;
}
