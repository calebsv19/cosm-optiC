#include <math.h>
#include <stdio.h>
#include <string.h>

#include "editor/scene_editor_primitive_preview_geometry.h"

static int failures = 0;

static void expect_true(const char* label, bool condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", label);
        failures += 1;
    }
}

static void accumulate_extents(
    const SceneEditorPrimitivePreviewTriangle* triangles,
    size_t triangle_count,
    double min_value[3],
    double max_value[3]) {
    for (size_t i = 0u; i < triangle_count; ++i) {
        const SceneEditorPrimitivePreviewPoint3 points[3] = {
            triangles[i].a, triangles[i].b, triangles[i].c};
        for (int point = 0; point < 3; ++point) {
            const double values[3] = {points[point].x, points[point].y, points[point].z};
            for (int axis = 0; axis < 3; ++axis) {
                if (values[axis] < min_value[axis]) min_value[axis] = values[axis];
                if (values[axis] > max_value[axis]) max_value[axis] = values[axis];
            }
        }
    }
}

static bool close_enough(double actual, double expected) {
    return fabs(actual - expected) <= 1e-12;
}

int main(void) {
    RuntimeSceneBridgePrimitiveSeed plane = {0};
    RuntimeSceneBridgePrimitiveSeed prism = {0};
    SceneEditorPrimitivePreviewTriangle
        triangles[SCENE_EDITOR_PRIMITIVE_PREVIEW_MAX_TRIANGLES];
    size_t triangle_count = 0u;
    double min_value[3] = {INFINITY, INFINITY, INFINITY};
    double max_value[3] = {-INFINITY, -INFINITY, -INFINITY};

    plane.kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE;
    plane.has_dimensions = true;
    plane.origin_x = 3.0;
    plane.origin_y = -2.0;
    plane.origin_z = 5.0;
    plane.axis_u_x = 1.0;
    plane.axis_v_z = 1.0;
    plane.normal_y = -1.0;
    plane.width = 4.0;
    plane.height = 6.0;
    expect_true("plane builds", SceneEditorPrimitivePreviewBuildTriangles(
                                    &plane, triangles, &triangle_count));
    expect_true("plane has two triangles", triangle_count == 2u);
    accumulate_extents(triangles, triangle_count, min_value, max_value);
    expect_true("plane width follows axis_u",
                close_enough(min_value[0], 1.0) && close_enough(max_value[0], 5.0));
    expect_true("plane stays on origin y",
                close_enough(min_value[1], -2.0) && close_enough(max_value[1], -2.0));
    expect_true("plane height follows axis_v",
                close_enough(min_value[2], 2.0) && close_enough(max_value[2], 8.0));

    memset(triangles, 0, sizeof(triangles));
    min_value[0] = min_value[1] = min_value[2] = INFINITY;
    max_value[0] = max_value[1] = max_value[2] = -INFINITY;
    prism.kind = RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM;
    prism.has_dimensions = true;
    prism.axis_u_x = 1.0;
    prism.axis_v_y = 1.0;
    prism.normal_z = 1.0;
    prism.width = 2.0;
    prism.height = 4.0;
    prism.depth = 6.0;
    expect_true("prism builds", SceneEditorPrimitivePreviewBuildTriangles(
                                    &prism, triangles, &triangle_count));
    expect_true("prism has twelve triangles", triangle_count == 12u);
    accumulate_extents(triangles, triangle_count, min_value, max_value);
    expect_true("prism x extent",
                close_enough(min_value[0], -1.0) && close_enough(max_value[0], 1.0));
    expect_true("prism y extent",
                close_enough(min_value[1], -2.0) && close_enough(max_value[1], 2.0));
    expect_true("prism z extent",
                close_enough(min_value[2], -3.0) && close_enough(max_value[2], 3.0));

    prism.guide_only = true;
    triangle_count = 99u;
    expect_true("guide prism is not filled",
                !SceneEditorPrimitivePreviewBuildTriangles(
                    &prism, triangles, &triangle_count) && triangle_count == 0u);

    if (failures != 0) return 1;
    puts("scene editor primitive preview geometry: PASS");
    return 0;
}
