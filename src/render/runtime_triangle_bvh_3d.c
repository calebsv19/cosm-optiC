#include "render/runtime_triangle_bvh_3d.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RUNTIME_TRIANGLE_BVH_LEAF_SIZE 4
#ifndef RUNTIME_TRIANGLE_BVH_MAX_STACK
#define RUNTIME_TRIANGLE_BVH_MAX_STACK 128
#endif

typedef struct {
    Vec3 min;
    Vec3 max;
    int left;
    int right;
    int start;
    int count;
} RuntimeTriangleBVH3DNode;

typedef struct {
    int index;
    double centroid;
} RuntimeTriangleBVH3DSortItem;

struct RuntimeTriangleBVH3D {
    RuntimeTriangleBVH3DNode* nodes;
    int nodeCount;
    int nodeCapacity;
    int leafCount;
    int maxDepth;
    int maxLeafTriangleCount;
    double buildCpuMs;
    uint64_t nodeBytes;
    uint64_t indexBytes;
    uint64_t centroidBytes;
    uint64_t sortScratchBytes;
    uint64_t buildScratchBytes;
    uint64_t totalBytes;
    int* indices;
    Vec3* centroids;
    RuntimeTriangleBVH3DSortItem* sortScratch;
    int indexCount;
};

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
static char gRuntimeTriangleBVH3DLastDiagnostics[512] = "ok";

static void runtime_triangle_bvh_set_diag(const char* message) {
    snprintf(gRuntimeTriangleBVH3DLastDiagnostics,
             sizeof(gRuntimeTriangleBVH3DLastDiagnostics),
             "%s",
             (message && message[0]) ? message : "ok");
}

const char* RuntimeTriangleMesh3D_BVHLastDiagnostics(void) {
    return gRuntimeTriangleBVH3DLastDiagnostics;
}

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

static double runtime_triangle_bvh_clock_ms(clock_t start_clock, clock_t end_clock) {
    if (end_clock <= start_clock) return 0.0;
    return ((double)(end_clock - start_clock) * 1000.0) / (double)CLOCKS_PER_SEC;
}

static Vec3 runtime_triangle_bvh_min3(Vec3 a, Vec3 b) {
    return vec3(fmin(a.x, b.x), fmin(a.y, b.y), fmin(a.z, b.z));
}

static Vec3 runtime_triangle_bvh_max3(Vec3 a, Vec3 b) {
    return vec3(fmax(a.x, b.x), fmax(a.y, b.y), fmax(a.z, b.z));
}

static Vec3 runtime_triangle_bvh_triangle_min(const RuntimeTriangle3D* triangle) {
    return runtime_triangle_bvh_min3(runtime_triangle_bvh_min3(triangle->p0, triangle->p1),
                                     triangle->p2);
}

static Vec3 runtime_triangle_bvh_triangle_max(const RuntimeTriangle3D* triangle) {
    return runtime_triangle_bvh_max3(runtime_triangle_bvh_max3(triangle->p0, triangle->p1),
                                     triangle->p2);
}

static Vec3 runtime_triangle_bvh_triangle_centroid(const RuntimeTriangle3D* triangle) {
    return vec3((triangle->p0.x + triangle->p1.x + triangle->p2.x) / 3.0,
                (triangle->p0.y + triangle->p1.y + triangle->p2.y) / 3.0,
                (triangle->p0.z + triangle->p1.z + triangle->p2.z) / 3.0);
}

static bool runtime_triangle_bvh_vec3_isfinite(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static double runtime_triangle_bvh_axis_value(Vec3 value, int axis) {
    if (axis == 0) return value.x;
    if (axis == 1) return value.y;
    return value.z;
}

static int runtime_triangle_bvh_compare_sort_item(const void* left, const void* right) {
    const RuntimeTriangleBVH3DSortItem* a =
        (const RuntimeTriangleBVH3DSortItem*)left;
    const RuntimeTriangleBVH3DSortItem* b =
        (const RuntimeTriangleBVH3DSortItem*)right;
    if (a->centroid < b->centroid) return -1;
    if (a->centroid > b->centroid) return 1;
    if (a->index < b->index) return -1;
    if (a->index > b->index) return 1;
    return 0;
}

static bool runtime_triangle_bvh_sort_indices(RuntimeTriangleBVH3D* bvh,
                                              int start,
                                              int count,
                                              int axis) {
    if (!bvh || !bvh->indices || !bvh->centroids || !bvh->sortScratch ||
        start < 0 || count <= 0 || start + count > bvh->indexCount) {
        char diag[256];
        snprintf(diag,
                 sizeof(diag),
                 "sort indices invalid input: start=%d count=%d index_count=%d axis=%d has_indices=%s has_centroids=%s has_sort_scratch=%s",
                 start,
                 count,
                 bvh ? bvh->indexCount : -1,
                 axis,
                 (bvh && bvh->indices) ? "true" : "false",
                 (bvh && bvh->centroids) ? "true" : "false",
                 (bvh && bvh->sortScratch) ? "true" : "false");
        runtime_triangle_bvh_set_diag(diag);
        return false;
    }
    for (int i = 0; i < count; ++i) {
        int index = bvh->indices[start + i];
        if (index < 0 || index >= bvh->indexCount) {
            char diag[256];
            snprintf(diag,
                     sizeof(diag),
                     "sort indices invalid triangle index: start=%d count=%d offset=%d triangle_index=%d index_count=%d",
                     start,
                     count,
                     i,
                     index,
                     bvh->indexCount);
            runtime_triangle_bvh_set_diag(diag);
            return false;
        }
        bvh->sortScratch[i].index = index;
        bvh->sortScratch[i].centroid =
            runtime_triangle_bvh_axis_value(bvh->centroids[index], axis);
        if (!isfinite(bvh->sortScratch[i].centroid)) {
            char diag[256];
            snprintf(diag,
                     sizeof(diag),
                     "sort indices nonfinite centroid: start=%d count=%d offset=%d triangle_index=%d axis=%d centroid=%g",
                     start,
                     count,
                     i,
                     index,
                     axis,
                     bvh->sortScratch[i].centroid);
            runtime_triangle_bvh_set_diag(diag);
            return false;
        }
    }
    qsort(bvh->sortScratch,
          (size_t)count,
          sizeof(*bvh->sortScratch),
          runtime_triangle_bvh_compare_sort_item);
    for (int i = 0; i < count; ++i) {
        bvh->indices[start + i] = bvh->sortScratch[i].index;
    }
    return true;
}

static int runtime_triangle_bvh_longest_axis(Vec3 min, Vec3 max) {
    double dx = max.x - min.x;
    double dy = max.y - min.y;
    double dz = max.z - min.z;
    if (dx >= dy && dx >= dz) return 0;
    if (dy >= dz) return 1;
    return 2;
}

static bool runtime_triangle_bvh_range_bounds(const RuntimeTriangleMesh3D* mesh,
                                              const int* indices,
                                              const Vec3* centroids,
                                              int start,
                                              int count,
                                              Vec3* out_min,
                                              Vec3* out_max,
                                              Vec3* out_centroid_min,
                                              Vec3* out_centroid_max) {
    Vec3 bounds_min = vec3(DBL_MAX, DBL_MAX, DBL_MAX);
    Vec3 bounds_max = vec3(-DBL_MAX, -DBL_MAX, -DBL_MAX);
    Vec3 centroid_min = vec3(DBL_MAX, DBL_MAX, DBL_MAX);
    Vec3 centroid_max = vec3(-DBL_MAX, -DBL_MAX, -DBL_MAX);
    if (!mesh || !indices || !centroids || start < 0 || count <= 0 ||
        start + count > mesh->triangleCount) {
        char diag[256];
        snprintf(diag,
                 sizeof(diag),
                 "range bounds invalid input: start=%d count=%d triangle_count=%d has_indices=%s has_centroids=%s",
                 start,
                 count,
                 mesh ? mesh->triangleCount : -1,
                 indices ? "true" : "false",
                 centroids ? "true" : "false");
        runtime_triangle_bvh_set_diag(diag);
        return false;
    }

    for (int i = 0; i < count; ++i) {
        int index = indices[start + i];
        if (index < 0 || index >= mesh->triangleCount) {
            char diag[256];
            snprintf(diag,
                     sizeof(diag),
                     "range bounds invalid triangle index: start=%d count=%d offset=%d triangle_index=%d triangle_count=%d",
                     start,
                     count,
                     i,
                     index,
                     mesh->triangleCount);
            runtime_triangle_bvh_set_diag(diag);
            return false;
        }
        const RuntimeTriangle3D* triangle = &mesh->triangles[index];
        Vec3 tri_min = runtime_triangle_bvh_triangle_min(triangle);
        Vec3 tri_max = runtime_triangle_bvh_triangle_max(triangle);
        Vec3 centroid = centroids[index];
        if (!runtime_triangle_bvh_vec3_isfinite(tri_min) ||
            !runtime_triangle_bvh_vec3_isfinite(tri_max) ||
            !runtime_triangle_bvh_vec3_isfinite(centroid)) {
            char diag[256];
            snprintf(diag,
                     sizeof(diag),
                     "range bounds nonfinite triangle: start=%d count=%d offset=%d triangle_index=%d tri_min=(%g,%g,%g) tri_max=(%g,%g,%g) centroid=(%g,%g,%g)",
                     start,
                     count,
                     i,
                     index,
                     tri_min.x,
                     tri_min.y,
                     tri_min.z,
                     tri_max.x,
                     tri_max.y,
                     tri_max.z,
                     centroid.x,
                     centroid.y,
                     centroid.z);
            runtime_triangle_bvh_set_diag(diag);
            return false;
        }
        bounds_min = runtime_triangle_bvh_min3(bounds_min, tri_min);
        bounds_max = runtime_triangle_bvh_max3(bounds_max, tri_max);
        centroid_min = runtime_triangle_bvh_min3(centroid_min, centroid);
        centroid_max = runtime_triangle_bvh_max3(centroid_max, centroid);
    }

    if (out_min) *out_min = bounds_min;
    if (out_max) *out_max = bounds_max;
    if (out_centroid_min) *out_centroid_min = centroid_min;
    if (out_centroid_max) *out_centroid_max = centroid_max;
    return true;
}

static int runtime_triangle_bvh_append_node(RuntimeTriangleBVH3D* bvh) {
    RuntimeTriangleBVH3DNode* nodes = NULL;
    int new_capacity = 0;
    if (!bvh) return -1;
    if (bvh->nodeCount >= bvh->nodeCapacity) {
        new_capacity = bvh->nodeCapacity > 0 ? bvh->nodeCapacity * 2 : 16;
        nodes = (RuntimeTriangleBVH3DNode*)realloc(bvh->nodes,
                                                   sizeof(*bvh->nodes) *
                                                       (size_t)new_capacity);
        if (!nodes) {
            char diag[256];
            snprintf(diag,
                     sizeof(diag),
                     "node realloc failed: node_count=%d old_capacity=%d new_capacity=%d bytes=%llu",
                     bvh->nodeCount,
                     bvh->nodeCapacity,
                     new_capacity,
                     (unsigned long long)(sizeof(*bvh->nodes) *
                                          (size_t)new_capacity));
            runtime_triangle_bvh_set_diag(diag);
            return -1;
        }
        bvh->nodes = nodes;
        bvh->nodeCapacity = new_capacity;
    }
    memset(&bvh->nodes[bvh->nodeCount], 0, sizeof(bvh->nodes[bvh->nodeCount]));
    bvh->nodes[bvh->nodeCount].left = -1;
    bvh->nodes[bvh->nodeCount].right = -1;
    return bvh->nodeCount++;
}

static int runtime_triangle_bvh_build_node(RuntimeTriangleBVH3D* bvh,
                                           const RuntimeTriangleMesh3D* mesh,
                                           int start,
                                           int count,
                                           int depth) {
    Vec3 bounds_min = {0};
    Vec3 bounds_max = {0};
    Vec3 centroid_min = {0};
    Vec3 centroid_max = {0};
    int node_index = runtime_triangle_bvh_append_node(bvh);
    int axis = 0;
    int mid = 0;
    if (node_index < 0) return -1;
    if (depth > bvh->maxDepth) {
        bvh->maxDepth = depth;
    }

    if (!runtime_triangle_bvh_range_bounds(mesh,
                                           bvh->indices,
                                           bvh->centroids,
                                           start,
                                           count,
                                           &bounds_min,
                                           &bounds_max,
                                           &centroid_min,
                                           &centroid_max)) {
        char diag[512];
        const char* lower_diag = RuntimeTriangleMesh3D_BVHLastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "build node range bounds failed: start=%d count=%d depth=%d lower=%s",
                 start,
                 count,
                 depth,
                 lower_diag ? lower_diag : "unknown");
        runtime_triangle_bvh_set_diag(diag);
        return -1;
    }

    bvh->nodes[node_index].min = bounds_min;
    bvh->nodes[node_index].max = bounds_max;
    bvh->nodes[node_index].start = start;
    bvh->nodes[node_index].count = count;

    axis = runtime_triangle_bvh_longest_axis(centroid_min, centroid_max);
    if (count <= RUNTIME_TRIANGLE_BVH_LEAF_SIZE ||
        runtime_triangle_bvh_axis_value(centroid_max, axis) -
                runtime_triangle_bvh_axis_value(centroid_min, axis) <=
            1e-12) {
        bvh->leafCount += 1;
        if (count > bvh->maxLeafTriangleCount) {
            bvh->maxLeafTriangleCount = count;
        }
        return node_index;
    }

    if (!runtime_triangle_bvh_sort_indices(bvh, start, count, axis)) {
        char diag[512];
        const char* lower_diag = RuntimeTriangleMesh3D_BVHLastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "build node sort failed: start=%d count=%d depth=%d axis=%d lower=%s",
                 start,
                 count,
                 depth,
                 axis,
                 lower_diag ? lower_diag : "unknown");
        runtime_triangle_bvh_set_diag(diag);
        return -1;
    }
    mid = start + count / 2;
    bvh->nodes[node_index].left =
        runtime_triangle_bvh_build_node(bvh, mesh, start, mid - start, depth + 1);
    if (bvh->nodes[node_index].left < 0) {
        char diag[512];
        const char* lower_diag = RuntimeTriangleMesh3D_BVHLastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "build node left child failed: start=%d count=%d depth=%d mid=%d lower=%s",
                 start,
                 count,
                 depth,
                 mid,
                 lower_diag ? lower_diag : "unknown");
        runtime_triangle_bvh_set_diag(diag);
        return -1;
    }
    bvh->nodes[node_index].right =
        runtime_triangle_bvh_build_node(bvh, mesh, mid, start + count - mid, depth + 1);
    if (bvh->nodes[node_index].right < 0) {
        char diag[512];
        const char* lower_diag = RuntimeTriangleMesh3D_BVHLastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "build node right child failed: start=%d count=%d depth=%d mid=%d lower=%s",
                 start,
                 count,
                 depth,
                 mid,
                 lower_diag ? lower_diag : "unknown");
        runtime_triangle_bvh_set_diag(diag);
        return -1;
    }
    bvh->nodes[node_index].count = 0;
    return node_index;
}

void RuntimeTriangleMesh3D_ClearBVH(RuntimeTriangleMesh3D* mesh) {
    if (!mesh || !mesh->bvh) {
        if (mesh) mesh->bvhDirty = false;
        return;
    }
    free(mesh->bvh->nodes);
    free(mesh->bvh->indices);
    free(mesh->bvh->centroids);
    free(mesh->bvh->sortScratch);
    free(mesh->bvh);
    mesh->bvh = NULL;
    mesh->bvhDirty = false;
}

static void runtime_triangle_bvh_free_build_scratch(RuntimeTriangleBVH3D* bvh) {
    if (!bvh) return;
    free(bvh->centroids);
    bvh->centroids = NULL;
    free(bvh->sortScratch);
    bvh->sortScratch = NULL;
}

static void runtime_triangle_bvh_destroy(RuntimeTriangleBVH3D* bvh) {
    if (!bvh) return;
    free(bvh->nodes);
    free(bvh->indices);
    runtime_triangle_bvh_free_build_scratch(bvh);
    free(bvh);
}

bool RuntimeTriangleMesh3D_CopyBVH(RuntimeTriangleMesh3D* dst,
                                   const RuntimeTriangleMesh3D* src) {
    RuntimeTriangleBVH3D* copy = NULL;
    const RuntimeTriangleBVH3D* source_bvh = NULL;
    if (!dst || !src) return false;

    RuntimeTriangleMesh3D_ClearBVH(dst);
    if (!src->bvh) {
        dst->bvhDirty = src->bvhDirty;
        return true;
    }

    source_bvh = src->bvh;
    copy = (RuntimeTriangleBVH3D*)calloc(1u, sizeof(*copy));
    if (!copy) return false;
    *copy = *source_bvh;
    copy->nodes = NULL;
    copy->indices = NULL;
    copy->centroids = NULL;
    copy->sortScratch = NULL;

    if (source_bvh->nodeCapacity > 0) {
        copy->nodes = (RuntimeTriangleBVH3DNode*)malloc(sizeof(*copy->nodes) *
                                                        (size_t)source_bvh->nodeCapacity);
        if (!copy->nodes) {
            runtime_triangle_bvh_destroy(copy);
            return false;
        }
        memcpy(copy->nodes,
               source_bvh->nodes,
               sizeof(*copy->nodes) * (size_t)source_bvh->nodeCapacity);
    }
    if (source_bvh->indexCount > 0) {
        copy->indices = (int*)malloc(sizeof(*copy->indices) *
                                     (size_t)source_bvh->indexCount);
        if (!copy->indices) {
            runtime_triangle_bvh_destroy(copy);
            return false;
        }
        memcpy(copy->indices,
               source_bvh->indices,
               sizeof(*copy->indices) * (size_t)source_bvh->indexCount);
    }
    if (source_bvh->centroids && source_bvh->indexCount > 0) {
        copy->centroids = (Vec3*)malloc(sizeof(*copy->centroids) *
                                        (size_t)source_bvh->indexCount);
        if (!copy->centroids) {
            runtime_triangle_bvh_destroy(copy);
            return false;
        }
        memcpy(copy->centroids,
               source_bvh->centroids,
               sizeof(*copy->centroids) * (size_t)source_bvh->indexCount);
    }
    if (source_bvh->sortScratch && source_bvh->indexCount > 0) {
        copy->sortScratch =
            (RuntimeTriangleBVH3DSortItem*)malloc(sizeof(*copy->sortScratch) *
                                                  (size_t)source_bvh->indexCount);
        if (!copy->sortScratch) {
            runtime_triangle_bvh_destroy(copy);
            return false;
        }
        memcpy(copy->sortScratch,
               source_bvh->sortScratch,
               sizeof(*copy->sortScratch) * (size_t)source_bvh->indexCount);
    }

    dst->bvh = copy;
    dst->bvhDirty = src->bvhDirty;
    return true;
}

bool RuntimeTriangleMesh3D_BuildBVH(RuntimeTriangleMesh3D* mesh) {
    RuntimeTriangleBVH3D* bvh = NULL;
    clock_t build_start = 0;
    clock_t build_end = 0;
    runtime_triangle_bvh_set_diag("ok");
    if (!mesh) {
        runtime_triangle_bvh_set_diag("build failed: mesh missing");
        return false;
    }
    RuntimeTriangleMesh3D_ClearBVH(mesh);
    if (!mesh->triangles || mesh->triangleCount <= 0) {
        runtime_triangle_bvh_set_diag("ok");
        return true;
    }

    build_start = clock();
    bvh = (RuntimeTriangleBVH3D*)calloc(1u, sizeof(*bvh));
    if (!bvh) {
        char diag[160];
        snprintf(diag,
                 sizeof(diag),
                 "initial bvh calloc failed: triangle_count=%d bytes=%llu",
                 mesh->triangleCount,
                 (unsigned long long)sizeof(*bvh));
        runtime_triangle_bvh_set_diag(diag);
        return false;
    }
    bvh->indexCount = mesh->triangleCount;
    bvh->indices = (int*)malloc(sizeof(*bvh->indices) * (size_t)bvh->indexCount);
    bvh->centroids = (Vec3*)malloc(sizeof(*bvh->centroids) * (size_t)bvh->indexCount);
    bvh->sortScratch =
        (RuntimeTriangleBVH3DSortItem*)malloc(sizeof(*bvh->sortScratch) *
                                              (size_t)bvh->indexCount);
    if (!bvh->indices || !bvh->centroids || !bvh->sortScratch) {
        char diag[384];
        snprintf(diag,
                 sizeof(diag),
                 "build scratch allocation failed: triangle_count=%d indices=%s centroids=%s sort_scratch=%s index_bytes=%llu centroid_bytes=%llu sort_scratch_bytes=%llu",
                 bvh->indexCount,
                 bvh->indices ? "ok" : "failed",
                 bvh->centroids ? "ok" : "failed",
                 bvh->sortScratch ? "ok" : "failed",
                 (unsigned long long)(sizeof(*bvh->indices) *
                                      (size_t)bvh->indexCount),
                 (unsigned long long)(sizeof(*bvh->centroids) *
                                      (size_t)bvh->indexCount),
                 (unsigned long long)(sizeof(*bvh->sortScratch) *
                                      (size_t)bvh->indexCount));
        runtime_triangle_bvh_set_diag(diag);
        runtime_triangle_bvh_destroy(bvh);
        return false;
    }
    for (int i = 0; i < bvh->indexCount; ++i) {
        bvh->indices[i] = i;
        bvh->centroids[i] = runtime_triangle_bvh_triangle_centroid(&mesh->triangles[i]);
        if (!runtime_triangle_bvh_vec3_isfinite(bvh->centroids[i])) {
            char diag[256];
            snprintf(diag,
                     sizeof(diag),
                     "centroid build nonfinite triangle: triangle_index=%d centroid=(%g,%g,%g)",
                     i,
                     bvh->centroids[i].x,
                     bvh->centroids[i].y,
                     bvh->centroids[i].z);
            runtime_triangle_bvh_set_diag(diag);
            runtime_triangle_bvh_destroy(bvh);
            return false;
        }
    }
    if (runtime_triangle_bvh_build_node(bvh, mesh, 0, bvh->indexCount, 1) < 0) {
        char diag[512];
        const char* lower_diag = RuntimeTriangleMesh3D_BVHLastDiagnostics();
        snprintf(diag,
                 sizeof(diag),
                 "build failed: triangle_count=%d lower=%s",
                 bvh->indexCount,
                 lower_diag ? lower_diag : "unknown");
        runtime_triangle_bvh_set_diag(diag);
        runtime_triangle_bvh_destroy(bvh);
        return false;
    }
    build_end = clock();
    bvh->buildCpuMs = runtime_triangle_bvh_clock_ms(build_start, build_end);
    bvh->nodeBytes = (uint64_t)sizeof(*bvh->nodes) * (uint64_t)bvh->nodeCapacity;
    bvh->indexBytes = (uint64_t)sizeof(*bvh->indices) * (uint64_t)bvh->indexCount;
    bvh->centroidBytes =
        (uint64_t)sizeof(*bvh->centroids) * (uint64_t)bvh->indexCount;
    bvh->sortScratchBytes =
        (uint64_t)sizeof(*bvh->sortScratch) * (uint64_t)bvh->indexCount;
    bvh->buildScratchBytes = bvh->centroidBytes + bvh->sortScratchBytes;
    bvh->totalBytes = bvh->nodeBytes + bvh->indexBytes + (uint64_t)sizeof(*bvh);
    runtime_triangle_bvh_free_build_scratch(bvh);
    mesh->bvh = bvh;
    mesh->bvhDirty = false;
    runtime_triangle_bvh_set_diag("ok");
    return true;
}

bool RuntimeTriangleMesh3D_HasReadyBVH(const RuntimeTriangleMesh3D* mesh) {
    return mesh && mesh->bvh && !mesh->bvhDirty && mesh->bvh->nodeCount > 0;
}

int RuntimeTriangleMesh3D_BVHNodeCount(const RuntimeTriangleMesh3D* mesh) {
    return RuntimeTriangleMesh3D_HasReadyBVH(mesh) ? mesh->bvh->nodeCount : 0;
}

int RuntimeTriangleMesh3D_BVHLeafCount(const RuntimeTriangleMesh3D* mesh) {
    return RuntimeTriangleMesh3D_HasReadyBVH(mesh) ? mesh->bvh->leafCount : 0;
}

bool RuntimeTriangleMesh3D_BVHBuildStats(const RuntimeTriangleMesh3D* mesh,
                                         RuntimeTriangleBVH3DBuildStats* out_stats) {
    if (!out_stats) return false;
    memset(out_stats, 0, sizeof(*out_stats));
    if (!RuntimeTriangleMesh3D_HasReadyBVH(mesh)) {
        if (mesh) out_stats->triangleCount = mesh->triangleCount;
        return false;
    }
    out_stats->ready = true;
    out_stats->triangleCount = mesh->triangleCount;
    out_stats->nodeCount = mesh->bvh->nodeCount;
    out_stats->leafCount = mesh->bvh->leafCount;
    out_stats->maxDepth = mesh->bvh->maxDepth;
    out_stats->leafSize = RUNTIME_TRIANGLE_BVH_LEAF_SIZE;
    out_stats->maxLeafTriangleCount = mesh->bvh->maxLeafTriangleCount;
    out_stats->buildCpuMs = mesh->bvh->buildCpuMs;
    out_stats->nodeBytes = mesh->bvh->nodeBytes;
    out_stats->indexBytes = mesh->bvh->indexBytes;
    out_stats->centroidBytes = mesh->bvh->centroidBytes;
    out_stats->sortScratchBytes = mesh->bvh->sortScratchBytes;
    out_stats->buildScratchBytes = mesh->bvh->buildScratchBytes;
    out_stats->totalBytes = mesh->bvh->totalBytes;
    return true;
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
