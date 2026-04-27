#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "render/integrators/integrator_common.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_scene_3d_builder.h"
#include "test_runtime_diffuse_temporal.h"
#include "test_support.h"

static int test_runtime_diffuse_bounce_3d_shadowed_hit_lift_contract(void) {
    AnimationConfig saved_anim = animSettings;
    RuntimeScene3D scene;
    HitInfo3D hit = {0};
    RuntimeDirectLight3DResult direct_result = {0};
    RuntimeDiffuseBounce3DResult diffuse_result = {0};
    bool ok = false;

    RuntimeScene3D_Init(&scene);
    scene.hasLight = true;
    animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    scene.light.position = vec3(2.0, -2.0, 0.0);
    scene.light.intensity = 10.0;
    scene.light.falloffDistance = 10.0;
    scene.light.falloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    scene.primitiveCapacity = 2;
    scene.triangleMesh.triangleCapacity = 2;
    scene.primitives = (RuntimePrimitive3D*)calloc((size_t)scene.primitiveCapacity,
                                                   sizeof(*scene.primitives));
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene.triangleMesh.triangleCapacity,
                                   sizeof(*scene.triangleMesh.triangles));
    assert_true("runtime_diffuse_bounce_shadowed_alloc_primitives", scene.primitives != NULL);
    assert_true("runtime_diffuse_bounce_shadowed_alloc_triangles", scene.triangleMesh.triangles != NULL);
    if (!scene.primitives || !scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return 0;
    }

    scene.primitiveCount = 2;
    scene.triangleMesh.triangleCount = 2;
    scene.primitives[0].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[0].source.sceneObjectIndex = 7;
    snprintf(scene.primitives[0].source.objectId,
             sizeof(scene.primitives[0].source.objectId),
             "%s",
             "floor");
    scene.primitives[1].source.kind = RUNTIME_PRIMITIVE_3D_KIND_PLANE;
    scene.primitives[1].source.sceneObjectIndex = 8;
    snprintf(scene.primitives[1].source.objectId,
             sizeof(scene.primitives[1].source.objectId),
             "%s",
             "bounce_card");

    scene.triangleMesh.triangles[0].p0 = vec3(-3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].p1 = vec3(-3.0, -5.0, 3.0);
    scene.triangleMesh.triangles[0].p2 = vec3(3.0, -5.0, -3.0);
    scene.triangleMesh.triangles[0].normal = vec3(0.0, 1.0, 0.0);
    scene.triangleMesh.triangles[0].primitiveIndex = 0;
    scene.triangleMesh.triangles[0].sceneObjectIndex = 7;

    scene.triangleMesh.triangles[1].p0 = vec3(0.75, -4.8, -1.0);
    scene.triangleMesh.triangles[1].p1 = vec3(0.75, -3.1, 0.0);
    scene.triangleMesh.triangles[1].p2 = vec3(0.75, -4.8, 1.0);
    scene.triangleMesh.triangles[1].normal = vec3(1.0, 0.0, 0.0);
    scene.triangleMesh.triangles[1].primitiveIndex = 1;
    scene.triangleMesh.triangles[1].sceneObjectIndex = 8;

    hit.t = 5.0;
    hit.position = vec3(0.0, -5.0, 0.0);
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.triangleIndex = 0;
    hit.primitiveIndex = 0;
    hit.sceneObjectIndex = 7;
    hit.source = scene.primitives[0].source;
    hit.baryU = 0.333333333333;
    hit.baryV = 0.333333333333;
    hit.baryW = 0.333333333334;

    ok = RuntimeDirectLight3D_ShadeHit(&scene, &hit, &direct_result);
    assert_true("runtime_diffuse_bounce_shadowed_direct_ok", ok);
    assert_true("runtime_diffuse_bounce_shadowed_direct_not_visible", !direct_result.visible);
    assert_close("runtime_diffuse_bounce_shadowed_direct_zero",
                 direct_result.radiance,
                 0.0,
                 1e-9);

    ok = RuntimeDiffuseBounce3D_ShadeHit(&scene, &hit, NULL, &diffuse_result);
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
    assert_true("runtime_diffuse_bounce_shadowed_total_positive",
                diffuse_result.radiance > 0.0);

    RuntimeScene3D_Free(&scene);
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
    uint8_t pixels_a[101 * 101];
    uint8_t pixels_a_repeat[101 * 101];
    uint8_t pixels_b[101 * 101];
    RuntimeNative3DRenderStats stats_a = {0};
    RuntimeNative3DRenderStats stats_a_repeat = {0};
    RuntimeNative3DRenderStats stats_b = {0};
    bool ok = false;
    animSettings.secondaryDiffuseSamples3D = 4;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.camera.zoom = 1.0;

    ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_diffuse_bounce_sequence_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }

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
    uint8_t pixels_single[101 * 101];
    uint8_t pixels_temporal[101 * 101];
    RuntimeNative3DRenderStats stats_single = {0};
    RuntimeNative3DRenderStats stats_temporal = {0};
    bool ok = false;

    animSettings.secondaryDiffuseSamples3D = 4;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
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
                stats_temporal.visiblePixelCount == (stats_single.visiblePixelCount * 4));
    assert_true("runtime_native_3d_temporal_bounded_grayscale",
                pixels_temporal[(50 * 101) + 50] <= 255);

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
    uint8_t full_pixels[101 * 101];
    uint8_t tiled_pixels[101 * 101];
    float* tile_luminance = NULL;
    bool ok = false;
    const int temporal_frames = 4;

    animSettings.secondaryDiffuseSamples3D = 4;
    animSettings.temporalFrames3D = temporal_frames;
    animSettings.lightIntensity = 10.0;
    animSettings.forwardDecay = 10.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_LINEAR;
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

    memset(tiled_pixels, 0, sizeof(tiled_pixels));
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
    tile_luminance = (float*)calloc((size_t)grid.tileSize * (size_t)grid.tileSize,
                                    sizeof(*tile_luminance));
    assert_true("runtime_native_3d_temporal_tile_buffer_ok", tile_luminance != NULL);
    if (!tile_luminance) {
        free(tile_luminance);
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

            memset(tile_luminance, 0, (size_t)tile->width * (size_t)tile->height * sizeof(*tile_luminance));
            subpass_frame.sampling.sampleSequence = sampling.sampleSequence + (uint32_t)subpass;
            ok = RuntimeNative3DRenderPreparedRegionLuminance(tile_luminance,
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
                                                                     tile_luminance,
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

    free(tile_luminance);
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
    animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
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
    test_runtime_native_3d_temporal_accumulation_contract();
    test_runtime_native_3d_temporal_tile_parity_contract();
    test_runtime_diffuse_bounce_3d_seed_branch_contract();

    return test_support_failures() - before;
}
