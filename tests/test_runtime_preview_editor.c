#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "app/preview_camera_projector.h"
#include "app/preview_camera_sample.h"
#include "app/preview_mode_route.h"
#include "app/preview_playback.h"
#include "app/preview_retained_scene_renderer.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/runtime_camera_3d_rays.h"
#include "test_runtime_preview_editor.h"
#include "test_support.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int test_runtime_camera_projector_3d_preview_projection_parity(void) {
    RuntimeCamera3D runtime_camera = {0};
    RuntimeCameraProjector3D runtime_projector = {0};
    PreviewCameraSample preview_sample = {0};
    PreviewCameraProjector preview_projector = {0};
    SDL_Rect viewport = {0, 0, 1000, 500};
    Vec3 world_point = vec3(0.0, -10.0, 0.0);
    double runtime_sx = 0.0;
    double runtime_sy = 0.0;
    double runtime_depth = 0.0;
    bool runtime_inside = false;
    double preview_sx = 0.0;
    double preview_sy = 0.0;
    double preview_depth = 0.0;
    bool preview_inside = false;
    bool ok = false;

    runtime_camera.position = vec3(0.0, 0.0, 0.0);
    runtime_camera.rotation = 0.0;
    runtime_camera.lookPitch = 0.0;
    runtime_camera.zoom = 1.0;
    runtime_camera.nearPlane = 0.1;

    preview_sample.valid = true;
    preview_sample.position_x = 0.0;
    preview_sample.position_y = 0.0;
    preview_sample.position_z = 0.0;
    preview_sample.yaw_radians = 0.0;
    preview_sample.pitch_radians = 0.0;
    preview_sample.fov_y_degrees = 55.0;
    preview_sample.aspect_ratio = 2.0;

    ok = RuntimeCameraProjector3D_Build(&runtime_camera, viewport.w, viewport.h, &runtime_projector);
    assert_true("runtime_camera_projector_3d_preview_parity_build_runtime", ok);
    assert_true("runtime_camera_projector_3d_preview_parity_build_preview",
                PreviewCameraProjectorBuild(&preview_sample, viewport, &preview_projector));
    if (!ok || !PreviewCameraProjectorBuild(&preview_sample, viewport, &preview_projector)) {
        return 0;
    }

    assert_true("runtime_camera_projector_3d_preview_parity_center",
                RuntimeCameraProjector3D_ProjectPoint(&runtime_projector,
                                                     world_point,
                                                     &runtime_sx,
                                                     &runtime_sy,
                                                     &runtime_depth,
                                                     &runtime_inside));
    assert_true("runtime_camera_projector_3d_preview_parity_center_preview",
                PreviewCameraProjectorProjectPoint(&preview_projector,
                                                   world_point.x,
                                                   world_point.y,
                                                   world_point.z,
                                                   &preview_sx,
                                                   &preview_sy,
                                                   &preview_depth,
                                                   &preview_inside));
    assert_close("runtime_camera_projector_3d_preview_parity_center_x",
                 runtime_sx,
                 preview_sx,
                 1e-6);
    assert_close("runtime_camera_projector_3d_preview_parity_center_y",
                 runtime_sy,
                 preview_sy,
                 1e-6);
    assert_close("runtime_camera_projector_3d_preview_parity_center_depth",
                 runtime_depth,
                 preview_depth,
                 1e-6);
    assert_true("runtime_camera_projector_3d_preview_parity_center_inside",
                runtime_inside == preview_inside);

    world_point = vec3(10.0, -10.0, 0.0);
    assert_true("runtime_camera_projector_3d_preview_parity_right",
                RuntimeCameraProjector3D_ProjectPoint(&runtime_projector,
                                                     world_point,
                                                     &runtime_sx,
                                                     &runtime_sy,
                                                     &runtime_depth,
                                                     &runtime_inside));
    assert_true("runtime_camera_projector_3d_preview_parity_right_preview",
                PreviewCameraProjectorProjectPoint(&preview_projector,
                                                   world_point.x,
                                                   world_point.y,
                                                   world_point.z,
                                                   &preview_sx,
                                                   &preview_sy,
                                                   &preview_depth,
                                                   &preview_inside));
    assert_close("runtime_camera_projector_3d_preview_parity_right_x",
                 runtime_sx,
                 preview_sx,
                 1e-6);
    assert_close("runtime_camera_projector_3d_preview_parity_right_y",
                 runtime_sy,
                 preview_sy,
                 1e-6);

    world_point = vec3(0.0, -10.0, 5.0);
    assert_true("runtime_camera_projector_3d_preview_parity_top",
                RuntimeCameraProjector3D_ProjectPoint(&runtime_projector,
                                                     world_point,
                                                     &runtime_sx,
                                                     &runtime_sy,
                                                     &runtime_depth,
                                                     &runtime_inside));
    assert_true("runtime_camera_projector_3d_preview_parity_top_preview",
                PreviewCameraProjectorProjectPoint(&preview_projector,
                                                   world_point.x,
                                                   world_point.y,
                                                   world_point.z,
                                                   &preview_sx,
                                                   &preview_sy,
                                                   &preview_depth,
                                                   &preview_inside));
    assert_close("runtime_camera_projector_3d_preview_parity_top_x",
                 runtime_sx,
                 preview_sx,
                 1e-6);
    assert_close("runtime_camera_projector_3d_preview_parity_top_y",
                 runtime_sy,
                 preview_sy,
                 1e-6);
    return 0;
}

static void test_preview_camera_sample_evaluate_contract(void) {
    Camera base_camera = {.x = 9.0, .y = -4.0, .zoom = 1.0, .rotation = 0.25};
    PreviewCameraSample sample = {0};
    Path camera_path = {0};
    CameraPath3D camera_path3d = {0};

    assert_true("preview_camera_sample_base",
                PreviewCameraSampleEvaluate(&base_camera,
                                            3.5,
                                            NULL,
                                            NULL,
                                            0.25,
                                            1600,
                                            900,
                                            &sample));
    assert_true("preview_camera_sample_base_valid", sample.valid);
    assert_true("preview_camera_sample_base_no_path", !sample.uses_authored_path);
    assert_close("preview_camera_sample_base_x", sample.position_x, 9.0, 1e-6);
    assert_close("preview_camera_sample_base_y", sample.position_y, -4.0, 1e-6);
    assert_close("preview_camera_sample_base_z", sample.position_z, 3.5, 1e-6);
    assert_close("preview_camera_sample_base_yaw", sample.yaw_radians, 0.25, 1e-6);
    assert_close("preview_camera_sample_base_pitch", sample.pitch_radians, 0.0, 1e-6);
    assert_close("preview_camera_sample_base_fov", sample.fov_y_degrees,
                 PREVIEW_CAMERA_SAMPLE_DEFAULT_FOV_Y_DEGREES, 1e-6);
    assert_close("preview_camera_sample_base_aspect", sample.aspect_ratio, 1600.0 / 900.0, 1e-6);

    camera_path.numPoints = 2;
    camera_path.mode = BEZIER_CUBIC;
    camera_path.points[0] = (Point){0.0, 0.0};
    camera_path.points[1] = (Point){10.0, 0.0};
    camera_path.rotations[0] = 0.0;
    camera_path.rotations[1] = M_PI / 2.0;
    camera_path3d.point_z[0] = 0.0;
    camera_path3d.point_z[1] = 10.0;
    camera_path3d.point_pitch[0] = 0.0;
    camera_path3d.point_pitch[1] = M_PI / 4.0;

    assert_true("preview_camera_sample_path",
                PreviewCameraSampleEvaluate(&base_camera,
                                            3.5,
                                            &camera_path,
                                            &camera_path3d,
                                            0.5,
                                            1200,
                                            800,
                                            &sample));
    assert_true("preview_camera_sample_path_authored", sample.uses_authored_path);
    {
        Point expected_point = GetPositionAlongPathNormalized(&camera_path, 0.5);
        double expected_yaw = GetRotationAlongPathNormalized(&camera_path, 0.5);
        double expected_z =
            CameraPath3D_GetPositionZNormalized(&camera_path, &camera_path3d, 0.5);
        assert_close("preview_camera_sample_path_x", sample.position_x, expected_point.x, 1e-6);
        assert_close("preview_camera_sample_path_y", sample.position_y, expected_point.y, 1e-6);
        assert_close("preview_camera_sample_path_z", sample.position_z, expected_z, 1e-6);
        assert_close("preview_camera_sample_path_yaw", sample.yaw_radians, expected_yaw, 1e-6);
    }
    assert_true("preview_camera_sample_path_pitch_bounds",
                sample.pitch_radians > 0.0 && sample.pitch_radians < (M_PI / 4.0));
    assert_close("preview_camera_sample_path_aspect", sample.aspect_ratio, 1.5, 1e-6);
}

static void test_preview_camera_projector_projection_contract(void) {
    PreviewCameraSample sample = {0};
    PreviewCameraProjector projector = {0};
    SDL_Rect viewport = {0, 0, 1000, 500};
    double sx = 0.0;
    double sy = 0.0;
    double depth = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double cz = 0.0;
    bool inside = false;

    sample.valid = true;
    sample.position_x = 0.0;
    sample.position_y = 0.0;
    sample.position_z = 0.0;
    sample.yaw_radians = 0.0;
    sample.pitch_radians = 0.0;
    sample.fov_y_degrees = 60.0;
    sample.aspect_ratio = 2.0;

    assert_true("preview_camera_projector_build",
                PreviewCameraProjectorBuild(&sample, viewport, &projector));
    assert_close("preview_camera_projector_forward_x", projector.forward_x, 0.0, 1e-6);
    assert_close("preview_camera_projector_forward_y", projector.forward_y, -1.0, 1e-6);
    assert_close("preview_camera_projector_forward_z", projector.forward_z, 0.0, 1e-6);
    assert_close("preview_camera_projector_right_x", projector.right_x, 1.0, 1e-6);
    assert_close("preview_camera_projector_right_y", projector.right_y, 0.0, 1e-6);
    assert_close("preview_camera_projector_right_z", projector.right_z, 0.0, 1e-6);
    assert_close("preview_camera_projector_up_x", projector.up_x, 0.0, 1e-6);
    assert_close("preview_camera_projector_up_y", projector.up_y, 0.0, 1e-6);
    assert_close("preview_camera_projector_up_z", projector.up_z, 1.0, 1e-6);

    PreviewCameraProjectorWorldToCamera(&projector, 0.0, -10.0, 0.0, &cx, &cy, &cz);
    assert_close("preview_camera_projector_cam_forward_x", cx, 0.0, 1e-6);
    assert_close("preview_camera_projector_cam_forward_y", cy, 0.0, 1e-6);
    assert_close("preview_camera_projector_cam_forward_z", cz, 10.0, 1e-6);

    assert_true("preview_camera_projector_project_center",
                PreviewCameraProjectorProjectPoint(&projector,
                                                   0.0,
                                                   -10.0,
                                                   0.0,
                                                   &sx,
                                                   &sy,
                                                   &depth,
                                                   &inside));
    assert_close("preview_camera_projector_center_x", sx, 500.0, 1e-6);
    assert_close("preview_camera_projector_center_y", sy, 250.0, 1e-6);
    assert_close("preview_camera_projector_center_depth", depth, 10.0, 1e-6);
    assert_true("preview_camera_projector_center_inside", inside);

    assert_true("preview_camera_projector_project_screen_right",
                PreviewCameraProjectorProjectPoint(&projector,
                                                   10.0,
                                                   -10.0,
                                                   0.0,
                                                   &sx,
                                                   &sy,
                                                   &depth,
                                                   &inside));
    assert_true("preview_camera_projector_screen_right_x", sx > 500.0);
    assert_close("preview_camera_projector_screen_right_y", sy, 250.0, 1e-6);
    assert_true("preview_camera_projector_screen_right_inside", inside);

    assert_true("preview_camera_projector_project_screen_top",
                PreviewCameraProjectorProjectPoint(&projector,
                                                   0.0,
                                                   -10.0,
                                                   5.0,
                                                   &sx,
                                                   &sy,
                                                   &depth,
                                                   &inside));
    assert_true("preview_camera_projector_screen_top_y", sy < 250.0);
    assert_true("preview_camera_projector_screen_top_inside", inside);

    inside = true;
    assert_true("preview_camera_projector_reject_behind",
                !PreviewCameraProjectorProjectPoint(&projector,
                                                    0.0,
                                                    10.0,
                                                    0.0,
                                                    &sx,
                                                    &sy,
                                                    &depth,
                                                    &inside));
    assert_true("preview_camera_projector_reject_behind_inside", !inside);
}

static void test_preview_retained_scene_line_segments_contract(void) {
    RuntimeSceneBridge3DDigestState digest = {0};
    PreviewRetainedSceneLineSegment segments[PREVIEW_RETAINED_SCENE_MAX_LINE_SEGMENTS];
    int count = 0;

    digest.valid = true;
    digest.has_scene_bounds = true;
    digest.bounds_enabled = true;
    digest.bounds_min_x = -6.0;
    digest.bounds_min_y = -5.0;
    digest.bounds_min_z = -1.0;
    digest.bounds_max_x = 6.0;
    digest.bounds_max_y = 5.0;
    digest.bounds_max_z = 5.5;
    digest.has_construction_plane = true;
    digest.construction_plane_offset = -1.0;
    digest.primitive_count = 2;
    digest.primitives[0].kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE;
    digest.primitives[0].origin_x = 0.0;
    digest.primitives[0].origin_y = 0.0;
    digest.primitives[0].origin_z = -1.0;
    digest.primitives[0].has_dimensions = true;
    digest.primitives[0].width = 8.0;
    digest.primitives[0].height = 6.0;
    digest.primitives[1].kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM;
    digest.primitives[1].origin_x = 1.0;
    digest.primitives[1].origin_y = 2.0;
    digest.primitives[1].origin_z = 1.5;
    digest.primitives[1].has_dimensions = true;
    digest.primitives[1].width = 2.0;
    digest.primitives[1].height = 3.0;
    digest.primitives[1].depth = 4.0;

    count = PreviewRetainedSceneBuildLineSegments(&digest,
                                                  segments,
                                                  PREVIEW_RETAINED_SCENE_MAX_LINE_SEGMENTS);
    assert_close("preview_retained_scene_line_count", (double)count, 32.0, 1e-6);
    assert_close("preview_retained_scene_bounds_first_ax", segments[0].ax, -6.0, 1e-6);
    assert_close("preview_retained_scene_bounds_first_ay", segments[0].ay, -5.0, 1e-6);
    assert_close("preview_retained_scene_bounds_first_az", segments[0].az, -1.0, 1e-6);
    assert_close("preview_retained_scene_plane_start", segments[12].az, -1.0, 1e-6);
    assert_close("preview_retained_scene_primitive_plane_z", segments[16].az, -1.0, 1e-6);
    assert_close("preview_retained_scene_prism_last_bz", segments[31].bz, 3.5, 1e-6);
}

static void test_preview_mode_route_select_contract(void) {
    RayTracingRuntimeRoute route = {0};
    RayTracingSceneDigestStatus digest_status = {0};
    PreviewModeRouteDecision decision = {0};

    route.requestedMode = SPACE_MODE_2D;
    route.routeFamily = RAY_TRACING_ROUTE_CANONICAL_2D;
    assert_true("preview_mode_route_2d",
                PreviewModeRouteSelect(&route, &digest_status, false, &decision));
    assert_close("preview_mode_route_2d_branch",
                 (double)decision.branch,
                 (double)PREVIEW_RENDER_BRANCH_LEGACY_2D,
                 1e-6);
    assert_true("preview_mode_route_2d_label",
                strstr(decision.branchLabel, "2D") != NULL);

    route.requestedMode = SPACE_MODE_3D;
    route.routeFamily = RAY_TRACING_ROUTE_COMPAT_3D_FALLBACK;
    digest_status.valid = false;
    assert_true("preview_mode_route_fallback",
                PreviewModeRouteSelect(&route, &digest_status, false, &decision));
    assert_close("preview_mode_route_fallback_branch",
                 (double)decision.branch,
                 (double)PREVIEW_RENDER_BRANCH_FALLBACK_2D,
                 1e-6);
    assert_true("preview_mode_route_fallback_status",
                strstr(decision.statusLine, "fallback") != NULL);

    digest_status.valid = true;
    digest_status.digestPrimitiveCount = 3;
    assert_true("preview_mode_route_retained",
                PreviewModeRouteSelect(&route, &digest_status, true, &decision));
    assert_close("preview_mode_route_retained_branch",
                 (double)decision.branch,
                 (double)PREVIEW_RENDER_BRANCH_RETAINED_3D,
                 1e-6);
    assert_true("preview_mode_route_retained_status",
                strstr(decision.statusLine, "primitives=3") != NULL);
}

static void test_preview_playback_evaluate_contract(void) {
    PreviewPlaybackSample sample = {0};

    assert_true("preview_playback_bounce_start",
                PreviewPlaybackEvaluate(0.0, 4.0, true, "stop", &sample));
    assert_close("preview_playback_bounce_start_t", sample.normalized_t, 0.0, 1e-6);
    assert_true("preview_playback_bounce_start_forward", !sample.reverse_direction);

    assert_true("preview_playback_bounce_mid",
                PreviewPlaybackEvaluate(2.0, 4.0, true, "stop", &sample));
    assert_close("preview_playback_bounce_mid_t", sample.normalized_t, 0.5, 1e-6);
    assert_true("preview_playback_bounce_mid_forward", !sample.reverse_direction);

    assert_true("preview_playback_bounce_reverse",
                PreviewPlaybackEvaluate(6.0, 4.0, true, "stop", &sample));
    assert_close("preview_playback_bounce_reverse_t", sample.normalized_t, 0.5, 1e-6);
    assert_true("preview_playback_bounce_reverse_dir", sample.reverse_direction);

    assert_true("preview_playback_loop_wrap",
                PreviewPlaybackEvaluate(5.0, 4.0, false, "loop", &sample));
    assert_close("preview_playback_loop_wrap_t", sample.normalized_t, 0.25, 1e-6);
    assert_true("preview_playback_loop_mode",
                sample.mode == PREVIEW_PLAYBACK_MODE_LOOP);

    assert_true("preview_playback_stop_clamp",
                PreviewPlaybackEvaluate(9.0, 4.0, false, "stop", &sample));
    assert_close("preview_playback_stop_clamp_t", sample.normalized_t, 1.0, 1e-6);
    assert_true("preview_playback_stop_clamped", sample.clamped);
}




int run_test_runtime_preview_editor_tests(void) {
    test_runtime_camera_projector_3d_preview_projection_parity();
    test_preview_camera_sample_evaluate_contract();
    test_preview_camera_projector_projection_contract();
    test_preview_retained_scene_line_segments_contract();
    test_preview_mode_route_select_contract();
    test_preview_playback_evaluate_contract();
    return 0;
}
