#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "render/integrators/integrator_common.h"
#include "render/ray_tracing2_preview.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_tile_occupancy.h"
#include "render/runtime_scene_3d.h"
#include "test_runtime_native_3d_render_internal.h"
#include "test_support.h"

static int test_runtime_native_3d_render_prepared_region_parity(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_3d_tiled_parity\","
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
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-2.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DRenderStats full_stats = {0};
    RuntimeNative3DRenderStats tiled_stats = {0};
    RuntimeNative3DPreparedFrame frame = {0};
    TileGrid grid = {0};
    uint8_t full_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t tiled_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_tile_parity_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.interactiveMode = false;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeNative3DRenderToPixelBuffer(full_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -2.0,
                                            &full_stats);
    assert_true("runtime_native_3d_tile_parity_full_ok", ok);

    memset(tiled_pixels, 0, sizeof(tiled_pixels));
    ok = RuntimeNative3DPrepareFrame(&frame, 51, 51, 0.0, 0.0, -2.0);
    assert_true("runtime_native_3d_tile_parity_prepare_ok", ok);
    if (ok) {
        TileGridEnsure(&grid, 51, 51, 16);
        ok = RuntimeNative3DPrepareFrameTileOccupancy(&frame, grid.tileSize);
        assert_true("runtime_native_3d_tile_parity_occupancy_prepare_ok", ok);
        for (size_t ti = 0; ti < grid.count; ++ti) {
            const IntegratorTile* tile = &grid.tiles[ti];
            RuntimeNative3DRenderStats tile_stats = {0};
            if (!RuntimeNative3DPreparedRegionMayContainGeometry(&frame,
                                                                 tile->originX,
                                                                 tile->originY,
                                                                 tile->originX + tile->width,
                                                                 tile->originY + tile->height)) {
                continue;
            }
            ok = RuntimeNative3DRenderPreparedRegion(tiled_pixels,
                                                     RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                                     &frame,
                                                     tile->originX,
                                                     tile->originY,
                                                     tile->originX + tile->width,
                                                     tile->originY + tile->height,
                                                     &tile_stats);
            assert_true("runtime_native_3d_tile_parity_region_ok", ok);
            if (!ok) break;
            RuntimeNative3DRenderStats_Accumulate(&tiled_stats, &tile_stats);
        }
    }

    assert_true("runtime_native_3d_tile_parity_pixels_match",
                memcmp(full_pixels, tiled_pixels, sizeof(full_pixels)) == 0);
    assert_true("runtime_native_3d_tile_parity_hit_count_match",
                full_stats.hitPixelCount == tiled_stats.hitPixelCount);
    assert_true("runtime_native_3d_tile_parity_visible_count_match",
                full_stats.visiblePixelCount == tiled_stats.visiblePixelCount);
    assert_true("runtime_native_3d_tile_parity_bounce_pixels_match",
                full_stats.bouncePixelCount == tiled_stats.bouncePixelCount);
    assert_true("runtime_native_3d_tile_parity_secondary_rays_match",
                full_stats.secondaryRayCount == tiled_stats.secondaryRayCount);
    assert_true("runtime_native_3d_tile_parity_secondary_hits_match",
                full_stats.secondaryHitCount == tiled_stats.secondaryHitCount);
    assert_true("runtime_native_3d_tile_parity_secondary_lit_hits_match",
                full_stats.secondaryContributingHitCount ==
                    tiled_stats.secondaryContributingHitCount);
    assert_close("runtime_native_3d_tile_parity_max_radiance_match",
                 full_stats.maxRadiance,
                 tiled_stats.maxRadiance,
                 1e-9);
    assert_close("runtime_native_3d_tile_parity_max_bounce_match",
                 full_stats.maxBounceRadiance,
                 tiled_stats.maxBounceRadiance,
                 1e-9);
    assert_close("runtime_native_3d_tile_parity_total_bounce_match",
                 full_stats.totalBounceRadiance,
                 tiled_stats.totalBounceRadiance,
                 1e-9);

    TileGridFree(&grid);
    RuntimeNative3DPreparedFrame_Free(&frame);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_render_visible_emitter_tile_parity(void) {
    RuntimeScene3D scene;
    RuntimeNative3DRenderStats full_stats = {0};
    RuntimeNative3DRenderStats tiled_stats = {0};
    RuntimeNative3DPreparedFrame frame = {0};
    TileGrid grid = {0};
    uint8_t full_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t tiled_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    bool ok = false;

    animSettings.environmentBrightness = 0.0;
    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -4.0, 0.0);
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
    assert_true("runtime_native_3d_visible_emitter_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_native_3d_visible_emitter_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }
    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 7;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "offscreen_plane");
    scene.triangleMesh.triangles[0].p0 = vec3(8.0, -5.0, -1.0);
    scene.triangleMesh.triangles[0].p1 = vec3(10.0, -5.0, -1.0);
    scene.triangleMesh.triangles[0].p2 = vec3(8.0, -5.0, 1.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 7;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 51, 51, &frame.projector);
    assert_true("runtime_native_3d_visible_emitter_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }
    frame.scene = scene;
    frame.width = 51;
    frame.height = 51;
    frame.valid = true;

    memset(full_pixels, 0, sizeof(full_pixels));
    ok = RuntimeNative3DRenderPreparedRegion(full_pixels,
                                             RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                             &frame,
                                             0,
                                             0,
                                             51,
                                             51,
                                             &full_stats);
    assert_true("runtime_native_3d_visible_emitter_full_ok", ok);
    assert_true("runtime_native_3d_visible_emitter_hits_positive",
                full_stats.hitPixelCount > 0);
    assert_true("runtime_native_3d_visible_emitter_visible_positive",
                full_stats.visiblePixelCount > 0);
    assert_true("runtime_native_3d_visible_emitter_radiance_positive",
                full_stats.maxRadiance > 0.0);
    assert_true("runtime_native_3d_visible_emitter_center_pixel_positive",
                native3d_test_pixel_r(full_pixels, 51, 25, 25) > 0);

    memset(tiled_pixels, 0, sizeof(tiled_pixels));
    TileGridEnsure(&grid, 51, 51, 16);
    ok = RuntimeNative3DPrepareFrameTileOccupancy(&frame, grid.tileSize);
    assert_true("runtime_native_3d_visible_emitter_occupancy_prepare_ok", ok);
    for (size_t ti = 0; ok && ti < grid.count; ++ti) {
        const IntegratorTile* tile = &grid.tiles[ti];
        RuntimeNative3DRenderStats tile_stats = {0};
        if (!RuntimeNative3DPreparedRegionMayContainGeometry(&frame,
                                                             tile->originX,
                                                             tile->originY,
                                                             tile->originX + tile->width,
                                                             tile->originY + tile->height)) {
            continue;
        }
        ok = RuntimeNative3DRenderPreparedRegion(tiled_pixels,
                                                 RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                                 &frame,
                                                 tile->originX,
                                                 tile->originY,
                                                 tile->originX + tile->width,
                                                 tile->originY + tile->height,
                                                 &tile_stats);
        assert_true("runtime_native_3d_visible_emitter_region_ok", ok);
        if (!ok) break;
        RuntimeNative3DRenderStats_Accumulate(&tiled_stats, &tile_stats);
    }

    assert_true("runtime_native_3d_visible_emitter_pixels_match",
                native3d_test_pixels_match_rgb_only(full_pixels, tiled_pixels, 51u * 51u));
    assert_true("runtime_native_3d_visible_emitter_hit_count_match",
                full_stats.hitPixelCount == tiled_stats.hitPixelCount);
    assert_true("runtime_native_3d_visible_emitter_visible_count_match",
                full_stats.visiblePixelCount == tiled_stats.visiblePixelCount);
    assert_close("runtime_native_3d_visible_emitter_max_radiance_match",
                 full_stats.maxRadiance,
                 tiled_stats.maxRadiance,
                 1e-9);

    TileGridFree(&grid);
    RuntimeNative3DPreparedFrame_Free(&frame);
    return 0;
}

static int test_runtime_native_3d_tile_occupancy_contract(void) {
    RuntimeScene3D scene = {0};
    RuntimeCamera3D camera = {0};
    RuntimeCameraProjector3D projector = {0};
    RuntimeNative3DTileOccupancy occupancy = {0};
    RuntimeTriangle3D* triangle = NULL;
    int occupied_tiles = 0;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&occupancy);

    triangle = (RuntimeTriangle3D*)calloc(1, sizeof(*triangle));
    assert_true("runtime_native_3d_tile_occupancy_triangle_alloc", triangle != NULL);
    if (!triangle) {
        RuntimeNative3DTileOccupancy_Free(&occupancy);
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    triangle->p0 = vec3(-1.0, -5.0, -1.0);
    triangle->p1 = vec3(1.0, -5.0, -1.0);
    triangle->p2 = vec3(0.0, -5.0, 1.0);
    triangle->normal = vec3(0.0, -1.0, 0.0);
    scene.triangleMesh.triangles = triangle;
    scene.triangleMesh.triangleCount = 1;
    scene.triangleMesh.triangleCapacity = 1;

    camera.position = vec3(0.0, 0.0, 0.0);
    camera.rotation = 0.0;
    camera.lookPitch = 0.0;
    camera.zoom = 1.0;
    camera.nearPlane = 0.1;

    ok = RuntimeCameraProjector3D_Build(&camera, 64, 64, &projector);
    assert_true("runtime_native_3d_tile_occupancy_projector_ok", ok);
    ok = RuntimeNative3DTileOccupancy_Build(&occupancy, &scene, &projector, 16);
    assert_true("runtime_native_3d_tile_occupancy_build_ok", ok);

    for (int ty = 0; ty < 4; ++ty) {
        for (int tx = 0; tx < 4; ++tx) {
            if (RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&occupancy,
                                                                      tx * 16,
                                                                      ty * 16,
                                                                      (tx + 1) * 16,
                                                                      (ty + 1) * 16)) {
                occupied_tiles += 1;
            }
        }
    }

    assert_true("runtime_native_3d_tile_occupancy_positive_tiles",
                occupied_tiles > 0);
    assert_true("runtime_native_3d_tile_occupancy_culls_some_tiles",
                occupied_tiles < 16);
    assert_true("runtime_native_3d_tile_occupancy_center_tile_hit",
                RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&occupancy,
                                                                      16,
                                                                      16,
                                                                      32,
                                                                      32));
    assert_true("runtime_native_3d_tile_occupancy_corner_tile_empty",
                !RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&occupancy,
                                                                       0,
                                                                       0,
                                                                       16,
                                                                       16));

    RuntimeNative3DTileOccupancy_Free(&occupancy);
    RuntimeScene3D_Free(&scene);
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

int run_test_runtime_native_3d_render_prepared_suite(void) {
    int before = test_support_failures();

    test_runtime_native_3d_render_prepared_region_parity();
    test_runtime_native_3d_render_visible_emitter_tile_parity();
    test_runtime_native_3d_tile_occupancy_contract();
    test_runtime_native_3d_dirty_rect_preview_base_parity();
    return test_support_failures() - before;
}
