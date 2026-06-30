#include "render/runtime_scene_accel_3d.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct RuntimeSceneAcceleration3DInstanceBounds {
    Vec3 min;
    Vec3 max;
    int primitiveIndex;
    int sceneObjectIndex;
    char objectId[RUNTIME_SCENE_3D_MAX_OBJECT_ID];
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
static int gRuntimeSceneAcceleration3DInstanceCount;
static RuntimeSceneAcceleration3DTLASNode* gRuntimeSceneAcceleration3DTLASNodes;
static int gRuntimeSceneAcceleration3DTLASNodeCount;
static int gRuntimeSceneAcceleration3DTLASNodeCapacity;
static RuntimeSceneAcceleration3DDiagnostics gRuntimeSceneAcceleration3DTLASDiagnostics;
static char gRuntimeSceneAcceleration3DLastDiagnostics[1024] = "ok";

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

static void runtime_scene_accel_3d_free_tlas(void) {
    free(gRuntimeSceneAcceleration3DInstances);
    gRuntimeSceneAcceleration3DInstances = NULL;
    gRuntimeSceneAcceleration3DInstanceCount = 0;
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
            valid = true;
        }
        if (!valid) continue;
        instances[instance_count++] = bounds;
    }

    gRuntimeSceneAcceleration3DInstances = instances;
    gRuntimeSceneAcceleration3DInstanceCount = instance_count;
    return true;
}

bool RuntimeSceneAcceleration3D_RebuildTLASFromScene(const RuntimeScene3D* scene) {
    int* indices = NULL;
    RuntimeSceneAcceleration3DDiagnostics prior = gRuntimeSceneAcceleration3DTLASDiagnostics;

    runtime_scene_accel_3d_set_diag("ok");
    runtime_scene_accel_3d_free_tlas();
    gRuntimeSceneAcceleration3DTLASDiagnostics.enabled = false;
    gRuntimeSceneAcceleration3DTLASDiagnostics.reuseStatus =
        RUNTIME_SCENE_ACCEL_3D_REUSE_DISABLED;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasNodeCount = 0u;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasInstanceCount = 0u;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasRebuilds = prior.tlasRebuilds;
    gRuntimeSceneAcceleration3DTLASDiagnostics.tlasRefits = prior.tlasRefits;

    if (!scene || scene->primitiveCount <= 0 || scene->triangleMesh.triangleCount <= 0) {
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
    return true;
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
