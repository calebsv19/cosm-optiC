#include "render/runtime_light_set_3d.h"
#include "render/runtime_light_radiometry_3d.h"

#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "test_support.h"

static int test_runtime_light_set_3d_empty_contract(void) {
    RuntimeLightSet3D set;

    RuntimeLightSet3D_Init(&set);
    assert_true("runtime_light_set_3d_empty_count", set.lightCount == 0);
    assert_true("runtime_light_set_3d_empty_enabled_count",
                RuntimeLightSet3D_EnabledCount(&set) == 0);
    assert_true("runtime_light_set_3d_empty_get_enabled_null",
                RuntimeLightSet3D_GetEnabled(&set, 0) == NULL);
    RuntimeLightSet3D_Free(&set);
    return 0;
}

static int test_runtime_light_set_3d_append_disabled_and_enabled_contract(void) {
    RuntimeLightSet3D set;
    RuntimeLightSource3D light;
    const RuntimeLightSource3D* enabled = NULL;
    int appended_index = -1;
    bool ok = false;

    RuntimeLightSet3D_Init(&set);
    RuntimeLightSource3D_Init(&light);
    snprintf(light.id, sizeof(light.id), "%s", "disabled_panel");
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_RECT;
    light.enabled = false;
    light.width = 2.0;
    light.height = 0.5;
    ok = RuntimeLightSet3D_Append(&set, &light, &appended_index);
    assert_true("runtime_light_set_3d_append_disabled_ok", ok);
    assert_true("runtime_light_set_3d_append_disabled_index", appended_index == 0);
    assert_true("runtime_light_set_3d_append_disabled_count", set.lightCount == 1);
    assert_true("runtime_light_set_3d_append_disabled_enabled_count",
                RuntimeLightSet3D_EnabledCount(&set) == 0);

    RuntimeLightSource3D_Init(&light);
    snprintf(light.id, sizeof(light.id), "%s", "enabled_sphere");
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE;
    light.radius = 0.25;
    light.color = vec3(0.25, 0.5, 1.0);
    light.intensity = 4.0;
    ok = RuntimeLightSet3D_Append(&set, &light, &appended_index);
    assert_true("runtime_light_set_3d_append_enabled_ok", ok);
    assert_true("runtime_light_set_3d_append_enabled_index", appended_index == 1);
    assert_true("runtime_light_set_3d_append_enabled_count", set.lightCount == 2);
    assert_true("runtime_light_set_3d_append_enabled_enabled_count",
                RuntimeLightSet3D_EnabledCount(&set) == 1);

    enabled = RuntimeLightSet3D_GetEnabled(&set, 0);
    assert_true("runtime_light_set_3d_get_enabled_present", enabled != NULL);
    if (enabled) {
        assert_true("runtime_light_set_3d_get_enabled_skips_disabled",
                    enabled->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE);
        assert_close("runtime_light_set_3d_get_enabled_color_b", enabled->color.z, 1.0, 1e-9);
    }

    RuntimeLightSet3D_Free(&set);
    return 0;
}

static int test_runtime_light_set_3d_compatibility_seed_contract(void) {
    RuntimeLightSet3D set;
    RuntimeLight3D compat = {0};
    const RuntimeLightSource3D* seeded = NULL;
    bool ok = false;

    RuntimeLightSet3D_Init(&set);
    compat.position = vec3(1.0, 2.0, 3.0);
    compat.radius = 0.75;
    compat.intensity = 12.0;
    compat.falloffDistance = 8.0;
    compat.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;

    ok = RuntimeLightSet3D_BuildFromCompatibilityLight(&set, &compat, true);
    assert_true("runtime_light_set_3d_compat_seed_ok", ok);
    assert_true("runtime_light_set_3d_compat_seed_count", set.lightCount == 1);
    assert_true("runtime_light_set_3d_compat_seed_enabled",
                RuntimeLightSet3D_EnabledCount(&set) == 1);

    seeded = RuntimeLightSet3D_GetEnabled(&set, 0);
    assert_true("runtime_light_set_3d_compat_seed_present", seeded != NULL);
    if (seeded) {
        assert_true("runtime_light_set_3d_compat_seed_origin",
                    seeded->origin == RUNTIME_LIGHT_SOURCE_3D_ORIGIN_COMPAT_SCENE_LIGHT);
        assert_true("runtime_light_set_3d_compat_seed_kind",
                    seeded->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE);
        assert_close("runtime_light_set_3d_compat_seed_position_x",
                     seeded->position.x,
                     1.0,
                     1e-9);
        assert_close("runtime_light_set_3d_compat_seed_radius", seeded->radius, 0.75, 1e-9);
        assert_close("runtime_light_set_3d_compat_seed_intensity",
                     seeded->intensity,
                     12.0,
                     1e-9);
        assert_close("runtime_light_set_3d_compat_seed_falloff",
                     seeded->falloffDistance,
                     8.0,
                     1e-9);
        assert_true("runtime_light_set_3d_compat_seed_falloff_mode",
                    seeded->falloffMode == FORWARD_FALLOFF_MODE_LINEAR);
        assert_close("runtime_light_set_3d_compat_seed_default_white_r",
                     seeded->color.x,
                     1.0,
                     1e-9);
    }

    ok = RuntimeLightSet3D_BuildFromCompatibilityLight(&set, &compat, false);
    assert_true("runtime_light_set_3d_compat_seed_clear_ok", ok);
    assert_true("runtime_light_set_3d_compat_seed_clear_count", set.lightCount == 0);

    RuntimeLightSet3D_Free(&set);
    return 0;
}

static int test_runtime_light_set_3d_copy_rgb_and_shape_contract(void) {
    RuntimeLightSet3D src;
    RuntimeLightSet3D dst;
    RuntimeLightSource3D light;
    const RuntimeLightSource3D* copied = NULL;
    bool ok = false;

    RuntimeLightSet3D_Init(&src);
    RuntimeLightSet3D_Init(&dst);
    RuntimeLightSource3D_Init(&light);
    snprintf(light.id, sizeof(light.id), "%s", "rgb_disk");
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_DISK;
    light.origin = RUNTIME_LIGHT_SOURCE_3D_ORIGIN_AUTHORED_LIGHT;
    light.position = vec3(3.0, 4.0, 5.0);
    light.axisU = vec3(1.0, 0.0, 0.0);
    light.axisV = vec3(0.0, 0.0, 1.0);
    light.normal = vec3(0.0, -1.0, 0.0);
    light.radius = 1.25;
    light.width = 2.5;
    light.height = 0.0;
    light.color = vec3(0.2, 0.4, 0.8);
    light.intensity = 6.0;

    ok = RuntimeLightSet3D_Append(&src, &light, NULL);
    assert_true("runtime_light_set_3d_copy_src_append_ok", ok);
    ok = RuntimeLightSet3D_CopyFrom(&dst, &src);
    assert_true("runtime_light_set_3d_copy_ok", ok);
    assert_true("runtime_light_set_3d_copy_count", dst.lightCount == 1);
    assert_true("runtime_light_set_3d_copy_enabled_count", dst.enabledCount == 1);

    copied = RuntimeLightSet3D_GetEnabled(&dst, 0);
    assert_true("runtime_light_set_3d_copy_present", copied != NULL);
    if (copied) {
        assert_true("runtime_light_set_3d_copy_kind",
                    copied->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_DISK);
        assert_close("runtime_light_set_3d_copy_rgb_r", copied->color.x, 0.2, 1e-9);
        assert_close("runtime_light_set_3d_copy_rgb_g", copied->color.y, 0.4, 1e-9);
        assert_close("runtime_light_set_3d_copy_rgb_b", copied->color.z, 0.8, 1e-9);
        assert_close("runtime_light_set_3d_copy_radius", copied->radius, 1.25, 1e-9);
        assert_close("runtime_light_set_3d_copy_width", copied->width, 2.5, 1e-9);
        assert_close("runtime_light_set_3d_copy_axis_u_x", copied->axisU.x, 1.0, 1e-9);
        assert_close("runtime_light_set_3d_copy_normal_y", copied->normal.y, -1.0, 1e-9);
    }

    RuntimeLightSet3D_Free(&dst);
    RuntimeLightSet3D_Free(&src);
    return 0;
}

static int test_runtime_light_set_3d_rect_lambertian_radiometry_contract(void) {
    RuntimeLightSource3D light;
    RuntimeLightRadiometry3DEvaluation evaluation;
    RuntimeCameraProjector3D projector = {0};
    Vec3 direction;
    double pdf = 0.0;
    double footprint_area = 0.0;

    RuntimeLightSource3D_Init(&light);
    light.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_RECT;
    light.emissionProfile = RUNTIME_LIGHT_SOURCE_3D_EMISSION_ONE_SIDED;
    light.radiometryMode = RUNTIME_LIGHT_RADIOMETRY_LAMBERTIAN_RADIANCE;
    light.width = 2.0;
    light.height = 1.0;
    light.normal = vec3(0.0, 0.0, -1.0);
    light.color = vec3(1.0, 0.5, 0.25);
    light.radiance = 4.0;

    assert_true("runtime_light_radiometry_rect_evaluate",
                RuntimeLightRadiometry3D_Evaluate(&light, &evaluation));
    assert_close("runtime_light_radiometry_rect_area", evaluation.areaM2, 2.0, 1e-12);
    assert_close("runtime_light_radiometry_rect_angular_integral",
                 evaluation.angularIntegralSr,
                 M_PI,
                 1e-12);
    assert_close("runtime_light_radiometry_rect_total_power_r",
                 evaluation.totalEmittedPower.x,
                 8.0 * M_PI,
                 1e-12);
    assert_close("runtime_light_radiometry_rect_total_power_g",
                 evaluation.totalEmittedPower.y,
                 4.0 * M_PI,
                 1e-12);
    assert_true("runtime_light_radiometry_rect_sample_direction",
                RuntimeLightRadiometry3D_SampleDirection(
                    &light, 0.25, 0.75, &direction, &pdf));
    assert_true("runtime_light_radiometry_rect_sample_front",
                vec3_dot(direction, light.normal) > 0.0 && pdf > 0.0);
    assert_close("runtime_light_radiometry_rect_sample_pdf",
                 pdf,
                 vec3_dot(direction, light.normal) / M_PI,
                 1e-12);
    assert_close("runtime_light_radiometry_rect_axis_irradiance",
                 RuntimeLightRadiometry3D_RectIrradianceScale(
                     &light, vec3(0.0, 0.0, 1.0), 2.0),
                 2.0,
                 1e-12);
    light.emissionProfile = RUNTIME_LIGHT_SOURCE_3D_EMISSION_TWO_SIDED;
    assert_true("runtime_light_radiometry_rect_two_sided_evaluate",
                RuntimeLightRadiometry3D_Evaluate(&light, &evaluation));
    assert_close("runtime_light_radiometry_rect_two_sided_power",
                 evaluation.totalEmittedPower.x,
                 16.0 * M_PI,
                 1e-12);
    projector.viewportWidth = 100;
    projector.viewportHeight = 100;
    projector.origin = vec3(0.0, 0.0, 0.0);
    projector.forward = vec3(0.0, 0.0, 1.0);
    projector.right = vec3(1.0, 0.0, 0.0);
    projector.up = vec3(0.0, 1.0, 0.0);
    projector.tanHalfFovX = 0.5;
    projector.tanHalfFovY = 0.5;
    assert_true("runtime_light_radiometry_receiver_footprint",
                RuntimeLightRadiometry3D_PerspectivePixelFootprintArea(
                    &projector,
                    49.5,
                    49.5,
                    vec3(0.0, 0.0, 10.0),
                    vec3(0.0, 0.0, -1.0),
                    &footprint_area));
    assert_close("runtime_light_radiometry_receiver_footprint_area",
                 footprint_area,
                 0.01,
                 1.0e-12);
    return 0;
}

int run_test_runtime_light_set_3d_tests(void) {
    test_runtime_light_set_3d_empty_contract();
    test_runtime_light_set_3d_append_disabled_and_enabled_contract();
    test_runtime_light_set_3d_compatibility_seed_contract();
    test_runtime_light_set_3d_copy_rgb_and_shape_contract();
    test_runtime_light_set_3d_rect_lambertian_radiometry_contract();
    return 0;
}
