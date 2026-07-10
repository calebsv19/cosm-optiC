#include "render/runtime_scene_accel_3d_internal.h"

#include "render/runtime_mesh_blas_cache_3d.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Vec3 runtime_scene_accel_3d_instances_min(Vec3 a, Vec3 b) {
    Vec3 out = a;
    if (b.x < out.x) out.x = b.x;
    if (b.y < out.y) out.y = b.y;
    if (b.z < out.z) out.z = b.z;
    return out;
}

static Vec3 runtime_scene_accel_3d_instances_max(Vec3 a, Vec3 b) {
    Vec3 out = a;
    if (b.x > out.x) out.x = b.x;
    if (b.y > out.y) out.y = b.y;
    if (b.z > out.z) out.z = b.z;
    return out;
}

void RuntimeSceneAcceleration3D_FreeInstanceIdentityMaps(
    RuntimeSceneAcceleration3DInstanceBounds* instances,
    int instance_count) {
    if (!instances || instance_count <= 0) return;
    for (int i = 0; i < instance_count; ++i) {
        free(instances[i].sceneTriangleIndexByLocalTriangle);
        instances[i].sceneTriangleIndexByLocalTriangle = NULL;
        instances[i].sceneTriangleIndexByLocalTriangleCount = 0;
    }
}

bool RuntimeSceneAcceleration3D_CaptureInstanceBounds(
    const RuntimeScene3D* scene,
    RuntimeSceneAcceleration3DInstanceBounds** out_instances,
    int* out_instance_count,
    RuntimeSceneAcceleration3DDiagnosticsFn set_diag) {
    RuntimeSceneAcceleration3DInstanceBounds* instances = NULL;
    int instance_count = 0;

    if (out_instances) *out_instances = NULL;
    if (out_instance_count) *out_instance_count = 0;
    if (!out_instances || !out_instance_count) return false;
    if (!scene || scene->primitiveCount <= 0 || scene->triangleMesh.triangleCount <= 0) {
        return true;
    }
    instances = (RuntimeSceneAcceleration3DInstanceBounds*)calloc(
        (size_t)scene->primitiveCount,
        sizeof(*instances));
    if (!instances) {
        if (set_diag) set_diag("TLAS build failed: instance allocation failed");
        return false;
    }

    for (int primitive_index = 0; primitive_index < scene->primitiveCount; ++primitive_index) {
        RuntimeSceneAcceleration3DInstanceBounds bounds;
        bool valid = false;
        int max_local_triangle_index = -1;
        memset(&bounds, 0, sizeof(bounds));
        bounds.min = vec3(DBL_MAX, DBL_MAX, DBL_MAX);
        bounds.max = vec3(-DBL_MAX, -DBL_MAX, -DBL_MAX);
        bounds.primitiveIndex = primitive_index;
        bounds.sceneObjectIndex = scene->primitives[primitive_index].source.sceneObjectIndex;
        snprintf(bounds.objectId,
                 sizeof(bounds.objectId),
                 "%s",
                 scene->primitives[primitive_index].source.objectId);

        for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
            const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
            const Vec3 points[3] = {triangle->p0, triangle->p1, triangle->p2};
            if (triangle->primitiveIndex != primitive_index) continue;
            for (int j = 0; j < 3; ++j) {
                bounds.min = runtime_scene_accel_3d_instances_min(bounds.min, points[j]);
                bounds.max = runtime_scene_accel_3d_instances_max(bounds.max, points[j]);
            }
            if (triangle->localTriangleIndex > max_local_triangle_index) {
                max_local_triangle_index = triangle->localTriangleIndex;
            }
            valid = true;
        }
        if (!valid) continue;
        if (max_local_triangle_index >= 0 && max_local_triangle_index < INT_MAX) {
            const int map_count = max_local_triangle_index + 1;
            bool duplicate_local_triangle_index = false;
            bounds.sceneTriangleIndexByLocalTriangle =
                (int*)malloc((size_t)map_count * sizeof(*bounds.sceneTriangleIndexByLocalTriangle));
            if (bounds.sceneTriangleIndexByLocalTriangle) {
                bounds.sceneTriangleIndexByLocalTriangleCount = map_count;
                for (int i = 0; i < map_count; ++i) {
                    bounds.sceneTriangleIndexByLocalTriangle[i] = -1;
                }
                for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
                    const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
                    const int local_index = triangle->localTriangleIndex;
                    if (triangle->primitiveIndex != primitive_index ||
                        local_index < 0 ||
                        local_index >= map_count) {
                        continue;
                    }
                    if (bounds.sceneTriangleIndexByLocalTriangle[local_index] >= 0) {
                        duplicate_local_triangle_index = true;
                        break;
                    }
                    bounds.sceneTriangleIndexByLocalTriangle[local_index] = i;
                }
                if (duplicate_local_triangle_index) {
                    free(bounds.sceneTriangleIndexByLocalTriangle);
                    bounds.sceneTriangleIndexByLocalTriangle = NULL;
                    bounds.sceneTriangleIndexByLocalTriangleCount = 0;
                }
            }
        }
        instances[instance_count++] = bounds;
    }

    *out_instances = instances;
    *out_instance_count = instance_count;
    return true;
}

static Vec3 runtime_scene_accel_3d_instances_rotate(Vec3 p, Vec3 rotation) {
    double cx = cos(rotation.x);
    double sx = sin(rotation.x);
    double cy = cos(rotation.y);
    double sy = sin(rotation.y);
    double cz = cos(rotation.z);
    double sz = sin(rotation.z);
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

static Vec3 runtime_scene_accel_3d_instances_inverse_rotate(Vec3 p, Vec3 rotation) {
    double cx = cos(-rotation.x);
    double sx = sin(-rotation.x);
    double cy = cos(-rotation.y);
    double sy = sin(-rotation.y);
    double cz = cos(-rotation.z);
    double sz = sin(-rotation.z);
    Vec3 q = p;
    Vec3 r = q;

    q.x = r.x * cz - r.y * sz;
    q.y = r.x * sz + r.y * cz;
    r = q;
    q.x = r.x * cy + r.z * sy;
    q.z = -r.x * sy + r.z * cy;
    r = q;
    q.y = r.y * cx - r.z * sx;
    q.z = r.y * sx + r.z * cx;
    return q;
}

Vec3 RuntimeSceneAcceleration3D_TransformPoint(
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    Vec3 local_point) {
    Vec3 scaled;
    Vec3 p;
    if (!instance) return local_point;
    scaled = vec3(local_point.x * instance->scale.x,
                  local_point.y * instance->scale.y,
                  local_point.z * instance->scale.z);
    p = vec3_sub(scaled, instance->pivotScaled);
    p = runtime_scene_accel_3d_instances_rotate(p, instance->rotation);
    p = vec3_add(p, instance->pivotScaled);
    return vec3_add(p, instance->position);
}

Vec3 RuntimeSceneAcceleration3D_InverseTransformPoint(
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    Vec3 world_point) {
    Vec3 p;
    if (!instance) return world_point;
    p = vec3_sub(world_point, instance->position);
    p = vec3_sub(p, instance->pivotScaled);
    p = runtime_scene_accel_3d_instances_inverse_rotate(p, instance->rotation);
    p = vec3_add(p, instance->pivotScaled);
    if (fabs(instance->scale.x) > 1e-12) p.x /= instance->scale.x;
    if (fabs(instance->scale.y) > 1e-12) p.y /= instance->scale.y;
    if (fabs(instance->scale.z) > 1e-12) p.z /= instance->scale.z;
    return p;
}

Vec3 RuntimeSceneAcceleration3D_InverseTransformDirection(
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    Vec3 world_direction) {
    Vec3 d;
    if (!instance) return world_direction;
    d = runtime_scene_accel_3d_instances_inverse_rotate(world_direction, instance->rotation);
    if (fabs(instance->scale.x) > 1e-12) d.x /= instance->scale.x;
    if (fabs(instance->scale.y) > 1e-12) d.y /= instance->scale.y;
    if (fabs(instance->scale.z) > 1e-12) d.z /= instance->scale.z;
    return d;
}

static Vec3 runtime_scene_accel_3d_instances_mesh_pivot(
    const CoreMeshAssetRuntimeDocument* document,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    if (!document || !instance) return vec3(0.0, 0.0, 0.0);
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

static int runtime_scene_accel_3d_instances_find_instance_for_primitive(
    const RuntimeSceneAcceleration3DInstanceBounds* instances,
    int instance_count,
    int primitive_index) {
    if (!instances || instance_count <= 0) return -1;
    for (int i = 0; i < instance_count; ++i) {
        if (instances[i].primitiveIndex == primitive_index) {
            return i;
        }
    }
    return -1;
}

static int runtime_scene_accel_3d_instances_find_primitive_for_mesh_instance(
    const RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetInstance* mesh_instance) {
    if (!scene || !mesh_instance) return -1;
    for (int i = 0; i < scene->primitiveCount; ++i) {
        const RuntimePrimitive3D* primitive = &scene->primitives[i];
        if (primitive->kind != RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH) continue;
        if (strcmp(primitive->source.objectId, mesh_instance->object_id) == 0) {
            return i;
        }
    }
    return -1;
}

bool RuntimeSceneAcceleration3D_ApplyMeshAssetRecords(
    const RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets,
    RuntimeSceneAcceleration3DInstanceBounds* instances,
    int instance_count) {
    if (!scene || !mesh_assets || mesh_assets->instance_count <= 0) return true;
    for (int i = 0; i < mesh_assets->instance_count; ++i) {
        const RayTracingRuntimeMeshAssetInstance* mesh_instance = &mesh_assets->instances[i];
        const RayTracingRuntimeMeshAsset* asset = NULL;
        RuntimeMeshBLASCache3DView blas_view;
        int primitive_index = -1;
        int accel_index = -1;
        RuntimeSceneAcceleration3DInstanceBounds* accel_instance = NULL;

        if (mesh_instance->asset_index < 0 ||
            mesh_instance->asset_index >= mesh_assets->asset_count) {
            continue;
        }
        asset = &mesh_assets->assets[mesh_instance->asset_index];
        primitive_index =
            runtime_scene_accel_3d_instances_find_primitive_for_mesh_instance(scene,
                                                                              mesh_instance);
        if (primitive_index < 0) continue;
        accel_index = runtime_scene_accel_3d_instances_find_instance_for_primitive(
            instances,
            instance_count,
            primitive_index);
        if (accel_index < 0) continue;
        if (!RuntimeMeshBLASCache3D_FindAsset(asset, &blas_view)) {
            continue;
        }

        accel_instance = &instances[accel_index];
        accel_instance->meshAccelerated = true;
        accel_instance->assetIndex = mesh_instance->asset_index;
        snprintf(accel_instance->assetId,
                 sizeof(accel_instance->assetId),
                 "%s",
                 asset->asset_id);
        snprintf(accel_instance->assetPath,
                 sizeof(accel_instance->assetPath),
                 "%s",
                 asset->path);
        accel_instance->position = vec3(mesh_instance->position_x,
                                        mesh_instance->position_y,
                                        mesh_instance->position_z);
        accel_instance->rotation = vec3(mesh_instance->rotation_x,
                                        mesh_instance->rotation_y,
                                        mesh_instance->rotation_z);
        accel_instance->scale = vec3(mesh_instance->scale_x,
                                     mesh_instance->scale_y,
                                     mesh_instance->scale_z);
        accel_instance->pivotScaled =
            runtime_scene_accel_3d_instances_mesh_pivot(&asset->document, mesh_instance);
        accel_instance->localMesh = blas_view.localMesh;
    }
    return true;
}
