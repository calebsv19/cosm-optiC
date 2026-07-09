#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "material/material.h"
#include "material/material_manager.h"
#include "render/integrators/integrator_common.h"
#include "render/ray_tracing2_preview.h"
#include "render/pipeline/ray_tracing2_preview_present.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_preview_reconstruction.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_render_request_snapshot.h"
#include "render/runtime_native_3d_render_unit.h"
#include "render/runtime_native_3d_tile_scheduler.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_render_trace_cost_ledger_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_triangle_bvh_3d.h"
#include "test_runtime_native_3d_render_prepared_suite_internal.h"
#include "test_support.h"

typedef struct RuntimeNative3DTileProgressProbe {
    int callback_count;
    size_t dirty_tile_count;
} RuntimeNative3DTileProgressProbe;

static bool runtime_native_3d_tile_progress_probe(
    const RuntimeNative3DTileSchedulerProgress* progress,
    void* user_data) {
    RuntimeNative3DTileProgressProbe* probe = (RuntimeNative3DTileProgressProbe*)user_data;
    if (!progress || !probe) {
        return false;
    }
    probe->callback_count += 1;
    probe->dirty_tile_count += progress->dirtyTileCount;
    return true;
}

static int test_runtime_native_3d_render_request_snapshot_copies_async_boundary_fields(void) {
    RuntimeNative3DRenderRequestSnapshot snapshot;
    RuntimeNative3DResourceBudget budget = {
        .cpuPercent = 75,
        .maxWorkerThreads = 4,
        .reserveCpuCount = 1,
    };
    RuntimeNative3DSamplingContext sampling = {
        .sampleSequence = 11u,
        .temporalSubpassIndex = 2u,
        .temporalSubpassCount = 8u,
    };
    volatile bool cancel_requested = false;
    RuntimeNative3DTileSchedulerCancelToken cancel_token = {
        .cancelRequested = &cancel_requested,
        .generation = 17u,
    };
    RuntimeNative3DRenderRequestSnapshotDesc desc = {
        .generationBound = true,
        .generation = 42u,
        .outputWidth = 640,
        .outputHeight = 360,
        .renderWidth = 320,
        .renderHeight = 180,
        .hostWidth = 640,
        .hostHeight = 360,
        .frameIndex = 3,
        .frameCount = 12,
        .temporalFrames = 8,
        .tileSize = 32,
        .integratorId = RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
        .sampling = &sampling,
        .resourceBudget = &budget,
        .preparedFrameBound = true,
        .preparedFrameValid = true,
        .preparedFrameWidth = 320,
        .preparedFrameHeight = 180,
        .preparedPrimitiveCount = 23u,
        .preparedTriangleCount = 89u,
        .materialSnapshotBound = true,
        .materialCount = 5u,
        .materialObjectBindingCount = 7u,
        .lightSnapshotBound = true,
        .enabledLightCount = 2u,
        .materialEmitterLightCount = 1u,
        .sceneAccelerationBound = true,
        .traceRoute = RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS,
        .tlasInstanceCount = 4u,
        .tlasNodeCount = 9u,
        .traceContextCallbackBound = true,
        .volumeEnabled = true,
        .volumeAttached = true,
        .volumeFrameSelectionDynamic = true,
        .waterSurfaceSourceFound = true,
        .waterSurfaceLoaded = true,
        .waterSurfaceMeshAttached = true,
        .waterSurfaceFrameSelectionDynamic = true,
        .waterSurfaceSampleCount = 128u,
        .waterSurfaceTriangleCount = 14,
        .frameDataflowLedgerEnabled = true,
        .outputRoot = "build/s7/output",
        .summaryPath = "build/s7/render_summary.json",
        .progressPath = "build/s7/render_progress.json",
        .cancelToken = &cancel_token,
    };
    bool ok = RuntimeNative3DRenderRequestSnapshot_Build(&snapshot, &desc);

    assert_true("runtime_native_3d_render_request_snapshot_build_ok", ok);
    assert_true("runtime_native_3d_render_request_snapshot_valid", snapshot.valid);
    assert_true("runtime_native_3d_render_request_snapshot_generation",
                snapshot.generationBound && snapshot.generation == 42u);
    assert_true("runtime_native_3d_render_request_snapshot_dimensions",
                snapshot.outputWidth == 640 && snapshot.outputHeight == 360 &&
                    snapshot.renderWidth == 320 && snapshot.renderHeight == 180 &&
                    snapshot.hostWidth == 640 && snapshot.hostHeight == 360);
    assert_true("runtime_native_3d_render_request_snapshot_sampling",
                snapshot.samplingBound &&
                    snapshot.sampling.sampleSequence == 11u &&
                    snapshot.sampling.temporalSubpassIndex == 2u &&
                    snapshot.sampling.temporalSubpassCount == 8u &&
                    snapshot.temporalFrames == 8 &&
                    snapshot.tileSize == 32);
    assert_true("runtime_native_3d_render_request_snapshot_budget",
                snapshot.resourceBudgetBound &&
                    snapshot.resourceBudget.cpuPercent == 75 &&
                    snapshot.resourceBudget.maxWorkerThreads == 4 &&
                    snapshot.resourceBudget.reserveCpuCount == 1);
    assert_true("runtime_native_3d_render_request_snapshot_scene_identity",
                snapshot.preparedFrameBound &&
                    snapshot.preparedFrameValid &&
                    snapshot.preparedPrimitiveCount == 23u &&
                    snapshot.preparedTriangleCount == 89u &&
                    snapshot.materialSnapshotBound &&
                    snapshot.materialCount == 5u &&
                    snapshot.materialObjectBindingCount == 7u);
    assert_true("runtime_native_3d_render_request_snapshot_light_identity",
                snapshot.lightSnapshotBound &&
                    snapshot.enabledLightCount == 2u &&
                    snapshot.materialEmitterLightCount == 1u);
    assert_true("runtime_native_3d_render_request_snapshot_accel",
                snapshot.sceneAccelerationBound &&
                    snapshot.traceRoute == RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS &&
                    snapshot.tlasInstanceCount == 4u &&
                    snapshot.tlasNodeCount == 9u &&
                    snapshot.traceContextCallbackBound);
    assert_true("runtime_native_3d_render_request_snapshot_volume_water",
                snapshot.volumeEnabled &&
                    snapshot.volumeAttached &&
                    snapshot.volumeFrameSelectionDynamic &&
                    snapshot.waterSurfaceSourceFound &&
                    snapshot.waterSurfaceLoaded &&
                    snapshot.waterSurfaceMeshAttached &&
                    snapshot.waterSurfaceFrameSelectionDynamic &&
                    snapshot.waterSurfaceSampleCount == 128u &&
                    snapshot.waterSurfaceTriangleCount == 14);
    assert_true("runtime_native_3d_render_request_snapshot_paths",
                snapshot.frameDataflowLedgerEnabled &&
                    snapshot.outputRootBound &&
                    snapshot.summaryDestinationBound &&
                    snapshot.progressDestinationBound &&
                    strcmp(snapshot.outputRoot, "build/s7/output") == 0 &&
                    strcmp(snapshot.summaryPath, "build/s7/render_summary.json") == 0 &&
                    strcmp(snapshot.progressPath, "build/s7/render_progress.json") == 0);
    assert_true("runtime_native_3d_render_request_snapshot_cancel",
                snapshot.cancelTokenBound &&
                    snapshot.cancelToken.cancelRequested == &cancel_requested &&
                    snapshot.cancelToken.generation == 17u &&
                    snapshot.cancelGeneration == 17u);

    RuntimeNative3DRenderRequestSnapshot_Init(&snapshot);
    assert_true("runtime_native_3d_render_request_snapshot_init_clears_valid",
                !snapshot.valid && !snapshot.generationBound &&
                    snapshot.outputRoot[0] == '\0');
    ok = RuntimeNative3DRenderRequestSnapshot_Build(&snapshot, NULL);
    assert_true("runtime_native_3d_render_request_snapshot_null_desc_fails", !ok);
    assert_true("runtime_native_3d_render_request_snapshot_null_desc_invalid",
                !snapshot.valid);
    return 0;
}

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
        RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
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
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
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

static int test_runtime_native_3d_disney_v2_mirror_stats_visible_from_render_path(void) {
    enum { kWidth = 21, kHeight = 21 };
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame frame = {0};
    float radiance[kWidth * kHeight * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    RuntimeNative3DRenderStats stats = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    memset(&animSettings, 0, sizeof(animSettings));
    MaterialManagerResetDefaults();

    sceneSettings.objectCount = 1;
    sceneSettings.sceneObjects[0].material_id = MATERIAL_PRESET_MIRROR;
    sceneSettings.sceneObjects[0].color = 0xFFFFFF;
    sceneSettings.sceneObjects[0].alpha = 1.0;
    sceneSettings.sceneObjects[0].opacity = 1.0;
    sceneSettings.sceneObjects[0].reflectivity = 0.95;
    sceneSettings.sceneObjects[0].roughness = 0.02;
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    animSettings.lightIntensity = 36.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.secondaryDiffuseSamples3D = 0;
    animSettings.transmissionSamples3D = 0;
    animSettings.bounceDepth3D = 1;
    animSettings.specularDepth3D = 1;
    animSettings.transmissionDepth3D = 1;
    animSettings.rouletteThreshold3D = 0.0;

    scene.hasLight = true;
    scene.light.position = vec3(0.0, 3.0, 0.0);
    scene.light.radius = 0.45;
    scene.light.intensity = 36.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.hasCamera = true;
    scene.camera.position = vec3(0.0, 2.0, 0.0);
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
    assert_true("runtime_native_3d_mirror_stats_alloc",
                scene.primitives != NULL && scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 0;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "mirror_stats_plane");
    scene.triangleMesh.triangles[0].p0 = vec3(-2.0, 0.0, -2.0);
    scene.triangleMesh.triangles[0].p1 = vec3(2.0, 0.0, -2.0);
    scene.triangleMesh.triangles[0].p2 = vec3(0.0, 0.0, 2.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].twoSided = true;
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 0;
    ok = RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh);
    assert_true("runtime_native_3d_mirror_stats_bvh", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }

    ok = RuntimeCameraProjector3D_Build(&scene.camera, kWidth, kHeight, &frame.projector);
    assert_true("runtime_native_3d_mirror_stats_projector", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        MaterialManagerResetDefaults();
        return 0;
    }

    frame.scene = scene;
    frame.width = kWidth;
    frame.height = kHeight;
    frame.valid = true;
    memset(radiance, 0, sizeof(radiance));
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance,
                                                        kWidth,
                                                        RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                        &frame,
                                                        0,
                                                        0,
                                                        kWidth,
                                                        kHeight,
                                                        &stats);
    assert_true("runtime_native_3d_mirror_stats_render", ok);
    assert_true("runtime_native_3d_mirror_stats_dominant_pixels",
                stats.mirrorDominantPixelCount > 0);
    assert_true("runtime_native_3d_mirror_stats_base_attenuated",
                stats.mirrorBaseAttenuatedPixelCount > 0);
    assert_true("runtime_native_3d_mirror_stats_reflection_hits",
                stats.mirrorReflectionHitPixelCount > 0 &&
                stats.mirrorEmitterReflectionPixelCount > 0);
    assert_true("runtime_native_3d_mirror_stats_dominance_max",
                stats.maxMirrorDominance > 0.90);
    assert_true("runtime_native_3d_mirror_stats_reflection_radiance",
                stats.maxMirrorSpecularReflectionRadiance > 0.0 &&
                stats.totalMirrorSpecularReflectionRadiance > 0.0);
    assert_true("runtime_native_3d_mirror_stats_base_totals",
                stats.totalMirrorBaseRadianceBeforeAttenuation > 0.0 &&
                stats.totalMirrorBaseRadianceAfterAttenuation <
                    stats.totalMirrorBaseRadianceBeforeAttenuation * 0.25);

    RuntimeNative3DPreparedFrame_Free(&frame);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    MaterialManagerResetDefaults();
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

static int test_runtime_native_3d_preview_reconstruction_rect_parity(void) {
    enum { kRenderWidth = 5, kRenderHeight = 4, kHostWidth = 17, kHostHeight = 13 };
    Uint8 render_buffer[kRenderWidth * kRenderHeight * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    Uint8 full_host[kHostWidth * kHostHeight * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    Uint8 rect_host[kHostWidth * kHostHeight * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    SDL_Rect rect_a = {.x = 0, .y = 0, .w = 8, .h = 6};
    SDL_Rect rect_b = {.x = 8, .y = 0, .w = 9, .h = 6};
    SDL_Rect rect_c = {.x = 0, .y = 6, .w = 17, .h = 7};
    const Runtime3DUpscaleMode modes[] = {
        RUNTIME_3D_UPSCALE_MODE_NEAREST,
        RUNTIME_3D_UPSCALE_MODE_BILINEAR,
    };
    bool ok = false;
    char label[96];

    for (int y = 0; y < kRenderHeight; ++y) {
        for (int x = 0; x < kRenderWidth; ++x) {
            const size_t base =
                ((size_t)y * (size_t)kRenderWidth + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            render_buffer[base] = (Uint8)(10 + x * 31 + y * 7);
            render_buffer[base + 1u] = (Uint8)(20 + x * 13 + y * 29);
            render_buffer[base + 2u] = (Uint8)(30 + x * 19 + y * 11);
            render_buffer[base + 3u] = 0xFFu;
        }
    }

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        memset(full_host, 0, sizeof(full_host));
        memset(rect_host, 0, sizeof(rect_host));

        ok = RuntimeNative3DPreviewReconstructABGRWithMode(render_buffer,
                                                           kRenderWidth,
                                                           kRenderHeight,
                                                           full_host,
                                                           kHostWidth,
                                                           kHostHeight,
                                                           modes[i]);
        snprintf(label, sizeof(label),
                 "runtime_native_3d_preview_reconstruction_full_ok_%zu", i);
        assert_true(label, ok);
        ok = RuntimeNative3DPreviewReconstructABGRRectWithMode(render_buffer,
                                                               kRenderWidth,
                                                               kRenderHeight,
                                                               rect_host,
                                                               kHostWidth,
                                                               kHostHeight,
                                                               &rect_a,
                                                               modes[i]);
        snprintf(label, sizeof(label),
                 "runtime_native_3d_preview_reconstruction_rect_a_ok_%zu", i);
        assert_true(label, ok);
        ok = RuntimeNative3DPreviewReconstructABGRRectWithMode(render_buffer,
                                                               kRenderWidth,
                                                               kRenderHeight,
                                                               rect_host,
                                                               kHostWidth,
                                                               kHostHeight,
                                                               &rect_b,
                                                               modes[i]);
        snprintf(label, sizeof(label),
                 "runtime_native_3d_preview_reconstruction_rect_b_ok_%zu", i);
        assert_true(label, ok);
        ok = RuntimeNative3DPreviewReconstructABGRRectWithMode(render_buffer,
                                                               kRenderWidth,
                                                               kRenderHeight,
                                                               rect_host,
                                                               kHostWidth,
                                                               kHostHeight,
                                                               &rect_c,
                                                               modes[i]);
        snprintf(label, sizeof(label),
                 "runtime_native_3d_preview_reconstruction_rect_c_ok_%zu", i);
        assert_true(label, ok);
        snprintf(label, sizeof(label),
                 "runtime_native_3d_preview_reconstruction_rect_parity_match_%zu", i);
        assert_true(label, memcmp(full_host, rect_host, sizeof(full_host)) == 0);
    }
    return 0;
}

static int test_runtime_native_3d_preview_reconstruction_dirty_tile_parity(void) {
    enum { kRenderWidth = 6, kRenderHeight = 4, kHostWidth = 19, kHostHeight = 13 };
    Uint8 render_buffer[kRenderWidth * kRenderHeight * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    Uint8 full_host[kHostWidth * kHostHeight * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    Uint8 dirty_host[kHostWidth * kHostHeight * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    SDL_Rect host_rect = {0};
    const SDL_Rect render_tiles[] = {
        {.x = 0, .y = 0, .w = 3, .h = 2},
        {.x = 3, .y = 0, .w = 3, .h = 2},
        {.x = 0, .y = 2, .w = 3, .h = 2},
        {.x = 3, .y = 2, .w = 3, .h = 2},
    };
    const Runtime3DUpscaleMode modes[] = {
        RUNTIME_3D_UPSCALE_MODE_NEAREST,
        RUNTIME_3D_UPSCALE_MODE_BILINEAR,
    };
    bool ok = false;
    char label[96];

    for (int y = 0; y < kRenderHeight; ++y) {
        for (int x = 0; x < kRenderWidth; ++x) {
            const size_t base =
                ((size_t)y * (size_t)kRenderWidth + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            render_buffer[base] = (Uint8)(15 + x * 23 + y * 9);
            render_buffer[base + 1u] = (Uint8)(25 + x * 17 + y * 21);
            render_buffer[base + 2u] = (Uint8)(35 + x * 11 + y * 27);
            render_buffer[base + 3u] = 0xFFu;
        }
    }

    for (size_t mode_index = 0; mode_index < sizeof(modes) / sizeof(modes[0]); ++mode_index) {
        memset(full_host, 0, sizeof(full_host));
        memset(dirty_host, 0, sizeof(dirty_host));

        ok = RuntimeNative3DPreviewReconstructABGRWithMode(render_buffer,
                                                           kRenderWidth,
                                                           kRenderHeight,
                                                           full_host,
                                                           kHostWidth,
                                                           kHostHeight,
                                                           modes[mode_index]);
        snprintf(label, sizeof(label),
                 "runtime_native_3d_preview_dirty_tile_full_ok_%zu", mode_index);
        assert_true(label, ok);

        for (size_t i = 0; i < sizeof(render_tiles) / sizeof(render_tiles[0]); ++i) {
            ok = RuntimeNative3DPreviewResolveDirtyHostRect(render_tiles[i].x,
                                                            render_tiles[i].y,
                                                            render_tiles[i].w,
                                                            render_tiles[i].h,
                                                            kRenderWidth,
                                                            kRenderHeight,
                                                            kHostWidth,
                                                            kHostHeight,
                                                            &host_rect);
            snprintf(label, sizeof(label),
                     "runtime_native_3d_preview_dirty_tile_rect_ok_%zu_%zu", mode_index, i);
            assert_true(label, ok);
            ok = RuntimeNative3DPreviewReconstructABGRRectWithMode(render_buffer,
                                                                   kRenderWidth,
                                                                   kRenderHeight,
                                                                   dirty_host,
                                                                   kHostWidth,
                                                                   kHostHeight,
                                                                   &host_rect,
                                                                   modes[mode_index]);
            snprintf(label, sizeof(label),
                     "runtime_native_3d_preview_dirty_tile_reconstruct_ok_%zu_%zu",
                     mode_index, i);
            assert_true(label, ok);
        }

        snprintf(label, sizeof(label),
                 "runtime_native_3d_preview_dirty_tile_parity_match_%zu", mode_index);
        assert_true(label, memcmp(full_host, dirty_host, sizeof(full_host)) == 0);
    }
    return 0;
}

static int test_runtime_native_3d_preview_final_truth_reconstruct_counters(void) {
    enum { kRenderWidth = 6, kRenderHeight = 4, kHostWidth = 19, kHostHeight = 13 };
    Uint8 render_buffer[kRenderWidth * kRenderHeight * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    Uint8 expected_host[kHostWidth * kHostHeight * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    Uint8 truth_host[kHostWidth * kHostHeight * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    const Runtime3DUpscaleMode modes[] = {
        RUNTIME_3D_UPSCALE_MODE_NEAREST,
        RUNTIME_3D_UPSCALE_MODE_BILINEAR,
    };
    bool ok = false;
    char label[96];

    for (int y = 0; y < kRenderHeight; ++y) {
        for (int x = 0; x < kRenderWidth; ++x) {
            const size_t base =
                ((size_t)y * (size_t)kRenderWidth + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            render_buffer[base] = (Uint8)(11 + x * 29 + y * 5);
            render_buffer[base + 1u] = (Uint8)(17 + x * 7 + y * 31);
            render_buffer[base + 2u] = (Uint8)(23 + x * 13 + y * 19);
            render_buffer[base + 3u] = 0xFFu;
        }
    }

    for (size_t mode_index = 0; mode_index < sizeof(modes) / sizeof(modes[0]); ++mode_index) {
        RuntimeNative3DRenderStats stats = {0};
        memset(expected_host, 0, sizeof(expected_host));
        memset(truth_host, 0x5A, sizeof(truth_host));

        ok = RuntimeNative3DPreviewReconstructABGRWithMode(render_buffer,
                                                           kRenderWidth,
                                                           kRenderHeight,
                                                           expected_host,
                                                           kHostWidth,
                                                           kHostHeight,
                                                           modes[mode_index]);
        snprintf(label, sizeof(label),
                 "runtime_native_3d_preview_t2_expected_truth_ok_%zu", mode_index);
        assert_true(label, ok);

        ok = RayTracing2PreviewPresent_ReconstructNative3DHostTruth(render_buffer,
                                                                    kRenderWidth,
                                                                    kRenderHeight,
                                                                    truth_host,
                                                                    kHostWidth,
                                                                    kHostHeight,
                                                                    modes[mode_index],
                                                                    &stats);
        snprintf(label, sizeof(label),
                 "runtime_native_3d_preview_t2_truth_reconstruct_ok_%zu", mode_index);
        assert_true(label, ok);
        snprintf(label, sizeof(label),
                 "runtime_native_3d_preview_t2_truth_reconstruct_match_%zu", mode_index);
        assert_true(label, memcmp(expected_host, truth_host, sizeof(expected_host)) == 0);
        assert_true("runtime_native_3d_preview_t2_host_resolve_counter",
                    stats.temporalHostFullResolveCount == 1);
        assert_true("runtime_native_3d_preview_t2_host_resolve_pixels",
                    stats.temporalFinalResolveHostPixels ==
                        (uint64_t)kHostWidth * (uint64_t)kHostHeight);
        assert_true("runtime_native_3d_preview_t2_host_resolve_bytes",
                    stats.temporalFinalResolveHostBytes ==
                        (uint64_t)kHostWidth * (uint64_t)kHostHeight *
                            (uint64_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
        assert_true("runtime_native_3d_preview_t2_no_dirty_present_counter",
                    stats.temporalDirtyPreviewPresentCount == 0);
        assert_true("runtime_native_3d_preview_t2_no_dirty_host_bytes",
                    stats.temporalDirtyPreviewHostBytes == 0u);
        assert_true("runtime_native_3d_preview_t2_no_final_present_counter",
                    stats.temporalFinalPreviewPresentCount == 0);
        assert_true("runtime_native_3d_preview_t2_no_history_promote_counter",
                    stats.temporalHistoryPromoteCount == 0);
    }
    return 0;
}

static int test_runtime_native_3d_tiled_presenter_final_truth_without_progress(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_3d_t2_presenter_truth\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":4.0,\"height\":4.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":1.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DRenderStats stats = {0};
    TileGrid grid = {0};
    Uint8 render_buffer[16 * 16 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    Uint8 host_buffer[32 * 32 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_preview_t2_presenter_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    memset(render_buffer, 0, sizeof(render_buffer));
    memset(host_buffer, 0, sizeof(host_buffer));
    animSettings.tileSize = 8;
    animSettings.renderScale3D = 1;
    animSettings.upscaleMode3D = RUNTIME_3D_UPSCALE_MODE_NEAREST;
    animSettings.disneyDenoiseEnabled = false;
    RuntimeNative3DTileSchedulerResetAdaptivePlan();
    TileGridEnsure(&grid, 16, 16, 8);

    ok = RayTracing2PreviewPresent_RenderNative3DTilesPreview(
        NULL,
        host_buffer,
        32,
        32,
        render_buffer,
        16,
        16,
        &grid,
        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        0.0,
        0.0,
        -2.0,
        NULL,
        1,
        false,
        false,
        &stats);
    assert_true("runtime_native_3d_preview_t2_presenter_truth_ok", ok);
    assert_true("runtime_native_3d_preview_t2_presenter_host_resolve",
                stats.temporalHostFullResolveCount == 1);
    assert_true("runtime_native_3d_preview_t2_presenter_history_promote",
                stats.temporalHistoryPromoteCount == 1);
    assert_true("runtime_native_3d_preview_t2_presenter_host_resolve_bytes",
                stats.temporalFinalResolveHostBytes ==
                    32u * 32u * (uint64_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
    assert_true("runtime_native_3d_preview_t2_presenter_history_promote_bytes",
                stats.temporalHistoryPromoteHostBytes ==
                    32u * 32u * (uint64_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
    assert_true("runtime_native_3d_preview_t2_presenter_no_history_seed_bytes",
                stats.temporalHistorySeedHostBytes == 0u);
    assert_true("runtime_native_3d_preview_t2_presenter_no_dirty_present",
                stats.temporalDirtyPreviewPresentCount == 0);
    assert_true("runtime_native_3d_preview_t2_presenter_no_dirty_bytes",
                stats.temporalDirtyPreviewHostBytes == 0u);
    assert_true("runtime_native_3d_preview_t2_presenter_no_final_present_without_renderer",
                stats.temporalFinalPreviewPresentCount == 0);
    assert_true("runtime_native_3d_preview_t2_presenter_scheduler_final_resolve",
                stats.temporalFinalFullResolveCount == 1);

    TileGridFree(&grid);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    RuntimeNative3DTileSchedulerResetAdaptivePlan();
    return 0;
}

static int test_runtime_native_3d_tiled_presenter_t6_capture_replay_without_renderer(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_3d_t6_presenter_capture\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":4.0,\"height\":4.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":1.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DRenderStats stats = {0};
    RayTracing2Native3DPresentationCapture capture;
    TileGrid grid = {0};
    Uint8 render_buffer[16 * 16 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    Uint8 host_buffer[32 * 32 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;
    int dirty_event_index = -1;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_preview_t6_capture_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    memset(render_buffer, 0, sizeof(render_buffer));
    memset(host_buffer, 0, sizeof(host_buffer));
    memset(&capture, 0, sizeof(capture));
    RayTracing2PreviewPresent_ResetNative3DPresentationCapture(&capture);
    RayTracing2PreviewPresent_SetNative3DPresentationCapture(&capture);
    animSettings.tileSize = 8;
    animSettings.renderScale3D = 1;
    animSettings.upscaleMode3D = RUNTIME_3D_UPSCALE_MODE_NEAREST;
    animSettings.disneyDenoiseEnabled = false;
    RuntimeNative3DTileSchedulerResetAdaptivePlan();
    TileGridEnsure(&grid, 16, 16, 8);

    ok = RayTracing2PreviewPresent_RenderNative3DTilesPreview(
        NULL,
        host_buffer,
        32,
        32,
        render_buffer,
        16,
        16,
        &grid,
        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        0.0,
        0.0,
        -2.0,
        NULL,
        1,
        false,
        true,
        &stats);
    RayTracing2PreviewPresent_SetNative3DPresentationCapture(NULL);

    assert_true("runtime_native_3d_preview_t6_capture_presenter_ok", ok);
    assert_true("runtime_native_3d_preview_t6_capture_events_recorded",
                capture.eventCount >= 4);
    assert_true("runtime_native_3d_preview_t6_capture_no_drops",
                capture.droppedEventCount == 0);
    assert_true("runtime_native_3d_preview_t6_capture_history_seed",
                capture.historySeedCount == 1);
    assert_true("runtime_native_3d_preview_t6_capture_dirty_progress",
                capture.dirtyProgressCount > 0);
    for (int i = 0; i < capture.eventCount; ++i) {
        if (capture.events[i].kind == RAY_TRACING2_NATIVE3D_PRESENT_EVENT_DIRTY_PROGRESS) {
            assert_true("runtime_native_3d_preview_t6_dirty_progress_single_tile",
                        capture.events[i].dirtyTileCount == 1);
            dirty_event_index = i;
            break;
        }
    }
    assert_true("runtime_native_3d_preview_t6_capture_dirty_event_found",
                dirty_event_index >= 0);
    if (dirty_event_index >= 0) {
        const RayTracing2Native3DPresentationEvent* dirty =
            &capture.events[dirty_event_index];
        assert_true("runtime_native_3d_preview_t6_capture_dirty_progress_subpass",
                    dirty->startedSubpasses == 1 &&
                    dirty->completedSubpasses >= 0 &&
                    dirty->totalSubpasses == 1 &&
                    dirty->completedTilesInSubpass > 0 &&
                    dirty->totalTilesInSubpass > 0);
        assert_true("runtime_native_3d_preview_t6_capture_dirty_tile_bounds",
                    dirty->dirtyTileBoundsValid != 0 &&
                    dirty->dirtyTileMinX >= 0 &&
                    dirty->dirtyTileMinY >= 0 &&
                    dirty->dirtyTileMaxX <= 16 &&
                    dirty->dirtyTileMaxY <= 16 &&
                    dirty->dirtyTileMaxX > dirty->dirtyTileMinX &&
                    dirty->dirtyTileMaxY > dirty->dirtyTileMinY);
    }
    assert_true("runtime_native_3d_preview_t6_capture_final_resolve",
                capture.finalResolveCount == 1);
    assert_true("runtime_native_3d_preview_t6_capture_no_renderer_final_present",
                capture.finalPresentCount == 0);
    assert_true("runtime_native_3d_preview_t6_capture_no_renderer_presents",
                capture.rendererPresentCount == 0);
    assert_true("runtime_native_3d_preview_t6_capture_history_promote",
                capture.historyPromoteCount == 1);
    assert_true("runtime_native_3d_preview_t6_capture_no_dirty_after_final",
                capture.dirtyAfterFinalResolveCount == 0);
    assert_true("runtime_native_3d_preview_t6_capture_dirty_before_final",
                capture.finalResolveBeforeDirtyCount == 0);
    assert_true("runtime_native_3d_preview_t6_capture_final_rect",
                capture.finalHostRect.x == 0 &&
                capture.finalHostRect.y == 0 &&
                capture.finalHostRect.w == 32 &&
                capture.finalHostRect.h == 32);
    assert_true("runtime_native_3d_preview_t6_capture_dirty_rect_valid",
                capture.lastDirtyHostRect.w > 0 &&
                capture.lastDirtyHostRect.h > 0 &&
                capture.lastDirtyHostRect.x >= 0 &&
                capture.lastDirtyHostRect.y >= 0 &&
                capture.lastDirtyHostRect.x + capture.lastDirtyHostRect.w <= 32 &&
                capture.lastDirtyHostRect.y + capture.lastDirtyHostRect.h <= 32);
    assert_true("runtime_native_3d_preview_t6_capture_stats_dirty_present_no_renderer",
                stats.temporalDirtyPreviewPresentCount == 0);
    assert_true("runtime_native_3d_preview_t6_capture_stats_final_present_no_renderer",
                stats.temporalFinalPreviewPresentCount == 0);
    assert_true("runtime_native_3d_preview_t6_capture_stats_final_resolve",
                stats.temporalHostFullResolveCount == 1);
    assert_true("runtime_native_3d_preview_t6_capture_stats_history_promote",
                stats.temporalHistoryPromoteCount == 1);
    assert_true("runtime_native_3d_preview_t6_capture_stats_dirty_host_bytes",
                stats.temporalDirtyPreviewHostBytes > 0u);
    assert_true("runtime_native_3d_preview_t6_capture_stats_dirty_host_pixels",
                stats.temporalDirtyPreviewHostPixels > 0u);
    assert_true("runtime_native_3d_preview_t6_capture_stats_final_host_bytes",
                stats.temporalFinalResolveHostBytes ==
                    32u * 32u * (uint64_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
    assert_true("runtime_native_3d_preview_t6_capture_stats_history_seed_bytes",
                stats.temporalHistorySeedHostBytes ==
                    32u * 32u * (uint64_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
    assert_true("runtime_native_3d_preview_t6_capture_stats_history_promote_bytes",
                stats.temporalHistoryPromoteHostBytes ==
                    32u * 32u * (uint64_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES);
    assert_true("runtime_native_3d_preview_t6_capture_no_final_present_bytes",
                stats.temporalFinalPreviewPresentHostBytes == 0u);

    TileGridFree(&grid);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    RuntimeNative3DTileSchedulerResetAdaptivePlan();
    return 0;
}

static int test_runtime_native_3d_adaptive_pixel_state_t3_measurement_contract(void) {
    RuntimeNative3DTemporalAccumulation accumulation = {0};
    RuntimeNative3DFeatureBuffer features = {0};
    RuntimeNative3DAdaptivePixelStateBuffer state = {0};
    float samples[8 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    const int width = 4;
    const int height = 2;
    const size_t pixel_count = (size_t)width * (size_t)height;
    bool ok = false;

    RuntimeNative3DTemporalAccumulation_Init(&accumulation);
    RuntimeNative3DFeatureBuffer_Init(&features);
    RuntimeNative3DAdaptivePixelStateBuffer_Init(&state);

    ok = RuntimeNative3DTemporalAccumulation_Ensure(&accumulation, width, height) &&
         RuntimeNative3DFeatureBuffer_Ensure(&features, width, height);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_setup_ok", ok);
    if (!ok) {
        RuntimeNative3DAdaptivePixelStateBuffer_Free(&state);
        RuntimeNative3DFeatureBuffer_Free(&features);
        RuntimeNative3DTemporalAccumulation_Free(&accumulation);
        return 0;
    }

    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t normal_base = i * 3u;
        features.hitMaskBuffer[i] = 1u;
        features.normalBuffer[normal_base] = 0.0f;
        features.normalBuffer[normal_base + 1u] = 1.0f;
        features.normalBuffer[normal_base + 2u] = 0.0f;
        features.depthBuffer[i] = 4.0f;
    }
    features.reflectivityBuffer[0] = 1.0f;
    features.roughnessBuffer[0] = 0.0f;
    features.directLightVisibilityOutcomeBuffer[1] =
        RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_CLEAR_VISIBLE;
    features.directLightVisibilityOutcomeBuffer[2] =
        RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_MIXED_PARTIAL;

    for (int pass = 0; pass < 4; ++pass) {
        ok = RuntimeNative3DTemporalAccumulation_AddRegion(&accumulation,
                                                           samples,
                                                           width,
                                                           0,
                                                           0,
                                                           width,
                                                           height);
        assert_true("runtime_native_3d_adaptive_state_t3_scatter_add_ok", ok);
        RuntimeNative3DTemporalAccumulation_CommitSubpass(&accumulation);
    }

    ok = RuntimeNative3DAdaptiveSampling_MeasurePixelState(&state,
                                                           &accumulation,
                                                           &features,
                                                           width,
                                                           2,
                                                           4);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_measure_ok", ok);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_measured_pixels",
                state.summary.measuredPixelCount == (int)pixel_count);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_high_risk_pixel",
                state.summary.highRiskPixelCount == 2);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_material_risk_pixel",
                state.summary.materialRiskPixelCount == 1 &&
                    state.summary.glossyRiskPixelCount == 1 &&
                    state.summary.transparentRiskPixelCount == 0);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_risk_range",
                state.summary.riskMax >= 0.99 && state.summary.riskSum >= 1.99);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_direct_light_counts",
                state.summary.directLightClearVisiblePixelCount == 1 &&
                    state.summary.directLightMixedPartialPixelCount == 1 &&
                    state.summary.directLightBoundaryRiskPixelCount == 1);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_stable_pixels",
                state.summary.stablePixelCount == (int)pixel_count - 2);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_probe_pixels",
                state.summary.probePixelCount == (int)pixel_count - 2);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_active_pixels_are_measurement",
                state.summary.activePixelCount == (int)pixel_count);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_tile_summary",
                state.summary.activeTileCount == 1 &&
                    state.summary.stableTileCount == 1 &&
                    state.summary.probeTileCount == 1 &&
                    state.summary.highRiskTileCount == 1);
    assert_true("runtime_native_3d_adaptive_state_t3_scatter_pixel_flags",
                (state.pixels[0].flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_HIGH_RISK) != 0u &&
                    (state.pixels[0].flags &
                     RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_MATERIAL_RISK) != 0u &&
                    (state.pixels[2].flags &
                     RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_DIRECT_LIGHT_RISK) != 0u &&
                    (state.pixels[1].flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_STABLE) != 0u &&
                    (state.pixels[1].flags & RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_PROBE) != 0u);

    RuntimeNative3DAdaptivePixelStateBuffer_Free(&state);
    RuntimeNative3DFeatureBuffer_Free(&features);
    RuntimeNative3DTemporalAccumulation_Free(&accumulation);
    return 0;
}

static int test_runtime_native_3d_adaptive_pixel_state_t4_activity_mask_contract(void) {
    RuntimeNative3DTemporalAccumulation accumulation = {0};
    RuntimeNative3DFeatureBuffer features = {0};
    RuntimeNative3DAdaptivePixelStateBuffer state = {0};
    RuntimeNative3DAdaptiveSamplingMask mask = {0};
    float samples[16 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    const int width = 8;
    const int height = 2;
    const size_t pixel_count = (size_t)width * (size_t)height;
    const size_t far_pixel = 7u;
    bool ok = false;

    RuntimeNative3DTemporalAccumulation_Init(&accumulation);
    RuntimeNative3DFeatureBuffer_Init(&features);
    RuntimeNative3DAdaptivePixelStateBuffer_Init(&state);
    RuntimeNative3DAdaptiveSamplingMask_Init(&mask);

    ok = RuntimeNative3DTemporalAccumulation_Ensure(&accumulation, width, height) &&
         RuntimeNative3DFeatureBuffer_Ensure(&features, width, height);
    assert_true("runtime_native_3d_adaptive_state_t4_setup_ok", ok);
    if (!ok) {
        RuntimeNative3DAdaptiveSamplingMask_Free(&mask);
        RuntimeNative3DAdaptivePixelStateBuffer_Free(&state);
        RuntimeNative3DFeatureBuffer_Free(&features);
        RuntimeNative3DTemporalAccumulation_Free(&accumulation);
        return 0;
    }

    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t normal_base = i * 3u;
        features.hitMaskBuffer[i] = 1u;
        features.normalBuffer[normal_base] = 0.0f;
        features.normalBuffer[normal_base + 1u] = 1.0f;
        features.normalBuffer[normal_base + 2u] = 0.0f;
        features.depthBuffer[i] = 4.0f;
    }
    features.reflectivityBuffer[0] = 1.0f;
    features.roughnessBuffer[0] = 0.0f;
    features.directLightVisibilityOutcomeBuffer[2] =
        RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_STABLE_PARTIAL;

    for (int pass = 0; pass < 3; ++pass) {
        ok = RuntimeNative3DTemporalAccumulation_AddRegion(&accumulation,
                                                           samples,
                                                           width,
                                                           0,
                                                           0,
                                                           width,
                                                           height);
        assert_true("runtime_native_3d_adaptive_state_t4_add_ok", ok);
        RuntimeNative3DTemporalAccumulation_CommitSubpass(&accumulation);
    }

    ok = RuntimeNative3DAdaptiveSampling_MeasurePixelState(&state,
                                                           &accumulation,
                                                           &features,
                                                           width,
                                                           2,
                                                           4) &&
         RuntimeNative3DAdaptiveSampling_RefreshActivityMaskFromPixelState(&mask,
                                                                           &state,
                                                                           width);
    assert_true("runtime_native_3d_adaptive_state_t4_mask_ok", ok);
    assert_true("runtime_native_3d_adaptive_state_t4_runtime_enabled",
                RuntimeNative3DAdaptiveSampling_RuntimeEnabled());
    assert_true("runtime_native_3d_adaptive_state_t4_state_has_stable_pixels",
                state.summary.stablePixelCount == (int)pixel_count - 2 &&
                    state.summary.probePixelCount == 0 &&
                    state.summary.highRiskPixelCount == 2);
    assert_true("runtime_native_3d_adaptive_state_t4_reason_counts",
                state.summary.materialRiskPixelCount == 1 &&
                    state.summary.glossyRiskPixelCount == 1 &&
                    state.summary.directLightStablePartialPixelCount == 1 &&
                    state.summary.directLightBoundaryRiskPixelCount == 1 &&
                    state.summary.mixedRiskTileCount == 1);
    assert_true("runtime_native_3d_adaptive_state_t4_mask_reduces_work",
                mask.activePixelCount > 0 && mask.activePixelCount < (int)pixel_count);
    assert_true("runtime_native_3d_adaptive_state_t4_high_risk_seed_active",
                mask.activeSampleMask[0] != 0u);
    assert_true("runtime_native_3d_adaptive_state_t4_padding_holds_neighbor",
                mask.activeSampleMask[1] != 0u);
    assert_true("runtime_native_3d_adaptive_state_t4_far_stable_pixel_skipped",
                mask.activeSampleMask[far_pixel] == 0u);
    assert_true("runtime_native_3d_adaptive_state_t4_tile_summary_active",
                mask.activeTileCount == 1 && mask.inactiveTileCount == 0);

    RuntimeNative3DAdaptiveSamplingMask_Free(&mask);
    RuntimeNative3DAdaptivePixelStateBuffer_Free(&state);
    RuntimeNative3DFeatureBuffer_Free(&features);
    RuntimeNative3DTemporalAccumulation_Free(&accumulation);
    return 0;
}

static int test_runtime_native_3d_adaptive_pixel_state_t5_conservative_stop_contract(void) {
    RuntimeNative3DTemporalAccumulation accumulation = {0};
    RuntimeNative3DFeatureBuffer features = {0};
    RuntimeNative3DAdaptivePixelStateBuffer state = {0};
    RuntimeNative3DAdaptiveSamplingMask mask = {0};
    float samples[16 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    const int width = 8;
    const int height = 2;
    const size_t pixel_count = (size_t)width * (size_t)height;
    const size_t far_pixel = 7u;
    bool ok = false;

    RuntimeNative3DTemporalAccumulation_Init(&accumulation);
    RuntimeNative3DFeatureBuffer_Init(&features);
    RuntimeNative3DAdaptivePixelStateBuffer_Init(&state);
    RuntimeNative3DAdaptiveSamplingMask_Init(&mask);

    ok = RuntimeNative3DTemporalAccumulation_Ensure(&accumulation, width, height) &&
         RuntimeNative3DFeatureBuffer_Ensure(&features, width, height);
    assert_true("runtime_native_3d_adaptive_state_t5_setup_ok", ok);
    if (!ok) {
        RuntimeNative3DAdaptiveSamplingMask_Free(&mask);
        RuntimeNative3DAdaptivePixelStateBuffer_Free(&state);
        RuntimeNative3DFeatureBuffer_Free(&features);
        RuntimeNative3DTemporalAccumulation_Free(&accumulation);
        return 0;
    }

    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t normal_base = i * 3u;
        features.hitMaskBuffer[i] = 1u;
        features.normalBuffer[normal_base] = 0.0f;
        features.normalBuffer[normal_base + 1u] = 1.0f;
        features.normalBuffer[normal_base + 2u] = 0.0f;
        features.depthBuffer[i] = 4.0f;
    }
    features.reflectivityBuffer[0] = 1.0f;
    features.roughnessBuffer[0] = 0.0f;
    features.directLightVisibilityOutcomeBuffer[2] =
        RUNTIME_NATIVE_3D_DIRECT_LIGHT_VISIBILITY_STABLE_PARTIAL;

    for (int pass = 0; pass < 3; ++pass) {
        ok = RuntimeNative3DTemporalAccumulation_AddRegion(&accumulation,
                                                           samples,
                                                           width,
                                                           0,
                                                           0,
                                                           width,
                                                           height);
        assert_true("runtime_native_3d_adaptive_state_t5_add_ok", ok);
        RuntimeNative3DTemporalAccumulation_CommitSubpass(&accumulation);
    }

    ok = RuntimeNative3DAdaptiveSampling_MeasurePixelState(&state,
                                                           &accumulation,
                                                           &features,
                                                           width,
                                                           2,
                                                           4) &&
         RuntimeNative3DAdaptiveSampling_RefreshConservativeEarlyStopMaskFromPixelState(
             &mask,
             &state,
             width);
    assert_true("runtime_native_3d_adaptive_state_t5_mask_ok", ok);
    assert_true("runtime_native_3d_adaptive_state_t5_default_opt_in_off",
                !RuntimeNative3DAdaptiveSampling_RiskEarlyStopEnabled());
    RuntimeNative3DAdaptiveSampling_SetRiskEarlyStopOverride(true, true);
    assert_true("runtime_native_3d_adaptive_state_t5_override_enabled",
                RuntimeNative3DAdaptiveSampling_RiskEarlyStopEnabled());
    RuntimeNative3DAdaptiveSampling_SetRiskEarlyStopOverride(false, false);
    assert_true("runtime_native_3d_adaptive_state_t5_risk_counts",
                state.summary.materialRiskPixelCount == 1 &&
                    state.summary.directLightBoundaryRiskPixelCount == 1);
    assert_true("runtime_native_3d_adaptive_state_t5_early_stop_hold_counts",
                state.summary.earlyStopEligiblePixelCount > 0 &&
                    state.summary.earlyStopHeldPixelCount > 0 &&
                    state.summary.earlyStopHoldMaterialRiskPixelCount == 1 &&
                    state.summary.earlyStopHoldDirectLightRiskPixelCount == 1);
    assert_true("runtime_native_3d_adaptive_state_t5_budget_counts",
                state.summary.budgetBucketPixelCounts[1] == (int)pixel_count &&
                    state.summary.budgetEligibleBucketPixelCounts[1] ==
                        state.summary.earlyStopEligiblePixelCount &&
                    state.summary.budgetHeldBucketPixelCounts[1] ==
                        state.summary.earlyStopHeldPixelCount &&
                    state.summary.budgetPartialHeldPixelCount == 1 &&
                    state.summary.budgetClearVisibleEligiblePixelCount == 0);
    assert_true("runtime_native_3d_adaptive_state_t5_region_counts",
                state.summary.earlyStopEligibleRegionCounts[0] +
                        state.summary.earlyStopEligibleRegionCounts[1] +
                        state.summary.earlyStopEligibleRegionCounts[2] +
                        state.summary.earlyStopEligibleRegionCounts[3] ==
                    state.summary.earlyStopEligiblePixelCount &&
                    state.summary.earlyStopHeldRegionCounts[0] +
                            state.summary.earlyStopHeldRegionCounts[1] +
                            state.summary.earlyStopHeldRegionCounts[2] +
                            state.summary.earlyStopHeldRegionCounts[3] ==
                        state.summary.earlyStopHeldPixelCount);
    assert_true("runtime_native_3d_adaptive_state_t5_mask_keeps_risk_active",
                mask.activeSampleMask[0] != 0u && mask.activeSampleMask[2] != 0u);
    assert_true("runtime_native_3d_adaptive_state_t5_padding_holds_boundary_neighbor",
                mask.activeSampleMask[1] != 0u &&
                    mask.conservativeEarlyStopPaddingHoldPixelCount > 0);
    assert_true("runtime_native_3d_adaptive_state_t5_base_active_counts",
                mask.conservativeEarlyStopBaseActivePixelCount > 0 &&
                    mask.conservativeEarlyStopEligiblePixelCount > 0 &&
                    mask.activePixelCount ==
                        mask.conservativeEarlyStopBaseActivePixelCount +
                            mask.conservativeEarlyStopPaddingHoldPixelCount);
    assert_true("runtime_native_3d_adaptive_state_t5_padding_seed_counts",
                mask.conservativeEarlyStopPaddingHoldHighSeedPixelCount > 0 &&
                    mask.conservativeEarlyStopPaddingHoldHighSeedPixelCount +
                            mask.conservativeEarlyStopPaddingHoldMediumSeedPixelCount ==
                        mask.conservativeEarlyStopPaddingHoldPixelCount);
    assert_true("runtime_native_3d_adaptive_state_t5_padding_region_counts",
                mask.conservativeEarlyStopPaddingHoldRegionCounts[0] +
                        mask.conservativeEarlyStopPaddingHoldRegionCounts[1] +
                        mask.conservativeEarlyStopPaddingHoldRegionCounts[2] +
                        mask.conservativeEarlyStopPaddingHoldRegionCounts[3] ==
                    mask.conservativeEarlyStopPaddingHoldPixelCount);
    assert_true("runtime_native_3d_adaptive_state_t5_far_stable_pixel_stops",
                mask.activeSampleMask[far_pixel] == 0u);
    assert_true("runtime_native_3d_adaptive_state_t5_mask_reduces_work",
                mask.activePixelCount > 0 && mask.activePixelCount < (int)pixel_count);

    RuntimeNative3DAdaptiveSamplingMask_Free(&mask);
    RuntimeNative3DAdaptivePixelStateBuffer_Free(&state);
    RuntimeNative3DFeatureBuffer_Free(&features);
    RuntimeNative3DTemporalAccumulation_Free(&accumulation);
    return 0;
}

static int test_runtime_native_3d_adaptive_pixel_state_t6_probe_only_does_not_pad(void) {
    RuntimeNative3DAdaptivePixelStateBuffer state = {0};
    RuntimeNative3DAdaptiveSamplingMask mask = {0};
    const int width = 5;
    const int height = 1;
    bool ok = false;

    RuntimeNative3DAdaptivePixelStateBuffer_Init(&state);
    RuntimeNative3DAdaptiveSamplingMask_Init(&mask);

    ok = RuntimeNative3DAdaptivePixelStateBuffer_Ensure(&state, width, height);
    assert_true("runtime_native_3d_adaptive_state_t6_setup_ok", ok);
    if (!ok) {
        RuntimeNative3DAdaptiveSamplingMask_Free(&mask);
        RuntimeNative3DAdaptivePixelStateBuffer_Free(&state);
        return 0;
    }

    state.summary.minSampleFloor = 2;
    state.pixels[0].flags = RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_STABLE |
                            RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVE |
                            RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_PROBE;
    state.pixels[1].flags = RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_STABLE;
    state.pixels[2].flags = RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_STABLE;
    state.pixels[3].flags = RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_STABLE |
                            RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVE |
                            RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_ACTIVITY_RISK;
    state.pixels[4].flags = RUNTIME_NATIVE_3D_ADAPTIVE_PIXEL_STABLE;

    ok = RuntimeNative3DAdaptiveSampling_RefreshConservativeEarlyStopMaskFromPixelState(
        &mask,
        &state,
        width);
    assert_true("runtime_native_3d_adaptive_state_t6_mask_ok", ok);
    assert_true("runtime_native_3d_adaptive_state_t6_probe_pixel_held",
                mask.activeSampleMask[0] != 0u);
    assert_true("runtime_native_3d_adaptive_state_t6_probe_neighbor_not_padded",
                mask.activeSampleMask[1] == 0u);
    assert_true("runtime_native_3d_adaptive_state_t6_activity_neighbors_padded",
                mask.activeSampleMask[2] != 0u && mask.activeSampleMask[4] != 0u);
    assert_true("runtime_native_3d_adaptive_state_t6_seed_counts",
                mask.conservativeEarlyStopPaddingHoldHighSeedPixelCount == 0 &&
                    mask.conservativeEarlyStopPaddingHoldMediumSeedPixelCount == 2 &&
                    mask.conservativeEarlyStopPaddingHoldPixelCount == 2);

    RuntimeNative3DAdaptiveSamplingMask_Free(&mask);
    RuntimeNative3DAdaptivePixelStateBuffer_Free(&state);
    return 0;
}

static int test_runtime_render_trace_cost_ledger_direct_light_visibility_attribution(void) {
    RuntimeRenderTraceCostLedger3D ledger = {0};

    RuntimeRenderTraceCostLedger3D_SetEnabled(true);
    RuntimeRenderTraceCostLedger3D_Reset();
    RuntimeRenderTraceCostLedger3D_RecordDirectLightVisibilityPolicy(
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_PRIMARY_HIT,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_SPHERE,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_AUTHORED_LIGHT,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_OMNI,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_CLEAR_VISIBLE,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_ALL_CLEAR,
        16,
        4,
        4,
        4,
        3.0,
        0.25,
        1.0,
        1.0);
    RuntimeRenderTraceCostLedger3D_RecordDirectLightVisibilityPolicy(
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_CALLER_SHADED_HIT,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_RECT,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_MATERIAL_EMITTER,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_EMISSION_ONE_SIDED,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_MIXED_PARTIAL,
        RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_STOP_FULL_SAMPLE_COUNT,
        16,
        4,
        16,
        12,
        12.0,
        0.005,
        0.0,
        0.7);

    RuntimeRenderTraceCostLedger3D_Snapshot(&ledger);
    assert_true("render_trace_cost_direct_light_attr_total_samples",
                ledger.directLightVisibilityPolicy.evaluatedSamples == 20u);
    assert_true("render_trace_cost_direct_light_attr_total_visibility",
                ledger.directLightVisibilityPolicy.visibilityTraces == 16u);
    assert_true("render_trace_cost_direct_light_attr_source_kind_visibility",
                ledger.directLightVisibilityPolicy.visibilityTracesBySourceKind
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_SPHERE] == 4u &&
                    ledger.directLightVisibilityPolicy.visibilityTracesBySourceKind
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SOURCE_RECT] == 12u);
    assert_true("render_trace_cost_direct_light_attr_origin_samples",
                ledger.directLightVisibilityPolicy.evaluatedSamplesBySourceOrigin
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_AUTHORED_LIGHT] ==
                        4u &&
                    ledger.directLightVisibilityPolicy.evaluatedSamplesBySourceOrigin
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_ORIGIN_MATERIAL_EMITTER] ==
                        16u);
    assert_true("render_trace_cost_direct_light_attr_outcome_visibility",
                ledger.directLightVisibilityPolicy.visibilityTracesByOutcome
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_CLEAR_VISIBLE] ==
                        4u &&
                    ledger.directLightVisibilityPolicy.visibilityTracesByOutcome
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_OUTCOME_MIXED_PARTIAL] ==
                        12u);
    assert_true("render_trace_cost_direct_light_attr_distance_importance_visibility",
                ledger.directLightVisibilityPolicy.visibilityTracesByDistanceImportance
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_MID]
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_HIGH] == 4u &&
                    ledger.directLightVisibilityPolicy.visibilityTracesByDistanceImportance
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_DISTANCE_FAR]
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_IMPORTANCE_LOW] == 12u);
    assert_true("render_trace_cost_direct_light_attr_sample_bucket_visibility",
                ledger.directLightVisibilityPolicy.visibilityTracesBySampleBucket
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_DECISION] == 4u &&
                    ledger.directLightVisibilityPolicy.visibilityTracesBySampleBucket
                            [RUNTIME_RENDER_TRACE_COST_DIRECT_LIGHT_SAMPLES_FULL] == 12u);

    RuntimeRenderTraceCostLedger3D_SetEnabled(false);
    RuntimeRenderTraceCostLedger3D_Reset();
    return 0;
}

static int test_runtime_render_trace_cost_ledger_transmission_sample_index_attribution(void) {
    RuntimeRenderTraceCostLedger3D ledger = {0};

    RuntimeRenderTraceCostLedger3D_SetEnabled(true);
    RuntimeRenderTraceCostLedger3D_Reset();
    RuntimeRenderTraceCostLedger3D_RecordTransmissionSample(
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_RECEIVER_HIT,
        0,
        0.9999,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_TOP_LEFT,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_FIRST_SUBPASS,
        2,
        1,
        0,
        true,
        0.8,
        0.4);
    RuntimeRenderTraceCostLedger3D_RecordTransmissionSample(
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_NO_HIT,
        1,
        0.9980,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_TOP_RIGHT,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_EARLY_SUBPASS,
        2,
        1,
        0,
        false,
        0.8,
        0.0);
    RuntimeRenderTraceCostLedger3D_RecordTransmissionSample(
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_PRIMARY,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_NO_CONTRIBUTION,
        3,
        0.9950,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_BOTTOM_LEFT,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_LATE_SUBPASS,
        1,
        1,
        0,
        false,
        0.8,
        0.0);
    RuntimeRenderTraceCostLedger3D_RecordTransmissionSample(
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_TERMINATION_RECEIVER_HIT,
        6,
        0.9800,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_BOTTOM_RIGHT,
        RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_EARLY_SUBPASS,
        2,
        1,
        0,
        true,
        0.8,
        0.05);

    RuntimeRenderTraceCostLedger3D_Snapshot(&ledger);
    assert_true("render_trace_cost_transmission_index_total",
                ledger.transmissionPathPolicy.sampleEvaluations == 4u);
    assert_true("render_trace_cost_transmission_index_counts",
                ledger.transmissionPathPolicy.sampleIndexCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_FIRST] == 1u &&
                    ledger.transmissionPathPolicy.sampleIndexCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_SECOND] == 1u &&
                    ledger.transmissionPathPolicy.sampleIndexCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_FOURTH] == 1u &&
                    ledger.transmissionPathPolicy.sampleIndexCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_LATER] == 1u);
    assert_true("render_trace_cost_transmission_index_source_counts",
                ledger.transmissionPathPolicy.sourceSampleIndexCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_FIRST] == 1u &&
                    ledger.transmissionPathPolicy.sourceSampleIndexCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_LATER] == 1u &&
                    ledger.transmissionPathPolicy.sourceSampleIndexCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_PRIMARY]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_FOURTH] == 1u);
    assert_true("render_trace_cost_transmission_index_productivity",
                ledger.transmissionPathPolicy.contributingSamplesByIndex
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_FIRST] == 1u &&
                    ledger.transmissionPathPolicy.contributingSamplesByIndex
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_LATER] == 1u &&
                    ledger.transmissionPathPolicy.noHitSamplesByIndex
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_SECOND] == 1u &&
                    ledger.transmissionPathPolicy.zeroContributionSamplesByIndex
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_SECOND] == 1u &&
                    ledger.transmissionPathPolicy.zeroContributionSamplesByIndex
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_FOURTH] == 1u);
    assert_true("render_trace_cost_transmission_index_source_productivity",
                ledger.transmissionPathPolicy.sourceContributingSamplesByIndex
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_FIRST] == 1u &&
                    ledger.transmissionPathPolicy.sourceContributingSamplesByIndex
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_LATER] == 1u &&
                    ledger.transmissionPathPolicy.sourceNoHitSamplesByIndex
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_SECOND] == 1u &&
                    ledger.transmissionPathPolicy.sourceZeroContributionSamplesByIndex
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_PRIMARY]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_FOURTH] == 1u &&
                    ledger.transmissionPathPolicy.sourceReceiverSamplesByIndex
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SAMPLE_INDEX_LATER] == 1u);
    assert_true("render_trace_cost_transmission_alignment_counts",
                ledger.transmissionPathPolicy.alignmentCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_AXIAL] == 1u &&
                    ledger.transmissionPathPolicy.alignmentCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_NARROW] == 1u &&
                    ledger.transmissionPathPolicy.alignmentCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_MEDIUM] == 1u &&
                    ledger.transmissionPathPolicy.alignmentCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_WIDE] == 1u);
    assert_true("render_trace_cost_transmission_alignment_source_productivity",
                ledger.transmissionPathPolicy.sourceContributingSamplesByAlignment
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_AXIAL] == 1u &&
                    ledger.transmissionPathPolicy.sourceNoHitSamplesByAlignment
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_NARROW] == 1u &&
                    ledger.transmissionPathPolicy.sourceZeroContributionSamplesByAlignment
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_PRIMARY]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_MEDIUM] == 1u &&
                    ledger.transmissionPathPolicy.sourceReceiverSamplesByAlignment
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_ALIGNMENT_WIDE] == 1u);
    assert_true("render_trace_cost_transmission_region_counts",
                ledger.transmissionPathPolicy.screenRegionCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_TOP_LEFT] == 1u &&
                    ledger.transmissionPathPolicy.screenRegionCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_TOP_RIGHT] == 1u &&
                    ledger.transmissionPathPolicy.screenRegionCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_BOTTOM_LEFT] == 1u &&
                    ledger.transmissionPathPolicy.screenRegionCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_BOTTOM_RIGHT] == 1u);
    assert_true("render_trace_cost_transmission_region_source_productivity",
                ledger.transmissionPathPolicy.sourceContributingSamplesByScreenRegion
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_TOP_LEFT] == 1u &&
                    ledger.transmissionPathPolicy.sourceNoHitSamplesByScreenRegion
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_TOP_RIGHT] == 1u &&
                    ledger.transmissionPathPolicy.sourceZeroContributionSamplesByScreenRegion
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_PRIMARY]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_BOTTOM_LEFT] == 1u &&
                    ledger.transmissionPathPolicy.sourceReceiverSamplesByScreenRegion
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SCREEN_REGION_BOTTOM_RIGHT] == 1u);
    assert_true("render_trace_cost_transmission_stability_counts",
                ledger.transmissionPathPolicy.pixelStabilityCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_FIRST_SUBPASS] ==
                        1u &&
                    ledger.transmissionPathPolicy.pixelStabilityCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_EARLY_SUBPASS] ==
                        2u &&
                    ledger.transmissionPathPolicy.pixelStabilityCounts
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_LATE_SUBPASS] ==
                        1u);
    assert_true("render_trace_cost_transmission_stability_source_productivity",
                ledger.transmissionPathPolicy.sourceContributingSamplesByPixelStability
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_FIRST_SUBPASS] ==
                        1u &&
                    ledger.transmissionPathPolicy.sourceNoHitSamplesByPixelStability
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_EARLY_SUBPASS] ==
                        1u &&
                    ledger.transmissionPathPolicy.sourceZeroContributionSamplesByPixelStability
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_PRIMARY]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_LATE_SUBPASS] ==
                        1u &&
                    ledger.transmissionPathPolicy.sourceReceiverSamplesByPixelStability
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_SOURCE_REFLECTED]
                            [RUNTIME_RENDER_TRACE_COST_TRANSMISSION_PIXEL_STABILITY_EARLY_SUBPASS] ==
                        1u);

    RuntimeRenderTraceCostLedger3D_SetEnabled(false);
    RuntimeRenderTraceCostLedger3D_Reset();
    return 0;
}

static int test_runtime_native_3d_render_unit_setup_defers_feature_prepass(void) {
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DRenderUnit unit;
    RuntimeNative3DSamplingContext sampling = {0};
    RuntimeScene3D scene;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DRenderUnit_Init(&unit);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);

    scene.hasCamera = true;
    scene.camera.position = vec3(0.0, -4.0, 1.5);
    scene.camera.rotation = 0.0;
    scene.camera.lookPitch = -0.1;
    scene.camera.zoom = 1.0;
    scene.camera.nearPlane = 0.1;
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 3.0);
    scene.light.radius = 0.2;
    scene.light.intensity = 1.0;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 8, 8, &frame.projector);
    assert_true("runtime_native_3d_unit_s1_projector_ok", ok);
    if (!ok) {
        RuntimeNative3DRenderUnit_Free(&unit);
        RuntimeNative3DPreparedFrame_Free(&frame);
        return 0;
    }

    frame.scene = scene;
    frame.width = 8;
    frame.height = 8;
    frame.valid = true;

    ok = RuntimeNative3DRenderUnit_Setup(&unit,
                                         RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                         &frame,
                                         0,
                                         0,
                                         8,
                                         8,
                                         &sampling,
                                         4,
                                         true);
    assert_true("runtime_native_3d_unit_s1_setup_ok", ok);
    assert_true("runtime_native_3d_unit_s1_feature_buffer_deferred",
                unit.featureBuffer.hitMaskBuffer == NULL && !unit.featuresPrepared);
    assert_true("runtime_native_3d_unit_s1_initial_adaptive_active",
                unit.adaptiveMask.activeSampleMask != NULL &&
                    unit.adaptiveMask.activePixelCount == 64);
    assert_true("runtime_native_3d_unit_s1_later_subpass_not_skipped",
                RuntimeNative3DRenderUnit_ShouldRenderSubpass(&unit, 1));

    RuntimeNative3DRenderUnit_Free(&unit);
    RuntimeNative3DPreparedFrame_Free(&frame);
    return 0;
}

static int test_runtime_native_3d_render_unit_scratch_reuses_compatible_setup(void) {
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DRenderUnit unit;
    RuntimeNative3DSamplingContext sampling = {0};
    RuntimeNative3DRenderStats scratch_stats = {0};
    RuntimeScene3D scene;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DRenderUnit_Init(&unit);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);

    scene.hasCamera = true;
    scene.camera.position = vec3(0.0, -4.0, 1.5);
    scene.camera.rotation = 0.0;
    scene.camera.lookPitch = -0.1;
    scene.camera.zoom = 1.0;
    scene.camera.nearPlane = 0.1;
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 3.0);
    scene.light.radius = 0.2;
    scene.light.intensity = 1.0;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 8, 8, &frame.projector);
    assert_true("runtime_native_3d_unit_scratch_projector_ok", ok);
    if (!ok) {
        RuntimeNative3DRenderUnit_Free(&unit);
        RuntimeNative3DPreparedFrame_Free(&frame);
        return 0;
    }

    frame.scene = scene;
    frame.width = 8;
    frame.height = 8;
    frame.valid = true;

    ok = RuntimeNative3DRenderUnit_Setup(&unit,
                                         RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                         &frame,
                                         0,
                                         0,
                                         8,
                                         8,
                                         &sampling,
                                         1,
                                         false);
    assert_true("runtime_native_3d_unit_scratch_first_setup_ok", ok);
    ok = ok && RuntimeNative3DRenderUnit_Setup(&unit,
                                               RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                               &frame,
                                               0,
                                               0,
                                               8,
                                               8,
                                               &sampling,
                                               1,
                                               false);
    assert_true("runtime_native_3d_unit_scratch_second_setup_ok", ok);
    RuntimeNative3DRenderUnit_RecordScratchStats(&unit, &scratch_stats);
    assert_true("runtime_native_3d_unit_scratch_resize_once",
                scratch_stats.renderUnitRadianceScratchResizeCalls == 1u);
    assert_true("runtime_native_3d_unit_scratch_reuse_once",
                scratch_stats.renderUnitRadianceScratchReuseCalls == 1u);
    assert_true("runtime_native_3d_unit_scratch_owned_bytes",
                scratch_stats.renderUnitScratchOwnedBytes > 0u &&
                    scratch_stats.renderUnitRadianceScratchCapacityBytesMax >=
                        scratch_stats.renderUnitRadianceScratchRequestedBytesMax);

    RuntimeNative3DRenderUnit_Free(&unit);
    RuntimeNative3DPreparedFrame_Free(&frame);
    return 0;
}

static int test_runtime_native_3d_render_unit_preserves_prepared_tlas_binding(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_3d_unit_tlas_binding\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":4.0,\"height\":4.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-3.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-3.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-1.0,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.5}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DRenderUnit unit;
    RuntimeNative3DSamplingContext sampling = {0};
    RuntimeNative3DRenderStats stats = {0};
    Ray3D ray = RuntimeRay3D_Make(vec3(0.0, 0.0, 0.0), vec3(0.0, -1.0, 0.0));
    HitInfo3D hit = {0};
    RuntimeSceneAcceleration3DTraceStatus trace_status;
    bool ok = false;

    RuntimeNative3DRenderUnit_Init(&unit);

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_unit_tlas_binding_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    animSettings.interactiveMode = false;
    animSettings.disneyDenoiseEnabled = false;
    animSettings.environmentBrightness = 0.0;
    animSettings.lightIntensity = 4.0;
    animSettings.forwardDecay = 8.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;

    ok = RuntimeNative3DPrepareFrameWithSampling(&frame,
                                                 16,
                                                 16,
                                                 0.0,
                                                 0.0,
                                                 -1.0,
                                                 &sampling);
    assert_true("runtime_native_3d_unit_tlas_binding_prepare_ok", ok);
    if (!ok) {
        RuntimeNative3DRenderUnit_Free(&unit);
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    RuntimeSceneAcceleration3D_ResetTraceStats();
    trace_status = RuntimeSceneAcceleration3D_TraceFirstHit(&frame.scene,
                                                            &ray,
                                                            0.001,
                                                            10.0,
                                                            &hit);
    assert_true("runtime_native_3d_unit_tlas_binding_initial_trace_bound",
                trace_status != RUNTIME_SCENE_ACCEL_3D_TRACE_UNREADY);

    ok = RuntimeNative3DRenderUnit_Setup(&unit,
                                         RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                         &frame,
                                         0,
                                         0,
                                         16,
                                         16,
                                         &sampling,
                                         2,
                                         false);
    assert_true("runtime_native_3d_unit_tlas_binding_setup_ok", ok);
    if (ok) {
        ok = RuntimeNative3DRenderUnit_RenderSubpass(&unit, 0, &stats);
        assert_true("runtime_native_3d_unit_tlas_binding_subpass_ok", ok);
    }

    HitInfo3D_Reset(&hit);
    trace_status = RuntimeSceneAcceleration3D_TraceFirstHit(&frame.scene,
                                                            &ray,
                                                            0.001,
                                                            10.0,
                                                            &hit);
    assert_true("runtime_native_3d_unit_tlas_binding_after_subpass_trace_bound",
                trace_status != RUNTIME_SCENE_ACCEL_3D_TRACE_UNREADY);

    RuntimeNative3DRenderUnit_Free(&unit);
    RuntimeNative3DPreparedFrame_Free(&frame);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_render_unit_raw_progress_resolve_bypasses_denoise(void) {
    RuntimeNative3DRenderUnit unit;
    float samples[4 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    uint8_t raw_pixels[2 * 2 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES] = {0};
    uint8_t denoised_pixels[2 * 2 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES] = {0};
    bool ok = false;

    RuntimeNative3DRenderUnit_Init(&unit);
    unit.width = 2;
    unit.height = 2;
    unit.startX = 0;
    unit.startY = 0;
    unit.endX = 2;
    unit.endY = 2;
    unit.integratorId = RAY_TRACING_3D_INTEGRATOR_DISNEY_V2;
    unit.committedSubpasses = 2;
    unit.useDenoise = true;
    unit.radianceCapacity = 4u * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    unit.resolvedRadiance = (float*)calloc(unit.radianceCapacity, sizeof(*unit.resolvedRadiance));
    assert_true("runtime_native_3d_unit_raw_progress_resolved_buffer_ok",
                unit.resolvedRadiance != NULL);
    if (!unit.resolvedRadiance) {
        RuntimeNative3DRenderUnit_Free(&unit);
        return 0;
    }
    ok = RuntimeNative3DTemporalAccumulation_Ensure(&unit.accumulation, 2, 2);
    assert_true("runtime_native_3d_unit_raw_progress_setup_ok", ok);
    if (!ok) {
        RuntimeNative3DRenderUnit_Free(&unit);
        return 0;
    }

    for (size_t i = 0; i < 4u; ++i) {
        const size_t base = i * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
        samples[base] = 0.35f;
        samples[base + 1u] = 0.45f;
        samples[base + 2u] = 0.55f;
        samples[base + RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL] = 0.08f;
    }

    ok = RuntimeNative3DTemporalAccumulation_AddRegion(&unit.accumulation,
                                                       samples,
                                                       2,
                                                       0,
                                                       0,
                                                       2,
                                                       2);
    assert_true("runtime_native_3d_unit_raw_progress_accumulate_ok", ok);
    RuntimeNative3DTemporalAccumulation_CommitSubpass(&unit.accumulation);

    ok = RuntimeNative3DRenderUnit_ResolveCurrentRawToPixels(&unit, raw_pixels, 2);
    assert_true("runtime_native_3d_unit_raw_progress_resolve_ok", ok);
    assert_true("runtime_native_3d_unit_raw_progress_resolve_lit",
                raw_pixels[0] > 0u || raw_pixels[1] > 0u || raw_pixels[2] > 0u);
    ok = RuntimeNative3DRenderUnit_ResolveCurrentToPixels(&unit, denoised_pixels, 2);
    assert_true("runtime_native_3d_unit_raw_progress_denoise_still_requires_features", !ok);

    RuntimeNative3DRenderUnit_Free(&unit);
    return 0;
}

static int test_runtime_native_3d_tiled_first_frame_occupancy_culls_empty_tiles(void) {
    AnimationConfig saved_anim = animSettings;
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeScene3D scene;
    RuntimeNative3DRenderStats stats = {0};
    RuntimeNative3DTileProgressProbe probe = {0};
    uint8_t pixel_buffer[32 * 32 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    memset(pixel_buffer, 0x5a, sizeof(pixel_buffer));
    animSettings.tileSize = 16;
    animSettings.renderScale3D = 1;
    animSettings.disneyDenoiseEnabled = false;
    RuntimeNative3DTileSchedulerResetAdaptivePlan();

    scene.hasCamera = true;
    scene.camera.position = vec3(0.0, -4.0, 1.5);
    scene.camera.rotation = 0.0;
    scene.camera.lookPitch = -0.1;
    scene.camera.zoom = 1.0;
    scene.camera.nearPlane = 0.1;
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 3.0);
    scene.light.radius = 0.2;
    scene.light.intensity = 1.0;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 32, 32, &frame.projector);
    assert_true("runtime_native_3d_tiled_t1_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        animSettings = saved_anim;
        return 0;
    }

    frame.scene = scene;
    frame.width = 32;
    frame.height = 32;
    frame.valid = true;

    ok = RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgress(
        pixel_buffer,
        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        &frame,
        1,
        NULL,
        NULL,
        runtime_native_3d_tile_progress_probe,
        &probe,
        &stats);
    assert_true("runtime_native_3d_tiled_t1_render_ok", ok);
    assert_true("runtime_native_3d_tiled_t1_occupancy_not_forced_conservative",
                stats.temporalConservativeFirstFrameTileRender == 0);
    assert_true("runtime_native_3d_tiled_t1_planned_tiles",
                stats.temporalPlannedParentTileCount == 4);
    assert_true("runtime_native_3d_tiled_t1_emitted_jobs",
                stats.temporalEmittedTileJobCount == 0);
    assert_true("runtime_native_3d_tiled_t1_occupancy_skips_empty_tiles",
                stats.temporalOccupancySkippedTileCount == 4);
    assert_true("runtime_native_3d_tiled_t1_dispatched_jobs",
                stats.temporalDispatchedTileJobCount == 0);
    assert_true("runtime_native_3d_tiled_t1_completed_jobs",
                stats.temporalCompletedTileJobCount == 0);
    assert_true("runtime_native_3d_tiled_t1_no_final_resolve_without_jobs",
                stats.temporalFinalFullResolveCount == 0);
    assert_true("runtime_native_3d_tiled_t1_no_progress_batches",
                stats.temporalProgressDirtyBatchCount == 0);
    assert_true("runtime_native_3d_tiled_t1_no_progress_tiles",
                stats.temporalProgressDirtyTileCount == 0);
    assert_true("runtime_native_3d_tiled_t1_probe_not_called", probe.callback_count == 0);
    assert_true("runtime_native_3d_tiled_t1_probe_tiles", probe.dirty_tile_count == 0u);
    assert_true("runtime_native_3d_tiled_t1_buffer_untouched",
                pixel_buffer[0] == 0x5au &&
                    pixel_buffer[sizeof(pixel_buffer) - 1u] == 0x5au);

    RuntimeNative3DPreparedFrame_Free(&frame);
    animSettings = saved_anim;
    RuntimeNative3DTileSchedulerResetAdaptivePlan();
    return 0;
}

static int test_runtime_native_3d_tiled_cancel_before_dispatch_blocks_publish(void) {
    AnimationConfig saved_anim = animSettings;
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeScene3D scene;
    RuntimeNative3DRenderStats stats = {0};
    volatile bool cancel_requested = true;
    RuntimeNative3DTileSchedulerCancelToken cancel_token = {0};
    RuntimeNative3DTileSchedulerControl scheduler_control = {0};
    uint8_t pixel_buffer[32 * 32 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    memset(pixel_buffer, 0x7c, sizeof(pixel_buffer));
    animSettings.tileSize = 16;
    animSettings.renderScale3D = 1;
    animSettings.disneyDenoiseEnabled = false;
    RuntimeNative3DTileSchedulerResetAdaptivePlan();

    scene.hasCamera = true;
    scene.camera.position = vec3(0.0, -4.0, 1.5);
    scene.camera.rotation = 0.0;
    scene.camera.lookPitch = -0.1;
    scene.camera.zoom = 1.0;
    scene.camera.nearPlane = 0.1;
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 3.0);
    scene.light.radius = 0.2;
    scene.light.intensity = 1.0;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 32, 32, &frame.projector);
    assert_true("runtime_native_3d_tiled_cancel_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        animSettings = saved_anim;
        return 0;
    }

    frame.scene = scene;
    frame.width = 32;
    frame.height = 32;
    frame.valid = true;
    frame.tileOccupancyConservativeAllTiles = true;
    cancel_token.cancelRequested = &cancel_requested;
    cancel_token.generation = 17u;
    scheduler_control.cancelToken = &cancel_token;

    ok = RuntimeNative3DRenderPreparedFrameTemporalTiledWithProgressBudgetAndControl(
        pixel_buffer,
        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
        &frame,
        1,
        NULL,
        NULL,
        runtime_native_3d_tile_progress_probe,
        NULL,
        NULL,
        &scheduler_control,
        &stats);
    assert_true("runtime_native_3d_tiled_cancel_render_reports_false", !ok);
    assert_true("runtime_native_3d_tiled_cancel_token_bound",
                stats.temporalTileSchedulerCancelTokenBound == 1);
    assert_true("runtime_native_3d_tiled_cancel_generation",
                stats.temporalTileSchedulerCancelGeneration == 17u);
    assert_true("runtime_native_3d_tiled_cancel_planned_tiles",
                stats.temporalPlannedParentTileCount == 4);
    assert_true("runtime_native_3d_tiled_cancel_emitted_jobs",
                stats.temporalEmittedTileJobCount == 4);
    assert_true("runtime_native_3d_tiled_cancel_dispatched_jobs_counted",
                stats.temporalDispatchedTileJobCount == 4);
    assert_true("runtime_native_3d_tiled_cancel_no_completed_jobs",
                stats.temporalCompletedTileJobCount == 0);
    assert_true("runtime_native_3d_tiled_cancel_no_committed_subpasses",
                stats.temporalCommittedSubpasses == 0);
    assert_true("runtime_native_3d_tiled_cancel_no_final_resolve",
                stats.temporalFinalFullResolveCount == 0);
    assert_true("runtime_native_3d_tiled_cancel_no_progress_publish",
                stats.temporalProgressDirtyBatchCount == 0 &&
                    stats.temporalProgressDirtyTileCount == 0);
    assert_true("runtime_native_3d_tiled_cancel_checkpoint",
                stats.temporalTileSchedulerCancelCheckCount >= 1 &&
                    stats.temporalTileSchedulerCancelRequestedCount >= 1 &&
                    stats.temporalTileSchedulerCancelBeforeDispatchCount == 1);
    assert_true("runtime_native_3d_tiled_cancel_shutdown_mode",
                stats.temporalTileSchedulerWorkerCancelShutdownCount == 1 &&
                    stats.temporalTileSchedulerWorkerDrainShutdownCount == 0);
    assert_true("runtime_native_3d_tiled_cancel_lifetime_owners",
                stats.temporalTileSchedulerJobArrayOwnerCount == 1 &&
                    stats.temporalTileSchedulerParentMetricArrayOwnerCount == 1 &&
                    stats.temporalTileSchedulerProgressTileArrayOwnerCount == 1 &&
                    stats.temporalTileSchedulerCompletionQueueOwnerCount == 1 &&
                    stats.temporalTileSchedulerWorkerPoolOwnerCount == 1);
    assert_true("runtime_native_3d_tiled_cancel_buffer_untouched",
                pixel_buffer[0] == 0x7cu &&
                    pixel_buffer[sizeof(pixel_buffer) - 1u] == 0x7cu);

    RuntimeNative3DPreparedFrame_Free(&frame);
    animSettings = saved_anim;
    RuntimeNative3DTileSchedulerResetAdaptivePlan();
    return 0;
}

int run_test_runtime_native_3d_render_prepared_scatter_preview_suite(void) {
    int before = test_support_failures();

    test_runtime_native_3d_background_volume_single_scatter_lifts_black_miss();
    test_runtime_native_3d_surface_volume_single_scatter_lifts_unlit_hit_across_tiers();
    test_runtime_native_3d_disney_v2_mirror_stats_visible_from_render_path();
    test_runtime_native_3d_dirty_rect_preview_base_parity();
    test_runtime_native_3d_preview_reconstruction_rect_parity();
    test_runtime_native_3d_preview_reconstruction_dirty_tile_parity();
    test_runtime_native_3d_preview_final_truth_reconstruct_counters();
    test_runtime_native_3d_render_request_snapshot_copies_async_boundary_fields();
    test_runtime_native_3d_tiled_presenter_final_truth_without_progress();
    test_runtime_native_3d_tiled_presenter_t6_capture_replay_without_renderer();
    test_runtime_native_3d_adaptive_pixel_state_t3_measurement_contract();
    test_runtime_native_3d_adaptive_pixel_state_t4_activity_mask_contract();
    test_runtime_native_3d_adaptive_pixel_state_t5_conservative_stop_contract();
    test_runtime_native_3d_adaptive_pixel_state_t6_probe_only_does_not_pad();
    test_runtime_render_trace_cost_ledger_direct_light_visibility_attribution();
    test_runtime_render_trace_cost_ledger_transmission_sample_index_attribution();
    test_runtime_native_3d_render_unit_setup_defers_feature_prepass();
    test_runtime_native_3d_render_unit_scratch_reuses_compatible_setup();
    test_runtime_native_3d_render_unit_preserves_prepared_tlas_binding();
    test_runtime_native_3d_render_unit_raw_progress_resolve_bypasses_denoise();
    test_runtime_native_3d_tiled_first_frame_occupancy_culls_empty_tiles();
    test_runtime_native_3d_tiled_cancel_before_dispatch_blocks_publish();
    return test_support_failures() - before;
}
