#include "editor/scene_editor_primitive_preview_geometry.h"

#include <math.h>
#include <string.h>

static SceneEditorPrimitivePreviewPoint3 scene_editor_primitive_preview_point(
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    double u,
    double v,
    double n) {
    return (SceneEditorPrimitivePreviewPoint3){
        primitive->origin_x + primitive->axis_u_x * u + primitive->axis_v_x * v +
            primitive->normal_x * n,
        primitive->origin_y + primitive->axis_u_y * u + primitive->axis_v_y * v +
            primitive->normal_y * n,
        primitive->origin_z + primitive->axis_u_z * u + primitive->axis_v_z * v +
            primitive->normal_z * n};
}

static void scene_editor_primitive_preview_set_triangle(
    SceneEditorPrimitivePreviewTriangle* triangle,
    const SceneEditorPrimitivePreviewPoint3* points,
    int a,
    int b,
    int c) {
    triangle->a = points[a];
    triangle->b = points[b];
    triangle->c = points[c];
}

bool SceneEditorPrimitivePreviewBuildTriangles(
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    SceneEditorPrimitivePreviewTriangle
        out_triangles[SCENE_EDITOR_PRIMITIVE_PREVIEW_MAX_TRIANGLES],
    size_t* out_triangle_count) {
    SceneEditorPrimitivePreviewPoint3 points[8] = {0};
    const double half_w = primitive ? fmax(0.05, fabs(primitive->width) * 0.5) : 0.0;
    const double half_h = primitive ? fmax(0.05, fabs(primitive->height) * 0.5) : 0.0;
    const double half_d = primitive ? fmax(0.05, fabs(primitive->depth) * 0.5) : 0.0;
    size_t count = 0u;

    if (out_triangle_count) *out_triangle_count = 0u;
    if (!primitive || !out_triangles || !out_triangle_count ||
        !primitive->has_dimensions || primitive->guide_only ||
        !isfinite(half_w) || !isfinite(half_h) || !isfinite(half_d)) {
        return false;
    }
    memset(out_triangles,
           0,
           sizeof(*out_triangles) * SCENE_EDITOR_PRIMITIVE_PREVIEW_MAX_TRIANGLES);

    if (primitive->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) {
        points[0] = scene_editor_primitive_preview_point(primitive, -half_w, -half_h, 0.0);
        points[1] = scene_editor_primitive_preview_point(primitive, -half_w, half_h, 0.0);
        points[2] = scene_editor_primitive_preview_point(primitive, half_w, -half_h, 0.0);
        points[3] = scene_editor_primitive_preview_point(primitive, half_w, half_h, 0.0);
        scene_editor_primitive_preview_set_triangle(&out_triangles[0], points, 0, 2, 3);
        scene_editor_primitive_preview_set_triangle(&out_triangles[1], points, 0, 3, 1);
        *out_triangle_count = 2u;
        return true;
    }
    if (primitive->kind != RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM &&
        primitive->kind != RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX) {
        return false;
    }

    points[0] = scene_editor_primitive_preview_point(primitive, -half_w, -half_h, -half_d);
    points[1] = scene_editor_primitive_preview_point(primitive, -half_w, -half_h, half_d);
    points[2] = scene_editor_primitive_preview_point(primitive, -half_w, half_h, -half_d);
    points[3] = scene_editor_primitive_preview_point(primitive, -half_w, half_h, half_d);
    points[4] = scene_editor_primitive_preview_point(primitive, half_w, -half_h, -half_d);
    points[5] = scene_editor_primitive_preview_point(primitive, half_w, -half_h, half_d);
    points[6] = scene_editor_primitive_preview_point(primitive, half_w, half_h, -half_d);
    points[7] = scene_editor_primitive_preview_point(primitive, half_w, half_h, half_d);
    {
        static const int faces[12][3] = {
            {0, 1, 3}, {0, 3, 2}, {4, 6, 7}, {4, 7, 5},
            {0, 4, 5}, {0, 5, 1}, {2, 3, 7}, {2, 7, 6},
            {0, 2, 6}, {0, 6, 4}, {1, 5, 7}, {1, 7, 3}};
        for (count = 0u; count < SCENE_EDITOR_PRIMITIVE_PREVIEW_MAX_TRIANGLES; ++count) {
            scene_editor_primitive_preview_set_triangle(&out_triangles[count],
                                                        points,
                                                        faces[count][0],
                                                        faces[count][1],
                                                        faces[count][2]);
        }
    }
    *out_triangle_count = SCENE_EDITOR_PRIMITIVE_PREVIEW_MAX_TRIANGLES;
    return true;
}
