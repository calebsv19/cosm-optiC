#include "render/runtime_volume_3d_integrate.h"

#include <math.h>

#include "render/runtime_volume_3d_sampling.h"

static const double kRuntimeVolume3DMinimumStep = 1e-4;
static const double kRuntimeVolume3DExtinctionScale = 1.0;
static const double kRuntimeVolume3DDefaultOpacityClamp = 1.0e30;
static double gRuntimeVolume3DDensityScale = 1.0;
static double gRuntimeVolume3DDensityGamma = 1.0;
static double gRuntimeVolume3DAbsorptionGain = 1.0;
static double gRuntimeVolume3DOpacityClamp = kRuntimeVolume3DDefaultOpacityClamp;

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

void RuntimeVolume3DMaterial_ResetTuning(void) {
    gRuntimeVolume3DDensityScale = 1.0;
    gRuntimeVolume3DDensityGamma = 1.0;
    gRuntimeVolume3DAbsorptionGain = 1.0;
    gRuntimeVolume3DOpacityClamp = kRuntimeVolume3DDefaultOpacityClamp;
}

void RuntimeVolume3DMaterial_SetDensityScale(double scale) {
    if (!(scale >= 0.0) || !isfinite(scale)) {
        gRuntimeVolume3DDensityScale = 1.0;
        return;
    }
    gRuntimeVolume3DDensityScale = scale;
}

void RuntimeVolume3DMaterial_SetDensityGamma(double gamma) {
    if (!(gamma > 0.0) || !isfinite(gamma)) {
        gRuntimeVolume3DDensityGamma = 1.0;
        return;
    }
    gRuntimeVolume3DDensityGamma = gamma;
}

void RuntimeVolume3DMaterial_SetAbsorptionGain(double gain) {
    if (!(gain >= 0.0) || !isfinite(gain)) {
        gRuntimeVolume3DAbsorptionGain = 1.0;
        return;
    }
    gRuntimeVolume3DAbsorptionGain = gain;
}

void RuntimeVolume3DMaterial_SetOpacityClamp(double clamp_value) {
    if (!(clamp_value >= 0.0) || !isfinite(clamp_value)) {
        gRuntimeVolume3DOpacityClamp = kRuntimeVolume3DDefaultOpacityClamp;
        return;
    }
    gRuntimeVolume3DOpacityClamp = clamp_value;
}

double RuntimeVolume3DMaterial_RemapDensity(double density) {
    double remapped = 0.0;

    if (!(density > 0.0) || !isfinite(density)) {
        return 0.0;
    }
    remapped = density * gRuntimeVolume3DDensityScale;
    if (!(remapped > 0.0)) {
        return 0.0;
    }
    if (gRuntimeVolume3DDensityGamma != 1.0) {
        remapped = pow(remapped, gRuntimeVolume3DDensityGamma);
    }
    return runtime_volume_3d_integrate_clamp(remapped,
                                             0.0,
                                             gRuntimeVolume3DOpacityClamp);
}

double RuntimeVolume3DMaterial_ExtinctionDensity(double density) {
    return RuntimeVolume3DMaterial_RemapDensity(density) *
           gRuntimeVolume3DAbsorptionGain;
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
        const double density = RuntimeVolume3DMaterial_ExtinctionDensity(
            (double)RuntimeVolume3D_SampleDensityAtPosition(attachment, sample_position));

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
