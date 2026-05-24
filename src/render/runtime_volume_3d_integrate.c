#include "render/runtime_volume_3d_integrate.h"

#include <math.h>

#include "render/runtime_volume_3d_sampling.h"

static const double kRuntimeVolume3DMinimumStep = 1e-4;
static const double kRuntimeVolume3DExtinctionScale = 1.0;

static double runtime_volume_3d_integrate_zero_length(void) {
    double zero [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    return zero;
}

static double runtime_volume_3d_integrate_unit_length(void) {
    double unit_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = 1.0;
    return unit_length;
}

static double runtime_volume_3d_integrate_minimum_step(void) {
    double minimum_step [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        kRuntimeVolume3DMinimumStep;
    return minimum_step;
}

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
    double t_min [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double t_max [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    RuntimeVisibility3DTransmittance transmittance = RuntimeVisibility3D_UnitTransmittance();
    double t_enter [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_min;
    double t_exit [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_max;
    double step [[fisics::dim(length)]] [[fisics::unit(meter)]] = runtime_volume_3d_integrate_zero_length();
    double optical_depth = 0.0;
    double zero_length = runtime_volume_3d_integrate_zero_length();

    if (!RuntimeVolume3D_HasActiveExtinction(attachment) || !ray) {
        return transmittance;
    }
    if (!RuntimeVolume3D_ClipRayToBounds(attachment, ray, t_min, t_max, &t_enter, &t_exit)) {
        return transmittance;
    }

    step = runtime_volume_3d_integrate_clamp(attachment->grid.voxelSize,
                                             runtime_volume_3d_integrate_minimum_step(),
                                             t_exit - t_enter);
    if (!(step > zero_length)) {
        step = runtime_volume_3d_integrate_unit_length();
    }
    for (double t [[fisics::dim(length)]] [[fisics::unit(meter)]] = t_enter;
         t < t_exit;
         t += step) {
        const double next_t [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            fmin(t + step, t_exit);
        const double sample_t [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            0.5 * (t + next_t);
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
