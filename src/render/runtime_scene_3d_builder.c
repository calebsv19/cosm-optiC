#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_scene_3d_samples.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static RuntimePrimitive3DKind runtime_scene_3d_builder_map_kind(
    RuntimeSceneBridgePrimitiveKind kind) {
    switch (kind) {
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE:
            return RUNTIME_PRIMITIVE_3D_KIND_PLANE;
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM:
            return RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM;
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_TRIANGLE_MESH:
            return RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_UNKNOWN:
        case RUNTIME_SCENE_BRIDGE_PRIMITIVE_BOX:
        default:
            return RUNTIME_PRIMITIVE_3D_KIND_INVALID;
    }
}

static int runtime_scene_3d_builder_triangle_count_for_kind(RuntimePrimitive3DKind kind) {
    switch (kind) {
        case RUNTIME_PRIMITIVE_3D_KIND_PLANE:
            return 2;
        case RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM:
            return 12;
        case RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH:
        case RUNTIME_PRIMITIVE_3D_KIND_INVALID:
        default:
            return 0;
    }
}

static Vec3 runtime_scene_3d_builder_default_u_from_normal(Vec3 normal) {
    Vec3 tangent = fabs(normal.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    Vec3 axis_u = vec3_cross(tangent, normal);
    if (vec3_length(axis_u) <= 1e-9) {
        axis_u = vec3(1.0, 0.0, 0.0);
    }
    return vec3_normalize(axis_u);
}

static void runtime_scene_3d_builder_resolve_basis(Vec3* io_axis_u,
                                                   Vec3* io_axis_v,
                                                   Vec3* io_normal) {
    Vec3 axis_u = io_axis_u ? *io_axis_u : vec3(0.0, 0.0, 0.0);
    Vec3 axis_v = io_axis_v ? *io_axis_v : vec3(0.0, 0.0, 0.0);
    Vec3 normal = io_normal ? *io_normal : vec3(0.0, 0.0, 0.0);
    Vec3 derived = vec3(0.0, 0.0, 0.0);

    if (vec3_length(normal) <= 1e-9) {
        normal = vec3(0.0, 0.0, 1.0);
    }
    normal = vec3_normalize(normal);

    if (vec3_length(axis_u) <= 1e-9) {
        axis_u = runtime_scene_3d_builder_default_u_from_normal(normal);
    }
    axis_u = vec3_normalize(axis_u);

    if (vec3_length(axis_v) <= 1e-9) {
        axis_v = vec3_cross(normal, axis_u);
    }
    axis_v = vec3_normalize(axis_v);

    derived = vec3_cross(axis_u, axis_v);
    if (vec3_length(derived) <= 1e-9) {
        axis_v = vec3_cross(normal, axis_u);
        axis_v = vec3_normalize(axis_v);
        derived = vec3_cross(axis_u, axis_v);
    }
    if (vec3_length(derived) <= 1e-9) {
        axis_u = runtime_scene_3d_builder_default_u_from_normal(normal);
        axis_v = vec3_normalize(vec3_cross(normal, axis_u));
        derived = vec3_cross(axis_u, axis_v);
    }

    derived = vec3_normalize(derived);
    if (vec3_dot(derived, normal) < 0.0) {
        axis_v = vec3_scale(axis_v, -1.0);
        derived = vec3_scale(derived, -1.0);
    }

    if (io_axis_u) *io_axis_u = axis_u;
    if (io_axis_v) *io_axis_v = axis_v;
    if (io_normal) *io_normal = derived;
}

static bool runtime_scene_3d_builder_reserve_primitives(RuntimeScene3D* scene,
                                                        int primitive_capacity) {
    RuntimePrimitive3D* primitives = NULL;
    if (!scene) return false;
    if (primitive_capacity <= scene->primitiveCapacity) return true;

    primitives = (RuntimePrimitive3D*)realloc(scene->primitives,
                                              sizeof(*scene->primitives) *
                                                  (size_t)primitive_capacity);
    if (!primitives) return false;

    scene->primitives = primitives;
    scene->primitiveCapacity = primitive_capacity;
    return true;
}

static bool runtime_scene_3d_builder_reserve_triangles(RuntimeScene3D* scene,
                                                       int triangle_capacity) {
    RuntimeTriangle3D* triangles = NULL;
    if (!scene) return false;
    if (triangle_capacity <= scene->triangleMesh.triangleCapacity) return true;

    triangles =
        (RuntimeTriangle3D*)realloc(scene->triangleMesh.triangles,
                                    sizeof(*scene->triangleMesh.triangles) *
                                        (size_t)triangle_capacity);
    if (!triangles) return false;

    scene->triangleMesh.triangles = triangles;
    scene->triangleMesh.triangleCapacity = triangle_capacity;
    return true;
}

static bool runtime_scene_3d_builder_append_triangle(RuntimeScene3D* scene,
                                                     int primitive_index,
                                                     int scene_object_index,
                                                     Vec3 p0,
                                                     Vec3 p1,
                                                     Vec3 p2,
                                                     Vec3 expected_normal) {
    RuntimeTriangle3D* triangle = NULL;
    Vec3 edge1;
    Vec3 edge2;
    Vec3 normal;
    int local_triangle_index = 0;
    if (!scene) return false;
    if (scene->triangleMesh.triangleCount >= scene->triangleMesh.triangleCapacity) return false;

    edge1 = vec3_sub(p1, p0);
    edge2 = vec3_sub(p2, p0);
    normal = vec3_normalize(vec3_cross(edge1, edge2));
    if (vec3_dot(normal, expected_normal) < 0.0) {
        Vec3 swap = p1;
        p1 = p2;
        p2 = swap;
        edge1 = vec3_sub(p1, p0);
        edge2 = vec3_sub(p2, p0);
        normal = vec3_normalize(vec3_cross(edge1, edge2));
    }

    triangle = &scene->triangleMesh.triangles[scene->triangleMesh.triangleCount++];
    memset(triangle, 0, sizeof(*triangle));
    for (int i = 0; i < scene->triangleMesh.triangleCount - 1; ++i) {
        if (scene->triangleMesh.triangles[i].sceneObjectIndex == scene_object_index) {
            local_triangle_index += 1;
        }
    }
    triangle->p0 = p0;
    triangle->p1 = p1;
    triangle->p2 = p2;
    triangle->normal = normal;
    triangle->primitiveIndex = primitive_index;
    triangle->sceneObjectIndex = scene_object_index;
    triangle->localTriangleIndex = local_triangle_index;
    return true;
}

static bool runtime_scene_3d_builder_append_quad(RuntimeScene3D* scene,
                                                 int primitive_index,
                                                 int scene_object_index,
                                                 Vec3 p0,
                                                 Vec3 p1,
                                                 Vec3 p2,
                                                 Vec3 p3,
                                                 Vec3 expected_normal) {
    if (!runtime_scene_3d_builder_append_triangle(scene,
                                                  primitive_index,
                                                  scene_object_index,
                                                  p0,
                                                  p1,
                                                  p2,
                                                  expected_normal)) {
        return false;
    }
    return runtime_scene_3d_builder_append_triangle(scene,
                                                    primitive_index,
                                                    scene_object_index,
                                                    p0,
                                                    p2,
                                                    p3,
                                                    expected_normal);
}

static bool runtime_scene_3d_builder_append_plane(RuntimeScene3D* scene,
                                                  int primitive_index,
                                                  const RuntimePrimitive3D* primitive) {
    Vec3 origin;
    Vec3 axis_u;
    Vec3 axis_v;
    Vec3 normal;
    Vec3 half_u;
    Vec3 half_v;
    Vec3 p0;
    Vec3 p1;
    Vec3 p2;
    Vec3 p3;
    if (!scene || !primitive) return false;

    origin = primitive->shape.plane.origin;
    axis_u = primitive->shape.plane.axisU;
    axis_v = primitive->shape.plane.axisV;
    normal = primitive->shape.plane.normal;
    runtime_scene_3d_builder_resolve_basis(&axis_u, &axis_v, &normal);

    half_u = vec3_scale(axis_u, primitive->shape.plane.width * 0.5);
    half_v = vec3_scale(axis_v, primitive->shape.plane.height * 0.5);
    p0 = vec3_sub(vec3_sub(origin, half_u), half_v);
    p1 = vec3_add(vec3_sub(origin, half_v), half_u);
    p2 = vec3_add(vec3_add(origin, half_u), half_v);
    p3 = vec3_add(vec3_sub(origin, half_u), half_v);

    return runtime_scene_3d_builder_append_quad(scene,
                                                primitive_index,
                                                primitive->source.sceneObjectIndex,
                                                p0,
                                                p1,
                                                p2,
                                                p3,
                                                normal);
}

static bool runtime_scene_3d_builder_append_rect_prism(RuntimeScene3D* scene,
                                                       int primitive_index,
                                                       const RuntimePrimitive3D* primitive) {
    Vec3 origin;
    Vec3 axis_u;
    Vec3 axis_v;
    Vec3 normal;
    Vec3 half_u;
    Vec3 half_v;
    Vec3 half_n;
    Vec3 bottom0;
    Vec3 bottom1;
    Vec3 bottom2;
    Vec3 bottom3;
    Vec3 top0;
    Vec3 top1;
    Vec3 top2;
    Vec3 top3;
    int scene_object_index;
    if (!scene || !primitive) return false;

    origin = primitive->shape.rectPrism.origin;
    axis_u = primitive->shape.rectPrism.axisU;
    axis_v = primitive->shape.rectPrism.axisV;
    normal = primitive->shape.rectPrism.normal;
    runtime_scene_3d_builder_resolve_basis(&axis_u, &axis_v, &normal);

    half_u = vec3_scale(axis_u, primitive->shape.rectPrism.width * 0.5);
    half_v = vec3_scale(axis_v, primitive->shape.rectPrism.height * 0.5);
    half_n = vec3_scale(normal, primitive->shape.rectPrism.depth * 0.5);
    scene_object_index = primitive->source.sceneObjectIndex;

    bottom0 = vec3_sub(vec3_sub(vec3_sub(origin, half_u), half_v), half_n);
    bottom1 = vec3_add(vec3_sub(vec3_sub(origin, half_v), half_n), half_u);
    bottom2 = vec3_add(vec3_add(vec3_sub(origin, half_n), half_u), half_v);
    bottom3 = vec3_add(vec3_sub(vec3_sub(origin, half_n), half_u), half_v);

    top0 = vec3_add(bottom0, vec3_scale(half_n, 2.0));
    top1 = vec3_add(bottom1, vec3_scale(half_n, 2.0));
    top2 = vec3_add(bottom2, vec3_scale(half_n, 2.0));
    top3 = vec3_add(bottom3, vec3_scale(half_n, 2.0));

    if (!runtime_scene_3d_builder_append_quad(scene,
                                              primitive_index,
                                              scene_object_index,
                                              top0,
                                              top1,
                                              top2,
                                              top3,
                                              normal)) {
        return false;
    }
    if (!runtime_scene_3d_builder_append_quad(scene,
                                              primitive_index,
                                              scene_object_index,
                                              bottom0,
                                              bottom3,
                                              bottom2,
                                              bottom1,
                                              vec3_scale(normal, -1.0))) {
        return false;
    }
    if (!runtime_scene_3d_builder_append_quad(scene,
                                              primitive_index,
                                              scene_object_index,
                                              bottom0,
                                              bottom1,
                                              top1,
                                              top0,
                                              vec3_scale(axis_v, -1.0))) {
        return false;
    }
    if (!runtime_scene_3d_builder_append_quad(scene,
                                              primitive_index,
                                              scene_object_index,
                                              bottom3,
                                              top3,
                                              top2,
                                              bottom2,
                                              axis_v)) {
        return false;
    }
    if (!runtime_scene_3d_builder_append_quad(scene,
                                              primitive_index,
                                              scene_object_index,
                                              bottom0,
                                              top0,
                                              top3,
                                              bottom3,
                                              vec3_scale(axis_u, -1.0))) {
        return false;
    }
    return runtime_scene_3d_builder_append_quad(scene,
                                                primitive_index,
                                                scene_object_index,
                                                bottom1,
                                                bottom2,
                                                top2,
                                                top1,
                                                axis_u);
}

static bool runtime_scene_3d_builder_append_triangles(RuntimeScene3D* scene,
                                                      int primitive_index,
                                                      const RuntimePrimitive3D* primitive) {
    if (!scene || !primitive) return false;
    switch (primitive->kind) {
        case RUNTIME_PRIMITIVE_3D_KIND_PLANE:
            return runtime_scene_3d_builder_append_plane(scene, primitive_index, primitive);
        case RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM:
            return runtime_scene_3d_builder_append_rect_prism(scene, primitive_index, primitive);
        case RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH:
        case RUNTIME_PRIMITIVE_3D_KIND_INVALID:
        default:
            return false;
    }
}

static void runtime_scene_3d_builder_fill_primitive(RuntimePrimitive3D* primitive,
                                                    const RuntimeSceneBridgePrimitiveSeed* seed) {
    Vec3 origin;
    Vec3 axis_u;
    Vec3 axis_v;
    Vec3 normal;
    RuntimePrimitive3DKind kind;
    if (!primitive || !seed) return;

    memset(primitive, 0, sizeof(*primitive));
    kind = runtime_scene_3d_builder_map_kind(seed->kind);
    primitive->kind = kind;
    primitive->source.kind = kind;
    primitive->source.sceneObjectIndex = seed->scene_object_index;
    snprintf(primitive->source.objectId,
             sizeof(primitive->source.objectId),
             "%s",
             seed->object_id);

    origin = vec3(seed->origin_x, seed->origin_y, seed->origin_z);
    axis_u = vec3(seed->axis_u_x, seed->axis_u_y, seed->axis_u_z);
    axis_v = vec3(seed->axis_v_x, seed->axis_v_y, seed->axis_v_z);
    normal = vec3(seed->normal_x, seed->normal_y, seed->normal_z);
    runtime_scene_3d_builder_resolve_basis(&axis_u, &axis_v, &normal);

    if (kind == RUNTIME_PRIMITIVE_3D_KIND_PLANE) {
        primitive->shape.plane.origin = origin;
        primitive->shape.plane.axisU = axis_u;
        primitive->shape.plane.axisV = axis_v;
        primitive->shape.plane.normal = normal;
        primitive->shape.plane.width = seed->width;
        primitive->shape.plane.height = seed->height;
    } else if (kind == RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM) {
        primitive->shape.rectPrism.origin = origin;
        primitive->shape.rectPrism.axisU = axis_u;
        primitive->shape.rectPrism.axisV = axis_v;
        primitive->shape.rectPrism.normal = normal;
        primitive->shape.rectPrism.width = seed->width;
        primitive->shape.rectPrism.height = seed->height;
        primitive->shape.rectPrism.depth = seed->depth;
    }
}

static void runtime_scene_3d_builder_apply_authored_samples(RuntimeScene3D* scene,
                                                            double normalized_t) {
    RuntimeLight3D light = {0};
    RuntimeCamera3D camera = {0};
    if (!scene) return;

    scene->environment.lightMode =
        animation_config_environment_light_mode_clamp(animSettings.environmentLightMode);
    scene->environment.ambientIntensity = fmax(0.0, fmin(1.0, animSettings.environmentBrightness / 255.0));
    scene->environment.topFillIntensity = fmax(0.0, fmin(20.0, animSettings.topFillStrength));
    scene->environment.ambientColor = vec3(1.0, 1.0, 1.0);
    scene->environment.backgroundTopColor = vec3(0.66, 0.70, 0.76);
    scene->environment.backgroundBottomColor = vec3(0.84, 0.85, 0.87);
    scene->environment.topDownBias = 0.18;

    if (RuntimeScene3DSampleAuthoredLight(normalized_t, &light)) {
        scene->light = light;
        scene->hasLight = true;
    }
    if (RuntimeScene3DSampleAuthoredCamera(normalized_t, &camera)) {
        scene->camera = camera;
        scene->hasCamera = true;
    }
}

bool RuntimeScene3DBuilder_BuildFromPrimitiveSeedStateAtT(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state,
    double normalized_t) {
    int retained_primitive_count = 0;
    int expected_triangle_count = 0;

    if (!scene || !seed_state || !seed_state->valid) return false;

    RuntimeScene3D_Reset(scene);
    runtime_scene_3d_builder_apply_authored_samples(scene, normalized_t);
    retained_primitive_count = seed_state->primitive_count;
    for (int i = 0; i < seed_state->primitive_count; ++i) {
        RuntimePrimitive3DKind kind =
            runtime_scene_3d_builder_map_kind(seed_state->primitives[i].kind);
        if (!RuntimePrimitive3DKindSupportedByR0(kind)) continue;
        expected_triangle_count += runtime_scene_3d_builder_triangle_count_for_kind(kind);
    }

    if (retained_primitive_count <= 0) return true;
    if (!runtime_scene_3d_builder_reserve_primitives(scene, retained_primitive_count)) return false;
    if (!runtime_scene_3d_builder_reserve_triangles(scene, expected_triangle_count)) return false;

    for (int i = 0; i < seed_state->primitive_count; ++i) {
        RuntimePrimitive3DKind kind =
            runtime_scene_3d_builder_map_kind(seed_state->primitives[i].kind);
        RuntimePrimitive3D* primitive = NULL;
        if (!RuntimePrimitive3DKindSupportedByR0(kind)) continue;

        primitive = &scene->primitives[scene->primitiveCount];
        runtime_scene_3d_builder_fill_primitive(primitive, &seed_state->primitives[i]);
        if (!runtime_scene_3d_builder_append_triangles(scene, scene->primitiveCount, primitive)) {
            RuntimeScene3D_Reset(scene);
            return false;
        }
        scene->primitiveCount += 1;
    }

    return true;
}

bool RuntimeScene3DBuilder_BuildFromPrimitiveSeedState(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state) {
    return RuntimeScene3DBuilder_BuildFromPrimitiveSeedStateAtT(scene, seed_state, 0.0);
}

bool RuntimeScene3DBuilder_BuildFromBridgeSeeds(RuntimeScene3D* scene) {
    return RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(scene, 0.0);
}

bool RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(RuntimeScene3D* scene, double normalized_t) {
    RuntimeSceneBridge3DPrimitiveSeedState seed_state = {0};
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seed_state);
    return RuntimeScene3DBuilder_BuildFromPrimitiveSeedStateAtT(scene, &seed_state, normalized_t);
}
