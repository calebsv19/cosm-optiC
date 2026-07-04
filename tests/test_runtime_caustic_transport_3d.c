#include "test_runtime_caustic_transport_3d.h"

#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "material/material.h"
#include "material/material_manager.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_caustic_settings_3d.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"
#include "render/runtime_caustic_transport_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_volume_3d.h"
#include "test_runtime_native_3d_render_prepared_suite_internal.h"
#include "test_support.h"

static bool test_caustic_transport_make_scene(RuntimeScene3D* scene) {
    if (!scene) return false;
    RuntimeScene3D_Init(scene);
    scene->hasLight = true;
    scene->light.position = vec3(0.0, -3.0, 0.0);
    scene->light.radius = 0.05;
    scene->light.intensity = 60.0;
    scene->light.falloffDistance = 6.0;
    scene->light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene->hasCamera = true;
    scene->camera.position = vec3(0.0, 1.8, 0.0);
    scene->camera.rotation = 0.0;
    scene->camera.lookPitch = 0.0;
    scene->camera.zoom = 1.0;
    scene->camera.nearPlane = 0.1;
    scene->primitiveCapacity = 2;
    scene->triangleMesh.triangleCapacity = 4;
    scene->primitives = (RuntimePrimitive3D*)calloc((size_t)scene->primitiveCapacity,
                                                    sizeof(*scene->primitives));
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) {
        RuntimeScene3D_Free(scene);
        return false;
    }

    scene->primitiveCount = 2;
    scene->triangleMesh.triangleCount = 4;
    scene->primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene->primitives[0].source.objectId,
             sizeof(scene->primitives[0].source.objectId),
             "%s",
             "transport_glass_pane");
    scene->primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[1].source.sceneObjectIndex = 1;
    snprintf(scene->primitives[1].source.objectId,
             sizeof(scene->primitives[1].source.objectId),
             "%s",
             "transport_receiver");

    scene->triangleMesh.triangles[0].p0 = vec3(-0.30, -1.0, -0.30);
    scene->triangleMesh.triangles[0].p1 = vec3(-0.30, -1.0, 0.30);
    scene->triangleMesh.triangles[0].p2 = vec3(0.30, -1.0, -0.30);
    scene->triangleMesh.triangles[0].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[0].primitiveIndex = 0;
    scene->triangleMesh.triangles[0].sceneObjectIndex = 0;
    scene->triangleMesh.triangles[1].p0 = vec3(-0.30, -1.0, 0.30);
    scene->triangleMesh.triangles[1].p1 = vec3(0.30, -1.0, 0.30);
    scene->triangleMesh.triangles[1].p2 = vec3(0.30, -1.0, -0.30);
    scene->triangleMesh.triangles[1].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[1].primitiveIndex = 0;
    scene->triangleMesh.triangles[1].sceneObjectIndex = 0;
    scene->triangleMesh.triangles[2].p0 = vec3(-0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[2].p1 = vec3(0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[2].p2 = vec3(-0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[2].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[2].primitiveIndex = 1;
    scene->triangleMesh.triangles[2].sceneObjectIndex = 1;
    scene->triangleMesh.triangles[3].p0 = vec3(0.80, 0.35, -0.80);
    scene->triangleMesh.triangles[3].p1 = vec3(0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[3].p2 = vec3(-0.80, 0.35, 0.80);
    scene->triangleMesh.triangles[3].normal = vec3(0.0, -1.0, 0.0);
    scene->triangleMesh.triangles[3].primitiveIndex = 1;
    scene->triangleMesh.triangles[3].sceneObjectIndex = 1;

    if (!prepared_suite_attach_dense_volume(&scene->volume,
                                            vec3(-0.60, -0.85, -0.60),
                                            6u,
                                            9u,
                                            6u,
                                            0.15,
                                            1.0f)) {
        RuntimeScene3D_Free(scene);
        return false;
    }
    RuntimeScene3D_RefreshCapabilities(scene);
    return true;
}

static void test_caustic_transport_enable_transport_with_flags(int sample_budget,
                                                               bool volume_cache,
                                                               bool surface_cache) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.volumeCacheEnabled = volume_cache;
    settings.surfaceCacheEnabled = surface_cache;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    RuntimeCausticTransport3D_SetRequestState(&settings);
}

static void test_caustic_transport_enable_transport_with_surface_calibration(
    int sample_budget,
    double radiance_scale,
    double footprint_scale) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.surfaceCacheEnabled = true;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    settings.surfaceRadianceScale = radiance_scale;
    settings.surfaceFootprintScale = footprint_scale;
    RuntimeCausticTransport3D_SetRequestState(&settings);
}

static void test_caustic_transport_enable_transport_surface_without_fallback(
    int sample_budget) {
    RuntimeCausticSettings3D settings;
    RuntimeCausticSettings3D_Default(&settings);
    settings.mode = RUNTIME_CAUSTIC_MODE_TRANSPORT;
    settings.surfaceCacheEnabled = true;
    settings.sampleBudget = sample_budget;
    settings.maxPathDepth = 2;
    settings.surfaceReceiverFallbackEnabled = false;
    RuntimeCausticTransport3D_SetRequestState(&settings);
}

static void test_caustic_transport_enable_transport_with_budget(int sample_budget) {
    test_caustic_transport_enable_transport_with_flags(sample_budget, true, false);
}

static void test_caustic_transport_enable_transport(void) {
    test_caustic_transport_enable_transport_with_budget(8);
}

static void test_caustic_transport_seed_material_state(void) {
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    MaterialManagerResetDefaults();
    sceneSettings.objectCount = 2;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_TRANSPARENT;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[1].material_id = MATERIAL_PRESET_DEFAULT;
    sceneSettings.sceneObjects[1].color = 0xA0A0A0;
    sceneSettings.sceneObjects[1].alpha = 1.0;
}

static int test_runtime_caustic_transport_populates_volume_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_scene",
                test_caustic_transport_make_scene(&scene));
    test_caustic_transport_enable_transport();

    ok = RuntimeCausticTransport3D_PopulateVolumeCache(&scene, &cache, &diagnostics);
    assert_true("runtime_caustic_transport_populate_ok", ok);
    assert_true("runtime_caustic_transport_active", diagnostics.active);
    assert_true("runtime_caustic_transport_allocated", diagnostics.cacheAllocated);
    assert_true("runtime_caustic_transport_light_count", diagnostics.lightCount > 0u);
    assert_true("runtime_caustic_transport_multi_target_eval",
                diagnostics.evaluatedPathCount > 2u);
    assert_true("runtime_caustic_transport_paths", diagnostics.emittedPathCount > 0u);
    assert_true("runtime_caustic_transport_multi_target_emit",
                diagnostics.emittedPathCount > 1u);
    assert_true("runtime_caustic_transport_specular", diagnostics.specularEventCount > 0u);
    assert_true("runtime_caustic_transport_segments", diagnostics.volumeSegmentCount > 0u);
    assert_true("runtime_caustic_transport_deposits",
                diagnostics.depositAcceptedCount > 0u);
    assert_true("runtime_caustic_transport_nonzero",
                diagnostics.cache.nonZeroCellCount > 0u);
    assert_true("runtime_caustic_transport_radiance",
                diagnostics.cache.totalRadianceR > 0.0);
    assert_true("runtime_caustic_transport_footprints",
                diagnostics.cache.footprintDepositCount > 0u);
    assert_true("runtime_caustic_transport_footprint_cells",
                diagnostics.cache.footprintCellContributionCount >
                    diagnostics.cache.footprintDepositCount);
    assert_true("runtime_caustic_transport_footprint_radius",
                diagnostics.cache.averageFootprintRadiusVoxels > 0.0);
    assert_close("runtime_caustic_transport_footprint_energy_r",
                 diagnostics.cache.footprintDepositedRadianceR,
                 diagnostics.cache.footprintInputRadianceR,
                 1e-4);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_respects_sample_budget(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&cache);
    assert_true("runtime_caustic_transport_budget_scene",
                test_caustic_transport_make_scene(&scene));
    test_caustic_transport_enable_transport_with_budget(3);

    ok = RuntimeCausticTransport3D_PopulateVolumeCache(&scene, &cache, &diagnostics);
    assert_true("runtime_caustic_transport_budget_populate_ok", ok);
    assert_true("runtime_caustic_transport_budget_eval_count",
                diagnostics.evaluatedPathCount == 3u);
    assert_true("runtime_caustic_transport_budget_emit_count",
                diagnostics.emittedPathCount > 0u && diagnostics.emittedPathCount <= 3u);
    assert_true("runtime_caustic_transport_budget_deposits",
                diagnostics.depositAcceptedCount > 0u);

    RuntimeCausticVolumeCache3D_Free(&cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_populates_surface_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    assert_true("runtime_caustic_transport_surface_scene",
                test_caustic_transport_make_scene(&scene));
    test_caustic_transport_enable_transport_with_flags(8, false, true);

    ok = RuntimeCausticTransport3D_PopulateCaches(&scene,
                                                  &volume_cache,
                                                  &surface_cache,
                                                  &diagnostics);
    assert_true("runtime_caustic_transport_surface_populate_ok", ok);
    assert_true("runtime_caustic_transport_surface_active", diagnostics.active);
    assert_true("runtime_caustic_transport_surface_allocated",
                diagnostics.surfaceCacheAllocated);
    assert_true("runtime_caustic_transport_surface_records",
                diagnostics.surfaceCache.recordCount > 0u);
    assert_true("runtime_caustic_transport_surface_deposits",
                diagnostics.surfaceCache.depositAcceptedCount > 0u);
    assert_true("runtime_caustic_transport_surface_no_volume",
                !RuntimeCausticVolumeCache3D_IsAllocated(&volume_cache));

    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_suppresses_volume_without_vf3d(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    assert_true("runtime_caustic_transport_no_vf3d_scene",
                test_caustic_transport_make_scene(&scene));
    RuntimeVolumeAttachment3D_Reset(&scene.volume);
    RuntimeScene3D_RefreshCapabilities(&scene);
    test_caustic_transport_enable_transport_with_flags(8, true, true);

    ok = RuntimeCausticTransport3D_PopulateCaches(&scene,
                                                  &volume_cache,
                                                  &surface_cache,
                                                  &diagnostics);
    assert_true("runtime_caustic_transport_no_vf3d_surface_ok", ok);
    assert_true("runtime_caustic_transport_no_vf3d_suppressed",
                diagnostics.volumeCacheSuppressedNoSampleableVolume);
    assert_true("runtime_caustic_transport_no_vf3d_no_volume_alloc",
                !RuntimeCausticVolumeCache3D_IsAllocated(&volume_cache));
    assert_true("runtime_caustic_transport_no_vf3d_no_volume_deposits",
                diagnostics.cache.nonZeroCellCount == 0u);
    assert_true("runtime_caustic_transport_no_vf3d_no_volume_segments",
                diagnostics.volumeSegmentCount == 0u);
    assert_true("runtime_caustic_transport_no_vf3d_surface_allocated",
                diagnostics.surfaceCacheAllocated);
    assert_true("runtime_caustic_transport_no_vf3d_surface_records",
                diagnostics.surfaceCache.recordCount > 0u);
    assert_true("runtime_caustic_transport_no_vf3d_surface_deposits",
                diagnostics.surfaceCache.depositAcceptedCount > 0u);

    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_surface_calibration_scales_records(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeCausticSurfaceCache3D default_cache;
    RuntimeCausticSurfaceCache3D calibrated_cache;
    RuntimeCausticTransport3DDiagnostics default_diagnostics;
    RuntimeCausticTransport3DDiagnostics calibrated_diagnostics;
    double default_radius = 0.0;
    double calibrated_radius = 0.0;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    RuntimeCausticSurfaceCache3D_Init(&default_cache);
    RuntimeCausticSurfaceCache3D_Init(&calibrated_cache);
    assert_true("runtime_caustic_transport_calibrated_scene",
                test_caustic_transport_make_scene(&scene));

    test_caustic_transport_enable_transport_with_surface_calibration(8, 1.0, 1.0);
    ok = RuntimeCausticTransport3D_PopulateCaches(&scene,
                                                  &volume_cache,
                                                  &default_cache,
                                                  &default_diagnostics);
    assert_true("runtime_caustic_transport_calibrated_default_ok", ok);
    assert_true("runtime_caustic_transport_calibrated_default_records",
                default_cache.recordCount > 0u);
    default_radius = default_cache.records[0].radius;

    test_caustic_transport_enable_transport_with_surface_calibration(8, 8.0, 3.0);
    ok = RuntimeCausticTransport3D_PopulateCaches(&scene,
                                                  &volume_cache,
                                                  &calibrated_cache,
                                                  &calibrated_diagnostics);
    assert_true("runtime_caustic_transport_calibrated_scaled_ok", ok);
    assert_true("runtime_caustic_transport_calibrated_scaled_records",
                calibrated_cache.recordCount > 0u);
    calibrated_radius = calibrated_cache.records[0].radius;

    assert_true("runtime_caustic_transport_calibrated_radius",
                calibrated_radius > default_radius * 2.0);
    assert_true("runtime_caustic_transport_calibrated_radiance",
                calibrated_diagnostics.surfaceCache.maxRecordRadiance >
                    default_diagnostics.surfaceCache.maxRecordRadiance * 4.0);

    RuntimeCausticSurfaceCache3D_Free(&calibrated_cache);
    RuntimeCausticSurfaceCache3D_Free(&default_cache);
    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_surface_without_receiver_fallback(void) {
    SceneConfig saved_scene = sceneSettings;
    RuntimeScene3D scene;
    RuntimeCausticVolumeCache3D volume_cache;
    RuntimeCausticSurfaceCache3D surface_cache;
    RuntimeCausticTransport3DDiagnostics diagnostics;
    bool ok = false;

    test_caustic_transport_seed_material_state();
    RuntimeCausticVolumeCache3D_Init(&volume_cache);
    RuntimeCausticSurfaceCache3D_Init(&surface_cache);
    assert_true("runtime_caustic_transport_no_fallback_scene",
                test_caustic_transport_make_scene(&scene));
    test_caustic_transport_enable_transport_surface_without_fallback(8);

    ok = RuntimeCausticTransport3D_PopulateCaches(&scene,
                                                  &volume_cache,
                                                  &surface_cache,
                                                  &diagnostics);
    assert_true("runtime_caustic_transport_no_fallback_ok", ok);
    assert_true("runtime_caustic_transport_no_fallback_records",
                diagnostics.surfaceCache.recordCount > 0u);
    assert_true("runtime_caustic_transport_no_fallback_hits",
                diagnostics.surfaceReceiverHitCount > 0u);
    assert_true("runtime_caustic_transport_no_fallback_unused",
                diagnostics.surfaceReceiverFallbackCount == 0u);

    RuntimeCausticSurfaceCache3D_Free(&surface_cache);
    RuntimeCausticVolumeCache3D_Free(&volume_cache);
    RuntimeScene3D_Free(&scene);
    sceneSettings = saved_scene;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_render_samples_volume_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DRenderStats stats = {0};
    float radiance[31 * 31 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    bool ok = false;

    test_caustic_transport_seed_material_state();
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    RuntimeCausticVolumeCache3D_Init(&frame.causticVolumeCache);
    RuntimeCausticSurfaceCache3D_Init(&frame.causticSurfaceCache);
    assert_true("runtime_caustic_transport_render_scene",
                test_caustic_transport_make_scene(&scene));
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 31, 31, &frame.projector);
    assert_true("runtime_caustic_transport_render_projector", ok);
    frame.scene = scene;
    frame.width = 31;
    frame.height = 31;
    frame.valid = true;
    test_caustic_transport_enable_transport();
    assert_true("runtime_caustic_transport_render_populate",
                RuntimeCausticTransport3D_PopulateVolumeCache(
                    &frame.scene,
                    &frame.causticVolumeCache,
                    &frame.causticTransportDiagnostics));

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
    assert_true("runtime_caustic_transport_render_ok", ok);
    assert_true("runtime_caustic_transport_render_transport_stats",
                stats.causticTransportPathEmissionActive > 0);
    assert_true("runtime_caustic_transport_render_no_bootstrap",
                stats.causticBootstrapTemporaryBridgeActive == 0);
    assert_true("runtime_caustic_transport_render_cache_bound",
                stats.causticVolumeCacheBound > 0);
    assert_true("runtime_caustic_transport_render_scatter_samples",
                stats.causticVolumeScatterSampleCount > 0);
    assert_true("runtime_caustic_transport_render_scatter_contrib",
                stats.causticVolumeScatterContributingSampleCount > 0);
    assert_true("runtime_caustic_transport_render_scatter_radiance",
                stats.totalCausticVolumeScatterRadianceR > 0.0);

    RuntimeNative3DPreparedFrame_Free(&frame);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_transport_render_samples_surface_cache(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeDisneyV2CausticMode3D saved_caustic_mode = RuntimeDisneyV2_3D_CausticMode();
    double saved_caustic_strength = RuntimeDisneyV2_3D_CausticSidecarStrength();
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DRenderStats stats = {0};
    float radiance[31 * 31 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    bool ok = false;

    test_caustic_transport_seed_material_state();
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    RuntimeDisneyV2_3D_SetCausticMode(RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF, 0.0);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    RuntimeCausticVolumeCache3D_Init(&frame.causticVolumeCache);
    RuntimeCausticSurfaceCache3D_Init(&frame.causticSurfaceCache);
    assert_true("runtime_caustic_transport_render_surface_scene",
                test_caustic_transport_make_scene(&scene));
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 31, 31, &frame.projector);
    assert_true("runtime_caustic_transport_render_surface_projector", ok);
    frame.scene = scene;
    frame.width = 31;
    frame.height = 31;
    frame.valid = true;
    test_caustic_transport_enable_transport_with_flags(8, false, true);
    assert_true("runtime_caustic_transport_render_surface_populate",
                RuntimeCausticTransport3D_PopulateCaches(
                    &frame.scene,
                    &frame.causticVolumeCache,
                    &frame.causticSurfaceCache,
                    &frame.causticTransportDiagnostics));

    memset(radiance, 0, sizeof(radiance));
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance,
                                                        31,
                                                        RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                        &frame,
                                                        0,
                                                        0,
                                                        31,
                                                        31,
                                                        &stats);
    assert_true("runtime_caustic_transport_render_surface_ok", ok);
    assert_true("runtime_caustic_transport_render_surface_bound",
                stats.causticSurfaceCacheBound > 0);
    assert_true("runtime_caustic_transport_render_surface_records",
                stats.causticSurfaceCacheRecordCount > 0);
    assert_true("runtime_caustic_transport_render_surface_samples",
                stats.causticSurfaceCacheSampleLookupCount > 0);
    assert_true("runtime_caustic_transport_render_surface_contrib",
                stats.causticSurfaceCacheSampleContributingCount > 0);
    assert_true("runtime_caustic_transport_render_surface_radiance",
                stats.totalCausticSurfaceRadianceR > 0.0);
    assert_true("runtime_caustic_transport_render_surface_no_sidecar",
                stats.causticSidecarEnabled == 0);

    RuntimeNative3DPreparedFrame_Free(&frame);
    RuntimeDisneyV2_3D_SetCausticMode(saved_caustic_mode, saved_caustic_strength);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

static int test_runtime_caustic_sidecar_uses_prepared_probe_snapshot(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeDisneyV2CausticMode3D saved_caustic_mode = RuntimeDisneyV2_3D_CausticMode();
    double saved_caustic_strength = RuntimeDisneyV2_3D_CausticSidecarStrength();
    RuntimeDisneyV2CausticSidecarDiagnostics3D sidecar_diagnostics = {0};
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DRenderStats stats = {0};
    float radiance[31 * 31 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    bool ok = false;

    test_caustic_transport_seed_material_state();
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    RuntimeDisneyV2_3D_SetCausticMode(RUNTIME_DISNEY_V2_CAUSTIC_MODE_ANALYTIC, 1.0);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    RuntimeCausticVolumeCache3D_Init(&frame.causticVolumeCache);
    RuntimeCausticSurfaceCache3D_Init(&frame.causticSurfaceCache);
    assert_true("runtime_caustic_sidecar_snapshot_scene",
                test_caustic_transport_make_scene(&scene));
    ok = RuntimeCameraProjector3D_Build(&scene.camera, 31, 31, &frame.projector);
    assert_true("runtime_caustic_sidecar_snapshot_projector", ok);
    frame.scene = scene;
    frame.width = 31;
    frame.height = 31;
    RuntimeDisneyV2_3D_ResetCausticSidecarDiagnostics();
    frame.causticSidecarProbeValid =
        RuntimeDisneyV2_3D_BuildCausticSidecarProbe(&frame.scene,
                                                    &frame.causticSidecarProbe);
    RuntimeDisneyV2_3D_SnapshotCausticSidecarDiagnostics(&sidecar_diagnostics);
    frame.valid = true;
    assert_true("runtime_caustic_sidecar_snapshot_probe_valid",
                frame.causticSidecarProbeValid);
    assert_true("runtime_caustic_sidecar_snapshot_probe_build_count",
                sidecar_diagnostics.probeBuildCount == 1u);
    assert_true("runtime_caustic_sidecar_snapshot_triangle_scan_count",
                sidecar_diagnostics.triangleScanCount == 4u);
    assert_true("runtime_caustic_sidecar_snapshot_object_lookup_count",
                sidecar_diagnostics.objectTransmissiveLookupCount == 1u);
    assert_true("runtime_caustic_sidecar_snapshot_material_resolve_count",
                sidecar_diagnostics.materialResolveCount == 0u);

    RuntimeDisneyV2_3D_SetCausticMode(RUNTIME_DISNEY_V2_CAUSTIC_MODE_OFF, 0.0);
    memset(radiance, 0, sizeof(radiance));
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance,
                                                        31,
                                                        RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                        &frame,
                                                        0,
                                                        0,
                                                        31,
                                                        31,
                                                        &stats);
    assert_true("runtime_caustic_sidecar_snapshot_render_ok", ok);
    assert_true("runtime_caustic_sidecar_snapshot_enabled",
                stats.causticSidecarEnabled > 0);
    assert_true("runtime_caustic_sidecar_snapshot_samples",
                stats.causticSidecarSampleCount > 0);

    RuntimeNative3DPreparedFrame_Free(&frame);
    RuntimeDisneyV2_3D_SetCausticMode(saved_caustic_mode, saved_caustic_strength);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    RuntimeCausticTransport3D_ResetRequestState();
    return 0;
}

int run_test_runtime_caustic_transport_3d_tests(void) {
    int before = test_support_failures();

    test_runtime_caustic_transport_populates_volume_cache();
    test_runtime_caustic_transport_respects_sample_budget();
    test_runtime_caustic_transport_populates_surface_cache();
    test_runtime_caustic_transport_suppresses_volume_without_vf3d();
    test_runtime_caustic_transport_surface_calibration_scales_records();
    test_runtime_caustic_transport_surface_without_receiver_fallback();
    test_runtime_caustic_transport_render_samples_volume_cache();
    test_runtime_caustic_transport_render_samples_surface_cache();
    test_runtime_caustic_sidecar_uses_prepared_probe_snapshot();

    return test_support_failures() - before;
}
