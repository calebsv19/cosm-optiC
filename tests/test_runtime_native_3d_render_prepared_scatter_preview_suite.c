#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "render/integrators/integrator_common.h"
#include "render/ray_tracing2_preview.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_scene_3d.h"
#include "test_runtime_native_3d_render_prepared_suite_internal.h"
#include "test_support.h"

static int test_runtime_native_3d_background_volume_single_scatter_lifts_black_miss(void) {
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame frame = {0};
    float baseline_radiance[51 * 51 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    float scatter_radiance[51 * 51 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    RuntimeNative3DRenderStats stats = {0};
    size_t center_idx = 0u;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    animSettings.environmentBrightness = 0.0;
    scene.hasLight = true;
    scene.light.position = vec3(4.0, -4.0, 0.0);
    scene.light.radius = 0.25;
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.hasCamera = true;
    scene.camera.position = vec3(0.0, 0.0, 0.0);
    scene.camera.rotation = 0.0;
    scene.camera.lookPitch = 0.0;
    scene.camera.zoom = 1.0;
    scene.camera.nearPlane = 0.1;
    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_native_3d_volume_scatter_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_native_3d_volume_scatter_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        animSettings = saved_anim;
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 9;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "offscreen_plane");
    scene.triangleMesh.triangles[0].p0 = vec3(8.0, -5.0, -1.0);
    scene.triangleMesh.triangles[0].p1 = vec3(10.0, -5.0, -1.0);
    scene.triangleMesh.triangles[0].p2 = vec3(8.0, -5.0, 1.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 9;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 51, 51, &frame.projector);
    assert_true("runtime_native_3d_volume_scatter_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        animSettings = saved_anim;
        return 0;
    }

    frame.scene = scene;
    frame.width = 51;
    frame.height = 51;
    frame.valid = true;
    memset(baseline_radiance, 0, sizeof(baseline_radiance));
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(baseline_radiance,
                                                        51,
                                                        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                                        &frame,
                                                        0,
                                                        0,
                                                        51,
                                                        51,
                                                        &stats);
    assert_true("runtime_native_3d_volume_scatter_baseline_ok", ok);

    ok = prepared_suite_attach_dense_volume(&frame.scene.volume,
                                            vec3(-0.5, -4.5, -0.5),
                                            2u,
                                            9u,
                                            2u,
                                            0.5,
                                            1.0f);
    assert_true("runtime_native_3d_volume_scatter_attach_ok", ok);
    memset(scatter_radiance, 0, sizeof(scatter_radiance));
    memset(&stats, 0, sizeof(stats));
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(scatter_radiance,
                                                        51,
                                                        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                                        &frame,
                                                        0,
                                                        0,
                                                        51,
                                                        51,
                                                        &stats);
    assert_true("runtime_native_3d_volume_scatter_render_ok", ok);

    center_idx = (((size_t)25u * 51u) + 25u) * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    assert_close("runtime_native_3d_volume_scatter_baseline_black",
                 baseline_radiance[center_idx],
                 0.0,
                 1e-9);
    assert_true("runtime_native_3d_volume_scatter_center_positive",
                scatter_radiance[center_idx] > 0.0f);
    assert_true("runtime_native_3d_volume_scatter_max_positive", stats.maxRadiance > 0.0);
    assert_close("runtime_native_3d_volume_scatter_floor_stays_black",
                 scatter_radiance[center_idx + RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL],
                 0.0,
                 1e-9);

    RuntimeNative3DPreparedFrame_Free(&frame);
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_surface_volume_single_scatter_lifts_unlit_hit_across_tiers(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const RayTracing3DIntegratorId integrators[] = {
        RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
        RAY_TRACING_3D_INTEGRATOR_MATERIAL,
        RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY,
        RAY_TRACING_3D_INTEGRATOR_DISNEY,
    };
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame frame = {0};
    float baseline_radiance[41 * 41 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    float scatter_radiance[41 * 41 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    RuntimeNative3DRenderStats stats = {0};
    size_t center_idx = 0u;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_GLOSSY;
    scene.hasLight = true;
    scene.light.position = vec3(4.0, -5.0, 0.0);
    scene.light.radius = 0.25;
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.hasCamera = true;
    scene.camera.position = vec3(0.0, 0.0, 0.0);
    scene.camera.rotation = 0.0;
    scene.camera.lookPitch = 0.0;
    scene.camera.zoom = 1.0;
    scene.camera.nearPlane = 0.1;
    scene.primitiveCapacity = 1;
    scene.triangleMesh.triangleCapacity = 1;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_native_3d_volume_scatter_surface_alloc_primitives",
                scene.primitives != NULL);
    assert_true("runtime_native_3d_volume_scatter_surface_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "center_plane");
    scene.triangleMesh.triangles[0].p0 = vec3(-2.0, -5.0, -2.0);
    scene.triangleMesh.triangles[0].p1 = vec3(2.0, -5.0, -2.0);
    scene.triangleMesh.triangles[0].p2 = vec3(-2.0, -5.0, 2.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 41, 41, &frame.projector);
    assert_true("runtime_native_3d_volume_scatter_surface_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.environmentBrightness = 0.0;
    animSettings.topFillLightEnabled = false;
    frame.scene = scene;
    frame.width = 41;
    frame.height = 41;
    frame.valid = true;
    center_idx = (((size_t)20u * 41u) + 20u) * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;

    for (size_t i = 0; i < (sizeof(integrators) / sizeof(integrators[0])); ++i) {
        char label[96];
        memset(baseline_radiance, 0, sizeof(baseline_radiance));
        memset(scatter_radiance, 0, sizeof(scatter_radiance));
        memset(&stats, 0, sizeof(stats));
        RuntimeVolumeAttachment3D_Reset(&frame.scene.volume);

        ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(baseline_radiance,
                                                            41,
                                                            integrators[i],
                                                            &frame,
                                                            0,
                                                            0,
                                                            41,
                                                            41,
                                                            &stats);
        snprintf(label, sizeof(label), "runtime_native_3d_volume_scatter_surface_baseline_ok_%zu", i);
        assert_true(label, ok);
        snprintf(label, sizeof(label), "runtime_native_3d_volume_scatter_surface_baseline_black_%zu", i);
        assert_close(label, baseline_radiance[center_idx], 0.0, 1e-9);

        ok = prepared_suite_attach_dense_volume(&frame.scene.volume,
                                                vec3(-0.5, -4.5, -0.5),
                                                2u,
                                                9u,
                                                2u,
                                                0.5,
                                                1.0f);
        snprintf(label, sizeof(label), "runtime_native_3d_volume_scatter_surface_attach_ok_%zu", i);
        assert_true(label, ok);
        memset(&stats, 0, sizeof(stats));
        ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(scatter_radiance,
                                                            41,
                                                            integrators[i],
                                                            &frame,
                                                            0,
                                                            0,
                                                            41,
                                                            41,
                                                            &stats);
        snprintf(label, sizeof(label), "runtime_native_3d_volume_scatter_surface_render_ok_%zu", i);
        assert_true(label, ok);
        snprintf(label, sizeof(label), "runtime_native_3d_volume_scatter_surface_center_positive_%zu", i);
        assert_true(label, scatter_radiance[center_idx] > 0.0f);
        snprintf(label, sizeof(label), "runtime_native_3d_volume_scatter_surface_max_positive_%zu", i);
        assert_true(label, stats.maxRadiance > 0.0);
    }

    RuntimeNative3DPreparedFrame_Free(&frame);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_dirty_rect_preview_base_parity(void) {
    enum { kWidth = 8, kHeight = 6 };
    Uint8 luminance[kWidth * kHeight];
    uint32_t full_abgr[kWidth * kHeight];
    uint32_t dirty_abgr[kWidth * kHeight];
    SDL_Rect rect_a = {.x = 0, .y = 0, .w = 4, .h = 3};
    SDL_Rect rect_b = {.x = 4, .y = 0, .w = 4, .h = 3};
    SDL_Rect rect_c = {.x = 0, .y = 3, .w = 8, .h = 3};

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            luminance[y * kWidth + x] = (Uint8)((x * 17) + (y * 29));
        }
    }

    memset(full_abgr, 0, sizeof(full_abgr));
    memset(dirty_abgr, 0, sizeof(dirty_abgr));

    RayTracingPreview_CopyLuminanceRectToABGR(full_abgr,
                                              kWidth,
                                              kHeight,
                                              luminance,
                                              NULL);
    RayTracingPreview_CopyLuminanceRectToABGR(dirty_abgr,
                                              kWidth,
                                              kHeight,
                                              luminance,
                                              &rect_a);
    RayTracingPreview_CopyLuminanceRectToABGR(dirty_abgr,
                                              kWidth,
                                              kHeight,
                                              luminance,
                                              &rect_b);
    RayTracingPreview_CopyLuminanceRectToABGR(dirty_abgr,
                                              kWidth,
                                              kHeight,
                                              luminance,
                                              &rect_c);

    assert_true("runtime_native_3d_dirty_rect_preview_base_parity_match",
                memcmp(full_abgr, dirty_abgr, sizeof(full_abgr)) == 0);
    return 0;
}

int run_test_runtime_native_3d_render_prepared_scatter_preview_suite(void) {
    int before = test_support_failures();

    test_runtime_native_3d_background_volume_single_scatter_lifts_black_miss();
    test_runtime_native_3d_surface_volume_single_scatter_lifts_unlit_hit_across_tiers();
    test_runtime_native_3d_dirty_rect_preview_base_parity();
    return test_support_failures() - before;
}
