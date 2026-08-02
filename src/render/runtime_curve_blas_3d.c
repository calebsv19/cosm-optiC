#include "render/runtime_curve_blas_3d.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RUNTIME_CURVE_BLAS_3D_LEAF_SIZE 4u
#define RUNTIME_CURVE_BLAS_3D_TRACE_STACK_CAPACITY 128u

typedef struct RuntimeCurveBLAS3DNode {
    Vec3 min;
    Vec3 max;
    int left;
    int right;
    size_t start;
    size_t count;
} RuntimeCurveBLAS3DNode;

struct RuntimeCurveBLAS3D {
    RuntimeCurveBLAS3DNode *nodes;
    size_t nodeCount;
    size_t nodeCapacity;
    size_t *indices;
    size_t indexCount;
    size_t leafCount;
    size_t maxDepth;
};

static RuntimeCurveBLAS3DTraceStats gRuntimeCurveBLAS3DTraceStats;

static void runtime_curve_blas_counter_add(uint64_t *counter, uint64_t value) {
    if (!counter) return;
    (void)__atomic_fetch_add(counter, value, __ATOMIC_RELAXED);
}

static uint64_t runtime_curve_blas_counter_load(const uint64_t *counter) {
    if (!counter) return 0u;
    return __atomic_load_n(counter, __ATOMIC_RELAXED);
}

static void runtime_curve_blas_counter_max(uint64_t *counter, uint64_t value) {
    uint64_t observed = runtime_curve_blas_counter_load(counter);
    while (observed < value &&
           !__atomic_compare_exchange_n(counter,
                                        &observed,
                                        value,
                                        false,
                                        __ATOMIC_RELAXED,
                                        __ATOMIC_RELAXED)) {
    }
}

static Vec3 runtime_curve_blas_min(Vec3 a, Vec3 b) {
    return vec3(fmin(a.x, b.x), fmin(a.y, b.y), fmin(a.z, b.z));
}

static Vec3 runtime_curve_blas_max(Vec3 a, Vec3 b) {
    return vec3(fmax(a.x, b.x), fmax(a.y, b.y), fmax(a.z, b.z));
}

static void runtime_curve_blas_primitive_bounds(
    const RuntimeCurvePrimitive3D *primitive,
    Vec3 *out_min,
    Vec3 *out_max) {
    const double radius = fmax(primitive->radius0, primitive->radius1);
    const Vec3 extent = vec3(radius, radius, radius);
    *out_min = vec3_sub(runtime_curve_blas_min(primitive->p0, primitive->p1),
                        extent);
    *out_max = vec3_add(runtime_curve_blas_max(primitive->p0, primitive->p1),
                        extent);
}

static Vec3 runtime_curve_blas_primitive_centroid(
    const RuntimeCurvePrimitive3D *primitive) {
    return vec3_scale(vec3_add(primitive->p0, primitive->p1), 0.5);
}

static double runtime_curve_blas_axis(Vec3 value, int axis) {
    if (axis == 0) return value.x;
    if (axis == 1) return value.y;
    return value.z;
}

static int runtime_curve_blas_longest_axis(Vec3 min, Vec3 max) {
    const Vec3 extent = vec3_sub(max, min);
    if (extent.x >= extent.y && extent.x >= extent.z) return 0;
    if (extent.y >= extent.z) return 1;
    return 2;
}

static bool runtime_curve_blas_append_node(RuntimeCurveBLAS3D *blas,
                                           size_t *out_index) {
    if (!blas || !out_index || blas->nodeCount >= blas->nodeCapacity) {
        return false;
    }
    *out_index = blas->nodeCount++;
    memset(&blas->nodes[*out_index], 0, sizeof(blas->nodes[*out_index]));
    blas->nodes[*out_index].left = -1;
    blas->nodes[*out_index].right = -1;
    return true;
}

static bool runtime_curve_blas_index_less(const RuntimeCurveAsset3D *asset,
                                          size_t left,
                                          size_t right,
                                          int axis) {
    const double left_value = runtime_curve_blas_axis(
        runtime_curve_blas_primitive_centroid(&asset->primitives[left]),
        axis);
    const double right_value = runtime_curve_blas_axis(
        runtime_curve_blas_primitive_centroid(&asset->primitives[right]),
        axis);
    return left_value < right_value ||
           (left_value == right_value && left < right);
}

static void runtime_curve_blas_heap_sift(
    const RuntimeCurveAsset3D *asset,
    size_t *indices,
    size_t count,
    size_t root,
    int axis) {
    while (root <= (SIZE_MAX - 1u) / 2u) {
        size_t child = root * 2u + 1u;
        size_t swap_index = root;
        size_t temporary = 0u;
        if (child >= count) return;
        if (runtime_curve_blas_index_less(
                asset, indices[swap_index], indices[child], axis)) {
            swap_index = child;
        }
        if (child + 1u < count &&
            runtime_curve_blas_index_less(
                asset, indices[swap_index], indices[child + 1u], axis)) {
            swap_index = child + 1u;
        }
        if (swap_index == root) return;
        temporary = indices[root];
        indices[root] = indices[swap_index];
        indices[swap_index] = temporary;
        root = swap_index;
    }
}

static void runtime_curve_blas_sort_range(const RuntimeCurveAsset3D *asset,
                                          RuntimeCurveBLAS3D *blas,
                                          size_t start,
                                          size_t count,
                                          int axis) {
    size_t *indices = &blas->indices[start];
    if (count < 2u) return;
    for (size_t root = count / 2u; root > 0u; --root) {
        runtime_curve_blas_heap_sift(
            asset, indices, count, root - 1u, axis);
    }
    for (size_t end = count - 1u; end > 0u; --end) {
        const size_t temporary = indices[0];
        indices[0] = indices[end];
        indices[end] = temporary;
        runtime_curve_blas_heap_sift(asset, indices, end, 0u, axis);
    }
}

static bool runtime_curve_blas_build_node(const RuntimeCurveAsset3D *asset,
                                          RuntimeCurveBLAS3D *blas,
                                          size_t start,
                                          size_t count,
                                          size_t depth,
                                          size_t *out_index) {
    RuntimeCurveBLAS3DNode *node = NULL;
    Vec3 bounds_min = vec3(DBL_MAX, DBL_MAX, DBL_MAX);
    Vec3 bounds_max = vec3(-DBL_MAX, -DBL_MAX, -DBL_MAX);
    Vec3 centroid_min = vec3(DBL_MAX, DBL_MAX, DBL_MAX);
    Vec3 centroid_max = vec3(-DBL_MAX, -DBL_MAX, -DBL_MAX);
    size_t node_index = 0u;

    if (!asset || !blas || !out_index || count == 0u ||
        !runtime_curve_blas_append_node(blas, &node_index)) {
        return false;
    }
    for (size_t i = start; i < start + count; ++i) {
        Vec3 primitive_min;
        Vec3 primitive_max;
        const size_t primitive_index = blas->indices[i];
        const Vec3 centroid = runtime_curve_blas_primitive_centroid(
            &asset->primitives[primitive_index]);
        runtime_curve_blas_primitive_bounds(
            &asset->primitives[primitive_index],
            &primitive_min,
            &primitive_max);
        bounds_min = runtime_curve_blas_min(bounds_min, primitive_min);
        bounds_max = runtime_curve_blas_max(bounds_max, primitive_max);
        centroid_min = runtime_curve_blas_min(centroid_min, centroid);
        centroid_max = runtime_curve_blas_max(centroid_max, centroid);
    }

    node = &blas->nodes[node_index];
    node->min = bounds_min;
    node->max = bounds_max;
    node->start = start;
    node->count = count;
    if (depth > blas->maxDepth) blas->maxDepth = depth;
    if (count <= RUNTIME_CURVE_BLAS_3D_LEAF_SIZE) {
        blas->leafCount += 1u;
        *out_index = node_index;
        return true;
    }

    {
        const int axis =
            runtime_curve_blas_longest_axis(centroid_min, centroid_max);
        const size_t left_count = count / 2u;
        size_t left = 0u;
        size_t right = 0u;
        runtime_curve_blas_sort_range(asset, blas, start, count, axis);
        if (!runtime_curve_blas_build_node(
                asset, blas, start, left_count, depth + 1u, &left) ||
            !runtime_curve_blas_build_node(asset,
                                           blas,
                                           start + left_count,
                                           count - left_count,
                                           depth + 1u,
                                           &right)) {
            return false;
        }
        node = &blas->nodes[node_index];
        node->left = (int)left;
        node->right = (int)right;
        node->count = 0u;
    }
    *out_index = node_index;
    return true;
}

void RuntimeCurveAsset3D_ClearBLAS(RuntimeCurveAsset3D *asset) {
    if (!asset || !asset->blas) {
        if (asset) asset->blasDirty = asset->primitiveCount > 0u;
        return;
    }
    free(asset->blas->nodes);
    free(asset->blas->indices);
    free(asset->blas);
    asset->blas = NULL;
    asset->blasDirty = asset->primitiveCount > 0u;
}

bool RuntimeCurveAsset3D_BuildBLAS(RuntimeCurveAsset3D *asset) {
    RuntimeCurveBLAS3D *blas = NULL;
    size_t root = 0u;
    if (!asset || !asset->primitives || asset->primitiveCount == 0u ||
        asset->primitiveCount > (SIZE_MAX - 1u) / 2u) {
        return false;
    }
    RuntimeCurveAsset3D_ClearBLAS(asset);
    blas = calloc(1u, sizeof(*blas));
    if (!blas) return false;
    blas->nodeCapacity = asset->primitiveCount * 2u - 1u;
    blas->indexCount = asset->primitiveCount;
    blas->nodes = calloc(blas->nodeCapacity, sizeof(*blas->nodes));
    blas->indices = calloc(blas->indexCount, sizeof(*blas->indices));
    if (!blas->nodes || !blas->indices) {
        free(blas->nodes);
        free(blas->indices);
        free(blas);
        return false;
    }
    for (size_t i = 0u; i < blas->indexCount; ++i) blas->indices[i] = i;
    asset->blas = blas;
    if (!runtime_curve_blas_build_node(
            asset, blas, 0u, blas->indexCount, 1u, &root) ||
        root != 0u) {
        RuntimeCurveAsset3D_ClearBLAS(asset);
        return false;
    }
    asset->blasDirty = false;
    return true;
}

bool RuntimeCurveAsset3D_HasReadyBLAS(const RuntimeCurveAsset3D *asset) {
    return asset && asset->blas && !asset->blasDirty &&
           asset->blas->nodeCount > 0u &&
           asset->blas->indexCount == asset->primitiveCount;
}

bool RuntimeCurveAsset3D_BLASBuildStats(
    const RuntimeCurveAsset3D *asset,
    RuntimeCurveBLAS3DBuildStats *out_stats) {
    if (!out_stats) return false;
    memset(out_stats, 0, sizeof(*out_stats));
    if (!RuntimeCurveAsset3D_HasReadyBLAS(asset)) return false;
    out_stats->ready = true;
    out_stats->primitiveCount = asset->primitiveCount;
    out_stats->nodeCount = asset->blas->nodeCount;
    out_stats->leafCount = asset->blas->leafCount;
    out_stats->maxDepth = asset->blas->maxDepth;
    out_stats->totalBytes =
        sizeof(*asset->blas) +
        asset->blas->nodeCapacity * sizeof(*asset->blas->nodes) +
        asset->blas->indexCount * sizeof(*asset->blas->indices);
    return true;
}

static bool runtime_curve_blas_intersect_aabb(const Ray3D *ray,
                                              Vec3 min,
                                              Vec3 max,
                                              double t_min,
                                              double t_max,
                                              double *out_enter) {
    double enter = t_min;
    double exit = t_max;
    for (int axis = 0; axis < 3; ++axis) {
        const double origin = runtime_curve_blas_axis(ray->origin, axis);
        const double direction = runtime_curve_blas_axis(ray->direction, axis);
        const double lower = runtime_curve_blas_axis(min, axis);
        const double upper = runtime_curve_blas_axis(max, axis);
        if (fabs(direction) <= 1.0e-15) {
            if (origin < lower || origin > upper) return false;
            continue;
        }
        {
            double near_t = (lower - origin) / direction;
            double far_t = (upper - origin) / direction;
            if (near_t > far_t) {
                const double swap = near_t;
                near_t = far_t;
                far_t = swap;
            }
            enter = fmax(enter, near_t);
            exit = fmin(exit, far_t);
            if (enter > exit) return false;
        }
    }
    if (out_enter) *out_enter = enter;
    return true;
}

static bool runtime_curve_blas_hit_better(const HitInfo3D *candidate,
                                          const HitInfo3D *best,
                                          bool found) {
    const double epsilon = 1.0e-12;
    if (!found) return true;
    if (candidate->t < best->t - epsilon) return true;
    if (fabs(candidate->t - best->t) > epsilon) return false;
    return candidate->curvePrimitiveIndex < best->curvePrimitiveIndex;
}

RuntimeCurveBLAS3DTraceResult RuntimeCurveBLAS3D_TraceFirstHitStatus(
    const RuntimeCurveAsset3D *asset,
    const Ray3D *ray,
    double t_min,
    double t_max,
    HitInfo3D *out_hit) {
    HitInfo3D best;
    size_t stack[RUNTIME_CURVE_BLAS_3D_TRACE_STACK_CAPACITY];
    size_t stack_count = 0u;
    size_t max_stack = 0u;
    bool found = false;

    if (!asset || !ray || !out_hit ||
        !RuntimeCurveAsset3D_HasReadyBLAS(asset)) {
        if (out_hit) HitInfo3D_Reset(out_hit);
        return RUNTIME_CURVE_BLAS_3D_TRACE_MISS;
    }
    runtime_curve_blas_counter_add(
        &gRuntimeCurveBLAS3DTraceStats.traceCalls, 1u);
    HitInfo3D_Reset(&best);
    stack[stack_count++] = 0u;
    max_stack = stack_count;
    while (stack_count > 0u) {
        const size_t node_index = stack[--stack_count];
        const RuntimeCurveBLAS3DNode *node = &asset->blas->nodes[node_index];
        double node_enter = 0.0;
        runtime_curve_blas_counter_add(
            &gRuntimeCurveBLAS3DTraceStats.nodeVisits, 1u);
        runtime_curve_blas_counter_add(
            &gRuntimeCurveBLAS3DTraceStats.aabbTests, 1u);
        if (!runtime_curve_blas_intersect_aabb(
                ray,
                node->min,
                node->max,
                t_min,
                found ? best.t : t_max,
                &node_enter)) {
            continue;
        }
        if (node->count > 0u) {
            for (size_t i = node->start; i < node->start + node->count; ++i) {
                const size_t primitive_index = asset->blas->indices[i];
                HitInfo3D candidate;
                runtime_curve_blas_counter_add(
                    &gRuntimeCurveBLAS3DTraceStats.primitiveTests, 1u);
                if (!RuntimeRay3D_IntersectCurvePrimitive(
                        ray,
                        &asset->primitives[primitive_index],
                        (int)primitive_index,
                        t_min,
                        found ? best.t : t_max,
                        &candidate)) {
                    continue;
                }
                runtime_curve_blas_counter_add(
                    &gRuntimeCurveBLAS3DTraceStats.primitiveHits, 1u);
                if (runtime_curve_blas_hit_better(&candidate, &best, found)) {
                    best = candidate;
                    found = true;
                }
            }
            continue;
        }
        if (node->left >= 0 && node->right >= 0) {
            double left_enter = 0.0;
            double right_enter = 0.0;
            const RuntimeCurveBLAS3DNode *left = &asset->blas->nodes[node->left];
            const RuntimeCurveBLAS3DNode *right = &asset->blas->nodes[node->right];
            const bool hit_left = runtime_curve_blas_intersect_aabb(
                ray,
                left->min,
                left->max,
                t_min,
                found ? best.t : t_max,
                &left_enter);
            const bool hit_right = runtime_curve_blas_intersect_aabb(
                ray,
                right->min,
                right->max,
                t_min,
                found ? best.t : t_max,
                &right_enter);
            runtime_curve_blas_counter_add(
                &gRuntimeCurveBLAS3DTraceStats.aabbTests, 2u);
            if (hit_left && hit_right) {
                const size_t near_node =
                    left_enter <= right_enter ? (size_t)node->left
                                              : (size_t)node->right;
                const size_t far_node =
                    left_enter <= right_enter ? (size_t)node->right
                                              : (size_t)node->left;
                if (stack_count + 2u >
                    RUNTIME_CURVE_BLAS_3D_TRACE_STACK_CAPACITY) {
                    HitInfo3D_Reset(out_hit);
                    runtime_curve_blas_counter_add(
                        &gRuntimeCurveBLAS3DTraceStats.traceOverflows, 1u);
                    return RUNTIME_CURVE_BLAS_3D_TRACE_OVERFLOW;
                }
                stack[stack_count++] = far_node;
                stack[stack_count++] = near_node;
            } else if (hit_left) {
                if (stack_count + 1u >
                    RUNTIME_CURVE_BLAS_3D_TRACE_STACK_CAPACITY) {
                    HitInfo3D_Reset(out_hit);
                    runtime_curve_blas_counter_add(
                        &gRuntimeCurveBLAS3DTraceStats.traceOverflows, 1u);
                    return RUNTIME_CURVE_BLAS_3D_TRACE_OVERFLOW;
                }
                stack[stack_count++] = (size_t)node->left;
            } else if (hit_right) {
                if (stack_count + 1u >
                    RUNTIME_CURVE_BLAS_3D_TRACE_STACK_CAPACITY) {
                    HitInfo3D_Reset(out_hit);
                    runtime_curve_blas_counter_add(
                        &gRuntimeCurveBLAS3DTraceStats.traceOverflows, 1u);
                    return RUNTIME_CURVE_BLAS_3D_TRACE_OVERFLOW;
                }
                stack[stack_count++] = (size_t)node->right;
            }
            if (stack_count > max_stack) max_stack = stack_count;
        }
    }
    runtime_curve_blas_counter_max(
        &gRuntimeCurveBLAS3DTraceStats.maxStackDepth, (uint64_t)max_stack);
    if (!found) {
        HitInfo3D_Reset(out_hit);
        runtime_curve_blas_counter_add(
            &gRuntimeCurveBLAS3DTraceStats.traceMisses, 1u);
        return RUNTIME_CURVE_BLAS_3D_TRACE_MISS;
    }
    *out_hit = best;
    runtime_curve_blas_counter_add(
        &gRuntimeCurveBLAS3DTraceStats.traceHits, 1u);
    return RUNTIME_CURVE_BLAS_3D_TRACE_HIT;
}

bool RuntimeCurveBLAS3D_TraceFirstHit(
    const RuntimeCurveAsset3D *asset,
    const Ray3D *ray,
    double t_min,
    double t_max,
    HitInfo3D *out_hit) {
    return RuntimeCurveBLAS3D_TraceFirstHitStatus(
               asset, ray, t_min, t_max, out_hit) ==
           RUNTIME_CURVE_BLAS_3D_TRACE_HIT;
}

void RuntimeCurveBLAS3D_ResetTraceStats(void) {
    memset(&gRuntimeCurveBLAS3DTraceStats,
           0,
           sizeof(gRuntimeCurveBLAS3DTraceStats));
}

void RuntimeCurveBLAS3D_SnapshotTraceStats(
    RuntimeCurveBLAS3DTraceStats *out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(*out_stats));
    out_stats->traceCalls = runtime_curve_blas_counter_load(
        &gRuntimeCurveBLAS3DTraceStats.traceCalls);
    out_stats->traceHits = runtime_curve_blas_counter_load(
        &gRuntimeCurveBLAS3DTraceStats.traceHits);
    out_stats->traceMisses = runtime_curve_blas_counter_load(
        &gRuntimeCurveBLAS3DTraceStats.traceMisses);
    out_stats->traceOverflows = runtime_curve_blas_counter_load(
        &gRuntimeCurveBLAS3DTraceStats.traceOverflows);
    out_stats->nodeVisits = runtime_curve_blas_counter_load(
        &gRuntimeCurveBLAS3DTraceStats.nodeVisits);
    out_stats->aabbTests = runtime_curve_blas_counter_load(
        &gRuntimeCurveBLAS3DTraceStats.aabbTests);
    out_stats->primitiveTests = runtime_curve_blas_counter_load(
        &gRuntimeCurveBLAS3DTraceStats.primitiveTests);
    out_stats->primitiveHits = runtime_curve_blas_counter_load(
        &gRuntimeCurveBLAS3DTraceStats.primitiveHits);
    out_stats->maxStackDepth = runtime_curve_blas_counter_load(
        &gRuntimeCurveBLAS3DTraceStats.maxStackDepth);
}
