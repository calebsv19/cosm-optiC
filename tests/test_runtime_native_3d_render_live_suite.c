#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_disney_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "test_runtime_native_3d_render_internal.h"
#include "test_support.h"

static int test_runtime_native_3d_render_live_buffer_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const double seed_light_x = 3.0;
    const double seed_light_y = -2.0;
    const double offset_light_x = 0.0;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_runtime_render\","
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
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DRenderStats centered_stats = {0};
    RuntimeNative3DRenderStats diffuse_stats = {0};
    RuntimeNative3DRenderStats material_stats = {0};
    RuntimeNative3DRenderStats emission_stats = {0};
    RuntimeNative3DRenderStats disney_stats = {0};
    RuntimeNative3DRenderStats offset_stats = {0};
    uint8_t centered_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t diffuse_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t material_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t emission_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t offset_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;
    bool material_differs = false;
    bool emission_lifts_some_material_pixel = false;
    bool disney_differs_from_material = false;
    int i = 0;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_render_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.bounceDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;
    animSettings.lightHeight = 0.0;
    animSettings.interactiveMode = true;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    /*
     * This lane intentionally exercises the live-light fallback path with no authored
     * runtime light. Route-selection coverage lives in test_runtime_mode_backend_policy.c.
     */
    ok = RuntimeNative3DRenderToPixelBuffer(centered_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                            51,
                                            51,
                                            0.0,
                                            seed_light_x,
                                            seed_light_y,
                                            &centered_stats);
    assert_true("runtime_native_3d_render_centered_ok", ok);
    assert_true("runtime_native_3d_render_centered_hits_positive",
                centered_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_centered_visible_positive",
                centered_stats.visiblePixelCount > 0);
    assert_true("runtime_native_3d_render_centered_radiance_positive",
                centered_stats.maxRadiance > 0.0);
    assert_true("runtime_native_3d_render_centered_pixel_positive",
                native3d_test_pixel_r(centered_pixels, 51, 25, 25) > 0);
    ok = RuntimeNative3DRenderToPixelBuffer(diffuse_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                            51,
                                            51,
                                            0.0,
                                            seed_light_x,
                                            seed_light_y,
                                            &diffuse_stats);
    assert_true("runtime_native_3d_render_diffuse_seed_ok", ok);
    assert_true("runtime_native_3d_render_diffuse_seed_hits_positive",
                diffuse_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_diffuse_seed_visible_positive",
                diffuse_stats.visiblePixelCount > 0);
    assert_true("runtime_native_3d_render_diffuse_seed_secondary_rays_positive",
                diffuse_stats.secondaryRayCount > 0);
    assert_true("runtime_native_3d_render_diffuse_seed_secondary_hits_zero",
                diffuse_stats.secondaryHitCount == 0);
    assert_true("runtime_native_3d_render_diffuse_seed_bounce_pixels_zero",
                diffuse_stats.bouncePixelCount == 0);
    assert_close("runtime_native_3d_render_diffuse_seed_radiance_match",
                 diffuse_stats.maxRadiance,
                 centered_stats.maxRadiance,
                 1e-9);
    assert_true("runtime_native_3d_render_diffuse_seed_pixel_match",
                native3d_test_pixel_r(diffuse_pixels, 51, 25, 25) ==
                    native3d_test_pixel_r(centered_pixels, 51, 25, 25));

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    ok = RuntimeNative3DRenderToPixelBuffer(material_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_MATERIAL,
                                            51,
                                            51,
                                            0.0,
                                            seed_light_x,
                                            seed_light_y,
                                            &material_stats);
    assert_true("runtime_native_3d_render_material_seed_ok", ok);
    assert_true("runtime_native_3d_render_material_seed_hits_positive",
                material_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_material_seed_visible_positive",
                material_stats.visiblePixelCount > 0);
    assert_true("runtime_native_3d_render_material_seed_secondary_rays_positive",
                material_stats.secondaryRayCount > 0);
    assert_true("runtime_native_3d_render_material_seed_radiance_differs",
                fabs(material_stats.maxRadiance - diffuse_stats.maxRadiance) > 1e-6);
    for (i = 0; i < (51 * 51); ++i) {
        if (material_pixels[i] != diffuse_pixels[i]) {
            material_differs = true;
            break;
        }
    }
    assert_true("runtime_native_3d_render_material_seed_pixels_differ",
                material_differs);

    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_EMISSIVE;
    sceneSettings.sceneObjects[0].emissiveStrength = 1.0;
    ok = RuntimeNative3DRenderToPixelBuffer(emission_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY,
                                            51,
                                            51,
                                            0.0,
                                            seed_light_x,
                                            seed_light_y,
                                            &emission_stats);
    assert_true("runtime_native_3d_render_emission_seed_ok", ok);
    assert_true("runtime_native_3d_render_emission_seed_hits_positive",
                emission_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_emission_seed_visible_positive",
                emission_stats.visiblePixelCount > 0);
    assert_true("runtime_native_3d_render_emission_seed_secondary_rays_positive",
                emission_stats.secondaryRayCount > 0);
    assert_true("runtime_native_3d_render_emission_seed_radiance_positive",
                emission_stats.maxRadiance > 0.0);
    ok = RuntimeNative3DRenderToPixelBuffer(material_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_MATERIAL,
                                            51,
                                            51,
                                            0.0,
                                            seed_light_x,
                                            seed_light_y,
                                            &material_stats);
    assert_true("runtime_native_3d_render_emission_material_reference_ok", ok);
    assert_true("runtime_native_3d_render_emission_seed_radiance_lifts_material",
                emission_stats.maxRadiance > material_stats.maxRadiance);
    for (i = 0; i < (51 * 51); ++i) {
        if (emission_pixels[i] > material_pixels[i]) {
            emission_lifts_some_material_pixel = true;
            break;
        }
    }
    assert_true("runtime_native_3d_render_emission_seed_some_pixel_lifts_material",
                emission_lifts_some_material_pixel);

    ok = RuntimeNative3DRenderToPixelBuffer(offset_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                            51,
                                            51,
                                            0.0,
                                            offset_light_x,
                                            seed_light_y,
                                            &offset_stats);
    assert_true("runtime_native_3d_render_offset_ok", ok);
    assert_true("runtime_native_3d_render_offset_pixel_changes",
                native3d_test_pixel_r(centered_pixels, 51, 25, 25) !=
                    native3d_test_pixel_r(offset_pixels, 51, 25, 25));

    sceneSettings.sceneObjects[0].color = 0x2060FF;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    ok = RuntimeNative3DRenderToPixelBuffer(material_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_MATERIAL,
                                            51,
                                            51,
                                            0.0,
                                            3.0,
                                            -2.0,
                                            &material_stats);
    assert_true("runtime_native_3d_render_disney_material_reference_ok", ok);
    memset(offset_pixels, 0, sizeof(offset_pixels));
    memset(&disney_stats, 0, sizeof(disney_stats));
    ok = RuntimeNative3DRenderToPixelBuffer(offset_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DISNEY,
                                            51,
                                            51,
                                            0.0,
                                            3.0,
                                            -2.0,
                                            &disney_stats);
    assert_true("runtime_native_3d_render_disney_branch_ok", ok);
    assert_true("runtime_native_3d_render_disney_hits_positive",
                disney_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_disney_visible_positive",
                disney_stats.visiblePixelCount > 0);
    assert_true("runtime_native_3d_render_disney_secondary_rays_positive",
                disney_stats.secondaryRayCount > 0);
    assert_true("runtime_native_3d_render_disney_radiance_positive",
                disney_stats.maxRadiance > 0.0);
    assert_true("runtime_native_3d_render_disney_differs_from_material_radiance",
                fabs(disney_stats.maxRadiance - material_stats.maxRadiance) > 1e-6);
    assert_true("runtime_native_3d_render_disney_pixel_positive",
                native3d_test_pixel_r(offset_pixels, 51, 25, 25) > 0);
    assert_true("runtime_native_3d_render_disney_pixel_blue_dominant",
                native3d_test_pixel_b(offset_pixels, 51, 25, 25) >
                    native3d_test_pixel_r(offset_pixels, 51, 25, 25) + 1u &&
                native3d_test_pixel_b(offset_pixels, 51, 25, 25) >
                    native3d_test_pixel_g(offset_pixels, 51, 25, 25) + 1u);
    for (i = 0; i < (51 * 51); ++i) {
        if (offset_pixels[i] != material_pixels[i]) {
            disney_differs_from_material = true;
            break;
        }
    }
    assert_true("runtime_native_3d_render_disney_pixels_differ_from_material",
                disney_differs_from_material);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_render_direct_light_color_tint_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_direct_light_color_tint\","
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
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DRenderStats stats = {0};
    RuntimeNative3DRenderStats red_stats = {0};
    uint8_t pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t red_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_render_direct_color_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightHeight = 0.0;
    animSettings.interactiveMode = true;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;
    sceneSettings.sceneObjects[0].color = 0x00FF00;

    memset(pixels, 0, sizeof(pixels));
    ok = RuntimeNative3DRenderToPixelBuffer(pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                            51,
                                            51,
                                            0.0,
                                            3.0,
                                            -2.0,
                                            &stats);
    assert_true("runtime_native_3d_render_direct_color_ok", ok);
    assert_true("runtime_native_3d_render_direct_color_hits_positive",
                stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_direct_color_center_green_positive",
                native3d_test_pixel_g(pixels, 51, 25, 25) > 0);
    assert_true("runtime_native_3d_render_direct_color_center_red_zero",
                native3d_test_pixel_r(pixels, 51, 25, 25) <
                    native3d_test_pixel_g(pixels, 51, 25, 25));
    assert_true("runtime_native_3d_render_direct_color_center_blue_zero",
                native3d_test_pixel_b(pixels, 51, 25, 25) <
                    native3d_test_pixel_g(pixels, 51, 25, 25));

    sceneSettings.sceneObjects[0].color = 0xFF0000;
    memset(red_pixels, 0, sizeof(red_pixels));
    ok = RuntimeNative3DRenderToPixelBuffer(red_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                            51,
                                            51,
                                            0.0,
                                            3.0,
                                            -2.0,
                                            &red_stats);
    assert_true("runtime_native_3d_render_direct_red_ok", ok);
    assert_true("runtime_native_3d_render_direct_red_hits_positive",
                red_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_render_direct_red_center_red_positive",
                native3d_test_pixel_r(red_pixels, 51, 25, 25) > 0);
    assert_true("runtime_native_3d_render_direct_red_center_red_dominates_blue",
                native3d_test_pixel_r(red_pixels, 51, 25, 25) >
                    native3d_test_pixel_b(red_pixels, 51, 25, 25));

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_disney_result_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_disney_contract\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"gloss_floor\","
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
    RuntimeScene3D scene = {0};
    RuntimeCameraProjector3D projector = {0};
    RuntimeEmissionTransparency3DResult emission_result = {0};
    RuntimeDisney3DResult result = {0};
    bool ok = false;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_disney_contract_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightHeight = 0.0;
    animSettings.interactiveMode = true;
    animSettings.spaceMode = SPACE_MODE_3D;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    sceneSettings.sceneObjects[0].color = 0x2040FF;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene, 0.0);
    assert_true("runtime_disney_contract_build_scene_ok", ok);
    if (ok) {
        scene.light.position = vec3(0.0, -2.0, 0.0);
        scene.light.radius = 0.12;
        scene.light.intensity = animSettings.lightIntensity;
        scene.light.falloffDistance = animSettings.forwardDecay;
        scene.light.falloffMode = animSettings.forwardFalloffMode;
        scene.hasLight = true;
        scene.camera.position = vec3(0.0, 0.0, 0.0);
        scene.camera.rotation = 0.0;
        scene.camera.zoom = 1.0;
        scene.camera.nearPlane = 0.1;
        scene.hasCamera = true;
        ok = RuntimeCameraProjector3D_Build(&scene.camera, 101, 101, &projector);
        assert_true("runtime_disney_contract_projector_ok", ok);
    }
    if (ok) {
        ok = RuntimeEmissionTransparency3D_ShadePixel(&scene,
                                                      &projector,
                                                      50.0,
                                                      50.0,
                                                      NULL,
                                                      &emission_result);
        assert_true("runtime_disney_contract_emission_reference_ok", ok);
    }
    if (ok) {
        ok = RuntimeDisney3D_ShadePixel(&scene, &projector, 50.0, 50.0, NULL, &result);
        assert_true("runtime_disney_contract_shade_ok", ok);
    }
    if (ok) {
        assert_true("runtime_disney_contract_hit", result.hit);
        assert_true("runtime_disney_contract_payload", result.payloadResolved);
        assert_true("runtime_disney_contract_radiance_positive", result.radiance > 0.0);
        assert_true("runtime_disney_contract_base_positive", result.baseRadiance > 0.0);
        assert_true("runtime_disney_contract_specular_positive",
                    result.specularRadiance > 0.0);
        assert_true("runtime_disney_contract_blue_total_dominant",
                    result.radianceB > result.radianceR + 1e-6 &&
                    result.radianceB > result.radianceG + 1e-6);
        assert_true("runtime_disney_contract_blue_base_dominant",
                    result.baseRadianceB > result.baseRadianceR + 1e-6 &&
                    result.baseRadianceB > result.baseRadianceG + 1e-6);
        assert_true("runtime_disney_contract_fresnel_bounded",
                    result.fresnelWeight >= 0.0 && result.fresnelWeight <= 1.0);
        assert_true("runtime_disney_contract_roughness_bounded",
                    result.roughnessWeight >= 0.35 && result.roughnessWeight <= 1.0);
        assert_true("runtime_disney_contract_secondary_passthrough",
                    result.secondaryRayCount > 0);
        assert_true("runtime_disney_contract_diverges_from_emission_total",
                    fabs(result.radiance - emission_result.radiance) > 1e-6);
        assert_true("runtime_disney_contract_diverges_from_emission_direct",
                    fabs(result.directRadiance - emission_result.directRadiance) > 1e-6);
    }

    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_render_live_visible_emitter_bounded(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_visible_emitter_bounded\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"offscreen_plane\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":2.0,\"height\":2.0,"
            "\"frame\":{\"origin\":{\"x\":8.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":8.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DRenderStats stats = {0};
    uint8_t pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_render_live_emitter_bounded_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightHeight = 0.0;
    animSettings.interactiveMode = true;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.environmentBrightness = 0.0;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeNative3DRenderToPixelBuffer(pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -4.0,
                                            &stats);
    assert_true("runtime_native_3d_render_live_emitter_bounded_ok", ok);
    assert_true("runtime_native_3d_render_live_emitter_bounded_center_positive",
                native3d_test_pixel_r(pixels, 51, 25, 25) > 0);
    assert_true("runtime_native_3d_render_live_emitter_bounded_corner_black",
                native3d_test_pixel_r(pixels, 51, 0, 0) == 0 &&
                    native3d_test_pixel_r(pixels, 51, 50, 0) == 0 &&
                    native3d_test_pixel_r(pixels, 51, 0, 50) == 0 &&
                    native3d_test_pixel_r(pixels, 51, 50, 50) == 0);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_render_environment_floor_lights_miss_pixels(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_environment_background\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"offscreen_plane\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":2.0,\"height\":2.0,"
            "\"frame\":{\"origin\":{\"x\":8.0,\"y\":-5.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":8.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DRenderStats stats = {0};
    uint8_t pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_render_environment_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.lightHeight = 0.0;
    animSettings.interactiveMode = true;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.environmentBrightness = 128.0;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_AMBIENT;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeNative3DRenderToPixelBuffer(pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -4.0,
                                            &stats);
    assert_true("runtime_native_3d_render_environment_ok", ok);
    assert_true("runtime_native_3d_render_environment_center_above_floor",
                native3d_test_pixel_r(pixels, 51, 25, 25) >= 128);
    assert_true("runtime_native_3d_render_environment_corner_matches_floor",
                native3d_test_pixel_r(pixels, 51, 0, 0) > 0 &&
                    native3d_test_pixel_b(pixels, 51, 0, 0) >=
                        native3d_test_pixel_r(pixels, 51, 0, 0));
    assert_true("runtime_native_3d_render_environment_corner_sky_tinted",
                native3d_test_pixel_b(pixels, 51, 0, 0) >
                    native3d_test_pixel_g(pixels, 51, 0, 0));

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

int run_test_runtime_native_3d_render_live_suite(void) {
    int before = test_support_failures();

    test_runtime_native_3d_render_live_buffer_contract();
    test_runtime_native_3d_render_direct_light_color_tint_contract();
    test_runtime_native_3d_disney_result_contract();
    test_runtime_native_3d_render_live_visible_emitter_bounded();
    test_runtime_native_3d_render_environment_floor_lights_miss_pixels();
    return test_support_failures() - before;
}
