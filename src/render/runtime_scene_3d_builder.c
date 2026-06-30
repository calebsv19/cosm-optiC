#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_mesh_blas_cache_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/runtime_triangle_bvh_3d.h"
#include "render/runtime_scene_3d_samples.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static char gRuntimeScene3DBuilderLastDiagnostics[4096] = "ok";
static RuntimeScene3DBuilderTimingStats gRuntimeScene3DBuilderTiming;

static double runtime_scene_3d_builder_elapsed_ms_since(const struct timespec* start_time) {
    struct timespec now = {0};
    double elapsed = 0.0;
    if (!start_time) return 0.0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    elapsed = (double)(now.tv_sec - start_time->tv_sec) * 1000.0;
    elapsed += (double)(now.tv_nsec - start_time->tv_nsec) / 1000000.0;
    return elapsed < 0.0 ? 0.0 : elapsed;
}

static void runtime_scene_3d_builder_set_diag(const char* message) {
    snprintf(gRuntimeScene3DBuilderLastDiagnostics,
             sizeof(gRuntimeScene3DBuilderLastDiagnostics),
             "%s",
             (message && message[0]) ? message : "ok");
}

const char* RuntimeScene3DBuilder_LastDiagnostics(void) {
    return gRuntimeScene3DBuilderLastDiagnostics;
}

void RuntimeScene3DBuilder_TimingReset(void) {
    memset(&gRuntimeScene3DBuilderTiming, 0, sizeof(gRuntimeScene3DBuilderTiming));
}

void RuntimeScene3DBuilder_TimingSnapshot(RuntimeScene3DBuilderTimingStats* out_stats) {
    if (!out_stats) return;
    *out_stats = gRuntimeScene3DBuilderTiming;
}

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

static Vec3 runtime_scene_3d_builder_rotate_instance(Vec3 p,
                                                     const RayTracingRuntimeMeshAssetInstance* instance) {
    double cx = cos(instance->rotation_x);
    double sx = sin(instance->rotation_x);
    double cy = cos(instance->rotation_y);
    double sy = sin(instance->rotation_y);
    double cz = cos(instance->rotation_z);
    double sz = sin(instance->rotation_z);
    Vec3 q = p;
    Vec3 r = q;

    q.y = r.y * cx - r.z * sx;
    q.z = r.y * sx + r.z * cx;
    r = q;
    q.x = r.x * cy + r.z * sy;
    q.z = -r.x * sy + r.z * cy;
    r = q;
    q.x = r.x * cz - r.y * sz;
    q.y = r.x * sz + r.y * cz;
    return q;
}

static Vec3 runtime_scene_3d_builder_transform_mesh_vertex(
    const CoreMeshAssetRuntimeVertex* vertex,
    const RayTracingRuntimeMeshAssetInstance* instance,
    Vec3 pivot) {
    Vec3 p = vec3(vertex->position.x * instance->scale_x,
                  vertex->position.y * instance->scale_y,
                  vertex->position.z * instance->scale_z);
    p = vec3_sub(p, pivot);
    p = runtime_scene_3d_builder_rotate_instance(p, instance);
    p = vec3_add(p, pivot);
    return vec3_add(p,
                    vec3(instance->position_x,
                         instance->position_y,
                         instance->position_z));
}

typedef struct {
    Vec3 min;
    Vec3 max;
    Vec3 extent;
    bool valid;
} RuntimeScene3DBuilderMeshBounds;

static Vec3 runtime_scene_3d_builder_mesh_rotation_pivot(
    const CoreMeshAssetRuntimeDocument* document,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    if (!document || !instance) {
        return vec3(0.0, 0.0, 0.0);
    }
    if (instance->rotation_pivot_policy ==
        RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM) {
        return vec3(instance->rotation_pivot_x * instance->scale_x,
                    instance->rotation_pivot_y * instance->scale_y,
                    instance->rotation_pivot_z * instance->scale_z);
    }
    if (document->vertex_count == 0u || !document->vertices ||
        instance->rotation_pivot_policy !=
            RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_BOUNDS_CENTER) {
        return vec3(0.0, 0.0, 0.0);
    }
    min = document->contract.local_bounds.min;
    max = document->contract.local_bounds.max;
    if (min.x == 0.0 && min.y == 0.0 && min.z == 0.0 &&
        max.x == 0.0 && max.y == 0.0 && max.z == 0.0) {
        min = document->vertices[0].position;
        max = document->vertices[0].position;
        for (size_t i = 1u; i < document->vertex_count; ++i) {
            const CoreObjectVec3 p = document->vertices[i].position;
            if (p.x < min.x) min.x = p.x;
            if (p.y < min.y) min.y = p.y;
            if (p.z < min.z) min.z = p.z;
            if (p.x > max.x) max.x = p.x;
            if (p.y > max.y) max.y = p.y;
            if (p.z > max.z) max.z = p.z;
        }
    }
    return vec3(((min.x + max.x) * 0.5) * instance->scale_x,
                ((min.y + max.y) * 0.5) * instance->scale_y,
                ((min.z + max.z) * 0.5) * instance->scale_z);
}

static bool runtime_scene_3d_builder_rebuild_bvh(RuntimeScene3D* scene) {
    struct timespec stage_start = {0};
    if (!scene) {
        runtime_scene_3d_builder_set_diag("bvh rebuild failed: scene missing");
        return false;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    if (!RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh)) {
        gRuntimeScene3DBuilderTiming.bvh_rebuild_wall_ms +=
            runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
        char diag[2048];
        snprintf(diag,
                 sizeof(diag),
                 "bvh rebuild failed: %s",
                 RuntimeTriangleMesh3D_BVHLastDiagnostics());
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    gRuntimeScene3DBuilderTiming.bvh_rebuild_wall_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
    if (scene->triangleMesh.triangleCount > 0 &&
        !RuntimeTriangleMesh3D_HasReadyBVH(&scene->triangleMesh)) {
        runtime_scene_3d_builder_set_diag("bvh rebuild failed: BVH not ready after build");
        return false;
    }
    return true;
}

static bool runtime_scene_3d_builder_rebuild_tlas(RuntimeScene3D* scene) {
    if (!RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene)) {
        char diag[2048];
        snprintf(diag,
                 sizeof(diag),
                 "TLAS rebuild failed: %s",
                 RuntimeSceneAcceleration3D_LastDiagnostics());
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    return true;
}

static RuntimeScene3DBuilderMeshBounds runtime_scene_3d_builder_mesh_bounds(
    const CoreMeshAssetRuntimeDocument* document,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    RuntimeScene3DBuilderMeshBounds bounds = {0};
    Vec3 pivot = runtime_scene_3d_builder_mesh_rotation_pivot(document, instance);
    if (!document || !instance || document->vertex_count == 0u) {
        return bounds;
    }
    for (size_t i = 0; i < document->vertex_count; ++i) {
        Vec3 p =
            runtime_scene_3d_builder_transform_mesh_vertex(&document->vertices[i],
                                                          instance,
                                                          pivot);
        if (i == 0u) {
            bounds.min = p;
            bounds.max = p;
        } else {
            if (p.x < bounds.min.x) bounds.min.x = p.x;
            if (p.y < bounds.min.y) bounds.min.y = p.y;
            if (p.z < bounds.min.z) bounds.min.z = p.z;
            if (p.x > bounds.max.x) bounds.max.x = p.x;
            if (p.y > bounds.max.y) bounds.max.y = p.y;
            if (p.z > bounds.max.z) bounds.max.z = p.z;
        }
    }
    bounds.extent = vec3(bounds.max.x - bounds.min.x,
                         bounds.max.y - bounds.min.y,
                         bounds.max.z - bounds.min.z);
    bounds.valid = true;
    return bounds;
}

static double runtime_scene_3d_builder_normalize_axis(double value,
                                                      double min_value,
                                                      double extent) {
    if (extent <= 1e-9) return 0.5;
    return (value - min_value) / extent;
}

static Vec3 runtime_scene_3d_builder_object_texture_coord(
    Vec3 p,
    const RuntimeScene3DBuilderMeshBounds* bounds) {
    if (!bounds || !bounds->valid) return vec3(0.5, 0.5, 0.5);
    return vec3(runtime_scene_3d_builder_normalize_axis(p.x,
                                                        bounds->min.x,
                                                        bounds->extent.x),
                runtime_scene_3d_builder_normalize_axis(p.y,
                                                        bounds->min.y,
                                                        bounds->extent.y),
                runtime_scene_3d_builder_normalize_axis(p.z,
                                                        bounds->min.z,
                                                        bounds->extent.z));
}

static int runtime_scene_3d_builder_resolve_mesh_scene_object_index(
    const RayTracingRuntimeMeshAssetInstance* instance) {
    char object_id[RUNTIME_SCENE_3D_MAX_OBJECT_ID] = {0};
    if (!instance) return -1;
    if (instance->scene_object_index >= 0 &&
        runtime_scene_bridge_get_last_object_id_for_scene_index(instance->scene_object_index,
                                                                object_id,
                                                                sizeof(object_id)) &&
        strcmp(object_id, instance->object_id) == 0) {
        return instance->scene_object_index;
    }
    for (int i = 0; i < MAX_OBJECTS; ++i) {
        if (runtime_scene_bridge_get_last_object_id_for_scene_index(i,
                                                                    object_id,
                                                                    sizeof(object_id)) &&
            strcmp(object_id, instance->object_id) == 0) {
            return i;
        }
    }
    return instance->scene_object_index;
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

static bool runtime_scene_3d_builder_append_triangle_internal(RuntimeScene3D* scene,
                                                              int primitive_index,
                                                              int scene_object_index,
                                                              Vec3 p0,
                                                              Vec3 p1,
                                                              Vec3 p2,
                                                              Vec3 expected_normal,
                                                              int local_triangle_index_override,
                                                              bool two_sided,
                                                              bool has_object_texture_coords,
                                                              Vec3 object_texture0,
                                                              Vec3 object_texture1,
                                                              Vec3 object_texture2) {
    RuntimeTriangle3D* triangle = NULL;
    Vec3 edge1;
    Vec3 edge2;
    Vec3 normal;
    int local_triangle_index = local_triangle_index_override;
    if (!scene) return false;
    if (scene->triangleMesh.triangleCount >= scene->triangleMesh.triangleCapacity) return false;

    edge1 = vec3_sub(p1, p0);
    edge2 = vec3_sub(p2, p0);
    normal = vec3_normalize(vec3_cross(edge1, edge2));
    if (vec3_dot(normal, expected_normal) < 0.0) {
        Vec3 swap = p1;
        Vec3 swap_texture = object_texture1;
        p1 = p2;
        p2 = swap;
        object_texture1 = object_texture2;
        object_texture2 = swap_texture;
        edge1 = vec3_sub(p1, p0);
        edge2 = vec3_sub(p2, p0);
        normal = vec3_normalize(vec3_cross(edge1, edge2));
    }

    triangle = &scene->triangleMesh.triangles[scene->triangleMesh.triangleCount++];
    memset(triangle, 0, sizeof(*triangle));
    if (local_triangle_index < 0) {
        local_triangle_index = 0;
        for (int i = 0; i < scene->triangleMesh.triangleCount - 1; ++i) {
            if (scene->triangleMesh.triangles[i].sceneObjectIndex == scene_object_index &&
                scene->triangleMesh.triangles[i].primitiveIndex == primitive_index) {
                local_triangle_index += 1;
            }
        }
    }
    triangle->p0 = p0;
    triangle->p1 = p1;
    triangle->p2 = p2;
    triangle->normal = normal;
    triangle->twoSided = two_sided;
    triangle->hasObjectTextureCoords = has_object_texture_coords;
    triangle->objectTexture0 = object_texture0;
    triangle->objectTexture1 = object_texture1;
    triangle->objectTexture2 = object_texture2;
    triangle->primitiveIndex = primitive_index;
    triangle->sceneObjectIndex = scene_object_index;
    triangle->localTriangleIndex = local_triangle_index;
    scene->triangleMesh.bvhDirty = true;
    return true;
}

static bool runtime_scene_3d_builder_append_quad(RuntimeScene3D* scene,
                                                 int primitive_index,
                                                 int scene_object_index,
                                                 Vec3 p0,
                                                 Vec3 p1,
                                                 Vec3 p2,
                                                 Vec3 p3,
                                                 Vec3 expected_normal,
                                                 bool two_sided) {
    if (!runtime_scene_3d_builder_append_triangle_internal(scene,
                                                           primitive_index,
                                                           scene_object_index,
                                                           p0,
                                                           p1,
                                                           p2,
                                                           expected_normal,
                                                           -1,
                                                           two_sided,
                                                           false,
                                                           vec3(0.0, 0.0, 0.0),
                                                           vec3(0.0, 0.0, 0.0),
                                                           vec3(0.0, 0.0, 0.0))) {
        return false;
    }
    return runtime_scene_3d_builder_append_triangle_internal(scene,
                                                            primitive_index,
                                                            scene_object_index,
                                                            p0,
                                                            p2,
                                                            p3,
                                                            expected_normal,
                                                            -1,
                                                            two_sided,
                                                            false,
                                                            vec3(0.0, 0.0, 0.0),
                                                            vec3(0.0, 0.0, 0.0),
                                                            vec3(0.0, 0.0, 0.0));
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
                                                normal,
                                                true);
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
                                              normal,
                                              false)) {
        return false;
    }
    if (!runtime_scene_3d_builder_append_quad(scene,
                                              primitive_index,
                                              scene_object_index,
                                              bottom0,
                                              bottom3,
                                              bottom2,
                                              bottom1,
                                              vec3_scale(normal, -1.0),
                                              false)) {
        return false;
    }
    if (!runtime_scene_3d_builder_append_quad(scene,
                                              primitive_index,
                                              scene_object_index,
                                              bottom0,
                                              bottom1,
                                              top1,
                                              top0,
                                              vec3_scale(axis_v, -1.0),
                                              false)) {
        return false;
    }
    if (!runtime_scene_3d_builder_append_quad(scene,
                                              primitive_index,
                                              scene_object_index,
                                              bottom3,
                                              top3,
                                              top2,
                                              bottom2,
                                              axis_v,
                                              false)) {
        return false;
    }
    if (!runtime_scene_3d_builder_append_quad(scene,
                                              primitive_index,
                                              scene_object_index,
                                              bottom0,
                                              top0,
                                              top3,
                                              bottom3,
                                              vec3_scale(axis_u, -1.0),
                                              false)) {
        return false;
    }
    return runtime_scene_3d_builder_append_quad(scene,
                                                primitive_index,
                                                scene_object_index,
                                                bottom1,
                                                bottom2,
                                                top2,
                                                top1,
                                                axis_u,
                                                false);
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

static bool runtime_scene_3d_builder_heightfield_quad_is_dry(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    uint32_t x,
    uint32_t z) {
    const uint32_t w = desc ? desc->grid_w : 0u;
    const double threshold = desc ? desc->dry_height + desc->dry_height_epsilon : 0.0;
    double h00;
    double h10;
    double h11;
    double h01;
    if (!desc || !desc->heights_y || w == 0u) return true;
    if (!desc->skip_dry_quads) return false;
    h00 = desc->heights_y[(size_t)z * (size_t)w + (size_t)x];
    h10 = desc->heights_y[(size_t)z * (size_t)w + (size_t)(x + 1u)];
    h11 = desc->heights_y[(size_t)(z + 1u) * (size_t)w + (size_t)(x + 1u)];
    h01 = desc->heights_y[(size_t)(z + 1u) * (size_t)w + (size_t)x];
    return h00 <= threshold && h10 <= threshold && h11 <= threshold && h01 <= threshold;
}

static int runtime_scene_3d_builder_heightfield_quad_count(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc) {
    int count = 0;
    if (!desc || desc->grid_w < 2u || desc->grid_d < 2u || !desc->heights_y) return 0;
    for (uint32_t z = 0u; z + 1u < desc->grid_d; ++z) {
        for (uint32_t x = 0u; x + 1u < desc->grid_w; ++x) {
            if (!runtime_scene_3d_builder_heightfield_quad_is_dry(desc, x, z)) {
                count += 1;
            }
        }
    }
    return count;
}

static Vec3 runtime_scene_3d_builder_heightfield_point(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    uint32_t x,
    uint32_t z) {
    const size_t index = (size_t)z * (size_t)desc->grid_w + (size_t)x;
    const double sample_x = desc->sample_origin_x + ((double)x * desc->sample_spacing_x);
    const double sample_z = desc->sample_origin_z + ((double)z * desc->sample_spacing_z);
    const double height_y = desc->heights_y[index];
    if (desc->map_y_height_to_scene_z) {
        return vec3(sample_x, sample_z, height_y);
    }
    return vec3(sample_x, height_y, sample_z);
}

static bool runtime_scene_3d_builder_heightfield_desc_valid(
    const RuntimeScene3DHeightfieldSurfaceDesc* desc) {
    if (!desc || !desc->heights_y) return false;
    if (desc->scene_object_index < 0 || desc->grid_w < 2u || desc->grid_d < 2u) return false;
    if (!(desc->sample_spacing_x > 0.0) || !(desc->sample_spacing_z > 0.0)) return false;
    if (!isfinite(desc->sample_origin_x) ||
        !isfinite(desc->sample_origin_z) ||
        !isfinite(desc->sample_spacing_x) ||
        !isfinite(desc->sample_spacing_z) ||
        !isfinite(desc->dry_height) ||
        !isfinite(desc->dry_height_epsilon)) {
        return false;
    }
    for (uint64_t i = 0u; i < (uint64_t)desc->grid_w * (uint64_t)desc->grid_d; ++i) {
        if (!isfinite(desc->heights_y[i])) return false;
    }
    return true;
}

bool RuntimeScene3DBuilder_AppendHeightfieldSurface(
    RuntimeScene3D* scene,
    const RuntimeScene3DHeightfieldSurfaceDesc* desc,
    int* out_appended_triangle_count) {
    const int old_primitive_count = scene ? scene->primitiveCount : 0;
    const int old_triangle_count = scene ? scene->triangleMesh.triangleCount : 0;
    const int valid_quad_count =
        runtime_scene_3d_builder_heightfield_quad_count(desc);
    const int extra_triangle_count = valid_quad_count * 2;
    RuntimePrimitive3D* primitive = NULL;
    int primitive_index = 0;
    int appended_triangle_count = 0;

    if (out_appended_triangle_count) *out_appended_triangle_count = 0;
    if (!scene || !runtime_scene_3d_builder_heightfield_desc_valid(desc)) return false;
    if (valid_quad_count <= 0) return true;
    if (!runtime_scene_3d_builder_reserve_primitives(scene, scene->primitiveCount + 1) ||
        !runtime_scene_3d_builder_reserve_triangles(scene,
                                                    scene->triangleMesh.triangleCount +
                                                        extra_triangle_count)) {
        return false;
    }

    primitive_index = scene->primitiveCount;
    primitive = &scene->primitives[primitive_index];
    memset(primitive, 0, sizeof(*primitive));
    primitive->kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    primitive->source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    primitive->source.sceneObjectIndex = desc->scene_object_index;
    snprintf(primitive->source.objectId,
             sizeof(primitive->source.objectId),
             "%s",
             (desc->object_id && desc->object_id[0]) ? desc->object_id : "water_surface");

    for (uint32_t z = 0u; z + 1u < desc->grid_d; ++z) {
        for (uint32_t x = 0u; x + 1u < desc->grid_w; ++x) {
            Vec3 p00;
            Vec3 p10;
            Vec3 p11;
            Vec3 p01;
            const Vec3 expected_normal = desc->map_y_height_to_scene_z
                                             ? vec3(0.0, 0.0, 1.0)
                                             : vec3(0.0, 1.0, 0.0);
            if (runtime_scene_3d_builder_heightfield_quad_is_dry(desc, x, z)) {
                continue;
            }
            p00 = runtime_scene_3d_builder_heightfield_point(desc, x, z);
            p10 = runtime_scene_3d_builder_heightfield_point(desc, x + 1u, z);
            p11 = runtime_scene_3d_builder_heightfield_point(desc, x + 1u, z + 1u);
            p01 = runtime_scene_3d_builder_heightfield_point(desc, x, z + 1u);
            if (!runtime_scene_3d_builder_append_quad(scene,
                                                      primitive_index,
                                                      desc->scene_object_index,
                                                      p00,
                                                      p01,
                                                      p11,
                                                      p10,
                                                      expected_normal,
                                                      desc->two_sided)) {
                scene->primitiveCount = old_primitive_count;
                scene->triangleMesh.triangleCount = old_triangle_count;
                scene->triangleMesh.bvhDirty = true;
                (void)runtime_scene_3d_builder_rebuild_bvh(scene);
                return false;
            }
            appended_triangle_count += 2;
        }
    }

    if (appended_triangle_count <= 0) {
        scene->primitiveCount = old_primitive_count;
        scene->triangleMesh.triangleCount = old_triangle_count;
        scene->triangleMesh.bvhDirty = true;
        return runtime_scene_3d_builder_rebuild_bvh(scene);
    }

    scene->primitiveCount += 1;
    scene->scope.triangleMeshEnabled = true;
    if (!runtime_scene_3d_builder_rebuild_bvh(scene)) {
        scene->primitiveCount = old_primitive_count;
        scene->triangleMesh.triangleCount = old_triangle_count;
        scene->triangleMesh.bvhDirty = true;
        (void)runtime_scene_3d_builder_rebuild_bvh(scene);
        return false;
    }
    if (out_appended_triangle_count) {
        *out_appended_triangle_count = appended_triangle_count;
    }
    return true;
}

static bool runtime_scene_3d_builder_append_mesh_asset_set(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets,
    bool require_ready_bvh) {
    int extra_primitives = 0;
    int extra_triangles = 0;
    struct timespec total_start = {0};
    struct timespec stage_start = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &total_start);
    gRuntimeScene3DBuilderTiming.mesh_append_calls += 1;
    if (!scene || !mesh_assets) {
        runtime_scene_3d_builder_set_diag("append mesh assets failed: scene or mesh set missing");
        return false;
    }
    if (mesh_assets->instance_count <= 0) return true;
    if (!RuntimeMeshBLASCache3D_PrepareAssetSet(mesh_assets)) {
        char diag[2048];
        snprintf(diag,
                 sizeof(diag),
                 "append mesh assets failed: BLAS cache prepare failed: %s",
                 RuntimeMeshBLASCache3D_LastDiagnostics());
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }

    gRuntimeScene3DBuilderTiming.mesh_append_assets += mesh_assets->asset_count;
    gRuntimeScene3DBuilderTiming.mesh_append_instances += mesh_assets->instance_count;
    for (int i = 0; i < mesh_assets->instance_count; ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance = &mesh_assets->instances[i];
        if (instance->asset_index < 0 || instance->asset_index >= mesh_assets->asset_count) {
            runtime_scene_3d_builder_set_diag("append mesh assets failed: invalid asset index");
            return false;
        }
        extra_primitives += 1;
        extra_triangles += (int)mesh_assets->assets[instance->asset_index].document.triangle_count;
    }
    gRuntimeScene3DBuilderTiming.mesh_append_triangles_expected += extra_triangles;
    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    if (!runtime_scene_3d_builder_reserve_primitives(scene,
                                                     scene->primitiveCount + extra_primitives)) {
        gRuntimeScene3DBuilderTiming.mesh_append_reserve_ms +=
            runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
        runtime_scene_3d_builder_set_diag("append mesh assets failed: primitive reserve failed");
        return false;
    }
    if (!runtime_scene_3d_builder_reserve_triangles(
            scene,
            scene->triangleMesh.triangleCount + extra_triangles)) {
        gRuntimeScene3DBuilderTiming.mesh_append_reserve_ms +=
            runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
        runtime_scene_3d_builder_set_diag("append mesh assets failed: triangle reserve failed");
        return false;
    }
    gRuntimeScene3DBuilderTiming.mesh_append_reserve_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&stage_start);

    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    for (int i = 0; i < mesh_assets->instance_count; ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance = &mesh_assets->instances[i];
        const RayTracingRuntimeMeshAsset* asset = &mesh_assets->assets[instance->asset_index];
        const CoreMeshAssetRuntimeDocument* document = &asset->document;
        RuntimePrimitive3D* primitive = &scene->primitives[scene->primitiveCount];
        int primitive_index = scene->primitiveCount;
        int scene_object_index =
            runtime_scene_3d_builder_resolve_mesh_scene_object_index(instance);
        RuntimeScene3DBuilderMeshBounds bounds =
            runtime_scene_3d_builder_mesh_bounds(document, instance);
        Vec3 pivot = runtime_scene_3d_builder_mesh_rotation_pivot(document, instance);
        int appended_triangle_count = 0;

        memset(primitive, 0, sizeof(*primitive));
        primitive->kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        primitive->source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        primitive->source.sceneObjectIndex = scene_object_index;
        snprintf(primitive->source.objectId,
                 sizeof(primitive->source.objectId),
                 "%s",
                 instance->object_id);

        for (size_t j = 0; j < document->triangle_count; ++j) {
            const CoreMeshAssetRuntimeTriangle* src = &document->triangles[j];
            Vec3 p0 = runtime_scene_3d_builder_transform_mesh_vertex(
                &document->vertices[src->a],
                instance,
                pivot);
            Vec3 p1 = runtime_scene_3d_builder_transform_mesh_vertex(
                &document->vertices[src->b],
                instance,
                pivot);
            Vec3 p2 = runtime_scene_3d_builder_transform_mesh_vertex(
                &document->vertices[src->c],
                instance,
                pivot);
            Vec3 tex0 = runtime_scene_3d_builder_object_texture_coord(p0, &bounds);
            Vec3 tex1 = runtime_scene_3d_builder_object_texture_coord(p1, &bounds);
            Vec3 tex2 = runtime_scene_3d_builder_object_texture_coord(p2, &bounds);
            Vec3 expected_normal = vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0));
            double normal_len = vec3_length(expected_normal);
            if (normal_len <= 1e-18) continue;
            expected_normal = vec3_scale(expected_normal, 1.0 / normal_len);
            if (!runtime_scene_3d_builder_append_triangle_internal(scene,
                                                                   primitive_index,
                                                                   scene_object_index,
                                                                   p0,
                                                                   p1,
                                                                   p2,
                                                                   expected_normal,
                                                                   (int)j,
                                                                   false,
                                                                   bounds.valid,
                                                                   tex0,
                                                                   tex1,
                                                                   tex2)) {
                runtime_scene_3d_builder_set_diag("append mesh assets failed: triangle append failed");
                RuntimeScene3D_Reset(scene);
                return false;
            }
            appended_triangle_count += 1;
        }
        gRuntimeScene3DBuilderTiming.mesh_append_triangles_appended +=
            appended_triangle_count;
        if (appended_triangle_count <= 0) continue;
        scene->primitiveCount += 1;
    }
    gRuntimeScene3DBuilderTiming.mesh_append_expand_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
    scene->scope.triangleMeshEnabled = true;
    if (require_ready_bvh) {
        if (!runtime_scene_3d_builder_rebuild_tlas(scene) ||
            !runtime_scene_3d_builder_rebuild_bvh(scene)) {
            RuntimeScene3D_Reset(scene);
            return false;
        }
    }
    gRuntimeScene3DBuilderTiming.mesh_append_total_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&total_start);
    return true;
}

bool RuntimeScene3DBuilder_AppendMeshAssetSet(
    RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets) {
    return runtime_scene_3d_builder_append_mesh_asset_set(scene, mesh_assets, true);
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

static RuntimeLight3D runtime_scene_3d_builder_compat_light_from_source(
    const RuntimeLightSource3D* source) {
    RuntimeLight3D light = {0};
    if (!source) return light;
    light.position = source->position;
    light.radius = source->radius;
    if (light.radius <= 0.0) {
        if (source->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_DISK && source->width > 0.0) {
            light.radius = source->width * 0.5;
        } else if (source->kind == RUNTIME_LIGHT_SOURCE_3D_KIND_RECT &&
                   (source->width > 0.0 || source->height > 0.0)) {
            light.radius = 0.5 * sqrt(source->width * source->width +
                                      source->height * source->height);
        }
    }
    light.intensity = source->intensity;
    light.falloffDistance = source->falloffDistance;
    light.falloffMode = source->falloffMode;
    return light;
}

static RuntimeLightSource3D runtime_scene_3d_builder_normalized_light_source(
    const RuntimeLightSource3D* source) {
    RuntimeLightSource3D normalized = {0};
    double authored_radius = animSettings.lightRadius;

    if (!source) return normalized;
    normalized = *source;
    if (normalized.radius <= 0.0 &&
        (normalized.kind == RUNTIME_LIGHT_SOURCE_3D_KIND_POINT ||
         normalized.kind == RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE) &&
        authored_radius > 0.0) {
        normalized.radius = authored_radius;
        normalized.kind = RUNTIME_LIGHT_SOURCE_3D_KIND_SPHERE;
    }
    normalized.falloffMode = animSettings.forwardFalloffMode;
    return normalized;
}

static bool runtime_scene_3d_builder_apply_light_seed_state(RuntimeScene3D* scene) {
    RuntimeSceneBridge3DLightSeedState light_state = {0};
    if (!scene) return false;
    runtime_scene_bridge_get_last_3d_light_seed_state(&light_state);
    if (!light_state.valid || light_state.light_count <= 0) {
        return true;
    }
    RuntimeLightSet3D_Reset(&scene->lightSet);
    for (int i = 0; i < light_state.light_count; ++i) {
        RuntimeLightSource3D source =
            runtime_scene_3d_builder_normalized_light_source(&light_state.lights[i]);
        if (!RuntimeLightSet3D_Append(&scene->lightSet, &source, NULL)) {
            runtime_scene_3d_builder_set_diag("authored light seed build failed: append failed");
            return false;
        }
    }
    if (scene->lightSet.lightCount > 0) {
        const RuntimeLightSource3D* first_enabled =
            RuntimeLightSet3D_GetEnabled(&scene->lightSet, 0);
        if (!first_enabled) first_enabled = &scene->lightSet.lights[0];
        scene->light = runtime_scene_3d_builder_compat_light_from_source(first_enabled);
        scene->hasLight = first_enabled->enabled;
    }
    return true;
}

static bool runtime_scene_3d_builder_apply_authored_samples(RuntimeScene3D* scene,
                                                            double normalized_t) {
    RuntimeLight3D light = {0};
    RuntimeCamera3D camera = {0};
    RuntimeSceneBridge3DScaffoldState scaffold = {0};
    if (!scene) return false;

    RuntimeEnvironment3D_ResolveFromAnimationConfig(&scene->environment, &animSettings);

    if (!runtime_scene_3d_builder_apply_light_seed_state(scene)) {
        return false;
    }
    runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
    if (scaffold.has_light_path && RuntimeScene3DSampleAuthoredLight(normalized_t, &light)) {
        scene->light = light;
        scene->hasLight = true;
        if (scene->lightSet.lightCount > 0) {
            if (!RuntimeLightSet3D_UpdateFirstEnabledFromCompatibilityLight(&scene->lightSet,
                                                                            &scene->light)) {
                runtime_scene_3d_builder_set_diag("authored light seed motion update failed");
                return false;
            }
        } else if (!RuntimeLightSet3D_BuildFromCompatibilityLight(&scene->lightSet,
                                                                  &scene->light,
                                                                  scene->hasLight)) {
            runtime_scene_3d_builder_set_diag("compat light seed build failed");
            return false;
        }
    }
    if (RuntimeScene3DSampleAuthoredCamera(normalized_t, &camera)) {
        scene->camera = camera;
        scene->hasCamera = true;
    }
    return true;
}

static bool runtime_scene_3d_builder_build_from_primitive_seed_state_at_t(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state,
    double normalized_t,
    bool require_ready_bvh) {
    int retained_primitive_count = 0;
    int expected_triangle_count = 0;
    struct timespec stage_start = {0};

    (void)clock_gettime(CLOCK_MONOTONIC, &stage_start);
    if (!scene || !seed_state || !seed_state->valid) {
        runtime_scene_3d_builder_set_diag("primitive seed build failed: invalid seed state");
        return false;
    }

    RuntimeScene3D_Reset(scene);
    if (!runtime_scene_3d_builder_apply_authored_samples(scene, normalized_t)) {
        return false;
    }
    retained_primitive_count = seed_state->primitive_count;
    for (int i = 0; i < seed_state->primitive_count; ++i) {
        RuntimePrimitive3DKind kind =
            runtime_scene_3d_builder_map_kind(seed_state->primitives[i].kind);
        if (!RuntimePrimitive3DKindSupportedByR0(kind)) continue;
        expected_triangle_count += runtime_scene_3d_builder_triangle_count_for_kind(kind);
    }

    if (retained_primitive_count <= 0) return true;
    if (!runtime_scene_3d_builder_reserve_primitives(scene, retained_primitive_count)) {
        runtime_scene_3d_builder_set_diag("primitive seed build failed: primitive reserve failed");
        return false;
    }
    if (!runtime_scene_3d_builder_reserve_triangles(scene, expected_triangle_count)) {
        runtime_scene_3d_builder_set_diag("primitive seed build failed: triangle reserve failed");
        return false;
    }

    for (int i = 0; i < seed_state->primitive_count; ++i) {
        RuntimePrimitive3DKind kind =
            runtime_scene_3d_builder_map_kind(seed_state->primitives[i].kind);
        RuntimePrimitive3D* primitive = NULL;
        if (!RuntimePrimitive3DKindSupportedByR0(kind)) continue;

        primitive = &scene->primitives[scene->primitiveCount];
        runtime_scene_3d_builder_fill_primitive(primitive, &seed_state->primitives[i]);
        if (!runtime_scene_3d_builder_append_triangles(scene, scene->primitiveCount, primitive)) {
            runtime_scene_3d_builder_set_diag("primitive seed build failed: triangle append failed");
            RuntimeScene3D_Reset(scene);
            return false;
        }
        scene->primitiveCount += 1;
    }

    if (require_ready_bvh) {
        if (!runtime_scene_3d_builder_rebuild_tlas(scene) ||
            !runtime_scene_3d_builder_rebuild_bvh(scene)) {
            RuntimeScene3D_Reset(scene);
            return false;
        }
    }
    gRuntimeScene3DBuilderTiming.primitive_seed_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&stage_start);
    return true;
}

bool RuntimeScene3DBuilder_BuildFromPrimitiveSeedStateAtT(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state,
    double normalized_t) {
    return runtime_scene_3d_builder_build_from_primitive_seed_state_at_t(scene,
                                                                         seed_state,
                                                                         normalized_t,
                                                                         true);
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
    const RayTracingRuntimeMeshAssetSet* mesh_assets = NULL;
    int mesh_instance_count = 0;
    int mesh_asset_count = 0;
    struct timespec total_start = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &total_start);
    runtime_scene_3d_builder_set_diag("ok");
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seed_state);
    if (!runtime_scene_3d_builder_build_from_primitive_seed_state_at_t(scene,
                                                                       &seed_state,
                                                                       normalized_t,
                                                                       false)) {
        char diag[2048];
        const char* lower_diag = RuntimeScene3DBuilder_LastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "bridge primitive seed build failed: seed_valid=%s seed_primitive_count=%d seed_plane_count=%d seed_rect_prism_count=%d seed_excluded_count=%d lower=%s",
                 seed_state.valid ? "true" : "false",
                 seed_state.primitive_count,
                 seed_state.plane_primitive_count,
                 seed_state.rect_prism_primitive_count,
                 seed_state.excluded_primitive_count,
                 lower_diag ? lower_diag : "unknown");
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    mesh_assets = ray_tracing_runtime_mesh_assets_last();
    mesh_instance_count = mesh_assets ? mesh_assets->instance_count : -1;
    mesh_asset_count = mesh_assets ? mesh_assets->asset_count : -1;
    if (!runtime_scene_3d_builder_append_mesh_asset_set(scene, mesh_assets, false)) {
        char diag[2048];
        const char* lower_diag = RuntimeScene3DBuilder_LastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "bridge mesh append failed: seed_valid=%s seed_primitive_count=%d mesh_instance_count=%d mesh_asset_count=%d lower=%s",
                 seed_state.valid ? "true" : "false",
                 seed_state.primitive_count,
                 mesh_instance_count,
                 mesh_asset_count,
                 lower_diag ? lower_diag : "unknown");
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    if (!scene || scene->primitiveCount <= 0 || scene->triangleMesh.triangleCount <= 0) {
        char diag[2048];
        snprintf(diag,
                 sizeof(diag),
                 "bridge geometry unavailable: seed_valid=%s seed_primitive_count=%d seed_plane_count=%d seed_rect_prism_count=%d seed_excluded_count=%d mesh_instance_count=%d mesh_asset_count=%d",
                 seed_state.valid ? "true" : "false",
                 seed_state.primitive_count,
                 seed_state.plane_primitive_count,
                 seed_state.rect_prism_primitive_count,
                 seed_state.excluded_primitive_count,
                 mesh_instance_count,
                 mesh_asset_count);
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    if (!runtime_scene_3d_builder_rebuild_tlas(scene)) {
        char diag[4096];
        const char* lower_diag = RuntimeScene3DBuilder_LastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "bridge TLAS rebuild failed: seed_valid=%s seed_primitive_count=%d seed_plane_count=%d seed_rect_prism_count=%d seed_excluded_count=%d mesh_instance_count=%d mesh_asset_count=%d primitive_count=%d triangle_count=%d lower=%s",
                 seed_state.valid ? "true" : "false",
                 seed_state.primitive_count,
                 seed_state.plane_primitive_count,
                 seed_state.rect_prism_primitive_count,
                 seed_state.excluded_primitive_count,
                 mesh_instance_count,
                 mesh_asset_count,
                 scene ? scene->primitiveCount : -1,
                 scene ? scene->triangleMesh.triangleCount : -1,
                 lower_diag ? lower_diag : "unknown");
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    if (!runtime_scene_3d_builder_rebuild_bvh(scene)) {
        char diag[4096];
        const char* lower_diag = RuntimeScene3DBuilder_LastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "bridge bvh rebuild failed: seed_valid=%s seed_primitive_count=%d seed_plane_count=%d seed_rect_prism_count=%d seed_excluded_count=%d mesh_instance_count=%d mesh_asset_count=%d primitive_count=%d triangle_count=%d lower=%s",
                 seed_state.valid ? "true" : "false",
                 seed_state.primitive_count,
                 seed_state.plane_primitive_count,
                 seed_state.rect_prism_primitive_count,
                 seed_state.excluded_primitive_count,
                 mesh_instance_count,
                 mesh_asset_count,
                 scene ? scene->primitiveCount : -1,
                 scene ? scene->triangleMesh.triangleCount : -1,
                 lower_diag ? lower_diag : "unknown");
        runtime_scene_3d_builder_set_diag(diag);
        return false;
    }
    runtime_scene_3d_builder_set_diag("ok");
    gRuntimeScene3DBuilderTiming.total_ms +=
        runtime_scene_3d_builder_elapsed_ms_since(&total_start);
    return true;
}

bool RuntimeScene3DBuilder_BuildRouteProbeFromBridgeSeedsAtT(RuntimeScene3D* scene,
                                                             double normalized_t) {
    RuntimeSceneBridge3DPrimitiveSeedState seed_state = {0};
    const RayTracingRuntimeMeshAssetSet* mesh_assets = NULL;
    runtime_scene_3d_builder_set_diag("ok");
    mesh_assets = ray_tracing_runtime_mesh_assets_last();
    if (!mesh_assets || mesh_assets->instance_count <= 0) {
        runtime_scene_bridge_get_last_3d_primitive_seed_state(&seed_state);
        if (!seed_state.valid || seed_state.primitive_count <= 0) {
            runtime_scene_3d_builder_set_diag("route probe failed: no mesh assets and no valid primitive seeds");
            return false;
        }
        for (int i = 0; i < seed_state.primitive_count; ++i) {
            if (!seed_state.primitives[i].has_dimensions) {
                runtime_scene_3d_builder_set_diag("route probe failed: primitive seed missing dimensions");
                return false;
            }
        }
        return RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(scene, normalized_t);
    }

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seed_state);
    if (!runtime_scene_3d_builder_build_from_primitive_seed_state_at_t(scene,
                                                                       &seed_state,
                                                                       normalized_t,
                                                                       false)) {
        return false;
    }
    return runtime_scene_3d_builder_append_mesh_asset_set(scene, mesh_assets, false);
}
