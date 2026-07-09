#include <math.h>
#include <string.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_scene_3d.h"
#include "test_runtime_mode_backend_policy.h"
#include "test_support.h"


static int test_mode_backend_route_2d_defaults(void) {
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_2D;
    animSettings.integratorMode = 1;
    animSettings.useTiledRenderer = true;
    animSettings.tileSize = 12;
    animSettings.tilePreviewEnabled = true;

    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();

    assert_true("route2d_family_canonical",
                route.routeFamily == RAY_TRACING_ROUTE_CANONICAL_2D);
    assert_true("route2d_is_canonical_helper",
                RayTracingModeBackend_IsCanonical2D(&route));
    assert_true("route2d_not_compat_helper",
                !RayTracingModeBackend_IsCompat3DFallback(&route));
    assert_true("route2d_not_native_helper",
                !RayTracingModeBackend_IsNative3D(&route));
    assert_true("route2d_lane_canonical", route.backendLane == RAY_TRACING_BACKEND_CANONICAL_2D);
    assert_true("route2d_no_fallback", !route.fallbackTo2DProjection);
    assert_true("route2d_projection_2d", route.projectionMode == SPACE_MODE_2D);
    assert_true("route2d_no_runtime_3d_scaffold", !route.usesRuntime3DScaffold);
    assert_close("route2d_runtime_camera_z_zero", route.runtimeCameraZ, 0.0, 1e-9);
    assert_close("route2d_ray_origin_y_offset_zero", route.rayOriginYOffset, 0.0, 1e-9);
    assert_true("route2d_tiles_enabled", route.useTiles);
    assert_true("route2d_tile_preview_enabled", route.tilePreviewEnabled);
    assert_true("route2d_cache_enabled", route.buildIrradianceCache);
    assert_true("route2d_not_3d_catalog", !route.integratorUses3DCatalog);
    assert_true("route2d_status_label_hybrid",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route), "Hybrid") != NULL);
    return 0;
}


static int test_mode_backend_route_3d_controlled_lane(void) {
    const char *runtime_json_route_3d =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_box\","
            "\"object_type\":\"box\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_plane\","
            "\"object_type\":\"plane\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_mesh\","
            "\"object_type\":\"triangle_mesh\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":20.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    animSettings.useTiledRenderer = true;
    animSettings.tileSize = 16;
    animSettings.tilePreviewEnabled = true;
    assert_true("route3d_seed_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d, &summary));

    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();

    assert_true("route3d_family_compat_fallback",
                route.routeFamily == RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK);
    assert_true("route3d_not_canonical_helper",
                !RayTracingModeBackend_IsCanonical2D(&route));
    assert_true("route3d_compat_helper",
                RayTracingModeBackend_IsCompat3DFallback(&route));
    assert_true("route3d_not_native_helper",
                !RayTracingModeBackend_IsNative3D(&route));
    assert_true("route3d_lane_controlled", route.backendLane == RAY_TRACING_BACKEND_CONTROLLED_3D);
    assert_true("route3d_fallback_projection", route.fallbackTo2DProjection);
    assert_true("route3d_projection_mode_2d", route.projectionMode == SPACE_MODE_2D);
    assert_true("route3d_runtime_scaffold_enabled", route.usesRuntime3DScaffold);
    assert_close("route3d_runtime_camera_z", route.runtimeCameraZ, 20.0, 1e-9);
    assert_true("route3d_ray_origin_y_offset_nonzero", fabs(route.rayOriginYOffset) > 0.0);
    assert_true("route3d_scaffold_primitive_count", route.scaffoldPrimitiveCount == 3);
    assert_true("route3d_compat_forces_direct_light_legacy_mode",
                route.integratorMode == RAY_TRACING_2D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("route3d_compat_forces_direct_light_3d_mode",
                route.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("route3d_compat_uses_3d_catalog", route.integratorUses3DCatalog);
    assert_true("route3d_compat_cache_off", !route.buildIrradianceCache);
    assert_true("route3d_tile_preview_off", !route.tilePreviewEnabled);
    assert_true("route3d_compat_status_label",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route), "compat 3D Direct Light") != NULL);
    return 0;
}


static int test_mode_backend_route_3d_native_lane(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json_route_3d_native =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d_native\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"block\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-3.0,\"z\":0.5},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":1.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":-1.5,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RayTracingRuntimeRoute route;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    animSettings.useTiledRenderer = true;
    animSettings.tileSize = 16;
    animSettings.tilePreviewEnabled = true;
    assert_true("route3d_native_seed_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d_native, &summary));

    route = RayTracingModeBackend_ResolveRoute();

    assert_true("route3d_native_family_native",
                route.routeFamily == RAY_TRACING_ROUTE_NATIVE_3D);
    assert_true("route3d_native_not_canonical_helper",
                !RayTracingModeBackend_IsCanonical2D(&route));
    assert_true("route3d_native_not_compat_helper",
                !RayTracingModeBackend_IsCompat3DFallback(&route));
    assert_true("route3d_native_helper",
                RayTracingModeBackend_IsNative3D(&route));
    assert_true("route3d_native_controlled_helper",
                RayTracingModeBackend_IsControlled3D(&route));
    assert_true("route3d_native_lane_controlled",
                route.backendLane == RAY_TRACING_BACKEND_CONTROLLED_3D);
    assert_true("route3d_native_no_fallback_projection",
                !route.fallbackTo2DProjection);
    assert_true("route3d_native_projection_mode_3d",
                route.projectionMode == SPACE_MODE_3D);
    assert_true("route3d_native_runtime_scaffold_enabled",
                route.usesRuntime3DScaffold);
    assert_close("route3d_native_runtime_camera_z", route.runtimeCameraZ, 8.0, 1e-9);
    assert_close("route3d_native_ray_origin_y_offset_zero", route.rayOriginYOffset, 0.0, 1e-9);
    assert_true("route3d_native_scaffold_primitive_count", route.scaffoldPrimitiveCount == 2);
    assert_true("route3d_native_legacy_mode_direct_light",
                route.integratorMode == RAY_TRACING_2D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("route3d_native_3d_mode_disney",
                route.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DISNEY);
    assert_true("route3d_native_uses_3d_catalog", route.integratorUses3DCatalog);
    assert_true("route3d_native_cache_off", !route.buildIrradianceCache);
    assert_true("route3d_native_tiles_enabled", route.useTiles);
    assert_true("route3d_native_tile_preview_on", route.tilePreviewEnabled);
    assert_true("route3d_native_status_label_disney",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route), "3D Disney") != NULL);

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
    route = RayTracingModeBackend_ResolveRoute();
    assert_true("route3d_native_3d_mode_diffuse_bounce",
                route.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE);
    assert_true("route3d_native_status_label_diffuse_bounce",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route),
                       "3D Diffuse Bounce") != NULL);

    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
    route = RayTracingModeBackend_ResolveRoute();
    assert_true("route3d_native_3d_mode_emission_transparency",
                route.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY);
    assert_true("route3d_native_status_label_emission_transparency",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route),
                       "3D Emission / Transparency") != NULL);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}

static int test_mode_backend_route_3d_native_lane_with_authoring_helpers(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json_route_3d_native_helpers =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d_native_helpers\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane_primitive\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"block\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-3.0,\"z\":0.5},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":1.0}"
          "},"
          "{"
            "\"object_id\":\"author_points\","
            "\"object_type\":\"point_set\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"author_curve\","
            "\"object_type\":\"curve_path\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"author_edges\","
            "\"object_type\":\"edge_set\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":-1.5,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RayTracingRuntimeRoute route;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    animSettings.useTiledRenderer = true;
    animSettings.tileSize = 16;
    animSettings.tilePreviewEnabled = true;
    assert_true("route3d_native_helpers_seed_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d_native_helpers, &summary));

    route = RayTracingModeBackend_ResolveRoute();

    assert_true("route3d_native_helpers_family_native",
                route.routeFamily == RAY_TRACING_ROUTE_NATIVE_3D);
    assert_true("route3d_native_helpers_helper",
                RayTracingModeBackend_IsNative3D(&route));
    assert_true("route3d_native_helpers_no_fallback_projection",
                !route.fallbackTo2DProjection);
    assert_true("route3d_native_helpers_projection_mode_3d",
                route.projectionMode == SPACE_MODE_3D);
    assert_true("route3d_native_helpers_scaffold_count_two",
                route.scaffoldPrimitiveCount == 2);
    assert_true("route3d_native_helpers_status_label_disney",
                strstr(RayTracingModeBackend_IntegratorStatusLabel(&route), "3D Disney") != NULL);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}


static int test_mode_backend_scene_digest_status_2d_canonical_empty(void) {
    RayTracingRuntimeRoute route;
    RayTracingSceneDigestStatus status;
    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_2D;

    route = RayTracingModeBackend_ResolveRoute();
    status = RayTracingModeBackend_BuildSceneDigestStatus(&route);

    assert_true("digest2d_route_canonical",
                route.routeFamily == RAY_TRACING_ROUTE_CANONICAL_2D);
    assert_true("digest2d_status_invalid", !status.valid);
    assert_true("digest2d_primitive_count_zero", status.digestPrimitiveCount == 0);
    assert_true("digest2d_scaffold_count_zero", status.scaffoldPrimitiveCount == 0);
    return 0;
}


static int test_mode_backend_scene_digest_status_ps4d_fixture(void) {
    RuntimeSceneBridgePreflight summary;
    RayTracingRuntimeRoute route;
    RayTracingSceneDigestStatus status;

    memset(&animSettings, 0, sizeof(animSettings));
    assert_true("digestps4d_apply_file_ok",
                runtime_scene_bridge_apply_file("../physics_sim/config/samples/ps4d_runtime_scene_visual_test.json",
                                                &summary));

    route = RayTracingModeBackend_ResolveRoute();
    status = RayTracingModeBackend_BuildSceneDigestStatus(&route);

    assert_true("digestps4d_route_native",
                route.routeFamily == RAY_TRACING_ROUTE_NATIVE_3D);
    assert_true("digestps4d_status_valid", status.valid);
    assert_true("digestps4d_bounds_present", status.hasSceneBounds);
    assert_true("digestps4d_bounds_enabled", status.boundsEnabled);
    assert_true("digestps4d_bounds_clamp", status.boundsClampOnEdit);
    assert_close("digestps4d_bounds_min_x", status.boundsMinX, -6.0, 1e-9);
    assert_close("digestps4d_bounds_min_y", status.boundsMinY, -5.0, 1e-9);
    assert_close("digestps4d_bounds_min_z", status.boundsMinZ, -2.5, 1e-9);
    assert_close("digestps4d_bounds_max_x", status.boundsMaxX, 6.0, 1e-9);
    assert_close("digestps4d_bounds_max_y", status.boundsMaxY, 5.0, 1e-9);
    assert_close("digestps4d_bounds_max_z", status.boundsMaxZ, 4.0, 1e-9);
    assert_true("digestps4d_plane_present", status.hasConstructionPlane);
    assert_true("digestps4d_plane_mode_axis_aligned",
                strcmp(status.constructionPlaneMode, "axis_aligned") == 0);
    assert_true("digestps4d_plane_axis_xy",
                strcmp(status.constructionPlaneAxis, "xy") == 0);
    assert_close("digestps4d_plane_offset",
                 status.constructionPlaneOffset,
                 -1.0,
                 1e-9);
    assert_true("digestps4d_primitive_count_three", status.digestPrimitiveCount == 3);
    assert_true("digestps4d_plane_count_one", status.planePrimitiveCount == 1);
    assert_true("digestps4d_prism_count_two", status.rectPrismPrimitiveCount == 2);
    assert_true("digestps4d_scaffold_count_three", status.scaffoldPrimitiveCount == 3);
    return 0;
}


static int test_mode_backend_scene_digest_status_3d_native_fixture(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json_route_3d_native =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d_native_digest\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"block\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-3.0,\"z\":0.5},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":1.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":-1.5,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RayTracingRuntimeRoute route;
    RayTracingSceneDigestStatus status;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    assert_true("digestnative_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d_native, &summary));

    route = RayTracingModeBackend_ResolveRoute();
    status = RayTracingModeBackend_BuildSceneDigestStatus(&route);

    assert_true("digestnative_route_native",
                route.routeFamily == RAY_TRACING_ROUTE_NATIVE_3D);
    assert_true("digestnative_status_valid", status.valid);
    assert_true("digestnative_scaffold_count_two", status.scaffoldPrimitiveCount == 2);
    assert_true("digestnative_primitive_count_two", status.digestPrimitiveCount == 2);
    assert_true("digestnative_plane_count_one", status.planePrimitiveCount == 1);
    assert_true("digestnative_prism_count_one", status.rectPrismPrimitiveCount == 1);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}


static int test_mode_backend_view_carrier_2d_defaults(void) {
    Camera camera = {0};
    RayTracingRuntimeRoute route;
    RayTracingViewCarrier carrier;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_2D;
    camera.x = 10.0;
    camera.y = -6.0;
    camera.zoom = 1.5;

    route = RayTracingModeBackend_ResolveRoute();
    carrier = RayTracingModeBackend_BuildViewCarrier(&camera, 320, 200, &route);

    assert_true("carrier2d_family_canonical",
                carrier.family == RAY_TRACING_VIEW_CARRIER_CANONICAL_2D);
    assert_true("carrier2d_projection_mode_2d",
                carrier.viewContext.mode == SPACE_MODE_2D);
    assert_close("carrier2d_camera_x", carrier.cameraXY.x, 10.0, 1e-9);
    assert_close("carrier2d_camera_y", carrier.cameraXY.y, -6.0, 1e-9);
    assert_close("carrier2d_camera_z", carrier.cameraZ, 0.0, 1e-9);
    assert_close("carrier2d_origin_x", carrier.originX, 10.0, 1e-9);
    assert_close("carrier2d_origin_y", carrier.originY, -6.0, 1e-9);
    assert_close("carrier2d_origin_z", carrier.originZ, 0.0, 1e-9);
    assert_true("carrier2d_not_compat_fallback", !carrier.usesCompatProjectionFallback);
    return 0;
}


static int test_mode_backend_view_carrier_3d_compat_fallback(void) {
    const char *runtime_json_route_3d =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d_carrier\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":2.0,\"y\":3.0,\"z\":12.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    Camera camera = {0};
    RayTracingRuntimeRoute route;
    RayTracingViewCarrier carrier;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    assert_true("carrier3d_seed_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d, &summary));

    camera.x = 2.0;
    camera.y = 3.0;
    camera.zoom = 1.0;

    route = RayTracingModeBackend_ResolveRoute();
    carrier = RayTracingModeBackend_BuildViewCarrier(&camera, 640, 480, &route);

    assert_true("carrier3d_route_is_compat_fallback",
                route.routeFamily == RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK);
    assert_true("carrier3d_family_compat",
                carrier.family == RAY_TRACING_VIEW_CARRIER_COMPAT_3D);
    assert_true("carrier3d_projection_matches_route",
                carrier.viewContext.mode == route.projectionMode);
    assert_true("carrier3d_compat_fallback", carrier.usesCompatProjectionFallback);
    assert_true("carrier3d_has_scaffold_camera", carrier.hasRuntimeScaffoldCamera);
    assert_close("carrier3d_camera_x", carrier.cameraXY.x, 2.0, 1e-9);
    assert_close("carrier3d_camera_y", carrier.cameraXY.y, 3.0, 1e-9);
    assert_close("carrier3d_camera_z", carrier.cameraZ, 12.0, 1e-9);
    assert_close("carrier3d_origin_x", carrier.originX, 2.0, 1e-9);
    assert_true("carrier3d_origin_y_offset_nonzero", fabs(carrier.originY - 3.0) > 0.0);
    assert_close("carrier3d_origin_z", carrier.originZ, 12.0, 1e-9);
    return 0;
}


static int test_mode_backend_primitive_prep_plan_2d_defaults(void) {
    RayTracingRuntimeRoute route;
    RayTracingPrimitivePrepPlan plan;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_2D;

    route = RayTracingModeBackend_ResolveRoute();
    plan = RayTracingModeBackend_BuildPrimitivePrepPlan(&route, 4);

    assert_true("prep2d_family_canonical",
                plan.family == RAY_TRACING_PRIMITIVE_PREP_CANONICAL_2D);
    assert_true("prep2d_uses_scene_objects", plan.usesLegacySceneObjects);
    assert_true("prep2d_not_compat_placeholders", !plan.usesCompatPlaceholderObjects);
    assert_true("prep2d_no_runtime_scaffold", !plan.hasRuntimeScaffoldPrimitives);
    assert_true("prep2d_scaffold_count_zero", plan.scaffoldPrimitiveCount == 0);
    assert_true("prep2d_mesh_enabled", plan.enableSurfaceMeshPrep);
    assert_true("prep2d_triangles_enabled", plan.enableTriangleMeshPrep);
    assert_true("prep2d_uniform_grid_enabled", plan.enableUniformGrid2D);
    assert_true("prep2d_ray2d_enabled", plan.enableRay2DIntersections);
    return 0;
}


static int test_mode_backend_primitive_prep_plan_3d_compat_placeholder(void) {
    const char *runtime_json_route_3d =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_route_3d_prep\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_box\","
            "\"object_type\":\"box\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_plane\","
            "\"object_type\":\"plane\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_mesh\","
            "\"object_type\":\"triangle_mesh\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":20.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    RayTracingRuntimeRoute route;
    RayTracingPrimitivePrepPlan plan;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    assert_true("prep3d_seed_runtime_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_route_3d, &summary));

    route = RayTracingModeBackend_ResolveRoute();
    plan = RayTracingModeBackend_BuildPrimitivePrepPlan(&route, sceneSettings.objectCount);

    assert_true("prep3d_family_compat_placeholder",
                plan.family == RAY_TRACING_PRIMITIVE_PREP_COMPAT_3D_PLACEHOLDER);
    assert_true("prep3d_uses_scene_objects", plan.usesLegacySceneObjects);
    assert_true("prep3d_uses_compat_placeholders", plan.usesCompatPlaceholderObjects);
    assert_true("prep3d_has_runtime_scaffold_primitives", plan.hasRuntimeScaffoldPrimitives);
    assert_true("prep3d_scaffold_count_three", plan.scaffoldPrimitiveCount == 3);
    assert_true("prep3d_mesh_enabled", plan.enableSurfaceMeshPrep);
    assert_true("prep3d_triangles_enabled", plan.enableTriangleMeshPrep);
    assert_true("prep3d_uniform_grid_enabled", plan.enableUniformGrid2D);
    assert_true("prep3d_ray2d_enabled", plan.enableRay2DIntersections);
    return 0;
}


static int test_mode_backend_primitive_prep_plan_native3d_placeholder_contract(void) {
    RayTracingRuntimeRoute route;
    RayTracingPrimitivePrepPlan plan;

    memset(&route, 0, sizeof(route));
    route.routeFamily = RAY_TRACING_ROUTE_NATIVE_3D;
    route.usesRuntime3DScaffold = true;
    route.scaffoldPrimitiveCount = 5;
    plan = RayTracingModeBackend_BuildPrimitivePrepPlan(&route, 7);

    assert_true("prepnative_family_native",
                plan.family == RAY_TRACING_PRIMITIVE_PREP_NATIVE_3D);
    assert_true("prepnative_not_legacy_scene_objects", !plan.usesLegacySceneObjects);
    assert_true("prepnative_not_compat_placeholders", !plan.usesCompatPlaceholderObjects);
    assert_true("prepnative_has_runtime_scaffold", plan.hasRuntimeScaffoldPrimitives);
    assert_true("prepnative_scaffold_count", plan.scaffoldPrimitiveCount == 5);
    assert_true("prepnative_mesh_disabled", !plan.enableSurfaceMeshPrep);
    assert_true("prepnative_triangles_disabled", !plan.enableTriangleMeshPrep);
    assert_true("prepnative_uniform_grid_disabled", !plan.enableUniformGrid2D);
    assert_true("prepnative_ray2d_disabled", !plan.enableRay2DIntersections);
    return 0;
}


static int test_runtime_scene_3d_r0_scope_contract_defaults(void) {
    RuntimeScene3D scene;

    RuntimeScene3D_Init(&scene);

    assert_true("runtime_scene_3d_scope_plane_enabled", scene.scope.planeEnabled);
    assert_true("runtime_scene_3d_scope_rect_prism_enabled", scene.scope.rectPrismEnabled);
    assert_true("runtime_scene_3d_scope_triangle_mesh_disabled", !scene.scope.triangleMeshEnabled);
    assert_true("runtime_scene_3d_kind_plane_supported",
                RuntimePrimitive3DKindSupportedByR0(RUNTIME_PRIMITIVE_3D_KIND_PLANE));
    assert_true("runtime_scene_3d_kind_rect_prism_supported",
                RuntimePrimitive3DKindSupportedByR0(RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM));
    assert_true("runtime_scene_3d_kind_triangle_mesh_not_supported",
                !RuntimePrimitive3DKindSupportedByR0(RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH));
    assert_true("runtime_scene_3d_kind_label_plane",
                strcmp(RuntimePrimitive3DKindLabel(RUNTIME_PRIMITIVE_3D_KIND_PLANE), "plane") == 0);

    RuntimeScene3D_Free(&scene);
    return 0;
}


static int test_runtime_scene_3d_r0_ownership_contract_defaults(void) {
    RuntimeScene3D scene;

    RuntimeScene3D_Init(&scene);

    assert_true("runtime_scene_3d_ownership_renderer_truth",
                scene.ownership.rendererOwnsGeometryTruth);
    assert_true("runtime_scene_3d_ownership_scene_objects_compat_only",
                scene.ownership.sceneObjectsRemainCompatOnly);
    assert_true("runtime_scene_3d_ownership_preview_digest_non_authoritative",
                scene.ownership.previewDigestIsNonAuthoritative);
    assert_true("runtime_scene_3d_camera_default_zoom",
                fabs(scene.camera.zoom - 1.0) < 1e-9);
    assert_true("runtime_scene_3d_camera_default_near_plane_positive",
                scene.camera.nearPlane > 0.0);

    RuntimeScene3D_Free(&scene);
    return 0;
}


int run_test_runtime_mode_backend_policy_tests(void) {
    int before = test_support_failures();

    test_mode_backend_route_2d_defaults();
    test_mode_backend_route_3d_controlled_lane();
    test_mode_backend_route_3d_native_lane();
    test_mode_backend_route_3d_native_lane_with_authoring_helpers();
    test_mode_backend_scene_digest_status_2d_canonical_empty();
    test_mode_backend_scene_digest_status_ps4d_fixture();
    test_mode_backend_scene_digest_status_3d_native_fixture();
    test_mode_backend_view_carrier_2d_defaults();
    test_mode_backend_view_carrier_3d_compat_fallback();
    test_mode_backend_primitive_prep_plan_2d_defaults();
    test_mode_backend_primitive_prep_plan_3d_compat_placeholder();
    test_mode_backend_primitive_prep_plan_native3d_placeholder_contract();
    test_runtime_scene_3d_r0_scope_contract_defaults();
    test_runtime_scene_3d_r0_ownership_contract_defaults();
    return test_support_failures() - before;
}
