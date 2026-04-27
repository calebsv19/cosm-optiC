#include <math.h>
#include "app/animation.h"
#include "config/config_scene_path_io.h"
#include "path/path_system.h"
#include "test_runtime_path_policy.h"
#include "test_support.h"


  static int test_path_eval_3d_uses_linear_handle_units(void) {
    Path path = {0};
    int original_space_mode = animSettings.spaceMode;
    Point p = {0};

    path.mode = BEZIER_CUBIC;
    path.numPoints = 2;
    path.points[0] = (Point){0.0, 0.0};
    path.points[1] = (Point){10.0, 0.0};
    path.handles[0][0] = (Velocity){0.0, 4.0};
    path.handles[0][1] = (Velocity){0.0, 4.0};

    animSettings.spaceMode = SPACE_MODE_2D;
    p = GetPositionAlongPath(&path, 0.5);
    assert_true("path_eval_2d_handle_compression_active", p.y < 0.5);

    animSettings.spaceMode = SPACE_MODE_3D;
    p = GetPositionAlongPath(&path, 0.5);
    assert_close("path_eval_3d_linear_handle_y", p.y, 3.0, 1e-9);

    animSettings.spaceMode = original_space_mode;
    return 0;
}


static int test_path_normalized_spacing_preserves_tail_motion(void) {
    Path path = {0};
    int original_space_mode = animSettings.spaceMode;
    const double t_values[] = {0.80, 0.85, 0.90, 0.95, 1.00};
    double min_step = 1e9;
    double max_step = 0.0;

    path.mode = BEZIER_CUBIC;
    path.numPoints = 3;
    path.points[0] = (Point){0.0, 0.0};
    path.points[1] = (Point){12.0, 0.0};
    path.points[2] = (Point){20.0, 0.0};
    path.handles[0][0] = (Velocity){4.0, 0.0};
    path.handles[0][1] = (Velocity){-4.0, 0.0};
    path.handles[1][0] = (Velocity){0.0, 40.0};
    path.handles[1][1] = (Velocity){0.0, -40.0};

    animSettings.spaceMode = SPACE_MODE_3D;
    for (int i = 0; i < 4; ++i) {
        double global_a = PathResolveNormalizedGlobalT(&path, t_values[i]);
        double global_b = PathResolveNormalizedGlobalT(&path, t_values[i + 1]);
        Point previous = GetPositionAlongPath(&path, global_a);
        double step = 0.0;
        const int integration_steps = 256;
        for (int sample = 1; sample <= integration_steps; ++sample) {
            double alpha = (double)sample / (double)integration_steps;
            double raw_t = global_a + (global_b - global_a) * alpha;
            Point current = GetPositionAlongPath(&path, raw_t);
            double dx = current.x - previous.x;
            double dy = current.y - previous.y;
            step += sqrt(dx * dx + dy * dy);
            previous = current;
        }
        if (step < min_step) min_step = step;
        if (step > max_step) max_step = step;
    }

    assert_true("path_norm_tail_steps_nonzero", min_step > 0.1);
    assert_true("path_norm_tail_uniformity", max_step / min_step < 1.2);

    animSettings.spaceMode = original_space_mode;
    return 0;
}


static int test_path_traversal_endpoints_follow_sampled_contract(void) {
    Path path = {0};
    CameraPath3D path3d = {0};
    PathTraversalEndpoints endpoints = {0};
    bool ok = false;
    int original_space_mode = animSettings.spaceMode;

    path.mode = BEZIER_CUBIC;
    path.numPoints = 3;
    path.points[0] = (Point){2.0, 3.0};
    path.points[1] = (Point){8.0, 11.0};
    path.points[2] = (Point){17.0, 19.0};
    path.handles[0][0] = (Velocity){1.5, 2.0};
    path.handles[0][1] = (Velocity){-1.0, -1.5};
    path.handles[1][0] = (Velocity){2.5, -0.5};
    path.handles[1][1] = (Velocity){-2.0, 1.0};
    path3d.point_z[0] = 4.0;
    path3d.point_z[1] = 9.0;
    path3d.point_z[2] = 13.0;
    path3d.handles_vz[0][0] = 1.0;
    path3d.handles_vz[0][1] = -1.5;
    path3d.handles_vz[1][0] = 0.5;
    path3d.handles_vz[1][1] = -0.75;

    animSettings.spaceMode = SPACE_MODE_3D;
    ok = PathResolveTraversalEndpoints(&path, &path3d, &endpoints);

    assert_true("path_traversal_endpoints_resolved", ok);
    assert_true("path_traversal_start_index", endpoints.start_point_index == 0);
    assert_true("path_traversal_end_index", endpoints.end_point_index == 2);
    assert_true("path_traversal_has_z", endpoints.has_z);
    assert_close("path_traversal_start_x", endpoints.start_xy.x, path.points[0].x, 1e-9);
    assert_close("path_traversal_start_y", endpoints.start_xy.y, path.points[0].y, 1e-9);
    assert_close("path_traversal_end_x", endpoints.end_xy.x, path.points[2].x, 1e-9);
    assert_close("path_traversal_end_y", endpoints.end_xy.y, path.points[2].y, 1e-9);
    assert_close("path_traversal_start_z", endpoints.start_z, path3d.point_z[0], 1e-9);
    assert_close("path_traversal_end_z", endpoints.end_z, path3d.point_z[2], 1e-9);

    animSettings.spaceMode = original_space_mode;
    return 0;
}


static int test_camera_path_default_preserves_empty_authored_state(void) {
    SceneConfig scene = {0};
    struct json_object* root = json_object_new_object();
    struct json_object* camera_path = json_object_new_object();
    struct json_object* points = json_object_new_array();
    bool loaded = false;

    scene.camera.x = 12.0;
    scene.camera.y = -4.0;
    scene.cameraZ = 7.5;
    config_scene_ensure_camera_path_default(&scene);
    assert_true("camera_path_default_empty_points", scene.cameraPath.numPoints == 0);

    json_object_object_add(camera_path, "mode", json_object_new_string("BEZIER_CUBIC"));
    json_object_object_add(camera_path, "points", points);
    json_object_object_add(root, "cameraPath", camera_path);
    loaded = config_scene_load_camera_path_from_json(root, "cameraPath", &scene.cameraPath);
    assert_true("camera_path_load_empty_allowed", loaded);
    assert_true("camera_path_load_empty_points", scene.cameraPath.numPoints == 0);

    json_object_put(root);
    return 0;
}


int run_test_runtime_path_policy_tests(void) {
    test_path_eval_3d_uses_linear_handle_units();
    test_path_normalized_spacing_preserves_tail_motion();
    test_path_traversal_endpoints_follow_sampled_contract();
    test_camera_path_default_preserves_empty_authored_state();
    return 0;
}
