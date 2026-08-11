#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "render/runtime_hair_scattering_3d.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures;

static void require_true(const char* name, bool condition) {
    if (!condition) {
        fprintf(stderr, "FAIL %-48s condition=false\n", name);
        failures += 1;
    }
}

static void require_close(const char* name, double a, double b, double tolerance) {
    if (!isfinite(a) || !isfinite(b) || fabs(a - b) > tolerance) {
        fprintf(stderr, "FAIL %-48s %.12f != %.12f\n", name, a, b);
        failures += 1;
    }
}

int main(void) {
    RuntimeHairOptics3D brown = RuntimeHairOptics3D_Default();
    RuntimeHairOptics3D blond = RuntimeHairOptics3D_Default();
    RuntimeHairOptics3D rough = RuntimeHairOptics3D_Default();
    RuntimeHairScattering3DResult first = {0};
    RuntimeHairScattering3DResult repeat = {0};
    RuntimeHairScattering3DResult blond_result = {0};
    RuntimeHairScattering3DResult rough_result = {0};
    double sweep_max = 0.0;
    bool sweep_valid = true;
    const Vec3 tangent = vec3_normalize(vec3(0.1, 0.0, 1.0));
    const Vec3 normal = vec3(0.0, 1.0, 0.0);
    const Vec3 light = vec3_normalize(vec3(0.4, 0.8, 0.45));
    const Vec3 view = vec3_normalize(vec3(-0.25, 0.9, 0.3));

    brown.enabled = true;
    blond.enabled = true;
    blond.absorptionR = 0.08;
    blond.absorptionG = 0.16;
    blond.absorptionB = 0.42;
    rough.enabled = true;
    rough.longitudinalRoughness = 0.85;
    rough.azimuthalRoughness = 0.82;

    require_true("curve_and_opt_in_required",
                 RuntimeHairScattering3D_ShouldApply(true, &brown));
    require_true("triangle_preserves_surface_dispatch",
                 !RuntimeHairScattering3D_ShouldApply(false, &brown));
    require_true("ordinary_curve_preserves_surface_dispatch",
                 !RuntimeHairScattering3D_ShouldApply(
                     true, &(RuntimeHairOptics3D){0}));
    require_true("brown_evaluate",
                 RuntimeHairScattering3D_EvaluateSingleFiber(
                     &brown, tangent, normal, light, view, &first));
    require_true("repeat_evaluate",
                 RuntimeHairScattering3D_EvaluateSingleFiber(
                     &brown, tangent, normal, light, view, &repeat));
    require_close("repeat_r", first.r, repeat.r, 1.0e-15);
    require_close("repeat_g", first.g, repeat.g, 1.0e-15);
    require_close("repeat_b", first.b, repeat.b, 1.0e-15);
    require_true("all_lobes_positive",
                 first.lobeR > 0.0 && first.lobeTT > 0.0 &&
                     first.lobeTRT > 0.0 && first.lobeHigherOrder > 0.0);
    require_true("finite_bounded_response",
                 first.r >= 0.0 && first.r <= 1.0 &&
                     first.g >= 0.0 && first.g <= 1.0 &&
                     first.b >= 0.0 && first.b <= 1.0);
    require_true("lobe_energy_partition_bounded",
                 first.lobeR + first.lobeTT + first.lobeTRT +
                         first.lobeHigherOrder <=
                     1.0 + 1.0e-9);
    require_true("blond_evaluate",
                 RuntimeHairScattering3D_EvaluateSingleFiber(
                     &blond, tangent, normal, light, view, &blond_result));
    require_true("absorption_changes_chroma",
                 fabs(blond_result.r - first.r) +
                         fabs(blond_result.g - first.g) +
                         fabs(blond_result.b - first.b) >
                     1.0e-5);
    require_true("rough_evaluate",
                 RuntimeHairScattering3D_EvaluateSingleFiber(
                     &rough, tangent, normal, light, view, &rough_result));
    require_true("roughness_changes_lobe_response",
                 fabs(rough_result.r - first.r) +
                         fabs(rough_result.g - first.g) +
                         fabs(rough_result.b - first.b) >
                     1.0e-5);
    for (int theta_step = 0; theta_step <= 24; ++theta_step) {
        const double theta = -1.2 + 2.4 * (double)theta_step / 24.0;
        for (int phi_step = 0; phi_step < 48; ++phi_step) {
            const double phi =
                -M_PI + 2.0 * M_PI * (double)phi_step / 48.0;
            const Vec3 sweep_light = vec3_normalize(vec3(
                cos(phi) * cos(theta),
                sin(phi) * cos(theta),
                sin(theta)));
            RuntimeHairScattering3DResult sweep = {0};
            if (!RuntimeHairScattering3D_EvaluateSingleFiber(
                    &brown,
                    vec3(0.0, 0.0, 1.0),
                    vec3(0.0, 1.0, 0.0),
                    sweep_light,
                    vec3_normalize(vec3(0.8, 0.5, 0.2)),
                    &sweep) ||
                sweep.r < 0.0 || sweep.r > 1.0 ||
                sweep.g < 0.0 || sweep.g > 1.0 ||
                sweep.b < 0.0 || sweep.b > 1.0) {
                sweep_valid = false;
            }
            sweep_max = fmax(sweep_max, fmax(sweep.r, fmax(sweep.g, sweep.b)));
        }
    }
    require_true("angular_sweep_finite_and_bounded", sweep_valid);
    require_true("angular_sweep_has_nonzero_highlight", sweep_max > 0.05);

    if (failures) {
        fprintf(stderr, "PSG-23G hair scattering lane failed: %d\n", failures);
        return 1;
    }
    printf("{\"schema\":\"ray_tracing.psg23g_hair_scattering\","
           "\"passed\":true,\"lobes\":[\"R\",\"TT\",\"TRT\",\"higher_order\"],"
           "\"brown_rgb\":[%.9f,%.9f,%.9f],"
           "\"blond_rgb\":[%.9f,%.9f,%.9f],"
           "\"angular_sweep_max\":%.9f}\n",
           first.r,
           first.g,
           first.b,
           blond_result.r,
           blond_result.g,
           blond_result.b,
           sweep_max);
    return 0;
}
