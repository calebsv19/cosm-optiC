#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "render/integrators/integrator_common.h"
#include "render/ray_tracing2_preview.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_tile_occupancy.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_triangle_bvh_3d.h"
#include "render/runtime_volume_3d_debug.h"
#include "render/runtime_volume_3d_integrate.h"
#include "test_runtime_native_3d_render_internal.h"
#include "test_runtime_native_3d_render_prepared_suite_internal.h"
#include "test_support.h"

static int test_runtime_native_3d_background_fill_uses_resolved_rgb(void) {
    RuntimeScene3D scene = {0};
    RuntimeCameraProjector3D projector = {0};
    uint8_t pixels[5 * 5 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    const size_t top_base = ((size_t)0 * 5u + 2u) * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    const size_t bottom_base = ((size_t)4 * 5u + 2u) * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeEnvironment3D_Init(&scene.environment);
    scene.environment.lightMode = ENVIRONMENT_LIGHT_MODE_AMBIENT;
    scene.environment.preset = ENVIRONMENT_PRESET_SKY;
    scene.environment.ambientIntensity = 0.35;
    scene.environment.backgroundIntensity = 0.35;
    scene.environment.backgroundIntensityDerivedFromAmbient = true;
    scene.environment.backgroundColor = vec3(1.0, 1.0, 1.0);
    RuntimeEnvironment3D_ApplyPreset(&scene.environment);
    scene.camera.position = vec3(0.0, 0.0, 0.0);
    scene.camera.rotation = 0.0;
    scene.camera.lookPitch = 0.0;
    scene.camera.zoom = 1.0;
    scene.camera.nearPlane = 0.1;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 5, 5, &projector);
    assert_true("runtime_native_3d_background_fill_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    memset(pixels, 0, sizeof(pixels));
    RuntimeNative3DFillPixelBufferBackground(pixels, 5, 5, &scene, &projector);

    assert_true("runtime_native_3d_background_fill_uses_color_channels",
                pixels[top_base] != pixels[top_base + 1u] ||
                    pixels[top_base + 1u] != pixels[top_base + 2u]);
    assert_true("runtime_native_3d_background_fill_uses_ray_gradient",
                pixels[top_base] != pixels[bottom_base] ||
                    pixels[top_base + 1u] != pixels[bottom_base + 1u] ||
                    pixels[top_base + 2u] != pixels[bottom_base + 2u]);

    RuntimeScene3D_Free(&scene);
    return 0;
}

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
    animSettings.tileSize = 16;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    animSettings.useTiledRenderer = false;
    memset(full_pixels, 0, sizeof(full_pixels));
    ok = RuntimeNative3DRenderToPixelBuffer(full_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -2.0,
                                            &full_stats);
    assert_true("runtime_native_3d_tile_parity_full_ok", ok);

    animSettings.useTiledRenderer = true;
    RuntimeNative3DFillPixelBufferEnvironment(tiled_pixels, 51u * 51u);
    ok = RuntimeNative3DRenderToPixelBuffer(tiled_pixels,
                                            RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE,
                                            51,
                                            51,
                                            0.0,
                                            0.0,
                                            -2.0,
                                            &tiled_stats);
    assert_true("runtime_native_3d_tile_parity_tiled_ok", ok);

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

static int test_runtime_native_3d_prepare_frame_attaches_configured_volume(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_native_3d_volume_attach\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
            "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0},"
            "\"axis_u\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
            "\"axis_v\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
            "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-4.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":-1.5,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeVolumeDebugSummary3D volume_summary;
    char dir[PATH_MAX] = {0};
    char vf3d_path[PATH_MAX] = {0};
    char manifest_path[PATH_MAX] = {0};
    const char* manifest_json =
        "{\n"
        "  \"manifest_version\": 2,\n"
        "  \"frame_contract\": \"vf3d\",\n"
        "  \"space_mode\": \"3d\",\n"
        "  \"frames\": [\n"
        "    {\n"
        "      \"frame_index\": 5,\n"
        "      \"time_seconds\": 2.0,\n"
        "      \"dt_seconds\": 0.02,\n"
        "      \"path\": \"frame_000005.vf3d\",\n"
        "      \"frame_contract\": \"vf3d\"\n"
        "    }\n"
        "  ]\n"
        "}\n";
    bool ok = false;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_native_3d_prepare_volume_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

    assert_true("runtime_native_3d_prepare_volume_temp_dir",
                prepared_suite_make_temp_dir(dir, sizeof(dir)));
    assert_true("runtime_native_3d_prepare_volume_vf3d_path",
                snprintf(vf3d_path, sizeof(vf3d_path), "%s/frame_000005.vf3d", dir) <
                    (int)sizeof(vf3d_path));
    assert_true("runtime_native_3d_prepare_volume_manifest_path",
                snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", dir) <
                    (int)sizeof(manifest_path));
    assert_true("runtime_native_3d_prepare_volume_write_vf3d",
                prepared_suite_write_sample_vf3d(vf3d_path));
    assert_true("runtime_native_3d_prepare_volume_write_manifest",
                prepared_suite_write_text_file(manifest_path, manifest_json));

    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    animSettings.interactiveMode = false;
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    animSettings.volumeInteractionEnabled = true;
    animSettings.volumeSourceKind = VOLUME_SOURCE_MANIFEST;
    snprintf(animSettings.volumeSourcePath,
             sizeof(animSettings.volumeSourcePath),
             "%s",
             manifest_path);
    animSettings.volumeAffectsLighting = false;
    animSettings.volumeDebugOverlayEnabled = true;
    sceneSettings.camera.x = 0.0;
    sceneSettings.camera.y = 0.0;
    sceneSettings.cameraZ = 0.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = RuntimeNative3DPrepareFrame(&frame, 41, 41, 0.0, 0.0, -1.5);
    assert_true("runtime_native_3d_prepare_volume_prepare_ok", ok);
    if (ok) {
        assert_true("runtime_native_3d_prepare_volume_enabled",
                    frame.scene.volume.enabled);
        assert_true("runtime_native_3d_prepare_volume_has_data",
                    frame.scene.volume.hasData);
        assert_true("runtime_native_3d_prepare_volume_source_raw",
                    frame.scene.volume.sourceKind == RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D);
        assert_true("runtime_native_3d_prepare_volume_affects_lighting_false",
                    !frame.scene.volume.affectsLighting);
        assert_true("runtime_native_3d_prepare_volume_caps_valid",
                    frame.scene.capabilities.valid);
        assert_true("runtime_native_3d_prepare_volume_caps_no_extinction",
                    !frame.scene.capabilities.hasLightingExtinctionVolume);
        assert_true("runtime_native_3d_prepare_volume_caps_skip_scatter",
                    frame.scene.capabilities.canSkipVolumeScatter);
        assert_true("runtime_native_3d_prepare_volume_debug_true",
                    frame.scene.volume.debugOverlayEnabled);
        assert_true("runtime_native_3d_prepare_volume_dims_d",
                    frame.scene.volume.grid.gridD == 2u);
        assert_true("runtime_native_3d_prepare_volume_density_value",
                    frame.scene.volume.channels.density &&
                        frame.scene.volume.channels.density[7] == 0.85f);
        ok = RuntimeVolumeDebugSummary3D_Build(&frame.scene.volume, &volume_summary);
        assert_true("runtime_native_3d_prepare_volume_summary_ok", ok);
        assert_true("runtime_native_3d_prepare_volume_summary_layout_valid",
                    volume_summary.layoutValid);
        assert_true("runtime_native_3d_prepare_volume_summary_density_range",
                    volume_summary.hasDensityRange);
        assert_true("runtime_native_3d_prepare_volume_summary_nonzero_density_count",
                    volume_summary.densityNonZeroCellCount == 8u);
        assert_close("runtime_native_3d_prepare_volume_summary_bounds_min_x",
                     volume_summary.boundsMin.x, -0.5, 1e-9);
        assert_close("runtime_native_3d_prepare_volume_summary_bounds_max_z",
                     volume_summary.boundsMax.z, 1.0, 1e-9);
        assert_close("runtime_native_3d_prepare_volume_summary_density_max",
                     volume_summary.densityMax, 0.85, 1e-6);
    }

    RuntimeNative3DPreparedFrame_Free(&frame);
    unlink(manifest_path);
    unlink(vf3d_path);
    rmdir(dir);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_runtime_native_3d_primary_volume_transmittance_darks_surface(void) {
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame frame = {0};
    float baseline_radiance[41 * 41 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    float attenuated_radiance[41 * 41 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    RuntimeNative3DRenderStats stats = {0};
    size_t center_idx = 0u;
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    scene.scope.triangleMeshEnabled = true;
    scene.hasLight = true;
    scene.light.position = vec3(0.0, -2.0, 2.0);
    scene.light.radius = 0.1;
    scene.light.intensity = 12.0;
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
    assert_true("runtime_native_3d_volume_surface_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_native_3d_volume_surface_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 3;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "center_plane");
    scene.triangleMesh.triangles[0].p0 = vec3(-2.0, -5.0, -2.0);
    scene.triangleMesh.triangles[0].p1 = vec3(2.0, -5.0, -2.0);
    scene.triangleMesh.triangles[0].p2 = vec3(0.0, -5.0, 2.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 3;
    ok = RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh);
    assert_true("runtime_native_3d_volume_surface_bvh_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 41, 41, &frame.projector);
    assert_true("runtime_native_3d_volume_surface_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    frame.scene = scene;
    frame.width = 41;
    frame.height = 41;
    frame.valid = true;
    memset(baseline_radiance, 0, sizeof(baseline_radiance));
    /* This shading-only fixture constructs no prepared TLAS. */
    RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(baseline_radiance,
                                                        41,
                                                        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                                        &frame,
                                                        0,
                                                        0,
                                                        41,
                                                        41,
                                                        &stats);
    assert_true("runtime_native_3d_volume_surface_baseline_ok", ok);

    ok = prepared_suite_attach_dense_volume(&frame.scene.volume,
                                            vec3(-0.5, -4.0, -0.5),
                                            2u,
                                            8u,
                                            2u,
                                            0.5,
                                            1.0f);
    assert_true("runtime_native_3d_volume_surface_attach_ok", ok);
    memset(attenuated_radiance, 0, sizeof(attenuated_radiance));
    memset(&stats, 0, sizeof(stats));
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(attenuated_radiance,
                                                        41,
                                                        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                                        &frame,
                                                        0,
                                                        0,
                                                        41,
                                                        41,
                                                        &stats);
    RuntimeRay3D_ResetTraceRouteForTests();
    assert_true("runtime_native_3d_volume_surface_attenuated_ok", ok);

    center_idx = (((size_t)20u * 41u) + 20u) * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    assert_true("runtime_native_3d_volume_surface_center_positive",
                baseline_radiance[center_idx] > 0.0f);
    assert_true("runtime_native_3d_volume_surface_center_darker",
                attenuated_radiance[center_idx] < baseline_radiance[center_idx]);

    RuntimeNative3DPreparedFrame_Free(&frame);
    return 0;
}

static int test_runtime_native_3d_primary_volume_transmittance_dims_visible_emitter(void) {
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame frame = {0};
    float baseline_radiance[51 * 51 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    float attenuated_radiance[51 * 51 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    RuntimeNative3DRenderStats stats = {0};
    size_t center_idx = 0u;
    bool ok = false;

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
    assert_true("runtime_native_3d_volume_emitter_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_native_3d_volume_emitter_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
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
    assert_true("runtime_native_3d_volume_emitter_projector_ok", ok);
    if (!ok) {
        RuntimeScene3D_Free(&scene);
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
    assert_true("runtime_native_3d_volume_emitter_baseline_ok", ok);

    ok = prepared_suite_attach_dense_volume(&frame.scene.volume,
                                            vec3(-1.5, -6.0, -1.5),
                                            6u,
                                            12u,
                                            6u,
                                            0.5,
                                            1.0f);
    assert_true("runtime_native_3d_volume_emitter_attach_ok", ok);
    memset(attenuated_radiance, 0, sizeof(attenuated_radiance));
    memset(&stats, 0, sizeof(stats));
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(attenuated_radiance,
                                                        51,
                                                        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                                        &frame,
                                                        0,
                                                        0,
                                                        51,
                                                        51,
                                                        &stats);
    assert_true("runtime_native_3d_volume_emitter_attenuated_ok", ok);

    center_idx = (((size_t)25u * 51u) + 25u) * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    assert_true("runtime_native_3d_volume_emitter_center_positive",
                baseline_radiance[center_idx] > 0.0f);
    assert_true("runtime_native_3d_volume_emitter_center_darker",
                attenuated_radiance[center_idx] < baseline_radiance[center_idx]);

    RuntimeNative3DPreparedFrame_Free(&frame);
    return 0;
}

static int test_runtime_native_3d_background_volume_transmittance_dims_environment_floor(void) {
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    RuntimeNative3DPreparedFrame frame = {0};
    float baseline_radiance[51 * 51 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    float attenuated_radiance[51 * 51 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
    uint8_t baseline_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    uint8_t attenuated_pixels[51 * 51 * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES];
    RuntimeNative3DRenderStats stats = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    animSettings.environmentBrightness = 128.0;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_AMBIENT;
    animSettings.environmentBackgroundLightingAuthored = false;
    RuntimeEnvironment3D_ResolveFromAnimationConfig(&scene.environment, &animSettings);
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
    assert_true("runtime_native_3d_volume_background_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_native_3d_volume_background_alloc_triangles",
                scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        animSettings = saved_anim;
        return 0;
    }

    scene.primitiveCount = 1;
    scene.triangleMesh.triangleCount = 1;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 8;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "offscreen_plane");
    scene.triangleMesh.triangles[0].p0 = vec3(8.0, -5.0, -1.0);
    scene.triangleMesh.triangles[0].p1 = vec3(10.0, -5.0, -1.0);
    scene.triangleMesh.triangles[0].p2 = vec3(8.0, -5.0, 1.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 8;

    ok = RuntimeCameraProjector3D_Build(&scene.camera, 51, 51, &frame.projector);
    assert_true("runtime_native_3d_volume_background_projector_ok", ok);
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
    assert_true("runtime_native_3d_volume_background_baseline_ok", ok);

    ok = prepared_suite_attach_dense_volume(&frame.scene.volume,
                                            vec3(-0.5, -4.5, -0.5),
                                            2u,
                                            9u,
                                            2u,
                                            0.5,
                                            1.0f);
    assert_true("runtime_native_3d_volume_background_attach_ok", ok);
    memset(attenuated_radiance, 0, sizeof(attenuated_radiance));
    memset(&stats, 0, sizeof(stats));
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(attenuated_radiance,
                                                        51,
                                                        RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT,
                                                        &frame,
                                                        0,
                                                        0,
                                                        51,
                                                        51,
                                                        &stats);
    assert_true("runtime_native_3d_volume_background_attenuated_ok", ok);

    memset(baseline_pixels, 0, sizeof(baseline_pixels));
    memset(attenuated_pixels, 0, sizeof(attenuated_pixels));
    RuntimeNative3DResolveRadianceRegionToPixels(baseline_pixels,
                                                 51,
                                                 baseline_radiance,
                                                 51,
                                                 0,
                                                 0,
                                                 51,
                                                 51);
    RuntimeNative3DResolveRadianceRegionToPixels(attenuated_pixels,
                                                 51,
                                                 attenuated_radiance,
                                                 51,
                                                 0,
                                                 0,
                                                 51,
                                                 51);

    assert_true("runtime_native_3d_volume_background_baseline_pixel_positive",
                native3d_test_pixel_r(baseline_pixels, 51, 25, 25) > 0);
    assert_true("runtime_native_3d_volume_background_baseline_sky_tinted",
                native3d_test_pixel_b(baseline_pixels, 51, 25, 25) >=
                    native3d_test_pixel_r(baseline_pixels, 51, 25, 25));
    assert_true("runtime_native_3d_volume_background_pixel_dimmer",
                native3d_test_pixel_r(attenuated_pixels, 51, 25, 25) <
                    native3d_test_pixel_r(baseline_pixels, 51, 25, 25));
    assert_true("runtime_native_3d_volume_background_pixel_positive",
                native3d_test_pixel_r(attenuated_pixels, 51, 25, 25) > 0);

    RuntimeNative3DPreparedFrame_Free(&frame);
    animSettings = saved_anim;
    return 0;
}

int run_test_runtime_native_3d_render_prepared_parity_volume_suite(void) {
    int before = test_support_failures();

    test_runtime_native_3d_background_fill_uses_resolved_rgb();
    test_runtime_native_3d_render_prepared_region_parity();
    test_runtime_native_3d_render_visible_emitter_tile_parity();
    test_runtime_native_3d_tile_occupancy_contract();
    test_runtime_native_3d_prepare_frame_attaches_configured_volume();
    test_runtime_native_3d_primary_volume_transmittance_darks_surface();
    test_runtime_native_3d_primary_volume_transmittance_dims_visible_emitter();
    test_runtime_native_3d_background_volume_transmittance_dims_environment_floor();
    return test_support_failures() - before;
}
