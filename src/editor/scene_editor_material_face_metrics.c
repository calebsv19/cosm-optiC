#include "editor/scene_editor_material_face_metrics.h"

#include <math.h>
#include <string.h>

#include "import/runtime_scene_bridge.h"
#include "math/vec3.h"

static bool scene_editor_material_face_metrics_valid_dimension(double value) {
    return value > 1e-6;
}

static double scene_editor_material_face_metrics_abs_dot(Vec3 axis, Vec3 target) {
    return fabs(vec3_dot(axis, target));
}

static bool scene_editor_material_face_metrics_orient_axes(Vec3 axis_a,
                                                           Vec3 axis_b,
                                                           double dim_a,
                                                           double dim_b,
                                                           SceneEditorMaterialFaceMetrics* out_metrics) {
    const Vec3 world_up = {0.0, 0.0, 1.0};
    const Vec3 world_right = {1.0, 0.0, 0.0};
    const Vec3 world_planar_up = {0.0, 1.0, 0.0};
    Vec3 oriented_u = axis_a;
    Vec3 oriented_v = axis_b;
    double width = dim_a;
    double height = dim_b;
    bool swap_axes = false;
    bool flip_u = false;
    bool flip_v = false;
    double axis_a_up = 0.0;
    double axis_b_up = 0.0;

    if (!out_metrics ||
        !scene_editor_material_face_metrics_valid_dimension(dim_a) ||
        !scene_editor_material_face_metrics_valid_dimension(dim_b)) {
        return false;
    }

    axis_a = vec3_normalize(axis_a);
    axis_b = vec3_normalize(axis_b);
    if (vec3_length(axis_a) <= 1e-6 || vec3_length(axis_b) <= 1e-6) {
        return false;
    }

    axis_a_up = scene_editor_material_face_metrics_abs_dot(axis_a, world_up);
    axis_b_up = scene_editor_material_face_metrics_abs_dot(axis_b, world_up);
    if (axis_a_up > 1e-4 || axis_b_up > 1e-4) {
        if (axis_a_up > axis_b_up) {
            swap_axes = true;
            oriented_u = axis_b;
            oriented_v = axis_a;
            width = dim_b;
            height = dim_a;
        }
        flip_v = vec3_dot(oriented_v, world_up) < 0.0;
    } else {
        double axis_a_planar_up = scene_editor_material_face_metrics_abs_dot(axis_a,
                                                                              world_planar_up);
        double axis_b_planar_up = scene_editor_material_face_metrics_abs_dot(axis_b,
                                                                              world_planar_up);
        if (axis_a_planar_up > axis_b_planar_up) {
            swap_axes = true;
            oriented_u = axis_b;
            oriented_v = axis_a;
            width = dim_b;
            height = dim_a;
        }
        if (scene_editor_material_face_metrics_abs_dot(oriented_v, world_planar_up) > 1e-4) {
            flip_v = vec3_dot(oriented_v, world_planar_up) < 0.0;
        } else if (scene_editor_material_face_metrics_abs_dot(oriented_v, world_right) > 1e-4) {
            flip_v = vec3_dot(oriented_v, world_right) < 0.0;
        }
    }

    if (scene_editor_material_face_metrics_abs_dot(oriented_u, world_right) > 1e-4) {
        flip_u = vec3_dot(oriented_u, world_right) < 0.0;
    } else if (scene_editor_material_face_metrics_abs_dot(oriented_u, world_planar_up) > 1e-4) {
        flip_u = vec3_dot(oriented_u, world_planar_up) < 0.0;
    }

    out_metrics->valid = true;
    out_metrics->width = width;
    out_metrics->height = height;
    out_metrics->swapAxes = swap_axes;
    out_metrics->flipU = flip_u;
    out_metrics->flipV = flip_v;
    return true;
}

static bool scene_editor_material_face_metrics_resolve_seed_dims(
    const RuntimeSceneBridgePrimitiveSeed* seed,
    int face_group_index,
    SceneEditorMaterialFaceMetrics* out_metrics) {
    Vec3 axis_a = {0};
    Vec3 axis_b = {0};
    Vec3 axis_u = {0};
    Vec3 axis_v = {0};
    Vec3 normal = {0};
    double dim_a = 0.0;
    double dim_b = 0.0;

    if (!seed || !out_metrics || face_group_index < 0) return false;
    axis_u = vec3(seed->axis_u_x, seed->axis_u_y, seed->axis_u_z);
    axis_v = vec3(seed->axis_v_x, seed->axis_v_y, seed->axis_v_z);
    normal = vec3(seed->normal_x, seed->normal_y, seed->normal_z);

    switch (seed->kind) {
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE:
            if (face_group_index != 0) return false;
            axis_a = axis_u;
            axis_b = axis_v;
            dim_a = seed->width;
            dim_b = seed->height;
            break;
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM:
            switch (face_group_index) {
                case 0:
                    axis_a = axis_u;
                    axis_b = axis_v;
                    dim_a = seed->width;
                    dim_b = seed->height;
                    break;
                case 1:
                    axis_a = axis_v;
                    axis_b = axis_u;
                    dim_a = seed->height;
                    dim_b = seed->width;
                    break;
                case 2:
                    axis_a = axis_u;
                    axis_b = normal;
                    dim_a = seed->width;
                    dim_b = seed->depth;
                    break;
                case 3:
                    axis_a = normal;
                    axis_b = axis_u;
                    dim_a = seed->depth;
                    dim_b = seed->width;
                    break;
                case 4:
                    axis_a = normal;
                    axis_b = axis_v;
                    dim_a = seed->depth;
                    dim_b = seed->height;
                    break;
                case 5:
                    axis_a = axis_v;
                    axis_b = normal;
                    dim_a = seed->height;
                    dim_b = seed->depth;
                    break;
                default:
                    return false;
            }
            break;
        default:
            return false;
    }
    return scene_editor_material_face_metrics_orient_axes(axis_a,
                                                          axis_b,
                                                          dim_a,
                                                          dim_b,
                                                          out_metrics);
}

bool SceneEditorMaterialFaceMetricsResolve(int primitive_index,
                                           int scene_object_index,
                                           int face_group_index,
                                           SceneEditorMaterialFaceMetrics* out_metrics) {
    RuntimeSceneBridge3DPrimitiveSeedState seed_state = {0};

    if (!out_metrics) return false;
    memset(out_metrics, 0, sizeof(*out_metrics));
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seed_state);
    if (!seed_state.valid || face_group_index < 0) return false;

    if (primitive_index >= 0 && primitive_index < seed_state.primitive_count) {
        const RuntimeSceneBridgePrimitiveSeed* seed = &seed_state.primitives[primitive_index];
        if ((scene_object_index < 0 || seed->scene_object_index == scene_object_index) &&
            scene_editor_material_face_metrics_resolve_seed_dims(seed,
                                                                 face_group_index,
                                                                 out_metrics)) {
            return true;
        }
    }

    for (int i = 0; i < seed_state.primitive_count; ++i) {
        const RuntimeSceneBridgePrimitiveSeed* seed = &seed_state.primitives[i];
        if (scene_object_index >= 0 && seed->scene_object_index != scene_object_index) {
            continue;
        }
        if (scene_editor_material_face_metrics_resolve_seed_dims(seed,
                                                                 face_group_index,
                                                                 out_metrics)) {
            return true;
        }
    }

    return false;
}

bool SceneEditorMaterialFaceMetricsGroundUV(
    const SceneEditorMaterialFaceMetrics* metrics,
    double face_u,
    double face_v,
    double* out_u,
    double* out_v) {
    double oriented_u = face_u;
    double oriented_v = face_v;
    if (out_u) *out_u = face_u;
    if (out_v) *out_v = face_v;
    if (!metrics || !metrics->valid ||
        !scene_editor_material_face_metrics_valid_dimension(metrics->width) ||
        !scene_editor_material_face_metrics_valid_dimension(metrics->height)) {
        return false;
    }

    if (metrics->swapAxes) {
        oriented_u = face_v;
        oriented_v = face_u;
    }
    if (metrics->flipU) oriented_u = 1.0 - oriented_u;
    if (metrics->flipV) oriented_v = 1.0 - oriented_v;

    if (out_u) *out_u = ((oriented_u - 0.5) * metrics->width) + 0.5;
    if (out_v) *out_v = ((oriented_v - 0.5) * metrics->height) + 0.5;
    return true;
}

bool SceneEditorMaterialFaceMetricsResolveGroundedUV(
    int primitive_index,
    int scene_object_index,
    int face_group_index,
    double face_u,
    double face_v,
    double* out_u,
    double* out_v) {
    SceneEditorMaterialFaceMetrics metrics = {0};

    if (out_u) *out_u = face_u;
    if (out_v) *out_v = face_v;
    if (!SceneEditorMaterialFaceMetricsResolve(primitive_index,
                                               scene_object_index,
                                               face_group_index,
                                               &metrics)) {
        return false;
    }
    return SceneEditorMaterialFaceMetricsGroundUV(&metrics,
                                                  face_u,
                                                  face_v,
                                                  out_u,
                                                  out_v);
}
