#include <math.h>
#include <stdio.h>

#include "editor/scene_editor_viewport_nav_math.h"

#define NAV_TEST_EPSILON (1e-9)

static int failures = 0;

static void expect_true(const char* name, bool condition) {
    if (condition) return;
    fprintf(stderr, "FAIL: %s\n", name);
    failures += 1;
}

static void expect_close(const char* name, double actual, double expected) {
    if (isfinite(actual) && fabs(actual - expected) <= NAV_TEST_EPSILON) return;
    fprintf(stderr, "FAIL: %s actual=%.12f expected=%.12f\n", name, actual, expected);
    failures += 1;
}

static SceneEditorViewportNavVec3 subtract_vec3(SceneEditorViewportNavVec3 lhs,
                                                 SceneEditorViewportNavVec3 rhs) {
    return (SceneEditorViewportNavVec3){lhs.x - rhs.x,
                                        lhs.y - rhs.y,
                                        lhs.z - rhs.z};
}

static double dot_vec3(SceneEditorViewportNavVec3 lhs,
                       SceneEditorViewportNavVec3 rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

static void test_basis_contract(void) {
    SceneEditorViewportNavBasis basis = {0};
    expect_true("basis_default_valid",
                SceneEditorViewportNavMathBuildBasis(0.0, 0.0, &basis));
    expect_close("basis_right_x", basis.right.x, 1.0);
    expect_close("basis_right_y", basis.right.y, 0.0);
    expect_close("basis_down_y", basis.screen_down.y, 1.0);
    expect_close("basis_forward_z", basis.forward.z, -1.0);

    basis.right.x = 19.0;
    expect_true("basis_nonfinite_rejected",
                !SceneEditorViewportNavMathBuildBasis(NAN, 0.0, &basis));
    expect_close("basis_invalid_nonmutation", basis.right.x, 19.0);
}

static void test_pan_contract(void) {
    SceneEditorViewportNavBasis basis = {0};
    SceneEditorViewportNavVec3 target = {4.0, 5.0, 6.0};
    SceneEditorViewportNavVec3 result = {0};
    SceneEditorViewportNavVec3 unchanged = {11.0, 12.0, 13.0};
    expect_true("pan_basis_valid",
                SceneEditorViewportNavMathBuildBasis(0.0, 0.0, &basis));
    expect_true("pan_applies",
                SceneEditorViewportNavMathPanTarget(&target,
                                                    &basis,
                                                    10.0,
                                                    20.0,
                                                    -10.0,
                                                    &result));
    expect_close("pan_drag_right_moves_target_left", result.x, 2.0);
    expect_close("pan_drag_up_moves_target_down", result.y, 6.0);
    expect_close("pan_preserves_depth_default", result.z, 6.0);

    expect_true("pan_invalid_scale_rejected",
                !SceneEditorViewportNavMathPanTarget(&target,
                                                     &basis,
                                                     0.0,
                                                     1.0,
                                                     1.0,
                                                     &unchanged));
    expect_close("pan_invalid_x_nonmutation", unchanged.x, 11.0);
    expect_close("pan_invalid_y_nonmutation", unchanged.y, 12.0);
    expect_close("pan_invalid_z_nonmutation", unchanged.z, 13.0);
}

static void test_anchor_zoom_contract(void) {
    SceneEditorViewportNavBasis basis = {0};
    SceneEditorViewportNavVec3 target = {0.0, 0.0, 0.0};
    SceneEditorViewportNavVec3 next_target = {0};
    SceneEditorViewportNavVec3 anchor_before = {0};
    SceneEditorViewportNavVec3 anchor_after = {0};
    const double offset_x = 50.0;
    const double offset_y = -30.0;
    const double old_scale = 100.0;
    const double new_scale = 200.0;
    expect_true("anchor_basis_valid",
                SceneEditorViewportNavMathBuildBasis(0.35, -0.25, &basis));
    anchor_before.x = target.x + basis.right.x * offset_x / old_scale +
                      basis.screen_down.x * offset_y / old_scale;
    anchor_before.y = target.y + basis.right.y * offset_x / old_scale +
                      basis.screen_down.y * offset_y / old_scale;
    anchor_before.z = target.z + basis.right.z * offset_x / old_scale +
                      basis.screen_down.z * offset_y / old_scale;
    expect_true("anchor_zoom_applies",
                SceneEditorViewportNavMathPreserveScreenAnchor(&target,
                                                               &basis,
                                                               old_scale,
                                                               new_scale,
                                                               offset_x,
                                                               offset_y,
                                                               &next_target));
    anchor_after.x = next_target.x + basis.right.x * offset_x / new_scale +
                     basis.screen_down.x * offset_y / new_scale;
    anchor_after.y = next_target.y + basis.right.y * offset_x / new_scale +
                     basis.screen_down.y * offset_y / new_scale;
    anchor_after.z = next_target.z + basis.right.z * offset_x / new_scale +
                     basis.screen_down.z * offset_y / new_scale;
    expect_close("anchor_world_x_preserved", anchor_after.x, anchor_before.x);
    expect_close("anchor_world_y_preserved", anchor_after.y, anchor_before.y);
    expect_close("anchor_world_z_preserved", anchor_after.z, anchor_before.z);
}

static void test_evn3_parity_pan_fixture(void) {
    const double degrees_to_radians = 3.14159265358979323846 / 180.0;
    SceneEditorViewportNavBasis basis = {0};
    const SceneEditorViewportNavVec3 target = {4.0, -3.0, 2.0};
    SceneEditorViewportNavVec3 result = {0};
    SceneEditorViewportNavVec3 delta = {0};

    expect_true("evn3_pan_basis_valid",
                SceneEditorViewportNavMathBuildBasis(35.0 * degrees_to_radians,
                                                      -20.0 * degrees_to_radians,
                                                      &basis));
    expect_true("evn3_pan_applies",
                SceneEditorViewportNavMathPanTarget(&target,
                                                    &basis,
                                                    16.0,
                                                    32.0,
                                                    -16.0,
                                                    &result));
    delta = subtract_vec3(result, target);
    expect_close("evn3_pan_right_delta", dot_vec3(delta, basis.right), -2.0);
    expect_close("evn3_pan_vertical_delta", dot_vec3(delta, basis.screen_down), 1.0);
    expect_close("evn3_pan_forward_delta", dot_vec3(delta, basis.forward), 0.0);
}

static void test_evn3_parity_anchor_fixture(void) {
    const double degrees_to_radians = 3.14159265358979323846 / 180.0;
    SceneEditorViewportNavBasis basis = {0};
    const SceneEditorViewportNavVec3 target = {4.0, -3.0, 2.0};
    SceneEditorViewportNavVec3 result = {0};
    SceneEditorViewportNavVec3 delta = {0};
    SceneEditorViewportNavVec3 sentinel = {91.0, 92.0, 93.0};

    expect_true("evn3_anchor_basis_valid",
                SceneEditorViewportNavMathBuildBasis(35.0 * degrees_to_radians,
                                                      -20.0 * degrees_to_radians,
                                                      &basis));
    expect_true("evn3_anchor_applies",
                SceneEditorViewportNavMathPreserveScreenAnchor(&target,
                                                                &basis,
                                                                16.0,
                                                                32.0,
                                                                48.0,
                                                                -24.0,
                                                                &result));
    delta = subtract_vec3(result, target);
    expect_close("evn3_anchor_right_compensation", dot_vec3(delta, basis.right), 1.5);
    expect_close("evn3_anchor_vertical_compensation",
                 dot_vec3(delta, basis.screen_down),
                 -0.75);
    expect_close("evn3_anchor_forward_compensation", dot_vec3(delta, basis.forward), 0.0);

    expect_true("evn3_anchor_invalid_rejected",
                !SceneEditorViewportNavMathPreserveScreenAnchor(&target,
                                                                 &basis,
                                                                 NAN,
                                                                 32.0,
                                                                 48.0,
                                                                 -24.0,
                                                                 &sentinel));
    expect_close("evn3_anchor_invalid_x_nonmutation", sentinel.x, 91.0);
    expect_close("evn3_anchor_invalid_y_nonmutation", sentinel.y, 92.0);
    expect_close("evn3_anchor_invalid_z_nonmutation", sentinel.z, 93.0);
}

int main(void) {
    test_basis_contract();
    test_pan_contract();
    test_anchor_zoom_contract();
    test_evn3_parity_pan_fixture();
    test_evn3_parity_anchor_fixture();
    if (failures != 0) {
        fprintf(stderr, "scene editor viewport navigation contract: %d failure(s)\n", failures);
        return 1;
    }
    puts("scene editor viewport navigation contract: PASS");
    return 0;
}
