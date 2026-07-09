#include "render/runtime_triangle_bvh_internal_3d.h"

static atomic_uint_fast64_t g_trace_calls;
static atomic_uint_fast64_t g_trace_hits;
static atomic_uint_fast64_t g_trace_misses;
static atomic_uint_fast64_t g_trace_overflows;
static atomic_uint_fast64_t g_flat_fallback_calls;
static atomic_uint_fast64_t g_overflow_fallback_calls;
static atomic_uint_fast64_t g_node_visits;
static atomic_uint_fast64_t g_leaf_visits;
static atomic_uint_fast64_t g_aabb_tests;
static atomic_uint_fast64_t g_aabb_hits;
static atomic_uint_fast64_t g_triangle_tests;
static atomic_uint_fast64_t g_triangle_hits;
static atomic_uint_fast64_t g_max_stack_depth;
static atomic_bool g_trace_stats_enabled;
static atomic_int g_traversal_stack_limit;

static bool runtime_triangle_bvh_trace_stats_enabled(void) {
    return atomic_load_explicit(&g_trace_stats_enabled, memory_order_relaxed);
}

static void runtime_triangle_bvh_counter_add(atomic_uint_fast64_t* counter,
                                             uint64_t value) {
    if (!runtime_triangle_bvh_trace_stats_enabled()) return;
    atomic_fetch_add_explicit(counter, value, memory_order_relaxed);
}

static uint64_t runtime_triangle_bvh_counter_load(const atomic_uint_fast64_t* counter) {
    return atomic_load_explicit(counter, memory_order_relaxed);
}

static void runtime_triangle_bvh_counter_store(atomic_uint_fast64_t* counter,
                                               uint64_t value) {
    atomic_store_explicit(counter, value, memory_order_relaxed);
}

static void runtime_triangle_bvh_counter_max(atomic_uint_fast64_t* counter,
                                             uint64_t value) {
    if (!runtime_triangle_bvh_trace_stats_enabled()) return;
    uint64_t current = atomic_load_explicit(counter, memory_order_relaxed);
    while (current < value &&
           !atomic_compare_exchange_weak_explicit(counter,
                                                  &current,
                                                  value,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed)) {
    }
}

static int runtime_triangle_bvh_traversal_stack_limit(void) {
    int limit = atomic_load_explicit(&g_traversal_stack_limit, memory_order_relaxed);
    if (limit <= 0 || limit > RUNTIME_TRIANGLE_BVH_MAX_STACK) {
        return RUNTIME_TRIANGLE_BVH_MAX_STACK;
    }
    return limit;
}

void RuntimeTriangleBVH3D_ResetTraceStats(void) {
    atomic_store_explicit(&g_trace_stats_enabled, true, memory_order_relaxed);
    runtime_triangle_bvh_counter_store(&g_trace_calls, 0u);
    runtime_triangle_bvh_counter_store(&g_trace_hits, 0u);
    runtime_triangle_bvh_counter_store(&g_trace_misses, 0u);
    runtime_triangle_bvh_counter_store(&g_trace_overflows, 0u);
    runtime_triangle_bvh_counter_store(&g_flat_fallback_calls, 0u);
    runtime_triangle_bvh_counter_store(&g_overflow_fallback_calls, 0u);
    runtime_triangle_bvh_counter_store(&g_node_visits, 0u);
    runtime_triangle_bvh_counter_store(&g_leaf_visits, 0u);
    runtime_triangle_bvh_counter_store(&g_aabb_tests, 0u);
    runtime_triangle_bvh_counter_store(&g_aabb_hits, 0u);
    runtime_triangle_bvh_counter_store(&g_triangle_tests, 0u);
    runtime_triangle_bvh_counter_store(&g_triangle_hits, 0u);
    runtime_triangle_bvh_counter_store(&g_max_stack_depth, 0u);
}

void RuntimeTriangleBVH3D_SnapshotTraceStats(RuntimeTriangleBVH3DTraceStats* out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(*out_stats));
    out_stats->traceCalls = runtime_triangle_bvh_counter_load(&g_trace_calls);
    out_stats->traceHits = runtime_triangle_bvh_counter_load(&g_trace_hits);
    out_stats->traceMisses = runtime_triangle_bvh_counter_load(&g_trace_misses);
    out_stats->traceOverflows = runtime_triangle_bvh_counter_load(&g_trace_overflows);
    out_stats->flatFallbackCalls = runtime_triangle_bvh_counter_load(&g_flat_fallback_calls);
    out_stats->overflowFallbackCalls =
        runtime_triangle_bvh_counter_load(&g_overflow_fallback_calls);
    out_stats->nodeVisits = runtime_triangle_bvh_counter_load(&g_node_visits);
    out_stats->leafVisits = runtime_triangle_bvh_counter_load(&g_leaf_visits);
    out_stats->aabbTests = runtime_triangle_bvh_counter_load(&g_aabb_tests);
    out_stats->aabbHits = runtime_triangle_bvh_counter_load(&g_aabb_hits);
    out_stats->triangleTests = runtime_triangle_bvh_counter_load(&g_triangle_tests);
    out_stats->triangleHits = runtime_triangle_bvh_counter_load(&g_triangle_hits);
    out_stats->maxStackDepth = runtime_triangle_bvh_counter_load(&g_max_stack_depth);
}

void RuntimeTriangleBVH3D_DisableTraceStats(void) {
    atomic_store_explicit(&g_trace_stats_enabled, false, memory_order_relaxed);
}

void RuntimeTriangleBVH3D_RecordFlatFallback(bool due_to_overflow) {
    runtime_triangle_bvh_counter_add(&g_flat_fallback_calls, 1u);
    if (due_to_overflow) {
        runtime_triangle_bvh_counter_add(&g_overflow_fallback_calls, 1u);
    }
}

void RuntimeTriangleBVH3D_SetTraversalStackLimitForTests(int max_stack_depth) {
    atomic_store_explicit(&g_traversal_stack_limit, max_stack_depth, memory_order_relaxed);
}

static bool runtime_triangle_bvh_intersect_aabb(const Ray3D* ray,
                                                Vec3 min,
                                                Vec3 max,
                                                double t_min,
                                                double t_max,
                                                double* out_t_enter) {
    double enter = t_min;
    double exit = t_max;
    const double origins[3] = {ray->origin.x, ray->origin.y, ray->origin.z};
    const double dirs[3] = {ray->direction.x, ray->direction.y, ray->direction.z};
    const double mins[3] = {min.x, min.y, min.z};
    const double maxs[3] = {max.x, max.y, max.z};

    runtime_triangle_bvh_counter_add(&g_aabb_tests, 1u);
    for (int axis = 0; axis < 3; ++axis) {
        if (fabs(dirs[axis]) <= 1e-12) {
            if (origins[axis] < mins[axis] || origins[axis] > maxs[axis]) {
                return false;
            }
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

    if (out_t_enter) *out_t_enter = enter;
    runtime_triangle_bvh_counter_add(&g_aabb_hits, 1u);
    return true;
}

static bool runtime_triangle_bvh_hit_better(const HitInfo3D* candidate,
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

RuntimeTriangleBVH3DTraceResult RuntimeTriangleBVH3D_TraceFirstHitStatus(
    const RuntimeTriangleMesh3D* mesh,
    const Ray3D* ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit) {
    int stack[RUNTIME_TRIANGLE_BVH_MAX_STACK];
    int stack_count = 0;
    int stack_limit = runtime_triangle_bvh_traversal_stack_limit();
    HitInfo3D best_hit = {0};
    bool found = false;
    if (!RuntimeTriangleMesh3D_HasReadyBVH(mesh) || !ray || !out_hit) {
        return RUNTIME_TRIANGLE_BVH_3D_TRACE_MISS;
    }

    runtime_triangle_bvh_counter_add(&g_trace_calls, 1u);
    HitInfo3D_Reset(&best_hit);
    stack[stack_count++] = 0;
    runtime_triangle_bvh_counter_max(&g_max_stack_depth, (uint64_t)stack_count);
    while (stack_count > 0) {
        int node_index = stack[--stack_count];
        const RuntimeTriangleBVH3DNode* node = NULL;
        double node_enter = 0.0;
        if (node_index < 0 || node_index >= mesh->bvh->nodeCount) continue;
        node = &mesh->bvh->nodes[node_index];
        runtime_triangle_bvh_counter_add(&g_node_visits, 1u);
        if (!runtime_triangle_bvh_intersect_aabb(ray,
                                                 node->min,
                                                 node->max,
                                                 t_min,
                                                 found ? best_hit.t : t_max,
                                                 &node_enter)) {
            continue;
        }

        if (node->count > 0) {
            runtime_triangle_bvh_counter_add(&g_leaf_visits, 1u);
            for (int i = 0; i < node->count; ++i) {
                int triangle_index = mesh->bvh->indices[node->start + i];
                HitInfo3D hit = {0};
                runtime_triangle_bvh_counter_add(&g_triangle_tests, 1u);
                if (!RuntimeRay3D_IntersectTriangle(ray,
                                                    &mesh->triangles[triangle_index],
                                                    triangle_index,
                                                    t_min,
                                                    found ? best_hit.t : t_max,
                                                    &hit)) {
                    continue;
                }
                runtime_triangle_bvh_counter_add(&g_triangle_hits, 1u);
                if (runtime_triangle_bvh_hit_better(&hit, &best_hit, found)) {
                    best_hit = hit;
                    found = true;
                }
            }
            continue;
        }

        if (node->left >= 0 && node->right >= 0) {
            double left_enter = 0.0;
            double right_enter = 0.0;
            bool hit_left = runtime_triangle_bvh_intersect_aabb(
                ray,
                mesh->bvh->nodes[node->left].min,
                mesh->bvh->nodes[node->left].max,
                t_min,
                found ? best_hit.t : t_max,
                &left_enter);
            bool hit_right = runtime_triangle_bvh_intersect_aabb(
                ray,
                mesh->bvh->nodes[node->right].min,
                mesh->bvh->nodes[node->right].max,
                t_min,
                found ? best_hit.t : t_max,
                &right_enter);
            if (hit_left && hit_right) {
                int near_node = left_enter <= right_enter ? node->left : node->right;
                int far_node = left_enter <= right_enter ? node->right : node->left;
                if (stack_count + 2 > stack_limit) {
                    runtime_triangle_bvh_counter_add(&g_trace_overflows, 1u);
                    HitInfo3D_Reset(out_hit);
                    return RUNTIME_TRIANGLE_BVH_3D_TRACE_OVERFLOW;
                }
                stack[stack_count++] = far_node;
                stack[stack_count++] = near_node;
                runtime_triangle_bvh_counter_max(&g_max_stack_depth, (uint64_t)stack_count);
            } else if (hit_left) {
                if (stack_count + 1 > stack_limit) {
                    runtime_triangle_bvh_counter_add(&g_trace_overflows, 1u);
                    HitInfo3D_Reset(out_hit);
                    return RUNTIME_TRIANGLE_BVH_3D_TRACE_OVERFLOW;
                }
                stack[stack_count++] = node->left;
                runtime_triangle_bvh_counter_max(&g_max_stack_depth, (uint64_t)stack_count);
            } else if (hit_right) {
                if (stack_count + 1 > stack_limit) {
                    runtime_triangle_bvh_counter_add(&g_trace_overflows, 1u);
                    HitInfo3D_Reset(out_hit);
                    return RUNTIME_TRIANGLE_BVH_3D_TRACE_OVERFLOW;
                }
                stack[stack_count++] = node->right;
                runtime_triangle_bvh_counter_max(&g_max_stack_depth, (uint64_t)stack_count);
            }
        }
    }

    if (!found) {
        HitInfo3D_Reset(out_hit);
        runtime_triangle_bvh_counter_add(&g_trace_misses, 1u);
        return RUNTIME_TRIANGLE_BVH_3D_TRACE_MISS;
    }
    *out_hit = best_hit;
    runtime_triangle_bvh_counter_add(&g_trace_hits, 1u);
    return RUNTIME_TRIANGLE_BVH_3D_TRACE_HIT;
}

bool RuntimeTriangleBVH3D_TraceFirstHit(const RuntimeTriangleMesh3D* mesh,
                                        const Ray3D* ray,
                                        double t_min,
                                        double t_max,
                                        HitInfo3D* out_hit) {
    return RuntimeTriangleBVH3D_TraceFirstHitStatus(mesh, ray, t_min, t_max, out_hit) ==
           RUNTIME_TRIANGLE_BVH_3D_TRACE_HIT;
}
