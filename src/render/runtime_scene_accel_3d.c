#include "render/runtime_scene_accel_3d_internal.h"

#include "render/runtime_dynamic_geometry_accel_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
enum { RUNTIME_SCENE_ACCEL_3D_COMPATIBLE_SCENE_CAPACITY = 64 };
/* Temporal/prepared rendering can bind several geometry-identical scene snapshots. */
static const RuntimeScene3D*
    gRuntimeSceneAcceleration3DCompatibleScenes[RUNTIME_SCENE_ACCEL_3D_COMPATIBLE_SCENE_CAPACITY];
static int gRuntimeSceneAcceleration3DCompatibleSceneCount;
static int gRuntimeSceneAcceleration3DPreparedPrimitiveCount;
static int gRuntimeSceneAcceleration3DPreparedTriangleCount;
static uint64_t gRuntimeSceneAcceleration3DPreparedGeometrySignature;
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

static void runtime_scene_accel_3d_free_tlas(void) {
    gRuntimeSceneAcceleration3DPreparedScene = NULL;
    memset(gRuntimeSceneAcceleration3DCompatibleScenes,
           0,
           sizeof(gRuntimeSceneAcceleration3DCompatibleScenes));
    gRuntimeSceneAcceleration3DCompatibleSceneCount = 0;
    gRuntimeSceneAcceleration3DPreparedPrimitiveCount = 0;
    gRuntimeSceneAcceleration3DPreparedTriangleCount = 0;
    gRuntimeSceneAcceleration3DPreparedGeometrySignature = 0u;
    RuntimeSceneAcceleration3D_FreeInstanceIdentityMaps(
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

static void runtime_scene_accel_3d_register_compatible_scene(
    const RuntimeScene3D* scene) {
    if (!scene) return;
    for (int i = 0; i < gRuntimeSceneAcceleration3DCompatibleSceneCount; ++i) {
        if (gRuntimeSceneAcceleration3DCompatibleScenes[i] == scene) return;
    }
    if (gRuntimeSceneAcceleration3DCompatibleSceneCount >=
        RUNTIME_SCENE_ACCEL_3D_COMPATIBLE_SCENE_CAPACITY) {
        return;
    }
    gRuntimeSceneAcceleration3DCompatibleScenes
        [gRuntimeSceneAcceleration3DCompatibleSceneCount++] = scene;
}

static bool runtime_scene_accel_3d_scene_is_compatible(
    const RuntimeScene3D* scene) {
    if (!scene) return false;
    for (int i = 0; i < gRuntimeSceneAcceleration3DCompatibleSceneCount; ++i) {
        if (gRuntimeSceneAcceleration3DCompatibleScenes[i] == scene) return true;
    }
    return false;
}

static uint64_t runtime_scene_accel_3d_hash_bytes(uint64_t hash,
                                                  const void* data,
                                                  size_t size) {
    const unsigned char* bytes = (const unsigned char*)data;
    if (!bytes && size > 0u) return hash;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= (uint64_t)bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

static uint64_t runtime_scene_accel_3d_hash_int(uint64_t hash, int value) {
    return runtime_scene_accel_3d_hash_bytes(hash, &value, sizeof(value));
}

static uint64_t runtime_scene_accel_3d_hash_bool(uint64_t hash, bool value) {
    const unsigned char byte = value ? 1u : 0u;
    return runtime_scene_accel_3d_hash_bytes(hash, &byte, sizeof(byte));
}

static uint64_t runtime_scene_accel_3d_hash_double(uint64_t hash, double value) {
    uint64_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return runtime_scene_accel_3d_hash_bytes(hash, &bits, sizeof(bits));
}

static uint64_t runtime_scene_accel_3d_hash_vec3(uint64_t hash, Vec3 value) {
    hash = runtime_scene_accel_3d_hash_double(hash, value.x);
    hash = runtime_scene_accel_3d_hash_double(hash, value.y);
    return runtime_scene_accel_3d_hash_double(hash, value.z);
}

static uint64_t runtime_scene_accel_3d_geometry_signature_prefix(
    const RuntimeScene3D* scene,
    int primitive_count,
    int triangle_count) {
    uint64_t hash = 1469598103934665603ull;
    const int sample_budget = 64;
    int last_sampled = -1;
    if (!scene) return 0u;
    if (primitive_count < 0 || primitive_count > scene->primitiveCount ||
        triangle_count < 0 || triangle_count > scene->triangleMesh.triangleCount) {
        return 0u;
    }
    hash = runtime_scene_accel_3d_hash_int(hash, primitive_count);
    hash = runtime_scene_accel_3d_hash_int(hash, triangle_count);
    for (int i = 0; i < primitive_count; ++i) {
        const RuntimePrimitive3D* primitive = &scene->primitives[i];
        hash = runtime_scene_accel_3d_hash_int(hash, (int)primitive->kind);
        hash = runtime_scene_accel_3d_hash_int(hash, (int)primitive->source.kind);
        hash = runtime_scene_accel_3d_hash_int(hash, primitive->source.sceneObjectIndex);
        hash = runtime_scene_accel_3d_hash_bytes(hash,
                                                 primitive->source.objectId,
                                                 strnlen(primitive->source.objectId,
                                                         sizeof(primitive->source.objectId)));
    }

    for (int sample = 0; sample < sample_budget && sample < triangle_count; ++sample) {
        int i = 0;
        const RuntimeTriangle3D* triangle = NULL;
        if (sample_budget >= triangle_count) {
            i = sample;
        } else if (sample == 0) {
            i = 0;
        } else if (sample == sample_budget - 1) {
            i = triangle_count - 1;
        } else {
            i = (int)(((long long)sample * (long long)(triangle_count - 1)) /
                      (long long)(sample_budget - 1));
        }
        if (i == last_sampled) continue;
        last_sampled = i;
        triangle = &scene->triangleMesh.triangles[i];
        hash = runtime_scene_accel_3d_hash_int(hash, i);
        hash = runtime_scene_accel_3d_hash_vec3(hash, triangle->p0);
        hash = runtime_scene_accel_3d_hash_vec3(hash, triangle->p1);
        hash = runtime_scene_accel_3d_hash_vec3(hash, triangle->p2);
        hash = runtime_scene_accel_3d_hash_vec3(hash, triangle->normal);
        hash = runtime_scene_accel_3d_hash_bool(hash, triangle->hasVertexNormals);
        hash = runtime_scene_accel_3d_hash_vec3(hash, triangle->vertexNormal0);
        hash = runtime_scene_accel_3d_hash_vec3(hash, triangle->vertexNormal1);
        hash = runtime_scene_accel_3d_hash_vec3(hash, triangle->vertexNormal2);
        hash = runtime_scene_accel_3d_hash_bool(hash, triangle->twoSided);
        hash = runtime_scene_accel_3d_hash_bool(hash, triangle->hasObjectTextureCoords);
        hash = runtime_scene_accel_3d_hash_vec3(hash, triangle->objectTexture0);
        hash = runtime_scene_accel_3d_hash_vec3(hash, triangle->objectTexture1);
        hash = runtime_scene_accel_3d_hash_vec3(hash, triangle->objectTexture2);
        hash = runtime_scene_accel_3d_hash_int(hash, triangle->primitiveIndex);
        hash = runtime_scene_accel_3d_hash_int(hash, triangle->sceneObjectIndex);
        hash = runtime_scene_accel_3d_hash_int(hash, triangle->localTriangleIndex);
    }
    return hash;
}

static uint64_t runtime_scene_accel_3d_geometry_signature(const RuntimeScene3D* scene) {
    if (!scene) return 0u;
    return runtime_scene_accel_3d_geometry_signature_prefix(
        scene,
        scene->primitiveCount,
        scene->triangleMesh.triangleCount);
}

static bool runtime_scene_accel_3d_has_compatible_dynamic_water_extension(
    const RuntimeScene3D* scene) {
    const RuntimePrimitive3D* water_primitive = NULL;
    const RuntimeTriangle3D* first_water_triangle = NULL;
    const RuntimeTriangle3D* last_water_triangle = NULL;
    if (!scene || scene->primitiveCount != gRuntimeSceneAcceleration3DPreparedPrimitiveCount + 1 ||
        scene->triangleMesh.triangleCount <= gRuntimeSceneAcceleration3DPreparedTriangleCount) {
        return false;
    }
    water_primitive = &scene->primitives[gRuntimeSceneAcceleration3DPreparedPrimitiveCount];
    if (strcmp(water_primitive->source.objectId, "water_surface") != 0) {
        return false;
    }
    first_water_triangle =
        &scene->triangleMesh.triangles[gRuntimeSceneAcceleration3DPreparedTriangleCount];
    last_water_triangle =
        &scene->triangleMesh.triangles[scene->triangleMesh.triangleCount - 1];
    if (first_water_triangle->primitiveIndex !=
            gRuntimeSceneAcceleration3DPreparedPrimitiveCount ||
        last_water_triangle->primitiveIndex !=
            gRuntimeSceneAcceleration3DPreparedPrimitiveCount) {
        return false;
    }
    return runtime_scene_accel_3d_geometry_signature_prefix(
               scene,
               gRuntimeSceneAcceleration3DPreparedPrimitiveCount,
               gRuntimeSceneAcceleration3DPreparedTriangleCount) ==
           gRuntimeSceneAcceleration3DPreparedGeometrySignature;
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
    if (!RuntimeSceneAcceleration3D_CaptureInstanceBounds(
            scene,
            &gRuntimeSceneAcceleration3DInstances,
            &gRuntimeSceneAcceleration3DInstanceCount,
            runtime_scene_accel_3d_set_diag)) {
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
    runtime_scene_accel_3d_register_compatible_scene(scene);
    gRuntimeSceneAcceleration3DPreparedPrimitiveCount = scene->primitiveCount;
    gRuntimeSceneAcceleration3DPreparedTriangleCount = scene->triangleMesh.triangleCount;
    gRuntimeSceneAcceleration3DPreparedGeometrySignature =
        runtime_scene_accel_3d_geometry_signature(scene);
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
    if (!RuntimeSceneAcceleration3D_ApplyMeshAssetRecords(
            scene,
            mesh_assets,
            gRuntimeSceneAcceleration3DInstances,
            gRuntimeSceneAcceleration3DInstanceCount)) {
        return false;
    }
    return true;
}

bool RuntimeSceneAcceleration3D_BindPreparedSceneForTracing(const RuntimeScene3D* scene) {
    struct timespec bind_start = {0};
    bool compatible_dynamic_water_extension = false;
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
    compatible_dynamic_water_extension =
        runtime_scene_accel_3d_has_compatible_dynamic_water_extension(scene);
    if (!compatible_dynamic_water_extension &&
        (scene->primitiveCount != gRuntimeSceneAcceleration3DPreparedPrimitiveCount ||
        scene->triangleMesh.triangleCount !=
            gRuntimeSceneAcceleration3DPreparedTriangleCount)) {
        runtime_scene_accel_3d_set_diag(
            "TLAS bind skipped: prepared scene geometry counts differ");
        gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBindMs +=
            runtime_scene_accel_3d_elapsed_ms_since(&bind_start);
        return false;
    }
    if (!compatible_dynamic_water_extension &&
        runtime_scene_accel_3d_geometry_signature(scene) !=
        gRuntimeSceneAcceleration3DPreparedGeometrySignature) {
        runtime_scene_accel_3d_set_diag(
            "TLAS bind skipped: prepared scene geometry signature differs");
        gRuntimeSceneAcceleration3DTLASDiagnostics.tlasBindMs +=
            runtime_scene_accel_3d_elapsed_ms_since(&bind_start);
        return false;
    }
    runtime_scene_accel_3d_register_compatible_scene(scene);
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
    double interval_epsilon =
        1e-9 * fmax(1.0, fmax(fabs(t_min), fabs(t_max)));
    double enter = t_min - interval_epsilon;
    double exit = t_max + interval_epsilon;
    if (!ray) return false;
    const double origins[3] = {ray->origin.x, ray->origin.y, ray->origin.z};
    const double dirs[3] = {ray->direction.x, ray->direction.y, ray->direction.z};
    const double mins[3] = {min_bounds.x, min_bounds.y, min_bounds.z};
    const double maxs[3] = {max_bounds.x, max_bounds.y, max_bounds.z};

    for (int axis = 0; axis < 3; ++axis) {
        double bounds_epsilon =
            1e-9 * fmax(1.0, fmax(fabs(mins[axis]), fabs(maxs[axis])));
        double padded_min = mins[axis] - bounds_epsilon;
        double padded_max = maxs[axis] + bounds_epsilon;
        if (fabs(dirs[axis]) <= 1e-12) {
            if (origins[axis] < padded_min || origins[axis] > padded_max) return false;
            continue;
        }
        double inv_dir = 1.0 / dirs[axis];
        double t0 = (padded_min - origins[axis]) * inv_dir;
        double t1 = (padded_max - origins[axis]) * inv_dir;
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

static bool runtime_scene_accel_3d_remap_hit(
    const RuntimeScene3D* scene,
    const RuntimeSceneAcceleration3DInstanceBounds* instance,
    const Ray3D* world_ray,
    const HitInfo3D* local_hit,
    double t_min,
    double t_max,
    HitInfo3D* out_hit,
    bool* out_identity_failure) {
    int scene_triangle_index = -1;
    const RuntimeTriangle3D* scene_triangle = NULL;

    if (out_identity_failure) *out_identity_failure = false;
    if (!scene || !instance || !world_ray || !local_hit || !out_hit) {
        if (out_identity_failure) *out_identity_failure = true;
        return false;
    }
    scene_triangle_index =
        runtime_scene_accel_3d_remap_scene_triangle(scene,
                                                    instance,
                                                    local_hit->localTriangleIndex);
    if (scene_triangle_index < 0 ||
        scene_triangle_index >= scene->triangleMesh.triangleCount) {
        if (out_identity_failure) *out_identity_failure = true;
        gRuntimeSceneAcceleration3DTraceStats.identityRemapTriangleLookupFailures += 1u;
        return false;
    }

    scene_triangle = &scene->triangleMesh.triangles[scene_triangle_index];
    if (!RuntimeRay3D_IntersectTriangle(world_ray,
                                       scene_triangle,
                                       scene_triangle_index,
                                       t_min,
                                       t_max,
                                       out_hit)) {
        return false;
    }
    if (instance->primitiveIndex >= 0 && instance->primitiveIndex < scene->primitiveCount) {
        out_hit->source = scene->primitives[instance->primitiveIndex].source;
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
    Vec3 local_direction;
    double local_direction_scale = 0.0;
    double local_t_min = 0.0;
    double local_t_max = DBL_MAX;
    double world_t_tolerance = 0.0;
    HitInfo3D local_hit = {0};
    RuntimeTriangleBVH3DTraceResult blas_result;
    bool identity_failure = false;

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
        RuntimeSceneAcceleration3D_InverseTransformPoint(instance, world_ray->origin);
    local_direction = RuntimeSceneAcceleration3D_InverseTransformDirection(
        instance,
        world_ray->direction);
    local_direction_scale = vec3_length(local_direction);
    if (!(local_direction_scale > 1e-9)) {
        return RUNTIME_SCENE_ACCEL_3D_TRACE_ERROR;
    }
    local_ray.direction = vec3_scale(local_direction, 1.0 / local_direction_scale);
    world_t_tolerance = 1e-9 * fmax(1.0, fmax(fabs(t_min), fabs(t_max)));
    local_t_min = fmax(0.0, t_min - world_t_tolerance) * local_direction_scale;
    if (t_max + world_t_tolerance < DBL_MAX / local_direction_scale) {
        local_t_max = (t_max + world_t_tolerance) * local_direction_scale;
    }

    gRuntimeSceneAcceleration3DTraceStats.blasTraceCalls += 1u;
    blas_result = RuntimeTriangleBVH3D_TraceFirstHitStatus(instance->localMesh,
                                                           &local_ray,
                                                           local_t_min,
                                                           local_t_max,
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
                                          out_hit,
                                          &identity_failure)) {
        if (identity_failure) {
            gRuntimeSceneAcceleration3DTraceStats.identityRemapFailures += 1u;
            return RUNTIME_SCENE_ACCEL_3D_TRACE_ERROR;
        }
        return RUNTIME_SCENE_ACCEL_3D_TRACE_MISS;
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
    if (!runtime_scene_accel_3d_scene_is_compatible(scene)) {
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
