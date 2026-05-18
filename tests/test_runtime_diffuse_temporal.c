#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "render/integrators/integrator_common.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_blue_noise.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_sampling.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "test_runtime_diffuse_temporal.h"
#include "test_support.h"

static uint8_t native3d_temporal_test_pixel_r(const uint8_t* pixels, int width, int x, int y) {
    size_t base =
        ((size_t)y * (size_t)width + (size_t)x) * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    return pixels[base + 2u];
}

static int test_runtime_diffuse_bounce_3d_shadowed_hit_lift_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_diffuse_bounce_shadowed_lift\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"bounce_card\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":1.0,\"height\":2.0,"
            "\"frame\":{\"origin\":{\"x\":0.75,\"y\":-4.1,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":1.0,\"y\":0.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.75,\"y\":-4.1,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":2.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDirectLight3DResult direct_result = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    animSettings.bounceDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightRadius = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_diffuse_bounce_shadowed_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[1].color = 0xFF0000;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_diffuse_bounce_shadowed_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_diffuse_bounce_shadowed_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, &direct_result);
    assert_true("runtime_diffuse_bounce_shadowed_direct_ok", ok);
    assert_true("runtime_diffuse_bounce_shadowed_direct_not_visible", !direct_result.visible);
    assert_close("runtime_diffuse_bounce_shadowed_direct_zero",
                 direct_result.radiance,
                 0.0,
                 1e-9);

    ok = RuntimeDiffuseBounce3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &diffuse_result);
    assert_true("runtime_diffuse_bounce_shadowed_diffuse_ok", ok);
    assert_true("runtime_diffuse_bounce_shadowed_diffuse_hit", diffuse_result.hit);
    assert_close("runtime_diffuse_bounce_shadowed_direct_preserved",
                 diffuse_result.directRadiance,
                 0.0,
                 1e-9);
    assert_true("runtime_diffuse_bounce_shadowed_secondary_rays_match",
                diffuse_result.secondaryRayCount == RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT);
    assert_true("runtime_diffuse_bounce_shadowed_secondary_hits_positive",
                diffuse_result.secondaryHitCount > 0);
    assert_true("runtime_diffuse_bounce_shadowed_secondary_lit_hits_positive",
                diffuse_result.secondaryContributingHitCount > 0);
    assert_true("runtime_diffuse_bounce_shadowed_bounce_positive",
                diffuse_result.bounceRadiance > 0.0);
    assert_true("runtime_diffuse_bounce_shadowed_bounce_red_positive",
                diffuse_result.bounceRadianceR > 0.0);
    assert_true("runtime_diffuse_bounce_shadowed_bounce_red_dominates_green",
                diffuse_result.bounceRadianceR > diffuse_result.bounceRadianceG + 1e-6);
    assert_true("runtime_diffuse_bounce_shadowed_bounce_red_dominates_blue",
                diffuse_result.bounceRadianceR > diffuse_result.bounceRadianceB + 1e-6);
    assert_true("runtime_diffuse_bounce_shadowed_total_red_dominates_green",
                diffuse_result.radianceR > diffuse_result.radianceG + 1e-6);
    assert_true("runtime_diffuse_bounce_shadowed_total_red_dominates_blue",
                diffuse_result.radianceR > diffuse_result.radianceB + 1e-6);
    assert_true("runtime_diffuse_bounce_shadowed_total_positive",
                diffuse_result.radiance > 0.0);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_diffuse_bounce_3d_sampling_sequence_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_diffuse_bounce_sampling_sequence\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"bounce_card\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":1.0,\"height\":2.0,"
            "\"frame\":{\"origin\":{\"x\":0.9,\"y\":-4.1,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":-1.0,\"y\":0.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.9,\"y\":-4.1,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":2.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DSamplingContext sampling_a = {.sampleSequence = 1U};
    RuntimeNative3DSamplingContext sampling_b = {.sampleSequence = 2U};
    uint8_t pixels_a[101 * 101 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t pixels_a_repeat[101 * 101 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t pixels_b[101 * 101 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    RuntimeNative3DRenderStats stats_a = {0};
    RuntimeNative3DRenderStats stats_a_repeat = {0};
    RuntimeNative3DRenderStats stats_b = {0};
    bool ok = false;
    animSettings.secondaryDiffuseSamples3D = 4;
    animSettings.bounceDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightRadius = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_diffuse_bounce_sequence_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[1].color = 0xFF0000;

    ok = RuntimeNative3DRenderToPixelBufferWithSampling(pixels_a,
                                                        RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                                        101,
                                                        101,
                                                        0.0,
                                                        2.0,
                                                        -2.0,
                                                        &sampling_a,
                                                        &stats_a);
    assert_true("runtime_diffuse_bounce_sequence_render_a_ok", ok);
    ok = RuntimeNative3DRenderToPixelBufferWithSampling(pixels_a_repeat,
                                                        RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                                        101,
                                                        101,
                                                        0.0,
                                                        2.0,
                                                        -2.0,
                                                        &sampling_a,
                                                        &stats_a_repeat);
    assert_true("runtime_diffuse_bounce_sequence_render_a_repeat_ok", ok);
    ok = RuntimeNative3DRenderToPixelBufferWithSampling(pixels_b,
                                                        RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                                        101,
                                                        101,
                                                        0.0,
                                                        2.0,
                                                        -2.0,
                                                        &sampling_b,
                                                        &stats_b);
    assert_true("runtime_diffuse_bounce_sequence_render_b_ok", ok);
    assert_true("runtime_diffuse_bounce_sequence_same_seed_repeat_matches",
                memcmp(pixels_a, pixels_a_repeat, sizeof(pixels_a)) == 0);
    assert_true("runtime_diffuse_bounce_sequence_differs_across_sequences",
                memcmp(pixels_a, pixels_b, sizeof(pixels_a)) != 0);
    assert_true("runtime_diffuse_bounce_sequence_stats_repeat_match",
                stats_a.secondaryHitCount == stats_a_repeat.secondaryHitCount &&
                stats_a.secondaryContributingHitCount == stats_a_repeat.secondaryContributingHitCount &&
                fabs(stats_a.totalBounceRadiance - stats_a_repeat.totalBounceRadiance) <= 1e-9);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_diffuse_bounce_3d_recursive_depth_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_diffuse_bounce_recursive_depth\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"ceiling\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-3.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":-1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-3.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"left_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":2.0,\"height\":2.0,"
            "\"frame\":{\"origin\":{\"x\":-1.0,\"y\":-4.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":1.0,\"y\":0.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":-1.0,\"y\":-4.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"right_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":2.0,\"height\":2.0,"
            "\"frame\":{\"origin\":{\"x\":1.0,\"y\":-4.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":-1.0,\"y\":0.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":1.0,\"y\":-4.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.8,\"y\":-3.4,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimeDiffuseBounce3DResult depth_one = {0};
    RuntimeDiffuseBounce3DResult depth_three = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    animSettings.secondaryDiffuseSamples3D = 12;
    animSettings.bounceDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 8.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightRadius = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_diffuse_bounce_recursive_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_diffuse_bounce_recursive_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_diffuse_bounce_recursive_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeDiffuseBounce3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &depth_one);
    assert_true("runtime_diffuse_bounce_recursive_depth_one_ok", ok);
    assert_true("runtime_diffuse_bounce_recursive_depth_one_hit", depth_one.hit);

    animSettings.bounceDepth3D = 3;
    ok = RuntimeDiffuseBounce3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &depth_three);
    assert_true("runtime_diffuse_bounce_recursive_depth_three_ok", ok);
    assert_true("runtime_diffuse_bounce_recursive_depth_three_hit", depth_three.hit);
    assert_true("runtime_diffuse_bounce_recursive_depth_more_secondary_rays",
                depth_three.secondaryRayCount > depth_one.secondaryRayCount);
    assert_true("runtime_diffuse_bounce_recursive_depth_more_secondary_hits",
                depth_three.secondaryHitCount > depth_one.secondaryHitCount);
    assert_true("runtime_diffuse_bounce_recursive_depth_bounce_not_reduced",
                depth_three.bounceRadiance >= depth_one.bounceRadiance - 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_temporal_accumulation_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_3d_temporal_accum\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"bounce_card\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":1.0,\"height\":2.0,"
            "\"frame\":{\"origin\":{\"x\":0.9,\"y\":-4.1,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":-1.0,\"y\":0.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.9,\"y\":-4.1,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":2.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DSamplingContext sampling = {.sampleSequence = 11U};
    uint8_t pixels_single[101 * 101 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t pixels_temporal[101 * 101 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    RuntimeNative3DRenderStats stats_single = {0};
    RuntimeNative3DRenderStats stats_temporal = {0};
    bool ok = false;

    animSettings.secondaryDiffuseSamples3D = 4;
    animSettings.bounceDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightRadius = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_temporal_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
        pixels_single,
        RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
        101,
        101,
        0.0,
        2.0,
        -2.0,
        &sampling,
        1,
        &stats_single);
    assert_true("runtime_native_3d_temporal_single_ok", ok);
    ok = RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
        pixels_temporal,
        RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
        101,
        101,
        0.0,
        2.0,
        -2.0,
        &sampling,
        4,
        &stats_temporal);
    assert_true("runtime_native_3d_temporal_multi_ok", ok);
    assert_true("runtime_native_3d_temporal_differs_from_single",
                memcmp(pixels_single, pixels_temporal, sizeof(pixels_single)) != 0);
    assert_true("runtime_native_3d_temporal_secondary_ray_growth",
                stats_temporal.secondaryRayCount ==
                    (stats_single.secondaryRayCount * 4));
    assert_true("runtime_native_3d_temporal_visible_preserved",
                stats_temporal.visiblePixelCount >= stats_single.visiblePixelCount &&
                    stats_temporal.visiblePixelCount <=
                        (stats_single.visiblePixelCount * 4));
    assert_true("runtime_native_3d_temporal_bounded_grayscale",
                native3d_temporal_test_pixel_r(pixels_temporal, 101, 50, 50) <= 255);

    ok = RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
        pixels_temporal,
        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        101,
        101,
        0.0,
        2.0,
        -2.0,
        &sampling,
        4,
        &stats_temporal);
    assert_true("runtime_native_3d_temporal_direct_light_ok", ok);
    ok = RuntimeNative3DRenderToPixelBufferWithSampling(
        pixels_single,
        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        101,
        101,
        0.0,
        2.0,
        -2.0,
        &sampling,
        &stats_single);
    assert_true("runtime_native_3d_temporal_direct_light_single_ok", ok);
    assert_true("runtime_native_3d_temporal_direct_light_bypasses",
                memcmp(pixels_single, pixels_temporal, sizeof(pixels_single)) == 0);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_temporal_accumulation_ema_and_clamp_contract(void) {
    RuntimeNative3DTemporalAccumulation accumulation = {0};
    float sample_a[3] = {0.2f, 0.2f, 0.2f};
    float sample_b[3] = {50.0f, 50.0f, 50.0f};
    float resolved[3] = {0.0f, 0.0f, 0.0f};
    bool ok = false;

    RuntimeNative3DTemporalAccumulation_Init(&accumulation);
    ok = RuntimeNative3DTemporalAccumulation_Ensure(&accumulation, 1, 1);
    assert_true("runtime_native_3d_temporal_accum_ema_alloc_ok", ok);
    if (!ok) {
        RuntimeNative3DTemporalAccumulation_Free(&accumulation);
        return 0;
    }

    RuntimeNative3DTemporalAccumulation_Clear(&accumulation);
    ok = RuntimeNative3DTemporalAccumulation_AddRegion(&accumulation, sample_a, 1, 0, 0, 1, 1);
    assert_true("runtime_native_3d_temporal_accum_ema_add_a_ok", ok);
    RuntimeNative3DTemporalAccumulation_CommitSubpass(&accumulation);
    ok = ok && RuntimeNative3DTemporalAccumulation_AddRegion(&accumulation, sample_b, 1, 0, 0, 1, 1);
    assert_true("runtime_native_3d_temporal_accum_ema_add_b_ok", ok);
    RuntimeNative3DTemporalAccumulation_CommitSubpass(&accumulation);
    ok = ok && RuntimeNative3DTemporalAccumulation_ResolveRegionToRadianceBuffer(&accumulation,
                                                                                  resolved,
                                                                                  1,
                                                                                  0,
                                                                                  0,
                                                                                  1,
                                                                                  1);
    assert_true("runtime_native_3d_temporal_accum_ema_resolve_ok", ok);
    assert_true("runtime_native_3d_temporal_accum_ema_clamps_firefly",
                resolved[0] < 5.0f);
    assert_true("runtime_native_3d_temporal_accum_ema_lifts_history",
                resolved[0] > sample_a[0]);

    RuntimeNative3DTemporalAccumulation_Free(&accumulation);
    return 0;
}

static int test_runtime_native_3d_sampling_stratified_subpass_contract(void) {
    RuntimeNative3DSamplingContext subpass_a = {
        .sampleSequence = 31u,
        .temporalSubpassIndex = 0u,
        .temporalSubpassCount = 4u
    };
    RuntimeNative3DSamplingContext subpass_b = {
        .sampleSequence = 32u,
        .temporalSubpassIndex = 1u,
        .temporalSubpassCount = 4u
    };
    double u_a = 0.0;
    double v_a = 0.0;
    double u_a_repeat = 0.0;
    double v_a_repeat = 0.0;
    double u_b = 0.0;
    double v_b = 0.0;

    RuntimeNative3DSampling_Stratified2D(&subpass_a, 0x12345678u, 4, 2, 7u, &u_a, &v_a);
    RuntimeNative3DSampling_Stratified2D(&subpass_a, 0x12345678u, 4, 2, 7u, &u_a_repeat, &v_a_repeat);
    RuntimeNative3DSampling_Stratified2D(&subpass_b, 0x12345678u, 4, 2, 7u, &u_b, &v_b);

    assert_true("runtime_native_3d_sampling_stratified_repeat_u_match",
                fabs(u_a - u_a_repeat) <= 1e-12);
    assert_true("runtime_native_3d_sampling_stratified_repeat_v_match",
                fabs(v_a - v_a_repeat) <= 1e-12);
    assert_true("runtime_native_3d_sampling_stratified_subpasses_diverge",
                fabs(u_a - u_b) > 1e-6 || fabs(v_a - v_b) > 1e-6);
    assert_true("runtime_native_3d_sampling_stratified_bounds_a",
                u_a >= 0.0 && u_a <= 1.0 && v_a >= 0.0 && v_a <= 1.0);
    assert_true("runtime_native_3d_sampling_stratified_bounds_b",
                u_b >= 0.0 && u_b <= 1.0 && v_b >= 0.0 && v_b <= 1.0);
    return 0;
}

static int test_runtime_native_3d_blue_noise_jitter_contract(void) {
    double u_a = 0.0;
    double v_a = 0.0;
    double u_a_repeat = 0.0;
    double v_a_repeat = 0.0;
    double u_b = 0.0;
    double v_b = 0.0;
    double u_dim = 0.0;
    double v_dim = 0.0;

    RuntimeNative3DBlueNoise_Jitter2D(31u, 0x12345678u, 7u, &u_a, &v_a);
    RuntimeNative3DBlueNoise_Jitter2D(31u, 0x12345678u, 7u, &u_a_repeat, &v_a_repeat);
    RuntimeNative3DBlueNoise_Jitter2D(32u, 0x12345678u, 7u, &u_b, &v_b);
    RuntimeNative3DBlueNoise_Jitter2D(31u, 0x12345678u, 8u, &u_dim, &v_dim);

    assert_true("runtime_native_3d_blue_noise_repeat_u_match",
                fabs(u_a - u_a_repeat) <= 1e-12);
    assert_true("runtime_native_3d_blue_noise_repeat_v_match",
                fabs(v_a - v_a_repeat) <= 1e-12);
    assert_true("runtime_native_3d_blue_noise_sequence_diverges",
                fabs(u_a - u_b) > 1e-6 || fabs(v_a - v_b) > 1e-6);
    assert_true("runtime_native_3d_blue_noise_dimension_diverges",
                fabs(u_a - u_dim) > 1e-6 || fabs(v_a - v_dim) > 1e-6);
    assert_true("runtime_native_3d_blue_noise_bounds_a",
                u_a >= 0.0 && u_a <= 1.0 && v_a >= 0.0 && v_a <= 1.0);
    assert_true("runtime_native_3d_blue_noise_bounds_b",
                u_b >= 0.0 && u_b <= 1.0 && v_b >= 0.0 && v_b <= 1.0);
    return 0;
}

static int test_runtime_native_3d_temporal_tile_parity_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_3d_temporal_tile_parity\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"bounce_card\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":1.0,\"height\":2.0,"
            "\"frame\":{\"origin\":{\"x\":0.9,\"y\":-4.1,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
            "\"normal\":{\"x\":-1.0,\"y\":0.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.9,\"y\":-4.1,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":2.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DSamplingContext sampling = {.sampleSequence = 21U};
    RuntimeNative3DRenderStats full_stats = {0};
    RuntimeNative3DRenderStats tiled_stats = {0};
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DTemporalAccumulation tile_accumulation = {0};
    TileGrid grid = {0};
    uint8_t full_pixels[101 * 101 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t tiled_pixels[101 * 101 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    float* tile_radiance = NULL;
    bool ok = false;
    const int temporal_frames = 4;

    animSettings.secondaryDiffuseSamples3D = 4;
    animSettings.bounceDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    animSettings.temporalFrames3D = temporal_frames;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightRadius = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_temporal_tile_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
        full_pixels,
        RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
        101,
        101,
        0.0,
        2.0,
        -2.0,
        &sampling,
        temporal_frames,
        &full_stats);
    assert_true("runtime_native_3d_temporal_tile_full_ok", ok);

    RuntimeNative3DFillPixelBufferEnvironment(tiled_pixels, 101u * 101u);
    ok = RuntimeNative3DPrepareFrameWithSampling(&frame, 101, 101, 0.0, 2.0, -2.0, &sampling);
    assert_true("runtime_native_3d_temporal_tile_prepare_ok", ok);
    if (ok) {
        TileGridEnsure(&grid, 101, 101, 16);
        ok = RuntimeNative3DPrepareFrameTileOccupancy(&frame, grid.tileSize);
        assert_true("runtime_native_3d_temporal_tile_occupancy_ok", ok);
    }
    if (!ok) {
        TileGridFree(&grid);
        RuntimeNative3DPreparedFrame_Free(&frame);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    RuntimeNative3DTemporalAccumulation_Init(&tile_accumulation);
    tile_radiance = (float*)calloc((size_t)grid.tileSize * (size_t)grid.tileSize *
                                       RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                   sizeof(*tile_radiance));
    assert_true("runtime_native_3d_temporal_tile_buffer_ok", tile_radiance != NULL);
    if (!tile_radiance) {
        free(tile_radiance);
        RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
        TileGridFree(&grid);
        RuntimeNative3DPreparedFrame_Free(&frame);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    for (size_t ti = 0; ok && ti < grid.count; ++ti) {
        const IntegratorTile* tile = &grid.tiles[ti];
        ok = RuntimeNative3DTemporalAccumulation_Ensure(&tile_accumulation, tile->width, tile->height);
        assert_true("runtime_native_3d_temporal_tile_accum_ok", ok);
        RuntimeNative3DTemporalAccumulation_Clear(&tile_accumulation);
        for (int subpass = 0; subpass < temporal_frames; ++subpass) {
            RuntimeNative3DPreparedFrame subpass_frame = frame;
            RuntimeNative3DRenderStats subpass_stats = {0};

            if (!RuntimeNative3DPreparedRegionMayContainGeometry(&frame,
                                                                 tile->originX,
                                                                 tile->originY,
                                                                 tile->originX + tile->width,
                                                                 tile->originY + tile->height)) {
                continue;
            }

            memset(tile_radiance,
                   0,
                   (size_t)tile->width * (size_t)tile->height *
                       RUNTIME_NATIVE_3D_RADIANCE_CHANNELS * sizeof(*tile_radiance));
            subpass_frame.sampling.sampleSequence = sampling.sampleSequence + (uint32_t)subpass;
            subpass_frame.sampling.temporalSubpassIndex = (uint16_t)subpass;
            subpass_frame.sampling.temporalSubpassCount = (uint16_t)temporal_frames;
            ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(tile_radiance,
                                                                tile->width,
                                                                RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                                                &subpass_frame,
                                                                tile->originX,
                                                                tile->originY,
                                                                tile->originX + tile->width,
                                                                tile->originY + tile->height,
                                                                &subpass_stats);
            assert_true("runtime_native_3d_temporal_tile_region_ok", ok);
            ok = ok && RuntimeNative3DTemporalAccumulation_AddRegion(&tile_accumulation,
                                                                     tile_radiance,
                                                                     tile->width,
                                                                     0,
                                                                     0,
                                                                     tile->width,
                                                                     tile->height);
            assert_true("runtime_native_3d_temporal_tile_add_ok", ok);
            RuntimeNative3DRenderStats_Accumulate(&tiled_stats, &subpass_stats);
            if (!ok) break;
            RuntimeNative3DTemporalAccumulation_CommitSubpass(&tile_accumulation);
        }
        RuntimeNative3DTemporalAccumulation_ResolveToPixelBufferAtOffset(&tile_accumulation,
                                                                         tiled_pixels,
                                                                         101,
                                                                         tile->originX,
                                                                         tile->originY);
    }

    assert_true("runtime_native_3d_temporal_tile_pixels_match",
                memcmp(full_pixels, tiled_pixels, sizeof(full_pixels)) == 0);
    assert_true("runtime_native_3d_temporal_tile_secondary_rays_match",
                full_stats.secondaryRayCount == tiled_stats.secondaryRayCount);
    assert_true("runtime_native_3d_temporal_tile_secondary_hits_match",
                full_stats.secondaryHitCount == tiled_stats.secondaryHitCount);
    assert_true("runtime_native_3d_temporal_tile_secondary_lit_hits_match",
                full_stats.secondaryContributingHitCount ==
                    tiled_stats.secondaryContributingHitCount);

    free(tile_radiance);
    RuntimeNative3DTemporalAccumulation_Free(&tile_accumulation);
    TileGridFree(&grid);
    RuntimeNative3DPreparedFrame_Free(&frame);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_diffuse_bounce_3d_seed_branch_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_diffuse_bounce_seed\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"lit_wall\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":8.0,\"height\":8.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeScene3D scene;
    RuntimeCameraProjector3D projector = {0};
    RuntimePrimaryHit3DResult primary_hit = {0};
    RuntimeDirectLight3DResult direct_result = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_diffuse_bounce_seed_apply_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightRadius = 0.0;
    animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    animSettings.bounceDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_diffuse_bounce_seed_build_ok", ok);
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
    assert_true("runtime_diffuse_bounce_seed_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    ok = RuntimeDirectLight3D_TracePrimaryHit(&scene, &projector, 50.0, 50.0, &primary_hit);
    assert_true("runtime_diffuse_bounce_seed_primary_hit_ok", ok);
    assert_true("runtime_diffuse_bounce_seed_primary_hit_found", primary_hit.hit);

    ok = RuntimeDirectLight3D_ShadePixel(&scene, &projector, 50.0, 50.0, &direct_result);
    assert_true("runtime_diffuse_bounce_seed_direct_ok", ok);
    ok = RuntimeDiffuseBounce3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &diffuse_result);
    assert_true("runtime_diffuse_bounce_seed_diffuse_ok", ok);
    assert_true("runtime_diffuse_bounce_seed_diffuse_hit", diffuse_result.hit);
    assert_true("runtime_diffuse_bounce_seed_same_triangle",
                diffuse_result.hitInfo.triangleIndex == primary_hit.hitInfo.triangleIndex);
    assert_true("runtime_diffuse_bounce_seed_secondary_rays_match",
                diffuse_result.secondaryRayCount == RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT);
    assert_true("runtime_diffuse_bounce_seed_secondary_hits_zero",
                diffuse_result.secondaryHitCount == 0);
    assert_close("runtime_diffuse_bounce_seed_direct_match",
                 diffuse_result.directRadiance,
                 direct_result.radiance,
                 1e-9);
    assert_close("runtime_diffuse_bounce_seed_total_match",
                 diffuse_result.radiance,
                 direct_result.radiance,
                 1e-9);
    assert_close("runtime_diffuse_bounce_seed_bounce_zero",
                 diffuse_result.bounceRadiance,
                 0.0,
                 1e-9);

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}


int run_test_runtime_diffuse_temporal_tests(void) {
    int before = test_support_failures();

    test_runtime_diffuse_bounce_3d_shadowed_hit_lift_contract();
    test_runtime_diffuse_bounce_3d_sampling_sequence_contract();
    test_runtime_diffuse_bounce_3d_recursive_depth_contract();
    test_runtime_native_3d_temporal_accumulation_contract();
    test_runtime_native_3d_temporal_accumulation_ema_and_clamp_contract();
    test_runtime_native_3d_blue_noise_jitter_contract();
    test_runtime_native_3d_sampling_stratified_subpass_contract();
    test_runtime_native_3d_temporal_tile_parity_contract();
    test_runtime_diffuse_bounce_3d_seed_branch_contract();

    return test_support_failures() - before;
}
