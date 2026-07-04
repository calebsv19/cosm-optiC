#include "render/runtime_scene_accel_3d.h"

#include "render/runtime_dynamic_geometry_accel_3d.h"
#include "render/runtime_mesh_blas_cache_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct RuntimeSceneAcceleration3DInstanceBounds {
    Vec3 min;
    Vec3 max;
    int primitiveIndex;
    int sceneObjectIndex;
    char objectId[RUNTIME_SCENE_3D_MAX_OBJECT_ID];
    bool meshAccelerated;
    int assetIndex;
    char assetId[64];
    char assetPath[RAY_TRACING_RUNTIME_MESH_ASSET_PATH_MAX];
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
    Vec3 pivotScaled;
    const RuntimeTriangleMesh3D* localMesh;
    int* sceneTriangleIndexByLocalTriangle;
    int sceneTriangleIndexByLocalTriangleCount;
} RuntimeSceneAcceleration3DInstanceBounds;

typedef struct RuntimeSceneAcceleration3DTLASNode {
    Vec3 min;
    Vec3 max;
    int left;
    int right;
    int firstInstance;
    int instanceCount;
    bool leaf;
} RuntimeSceneAcceleration3DTLASNode;

static RuntimeSceneAcceleration3DInstanceBounds* gRuntimeSceneAcceleration3DInstances;
static const RuntimeScene3D* gRuntimeSceneAcceleration3DPreparedScene;
static int gRuntimeSceneAcceleration3DPreparedPrimitiveCount;
static int gRuntimeSceneAcceleration3DPreparedTriangleCount;
static int gRuntimeSceneAcceleration3DInstanceCount;
static int* gRuntimeSceneAcceleration3DInstanceOrder;
static RuntimeSceneAcceleration3DTLASNode* gRuntimeSceneAcceleration3DTLASNodes;
static int gRuntimeSceneAcceleration3DTLASNodeCount;
static int gRuntimeSceneAcceleration3DTLASNodeCapacity;
static RuntimeSceneAcceleration3DDiagnostics gRuntimeSceneAcceleration3DTLASDiagnostics;
static RuntimeSceneAcceleration3DTraceStats gRuntimeSceneAcceleration3DTraceStats;
static char gRuntimeSceneAcceleration3DLastDiagnostics[1024] = "ok";

static double runtime_scene_accel_3d_elapsed_ms_since(const struct timespec* start_time) {
    struct timespec now = {0};
    double elapsed = 0.0;
    if (!start_time) return 0.0;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0.0;
    elapsed = (double)(now.tv_sec - start_time->tv_sec) * 1000.0;
    elapsed += (double)(now.tv_nsec - start_time->tv_nsec) / 1000000.0;
    return elapsed < 0.0 ? 0.0 : elapsed;
}

static void runtime_scene_accel_3d_set_diag(const char* message) {
    snprintf(gRuntimeSceneAcceleration3DLastDiagnostics,
             sizeof(gRuntimeSceneAcceleration3DLastDiagnostics),
             "%s",
             (message && message[0]) ? message : "ok");
}

static Vec3 runtime_scene_accel_3d_min(Vec3 a, Vec3 b) {
    Vec3 out = a;
    if (b.x < out.x) out.x = b.x;
    if (b.y < out.y) out.y = b.y;
    if (b.z < out.z) out.z = b.z;
    return out;
}

static Vec3 runtime_scene_accel_3d_max(Vec3 a, Vec3 b) {
    Vec3 out = a;
    if (b.x > out.x) out.x = b.x;
    if (b.y > out.y) out.y = b.y;
    if (b.z > out.z) out.z = b.z;
    return out;
}

static Vec3 runtime_scene_accel_3d_centroid(
    const RuntimeSceneAcceleration3DInstanceBounds* bounds) {
    if (!bounds) return vec3(0.0, 0.0, 0.0);
    return vec3((bounds->min.x + bounds->max.x) * 0.5,
                (bounds->min.y + bounds->max.y) * 0.5,
                (bounds->min.z + bounds->max.z) * 0.5);
}

static double runtime_scene_accel_3d_axis_value(Vec3 v, int axis) {
    if (axis == 0) return v.x;
    if (axis == 1) return v.y;
    return v.z;
}

static void runtime_scene_accel_3d_sort_indices_by_axis(int* indices,
                                                        int start,
                                                        int end,
                                                        int axis) {
    for (int i = start + 1; i < end; ++i) {
        int key = indices[i];
        double key_value = runtime_scene_accel_3d_axis_value(
            runtime_scene_accel_3d_centroid(&gRuntimeSceneAcceleration3DInstances[key]),
            axis);
        int j = i - 1;
        while (j >= start) {
            double j_value = runtime_scene_accel_3d_axis_value(
                runtime_scene_accel_3d_centroid(
                    &gRuntimeSceneAcceleration3DInstances[indices[j]]),
                axis);
            if (j_value <= key_value) break;
            indices[j + 1] = indices[j];
            --j;
        }
        indices[j + 1] = key;
    }
}

static int runtime_scene_accel_3d_push_node(void) {
    RuntimeSceneAcceleration3DTLASNode* nodes = NULL;
    int next_capacity = 0;
    if (gRuntimeSceneAcceleration3DTLASNodeCount <
        gRuntimeSceneAcceleration3DTLASNodeCapacity) {
        return gRuntimeSceneAcceleration3DTLASNodeCount++;
    }
    next_capacity = gRuntimeSceneAcceleration3DTLASNodeCapacity > 0
                        ? gRuntimeSceneAcceleration3DTLASNodeCapacity * 2
                        : 32;
    nodes = (RuntimeSceneAcceleration3DTLASNode*)realloc(
        gRuntimeSceneAcceleration3DTLASNodes,
        (size_t)next_capacity * sizeof(*nodes));
    if (!nodes) {
        runtime_scene_accel_3d_set_diag("TLAS build failed: node allocation failed");
        return -1;
    }
    gRuntimeSceneAcceleration3DTLASNodes = nodes;
    gRuntimeSceneAcceleration3DTLASNodeCapacity = next_capacity;
    return gRuntimeSceneAcceleration3DTLASNodeCount++;
}

static bool runtime_scene_accel_3d_instance_range_bounds(int* indices,
                                                        int start,
                                                        int end,
                                                        Vec3* out_min,
                                                        Vec3* out_max) {
    if (!indices || start >= end || !out_min || !out_max) return false;
    *out_min = gRuntimeSceneAcceleration3DInstances[indices[start]].min;
    *out_max = gRuntimeSceneAcceleration3DInstances[indices[start]].max;
    for (int i = start + 1; i < end; ++i) {
        *out_min = runtime_scene_accel_3d_min(
            *out_min,
            gRuntimeSceneAcceleration3DInstances[indices[i]].min);
        *out_max = runtime_scene_accel_3d_max(
            *out_max,
            gRuntimeSceneAcceleration3DInstances[indices[i]].max);
    }
    return true;
}

static int runtime_scene_accel_3d_choose_split_axis(int* indices, int start, int end) {
    Vec3 min_centroid = vec3(DBL_MAX, DBL_MAX, DBL_MAX);
    Vec3 max_centroid = vec3(-DBL_MAX, -DBL_MAX, -DBL_MAX);
    Vec3 extent;
    for (int i = start; i < end; ++i) {
        Vec3 c = runtime_scene_accel_3d_centroid(
            &gRuntimeSceneAcceleration3DInstances[indices[i]]);
        min_centroid = runtime_scene_accel_3d_min(min_centroid, c);
        max_centroid = runtime_scene_accel_3d_max(max_centroid, c);
    }
    extent = vec3(max_centroid.x - min_centroid.x,
                  max_centroid.y - min_centroid.y,
                  max_centroid.z - min_centroid.z);
    if (extent.x >= extent.y && extent.x >= extent.z) return 0;
    if (extent.y >= extent.z) return 1;
    return 2;
}

static int runtime_scene_accel_3d_build_node(int* indices, int start, int end) {
    int node_index = runtime_scene_accel_3d_push_node();
    RuntimeSceneAcceleration3DTLASNode* node = NULL;
    Vec3 min_bounds;
    Vec3 max_bounds;
    int count = end - start;
    int axis = 0;
    int split = 0;
    int left = -1;
    int right = -1;

    if (node_index < 0) return -1;
    if (!runtime_scene_accel_3d_instance_range_bounds(indices,
                                                      start,
                                                      end,
                                                      &min_bounds,
                                                      &max_bounds)) {
        runtime_scene_accel_3d_set_diag("TLAS build failed: invalid instance range");
        return -1;
    }
    node = &gRuntimeSceneAcceleration3DTLASNodes[node_index];
    memset(node, 0, sizeof(*node));
    node->min = min_bounds;
    node->max = max_bounds;
    node->left = -1;
    node->right = -1;
    node->firstInstance = start;
    node->instanceCount = count;
    node->leaf = count <= 1;
    if (node->leaf) return node_index;

    axis = runtime_scene_accel_3d_choose_split_axis(indices, start, end);
    runtime_scene_accel_3d_sort_indices_by_axis(indices, start, end, axis);
    split = start + count / 2;
    left = runtime_scene_accel_3d_build_node(indices, start, split);
    if (left < 0) return -1;
    right = runtime_scene_accel_3d_build_node(indices, split, end);
    if (right < 0) return -1;
    node = &gRuntimeSceneAcceleration3DTLASNodes[node_index];
    node->left = left;
    node->right = right;
    return node_index;
}

static void runtime_scene_accel_3d_free_instance_identity_maps(
    RuntimeSceneAcceleration3DInstanceBounds* instances,
    int instance_count) {
    if (!instances || instance_count <= 0) return;
    for (int i = 0; i < instance_count; ++i) {
        free(instances[i].sceneTriangleIndexByLocalTriangle);
        instances[i].sceneTriangleIndexByLocalTriangle = NULL;
        instances[i].sceneTriangleIndexByLocalTriangleCount = 0;
    }
}

static void runtime_scene_accel_3d_free_tlas(void) {
    gRuntimeSceneAcceleration3DPreparedScene = NULL;
    gRuntimeSceneAcceleration3DPreparedPrimitiveCount = 0;
    gRuntimeSceneAcceleration3DPreparedTriangleCount = 0;
    runtime_scene_accel_3d_free_instance_identity_maps(
        gRuntimeSceneAcceleration3DInstances,
        gRuntimeSceneAcceleration3DInstanceCount);
    free(gRuntimeSceneAcceleration3DInstances);
    gRuntimeSceneAcceleration3DInstances = NULL;
    gRuntimeSceneAcceleration3DInstanceCount = 0;
    free(gRuntimeSceneAcceleration3DInstanceOrder);
    gRuntimeSceneAcceleration3DInstanceOrder = NULL;
    free(gRuntimeSceneAcceleration3DTLASNodes);
    gRuntimeSceneAcceleration3DTLASNodes = NULL;
    gRuntimeSceneAcceleration3DTLASNodeCount = 0;
    gRuntimeSceneAcceleration3DTLASNodeCapacity = 0;
}

static bool runtime_scene_accel_3d_capture_instance_bounds(const RuntimeScene3D* scene) {
    RuntimeSceneAcceleration3DInstanceBounds* instances = NULL;
    int instance_count = 0;

    if (!scene || scene->primitiveCount <= 0 || scene->triangleMesh.triangleCount <= 0) {
        return true;
    }
    instances = (RuntimeSceneAcceleration3DInstanceBounds*)calloc(
        (size_t)scene->primitiveCount,
        sizeof(*instances));
    if (!instances) {
        runtime_scene_accel_3d_set_diag("TLAS build failed: instance allocation failed");
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
                bounds.min = runtime_scene_accel_3d_min(bounds.min, points[j]);
                bounds.max = runtime_scene_accel_3d_max(bounds.max, points[j]);
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

    gRuntimeSceneAcceleration3DInstances = instances;
    gRuntimeSceneAcceleration3DInstanceCount = instance_count;
    return true;
}

static Vec3 runtime_scene_accel_3d_rotate(Vec3 p, Vec3 rotation) {
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

static Vec3 runtime_scene_accel_3d_inverse_rotate(Vec3 p, Vec3 rotation) {
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

static Vec3 runtime_scene_accel_3d_transform_point(
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    Vec3 local_point) {
    Vec3 scaled;
    Vec3 p;
    if (!instance) return local_point;
    scaled = vec3(local_point.x * instance->scale.x,
                  local_point.y * instance->scale.y,
                  local_point.z * instance->scale.z);
    p = vec3_sub(scaled, instance->pivotScaled);
    p = runtime_scene_accel_3d_rotate(p, instance->rotation);
    p = vec3_add(p, instance->pivotScaled);
    return vec3_add(p, instance->position);
}

static Vec3 runtime_scene_accel_3d_inverse_transform_point(
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    Vec3 world_point) {
    Vec3 p;
    if (!instance) return world_point;
    p = vec3_sub(world_point, instance->position);
    p = vec3_sub(p, instance->pivotScaled);
    p = runtime_scene_accel_3d_inverse_rotate(p, instance->rotation);
    p = vec3_add(p, instance->pivotScaled);
    if (fabs(instance->scale.x) > 1e-12) p.x /= instance->scale.x;
    if (fabs(instance->scale.y) > 1e-12) p.y /= instance->scale.y;
    if (fabs(instance->scale.z) > 1e-12) p.z /= instance->scale.z;
    return p;
}

static Vec3 runtime_scene_accel_3d_inverse_transform_direction(
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    Vec3 world_direction) {
    Vec3 d;
    if (!instance) return world_direction;
    d = runtime_scene_accel_3d_inverse_rotate(world_direction, instance->rotation);
    if (fabs(instance->scale.x) > 1e-12) d.x /= instance->scale.x;
    if (fabs(instance->scale.y) > 1e-12) d.y /= instance->scale.y;
    if (fabs(instance->scale.z) > 1e-12) d.z /= instance->scale.z;
    return d;
}

static Vec3 runtime_scene_accel_3d_mesh_pivot(
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

static int runtime_scene_accel_3d_find_instance_for_primitive(int primitive_index) {
    for (int i = 0; i < gRuntimeSceneAcceleration3DInstanceCount; ++i) {
        if (gRuntimeSceneAcceleration3DInstances[i].primitiveIndex == primitive_index) {
            return i;
        }
    }
    return -1;
}

static int runtime_scene_accel_3d_find_primitive_for_mesh_instance(
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

static bool runtime_scene_accel_3d_apply_mesh_asset_records(
    const RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets) {
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
            runtime_scene_accel_3d_find_primitive_for_mesh_instance(scene, mesh_instance);
        if (primitive_index < 0) continue;
        accel_index = runtime_scene_accel_3d_find_instance_for_primitive(primitive_index);
        if (accel_index < 0) continue;
        if (!RuntimeMeshBLASCache3D_FindAsset(asset, &blas_view)) {
            continue;
        }

        accel_instance = &gRuntimeSceneAcceleration3DInstances[accel_index];
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
            runtime_scene_accel_3d_mesh_pivot(&asset->document, mesh_instance);
        accel_instance->localMesh = blas_view.localMesh;
    }
    return true;
}

bool RuntimeSceneAcceleration3D_RebuildTLASFromScene(const RuntimeScene3D* scene) {
    int* indices = NULL;
    RuntimeSceneAcceleration3DDiagnostics prior = gRuntimeSceneAcceleration3DTLASDiagnostics;
    struct timespec build_start = {0};

    runtime_scene_accel_3d_set_diag("ok");
    (void)clock_gettime(CLOCK_MONOTONIC, &build_start);
    runtime_scene_accel_3d_free_tlas();
    gRuntimeSceneAcceleration3DTLASDiagnostics.enabled = false;
    gRuntimeSceneAcceleration3DTLASDiagnostics.reuseStatus =
        RUNTIME_SCENE_ACCEL_3D_REUSE_DISABLED;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasNodeCount = 0u;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasInstanceCount = 0u;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasRebuilds = prior.tlasRebuilds;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasRefits = prior.tlasRefits;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBuildMs = prior.tlasBuildMs;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBindMs = prior.tlasBindMs;

    if (!scene || scene->primitiveCount <= 0 || scene->triangleMesh.triangleCount <= 0) {
        gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBuildMs +=
            runtime_scene_accel_3d_elapsed_ms_since(&build_start);
        return true;
    }
    if (!runtime_scene_accel_3d_capture_instance_bounds(scene)) {
        return false;
    }
    if (gRuntimeSceneAcceleration3DInstanceCount <= 0) {
        return true;
    }

    indices = (int*)calloc((size_t)gRuntimeSceneAcceleration3DInstanceCount,
                           sizeof(*indices));
    if (!indices) {
        runtime_scene_accel_3d_set_diag("TLAS build failed: index allocation failed");
        return false;
    }
    for (int i = 0; i < gRuntimeSceneAcceleration3DInstanceCount; ++i) {
        indices[i] = i;
    }
    if (runtime_scene_accel_3d_build_node(indices,
                                          0,
                                          gRuntimeSceneAcceleration3DInstanceCount) < 0) {
        free(indices);
        return false;
    }
    gRuntimeSceneAcceleration3DInstanceOrder =
        (int*)calloc((size_t)gRuntimeSceneAcceleration3DInstanceCount,
                     sizeof(*gRuntimeSceneAcceleration3DInstanceOrder));
    if (!gRuntimeSceneAcceleration3DInstanceOrder) {
        free(indices);
        runtime_scene_accel_3d_set_diag("TLAS build failed: instance order allocation failed");
        return false;
    }
    memcpy(gRuntimeSceneAcceleration3DInstanceOrder,
           indices,
           (size_t)gRuntimeSceneAcceleration3DInstanceCount *
               sizeof(*gRuntimeSceneAcceleration3DInstanceOrder));
    free(indices);

    gRuntimeSceneAcceleration3DTLASDiagnostics.enabled = true;
    gRuntimeSceneAcceleration3DTLASDiagnostics.reuseStatus =
        RUNTIME_SCENE_ACCEL_3D_REUSE_REBUILT;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasInstanceCount =
        (uint64_t)gRuntimeSceneAcceleration3DInstanceCount;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasNodeCount =
        (uint64_t)gRuntimeSceneAcceleration3DTLASNodeCount;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasRebuilds = prior.tlasRebuilds + 1u;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasRefits = prior.tlasRefits;
    gRuntimeSceneAcceleration3DPreparedScene = scene;
    gRuntimeSceneAcceleration3DPreparedPrimitiveCount = scene->primitiveCount;
    gRuntimeSceneAcceleration3DPreparedTriangleCount = scene->triangleMesh.triangleCount;
    RuntimeRay3D_SetSceneAccelerationTraceFirstHit(
        (RuntimeRay3DSceneAccelerationTraceFirstHitFn)
            RuntimeSceneAcceleration3D_TraceFirstHit);
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBuildMs +=
        runtime_scene_accel_3d_elapsed_ms_since(&build_start);
    return true;
}

bool RuntimeSceneAcceleration3D_RebuildPreparedFromSceneAndMeshAssets(
    const RuntimeScene3D* scene,
    const RayTracingRuntimeMeshAssetSet* mesh_assets) {
    if (!RuntimeSceneAcceleration3D_RebuildTLASFromScene(scene)) {
        return false;
    }
    if (!runtime_scene_accel_3d_apply_mesh_asset_records(scene, mesh_assets)) {
        return false;
    }
    return true;
}

bool RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(const RuntimeScene3D* scene) {
    struct timespec bind_start = {0};
    (void)clock_gettime(CLOCK_MONOTONIC, &bind_start);
    if (!scene) {
        runtime_scene_accel_3d_set_diag("TLAS bind skipped: scene missing");
        gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBindMs +=
            runtime_scene_accel_3d_elapsed_ms_since(&bind_start);
        return false;
    }
    if (gRuntimeSceneAcceleration3DTLASNodeCount <= 0 ||
        !gRuntimeSceneAcceleration3DTLASNodes ||
        !gRuntimeSceneAcceleration3DInstances ||
        !gRuntimeSceneAcceleration3DInstanceOrder) {
        runtime_scene_accel_3d_set_diag("TLAS bind skipped: accelerator unavailable");
        gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBindMs +=
            runtime_scene_accel_3d_elapsed_ms_since(&bind_start);
        return false;
    }
    if (scene->primitiveCount != gRuntimeSceneAcceleration3DPreparedPrimitiveCount ||
        scene->triangleMesh.triangleCount !=
            gRuntimeSceneAcceleration3DPreparedTriangleCount) {
        runtime_scene_accel_3d_set_diag(
            "TLAS bind skipped: prepared scene geometry counts differ");
        gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBindMs +=
            runtime_scene_accel_3d_elapsed_ms_since(&bind_start);
        return false;
    }
    gRuntimeSceneAcceleration3DPreparedScene = scene;
    RuntimeRay3D_SetSceneAccelerationTraceFirstHit(
        (RuntimeRay3DSceneAccelerationTraceFirstHitFn)
            RuntimeSceneAcceleration3D_TraceFirstHit);
    runtime_scene_accel_3d_set_diag("ok");
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBindMs +=
        runtime_scene_accel_3d_elapsed_ms_since(&bind_start);
    return true;
}

static bool runtime_scene_accel_3d_intersect_aabb(const Ray3D* ray,
                                                  Vec3 min_bounds,
                                                  Vec3 max_bounds,
                                                  double t_min,
                                                  double t_max) {
    double enter = t_min;
    double exit = t_max;
    if (!ray) return false;
    const double origins[3] = {ray->origin.x, ray->origin.y, ray->origin.z};
    const double dirs[3] = {ray->direction.x, ray->direction.y, ray->direction.z};
    const double mins[3] = {min_bounds.x, min_bounds.y, min_bounds.z};
    const double maxs[3] = {max_bounds.x, max_bounds.y, max_bounds.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (fabs(dirs[axis]) <= 1e-12) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) return false;
            continue;
        }
        double inv_dir = 1.0 / dirs[axis];
        double t0 = (mins[axis] - origins[axis]) * inv_dir;
        double t1 = (maxs[axis] - origins[axis]) * inv_dir;
        if (t0 > t1) {
            double tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        if (t0 > enter) enter = t0;
        if (t1 < exit) exit = t1;
        if (enter > exit) return false;
    }
    return true;
}

static int runtime_scene_accel_3d_remap_scene_triangle(
    const RuntimeScene3D* scene,
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    int local_triangle_index) {
    if (!scene || !instance || local_triangle_index < 0) return -1;
    if (instance->sceneTriangleIndexByLocalTriangle &&
        local_triangle_index < instance->sceneTriangleIndexByLocalTriangleCount) {
        int scene_triangle_index =
            instance->sceneTriangleIndexByLocalTriangle[local_triangle_index];
        if (scene_triangle_index >= 0 &&
            scene_triangle_index < scene->triangleMesh.triangleCount) {
            gRuntimeSceneAcceleration3DTraceStats.identityRemapMapHits += 1u;
            return scene_triangle_index;
        }
    }

    gRuntimeSceneAcceleration3DTraceStats.identityRemapFallbackScans += 1u;
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
        if (triangle->primitiveIndex == instance->primitiveIndex &&
            triangle->localTriangleIndex == local_triangle_index) {
            return i;
        }
    }
    return -1;
}

static Vec3 runtime_scene_accel_3d_orient_normal(Vec3 normal, const Ray3D* ray) {
    normal = vec3_normalize(normal);
    if (!ray || vec3_length(normal) <= 1e-9) return normal;
    if (vec3_dot(normal, ray->direction) > 0.0) {
        normal = vec3_scale(normal, -1.0);
    }
    return normal;
}

static bool runtime_scene_accel_3d_remap_hit(
    const RuntimeScene3D* scene,
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    const Ray3D* world_ray,
    const HitInfo3D* local_hit,
    double t_min,
    double t_max,
    HitInfo3D* out_hit) {
    int scene_triangle_index = -1;
    const RuntimeTriangle3D* scene_triangle = NULL;
    Vec3 world_position;
    Vec3 ray_delta;
    double ray_length_sq = 0.0;
    double world_t = 0.0;

    if (!scene || !instance || !world_ray || !local_hit || !out_hit) return false;
    scene_triangle_index =
        runtime_scene_accel_3d_remap_scene_triangle(scene,
                                                    instance,
                                                    local_hit->localTriangleIndex);
    if (scene_triangle_index < 0 ||
        scene_triangle_index >= scene->triangleMesh.triangleCount) {
        return false;
    }

    scene_triangle = &scene->triangleMesh.triangles[scene_triangle_index];
    world_position =
        runtime_scene_accel_3d_transform_point(instance, local_hit->position);
    ray_delta = vec3_sub(world_position, world_ray->origin);
    ray_length_sq = vec3_dot(world_ray->direction, world_ray->direction);
    if (!(ray_length_sq > 1e-18)) return false;
    world_t = vec3_dot(ray_delta, world_ray->direction) / ray_length_sq;
    if (world_t < t_min || world_t > t_max) return false;

    HitInfo3D_Reset(out_hit);
    out_hit->t = world_t;
    out_hit->position = vec3_add(world_ray->origin,
                                 vec3_scale(world_ray->direction, world_t));
    out_hit->normal =
        runtime_scene_accel_3d_orient_normal(scene_triangle->normal, world_ray);
    out_hit->triangleIndex = scene_triangle_index;
    out_hit->localTriangleIndex = scene_triangle->localTriangleIndex;
    out_hit->primitiveIndex = instance->primitiveIndex;
    out_hit->sceneObjectIndex = instance->sceneObjectIndex;
    if (instance->primitiveIndex >= 0 && instance->primitiveIndex < scene->primitiveCount) {
        out_hit->source = scene->primitives[instance->primitiveIndex].source;
    }
    out_hit->baryU = local_hit->baryU;
    out_hit->baryV = local_hit->baryV;
    out_hit->baryW = local_hit->baryW;
    if (scene_triangle->hasObjectTextureCoords) {
        out_hit->hasObjectTextureCoord = true;
        out_hit->objectTextureCoord =
            vec3_add(vec3_add(vec3_scale(scene_triangle->objectTexture0,
                                         out_hit->baryU),
                              vec3_scale(scene_triangle->objectTexture1,
                                         out_hit->baryV)),
                     vec3_scale(scene_triangle->objectTexture2,
                                out_hit->baryW));
    }
    return true;
}

static bool runtime_scene_accel_3d_hit_better(const HitInfo3D* candidate,
                                              const HitInfo3D* best,
                                              bool found) {
    if (!candidate) return false;
    if (!found || !best) return true;
    if (candidate->t < best->t - 1e-9) return true;
    if (fabs(candidate->t - best->t) <= 1e-9 &&
        candidate->triangleIndex > best->triangleIndex) {
        return true;
    }
    return false;
}

static bool runtime_scene_accel_3d_trace_scene_triangle(
    const RuntimeScene3D* scene,
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    int scene_triangle_index,
    const Ray3D* ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit) {
    HitInfo3D hit = {0};
    if (!scene || !instance || !ray || !out_hit) return false;
    if (scene_triangle_index < 0 ||
        scene_triangle_index >= scene->triangleMesh.triangleCount) {
        return false;
    }
    if (!RuntimeRay3D_IntersectTriangle(ray,
                                        &scene->triangleMesh.triangles[scene_triangle_index],
                                        scene_triangle_index,
                                        t_min,
                                        t_max,
                                        &hit)) {
        return false;
    }
    if (instance->primitiveIndex >= 0 && instance->primitiveIndex < scene->primitiveCount) {
        hit.source = scene->primitives[instance->primitiveIndex].source;
        hit.sceneObjectIndex = scene->primitives[instance->primitiveIndex].source.sceneObjectIndex;
    }
    *out_hit = hit;
    return true;
}

static RuntimeSceneAcceleration3DTraceStatus
runtime_scene_accel_3d_trace_primitive_instance(
    const RuntimeScene3D* scene,
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    const Ray3D* ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit) {
    HitInfo3D best_hit = {0};
    bool found = false;

    if (!scene || !instance || !ray || !out_hit) {
        return RUNTIME_SCENE_ACCEL_3D_TRACE_UNREADY;
    }
    HitInfo3D_Reset(&best_hit);
    if (instance->sceneTriangleIndexByLocalTriangle &&
        instance->sceneTriangleIndexByLocalTriangleCount > 0) {
        for (int i = 0; i < instance->sceneTriangleIndexByLocalTriangleCount; ++i) {
            HitInfo3D candidate = {0};
            int scene_triangle_index = instance->sceneTriangleIndexByLocalTriangle[i];
            if (scene_triangle_index < 0) continue;
            if (!runtime_scene_accel_3d_trace_scene_triangle(scene,
                                                             instance,
                                                             scene_triangle_index,
                                                             ray,
                                                             t_min,
                                                             found ? best_hit.t : t_max,
                                                             &candidate)) {
                continue;
            }
            if (runtime_scene_accel_3d_hit_better(&candidate, &best_hit, found)) {
                best_hit = candidate;
                found = true;
            }
        }
    } else {
        for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
            HitInfo3D candidate = {0};
            const RuntimeTriangle3D* triangle = &scene->triangleMesh.triangles[i];
            if (triangle->primitiveIndex != instance->primitiveIndex) continue;
            if (!runtime_scene_accel_3d_trace_scene_triangle(scene,
                                                             instance,
                                                             i,
                                                             ray,
                                                             t_min,
                                                             found ? best_hit.t : t_max,
                                                             &candidate)) {
                continue;
            }
            if (runtime_scene_accel_3d_hit_better(&candidate, &best_hit, found)) {
                best_hit = candidate;
                found = true;
            }
        }
    }

    if (!found) {
        HitInfo3D_Reset(out_hit);
        return RUNTIME_SCENE_ACCEL_3D_TRACE_MISS;
    }
    *out_hit = best_hit;
    return RUNTIME_SCENE_ACCEL_3D_TRACE_HIT;
}

static RuntimeSceneAcceleration3DTraceStatus runtime_scene_accel_3d_trace_instance(
    const RuntimeScene3D* scene,
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    const Ray3D* world_ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit) {
    Ray3D local_ray;
    HitInfo3D local_hit = {0};
    RuntimeTriangleBVH3DTraceResult blas_result;

    if (!instance) {
        return RUNTIME_SCENE_ACCEL_3D_TRACE_UNREADY;
    }
    if (!instance->meshAccelerated || !instance->localMesh) {
        return runtime_scene_accel_3d_trace_primitive_instance(scene,
                                                               instance,
                                                               world_ray,
                                                               t_min,
                                                               t_max,
                                                               out_hit);
    }
    if (!RuntimeTriangleMesh3D_HasReadyBVH(instance->localMesh)) {
        return RUNTIME_SCENE_ACCEL_3D_TRACE_UNREADY;
    }

    local_ray.origin =
        runtime_scene_accel_3d_inverse_transform_point(instance, world_ray->origin);
    local_ray.direction = vec3_normalize(
        runtime_scene_accel_3d_inverse_transform_direction(instance,
                                                           world_ray->direction));
    if (!(vec3_length(local_ray.direction) > 1e-9)) {
        return RUNTIME_SCENE_ACCEL_3D_TRACE_ERROR;
    }

    gRuntimeSceneAcceleration3DTraceStats.blasTraceCalls += 1u;
    blas_result = RuntimeTriangleBVH3D_TraceFirstHitStatus(instance->localMesh,
                                                           &local_ray,
                                                           1e-9,
                                                           DBL_MAX,
                                                           &local_hit);
    if (blas_result == RUNTIME_TRIANGLE_BVH_3D_TRACE_MISS) {
        return RUNTIME_SCENE_ACCEL_3D_TRACE_MISS;
    }
    if (blas_result == RUNTIME_TRIANGLE_BVH_3D_TRACE_OVERFLOW) {
        return RUNTIME_SCENE_ACCEL_3D_TRACE_UNSUPPORTED;
    }
    if (!runtime_scene_accel_3d_remap_hit(scene,
                                          instance,
                                          world_ray,
                                          &local_hit,
                                          t_min,
                                          t_max,
                                          out_hit)) {
        gRuntimeSceneAcceleration3DTraceStats.identityRemapFailures += 1u;
        return RUNTIME_SCENE_ACCEL_3D_TRACE_ERROR;
    }
    gRuntimeSceneAcceleration3DTraceStats.blasTraceHits += 1u;
    return RUNTIME_SCENE_ACCEL_3D_TRACE_HIT;
}

RuntimeSceneAcceleration3DTraceStatus RuntimeSceneAcceleration3D_TraceFirstHit(
    const RuntimeScene3D* scene,
    const Ray3D* ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit) {
    int stack[128];
    int stack_count = 0;
    HitInfo3D best_hit = {0};
    bool found = false;
    bool saw_unsupported = false;
    bool saw_error = false;
    bool stack_overflow = false;
    HitInfo3D water_hit = {0};

    gRuntimeSceneAcceleration3DTraceStats.traceCalls += 1u;
    if (out_hit) HitInfo3D_Reset(out_hit);
    if (!scene || !ray || !out_hit) {
        gRuntimeSceneAcceleration3DTraceStats.traceUnready += 1u;
        runtime_scene_accel_3d_set_diag("TLAS trace unready: missing scene, ray, or output hit");
        return RUNTIME_SCENE_ACCEL_3D_TRACE_UNREADY;
    }
    if (RuntimeDynamicGeometryAcceleration3D_TraceWaterSurfaceFirstHit(scene,
                                                                       ray,
                                                                       t_min,
                                                                       t_max,
                                                                       &water_hit)) {
        best_hit = water_hit;
        found = true;
    }
    if (scene != gRuntimeSceneAcceleration3DPreparedScene) {
        if (found) {
            *out_hit = best_hit;
            gRuntimeSceneAcceleration3DTraceStats.traceHits += 1u;
            return RUNTIME_SCENE_ACCEL_3D_TRACE_HIT;
        }
        gRuntimeSceneAcceleration3DTraceStats.traceUnready += 1u;
        runtime_scene_accel_3d_set_diag("TLAS trace unready: scene is not bound to prepared TLAS");
        return RUNTIME_SCENE_ACCEL_3D_TRACE_UNREADY;
    }
    if (gRuntimeSceneAcceleration3DTLASNodeCount <= 0 ||
        !gRuntimeSceneAcceleration3DTLASNodes ||
        !gRuntimeSceneAcceleration3DInstances ||
        !gRuntimeSceneAcceleration3DInstanceOrder) {
        if (found) {
            *out_hit = best_hit;
            gRuntimeSceneAcceleration3DTraceStats.traceHits += 1u;
            return RUNTIME_SCENE_ACCEL_3D_TRACE_HIT;
        }
        gRuntimeSceneAcceleration3DTraceStats.traceUnready += 1u;
        runtime_scene_accel_3d_set_diag("TLAS trace unready: accelerator unavailable");
        return RUNTIME_SCENE_ACCEL_3D_TRACE_UNREADY;
    }

    if (!found) HitInfo3D_Reset(&best_hit);
    stack[stack_count++] = 0;
    while (stack_count > 0) {
        int node_index = stack[--stack_count];
        const RuntimeSceneAcceleration3DTLASNode* node = NULL;
        if (node_index < 0 || node_index >= gRuntimeSceneAcceleration3DTLASNodeCount) {
            continue;
        }
        node = &gRuntimeSceneAcceleration3DTLASNodes[node_index];
        gRuntimeSceneAcceleration3DTraceStats.tlasNodeTests += 1u;
        if (!runtime_scene_accel_3d_intersect_aabb(ray,
                                                   node->min,
                                                   node->max,
                                                   t_min,
                                                   found ? best_hit.t : t_max)) {
            continue;
        }
        gRuntimeSceneAcceleration3DTraceStats.tlasNodeHits += 1u;
        if (node->leaf) {
            for (int i = 0; i < node->instanceCount; ++i) {
                int order_index = node->firstInstance + i;
                int instance_index = -1;
                const RuntimeSceneAcceleration3DInstanceBounds* instance = NULL;
                HitInfo3D candidate = {0};
                RuntimeSceneAcceleration3DTraceStatus status;
                if (order_index < 0 ||
                    order_index >= gRuntimeSceneAcceleration3DInstanceCount) {
                    continue;
                }
                instance_index = gRuntimeSceneAcceleration3DInstanceOrder[order_index];
                if (instance_index < 0 ||
                    instance_index >= gRuntimeSceneAcceleration3DInstanceCount) {
                    continue;
                }
                instance = &gRuntimeSceneAcceleration3DInstances[instance_index];
                if (!runtime_scene_accel_3d_intersect_aabb(ray,
                                                           instance->min,
                                                           instance->max,
                                                           t_min,
                                                           found ? best_hit.t : t_max)) {
                    continue;
                }
                gRuntimeSceneAcceleration3DTraceStats.tlasInstanceTests += 1u;
                status = runtime_scene_accel_3d_trace_instance(scene,
                                                               instance,
                                                               ray,
                                                               t_min,
                                                               found ? best_hit.t : t_max,
                                                               &candidate);
                if (status == RUNTIME_SCENE_ACCEL_3D_TRACE_HIT) {
                    if (runtime_scene_accel_3d_hit_better(&candidate, &best_hit, found)) {
                        best_hit = candidate;
                        found = true;
                    }
                } else if (status == RUNTIME_SCENE_ACCEL_3D_TRACE_UNSUPPORTED) {
                    saw_unsupported = true;
                } else if (status == RUNTIME_SCENE_ACCEL_3D_TRACE_ERROR) {
                    saw_error = true;
                }
            }
            continue;
        }
        if (node->left >= 0) {
            if (stack_count < (int)(sizeof(stack) / sizeof(stack[0]))) {
                stack[stack_count++] = node->left;
            } else {
                stack_overflow = true;
            }
        }
        if (node->right >= 0) {
            if (stack_count < (int)(sizeof(stack) / sizeof(stack[0]))) {
                stack[stack_count++] = node->right;
            } else {
                stack_overflow = true;
            }
        }
    }

    if (saw_error || stack_overflow) {
        gRuntimeSceneAcceleration3DTraceStats.traceErrors += 1u;
        if (stack_overflow) {
            runtime_scene_accel_3d_set_diag("TLAS trace failed: traversal stack overflow");
        }
        return RUNTIME_SCENE_ACCEL_3D_TRACE_ERROR;
    }
    if (saw_unsupported) {
        gRuntimeSceneAcceleration3DTraceStats.traceUnsupported += 1u;
        return RUNTIME_SCENE_ACCEL_3D_TRACE_UNSUPPORTED;
    }
    if (found) {
        *out_hit = best_hit;
        gRuntimeSceneAcceleration3DTraceStats.traceHits += 1u;
        return RUNTIME_SCENE_ACCEL_3D_TRACE_HIT;
    }
    gRuntimeSceneAcceleration3DTraceStats.traceMisses += 1u;
    return RUNTIME_SCENE_ACCEL_3D_TRACE_MISS;
}

void RuntimeSceneAcceleration3D_ResetTraceStats(void) {
    memset(&gRuntimeSceneAcceleration3DTraceStats,
           0,
           sizeof(gRuntimeSceneAcceleration3DTraceStats));
}

void RuntimeSceneAcceleration3D_SnapshotTraceStats(
    RuntimeSceneAcceleration3DTraceStats* out_stats) {
    if (!out_stats) return;
    *out_stats = gRuntimeSceneAcceleration3DTraceStats;
}

void RuntimeSceneAcceleration3D_AppendTLASDiagnostics(
    RuntimeSceneAcceleration3DDiagnostics* diagnostics) {
    if (!diagnostics) return;
    diagnostics->enabled =
        diagnostics->enabled || gRuntimeSceneAcceleration3DTLASDiagnostics.enabled;
    if (diagnostics->reuseStatus == RUNTIME_SCENE_ACCEL_3D_REUSE_DISABLED &&
        gRuntimeSceneAcceleration3DTLASDiagnostics.reuseStatus !=
            RUNTIME_SCENE_ACCEL_3D_REUSE_DISABLED) {
        diagnostics->reuseStatus = gRuntimeSceneAcceleration3DTLASDiagnostics.reuseStatus;
    }
    diagnostics->tlasNodeCount = gRuntimeSceneAcceleration3DTLASDiagnostics.tlasNodeCount;
    diagnostics->tlasInstanceCount =
        gRuntimeSceneAcceleration3DTLASDiagnostics.tlasInstanceCount;
    diagnostics->tlasRebuilds = gRuntimeSceneAcceleration3DTLASDiagnostics.tlasRebuilds;
    diagnostics->tlasRefits = gRuntimeSceneAcceleration3DTLASDiagnostics.tlasRefits;
    diagnostics->tlasBuildMs = gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBuildMs;
    diagnostics->tlasBindMs = gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBindMs;
}

void RuntimeSceneAcceleration3D_ResetTLASForTests(void) {
    runtime_scene_accel_3d_free_tlas();
    gRuntimeSceneAcceleration3DTLASDiagnostics =
        RuntimeSceneAcceleration3DDiagnostics_Disabled();
    runtime_scene_accel_3d_set_diag("ok");
}

const char* RuntimeSceneAcceleration3D_LastDiagnostics(void) {
    return gRuntimeSceneAcceleration3DLastDiagnostics;
}
