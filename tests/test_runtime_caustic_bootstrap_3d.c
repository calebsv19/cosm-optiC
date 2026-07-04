#include "test_runtime_caustic_bootstrap_3d.h"

#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "material/material.h"
#include "material/material_manager.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_caustic_bootstrap_3d.h"
#include "render/runtime_caustic_settings_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_scene_3d.h"
#include "test_runtime_native_3d_render_prepared_suite_internal.h"
#include "test_support.h"

static bool test_caustic_bootstrap_make_scene(RuntimeScene3D* scene) {
    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, -4.0, 3.0);
    scene->light.radius = 0.15;
    scene->light.intensity = 35.0;
    scene->light.falloffDistance = 8.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->hasCamera = true;
    scene->camera.position = vec3(0.0, 0.0, -0.20);
    scene->camera.rotation = 0.0;
    scene->camera.lookPitch = 0.0;
    scene->camera.zoom = 1.0;
    scene->camera.nearPlane = 0.1;
    scene->primitiveCapacity = 1;
    scene->triangleMesh.triangleCapacity = 2;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }

    scene->primitiveCount = 1;
    scene->triangleMesh.triangleCount = 2;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene->primitives[0].source.objectId,
             sizeof(scene->primitives[0].source.objectId),
             "%s",
             "bootstrap_glass_probe");

    scene->triangleMesh.triangles[0].p0 = vec3(-0.25, -4.0, -0.05);
    scene->triangleMesh.triangles[0].p1 = vec3(0.25, -4.0, -0.05);
    scene->triangleMesh.triangles[0].p2 = vec3(-0.25, -4.0, 0.25);
    scene->triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    scene->triangleMesh.triangles[0].sceneObjectIndex = 0;
    scene->triangleMesh.triangles[1].p0 = vec3(0.25, -4.0, -0.05);
    scene->triangleMesh.triangles[1].p1 = vec3(0.25, -4.0, 0.25);
    scene->triangleMesh.triangles[1].p2 = vec3(-0.25, -4.0, 0.25);
    scene->triangleMesh.triangles[1].normal = vec3(0.0, 1.0, 0.0);
    scene->triangleMesh.triangles[1].primitiveIndex = 0;
    scene->triangleMesh.triangles[1].sceneObjectIndex = 0;

    if (!prepared_suite_attach_dense_volume(&scene->volume,
                                            vec3(-0.60, -4.60, -0.90),
                                            5u,
                                            5u,
                                            7u,
                                            0.20,
                                            1.0f)) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    RuntimeScene3D_RefreshCapabilities(scene);
    return true;
}

static void test_caustic_bootstrap_enable_spatial_cache(void) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_SPATIAL_CACHE;
    settings.volumeCacheEnabled = true;
    settings.sampleBudget = 0;
    RuntimeCausticBootstrap3D_SetRequestState(&settings);
}

static int test_runtime_caustic_bootstrap_populates_volume_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticBootstrap3DDiagnostics diagnostics;
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    MaterialManagerResetDefaults();
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_bootstrap_scene",
                test_caustic_bootstrap_make_scene(&scene));
    test_caustic_bootstrap_enable_spatial_cache();

    ok = RuntimeCausticBootstrap3D_PopulateAnalyticVolumeCache(&scene,
                                                               &cache,
                                                               &diagnostics);
    assert_true("runtime_caustic_bootstrap_populate_ok", ok);
    assert_true("runtime_caustic_bootstrap_temporary",
                diagnostics.temporaryAnalyticBridge);
    assert_true("runtime_caustic_bootstrap_probe", diagnostics.probeBuilt);
    assert_true("runtime_caustic_bootstrap_allocated", diagnostics.cacheAllocated);
    assert_true("runtime_caustic_bootstrap_deposits",
                diagnostics.depositAcceptedCount > 0u);
    assert_true("runtime_caustic_bootstrap_nonzero",
                diagnostics.cache.nonZeroCellCount > 0u);
    assert_true("runtime_caustic_bootstrap_radiance",
                diagnostics.cache.totalRadianceR > 0.0);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticBootstrap3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_bootstrap_render_samples_volume_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DRenderStats stats = {0};
    float radiance[31 * 31 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    bool ok = false;

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    MaterialManagerResetDefaults();
    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    RuntimeCausticVolumeCache3D_Init(&frame.causticVolumeCache);
    assert_true("runtime_caustic_bootstrap_render_scene",
                test_caustic_bootstrap_make_scene(&scene));
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 31, 31, &frame.projector);
    assert_true("runtime_caustic_bootstrap_render_projector", ok);
    frame.scene = scene;
    frame.width = 31;
    frame.height = 31;
    frame.valid = true;
    test_caustic_bootstrap_enable_spatial_cache();
    assert_true("runtime_caustic_bootstrap_render_populate",
                RuntimeCausticBootstrap3D_PopulateAnalyticVolumeCache(
                    &frame.scene,
                    &frame.causticVolumeCache,
                    &frame.causticBootstrapDiagnostics));

    memset(radiance, 0, sizeof(radiance));
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance,
                                                        31,
                                                        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                                        &frame,
                                                        0,
                                                        0,
                                                        31,
                                                        31,
                                                        &stats);
    assert_true("runtime_caustic_bootstrap_render_ok", ok);
    assert_true("runtime_caustic_bootstrap_render_bridge_stats",
                stats.causticBootstrapTemporaryBridgeActive > 0);
    assert_true("runtime_caustic_bootstrap_render_cache_bound",
                stats.causticVolumeCacheBound > 0);
    assert_true("runtime_caustic_bootstrap_render_scatter_samples",
                stats.causticVolumeScatterSampleCount > 0);
    assert_true("runtime_caustic_bootstrap_render_scatter_contrib",
                stats.causticVolumeScatterContributingSampleCount > 0);
    assert_true("runtime_caustic_bootstrap_render_scatter_radiance",
                stats.totalCausticVolumeScatterRadianceR > 0.0);

    RuntimeNative3DPreparedFrame_Free(&frame);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    RuntimeCausticBootstrap3D_ResetRequestState();
    return 0;
}

int run_test_runtime_caustic_bootstrap_3d_tests(void) {
    int before = test_support_failures();

    test_runtime_caustic_bootstrap_populates_volume_cache();
    test_runtime_caustic_bootstrap_render_samples_volume_cache();

    return test_support_failures() - before;
}
