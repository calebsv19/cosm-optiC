#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

static int g_failures = 0;

static void assert_true(const char* name, bool condition) {
    if (!condition) {
        printf("FAIL %-48s condition=false\n", name);
        g_failures += 1;
    }
}

static void assert_near(const char* name, double actual, double expected, double epsilon) {
    if (fabs(actual - expected) > epsilon) {
        printf("FAIL %-48s actual=%f expected=%f\n", name, actual, expected);
        g_failures += 1;
    }
}

static RuntimeTriangle3D make_triangle(Vec3 p0,
                                       Vec3 p1,
                                       Vec3 p2,
                                       int primitive_index,
                                       int scene_object_index,
                                       int local_triangle_index) {
    RuntimeTriangle3D triangle;
    memset(&triangle, 0, sizeof(triangle));
    triangle.p0 = p0;
    triangle.p1 = p1;
    triangle.p2 = p2;
    triangle.normal = vec3_normalize(vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0)));
    triangle.primitiveIndex = primitive_index;
    triangle.sceneObjectIndex = scene_object_index;
    triangle.localTriangleIndex = local_triangle_index;
    return triangle;
}

static bool append_test_square(RuntimeScene3D* scene,
                               int square_index,
                               double center_x,
                               double center_y,
                               double z,
                               double half_size) {
    int base_triangle = square_index * 2;
    RuntimeTriangle3D* triangles = scene->triangleMesh.triangles;
    if (!scene || !triangles || scene->triangleMesh.triangleCount + 2 >
                                   scene->triangleMesh.triangleCapacity) {
        return false;
    }
    triangles[scene->triangleMesh.triangleCount++] =
        make_triangle(vec3(center_x - half_size, center_y, z - half_size),
                      vec3(center_x + half_size, center_y, z - half_size),
                      vec3(center_x + half_size, center_y, z + half_size),
                      square_index,
                      square_index + 10,
                      base_triangle);
    triangles[scene->triangleMesh.triangleCount++] =
        make_triangle(vec3(center_x - half_size, center_y, z - half_size),
                      vec3(center_x + half_size, center_y, z + half_size),
                      vec3(center_x - half_size, center_y, z + half_size),
                      square_index,
                      square_index + 10,
                      base_triangle + 1);
    scene->triangleMesh.bvhDirty = true;
    return true;
}

static bool build_test_scene_with_half_size(RuntimeScene3D* scene, double half_size) {
    const int square_count = 8;
    RuntimeScene3D_Init(scene);
    scene->primitiveCapacity = square_count;
    scene->primitiveCount = square_count;
    scene->primitives =
        (RuntimePrimitive3D*)calloc((size_t)square_count, sizeof(*scene->primitives));
    scene->triangleMesh.triangleCapacity = square_count * 2;
    scene->triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc((size_t)scene->triangleMesh.triangleCapacity,
                                   sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) return false;

    for (int i = 0; i < square_count; ++i) {
        RuntimePrimitive3D* primitive = &scene->primitives[i];
        primitive->kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        primitive->source.kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
        primitive->source.sceneObjectIndex = i + 10;
        snprintf(primitive->source.objectId, sizeof(primitive->source.objectId), "panel_%d", i);
        if (!append_test_square(scene,
                                i,
                                -3.5 + (double)i,
                                5.0 + (double)(i % 3),
                                1.0,
                                half_size)) {
            return false;
        }
    }
    return RuntimeTriangleMesh3D_BuildBVH(&scene->triangleMesh);
}

static bool build_test_scene(RuntimeScene3D* scene) {
    return build_test_scene_with_half_size(scene, 0.35);
}

static void assert_hit_matches(const char* name, const HitInfo3D* flat, const HitInfo3D* bvh) {
    char label[96];
    snprintf(label, sizeof(label), "%s_triangle", name);
    assert_true(label, flat->triangleIndex == bvh->triangleIndex);
    snprintf(label, sizeof(label), "%s_primitive", name);
    assert_true(label, flat->primitiveIndex == bvh->primitiveIndex);
    snprintf(label, sizeof(label), "%s_scene_object", name);
    assert_true(label, flat->sceneObjectIndex == bvh->sceneObjectIndex);
    snprintf(label, sizeof(label), "%s_local_triangle", name);
    assert_true(label, flat->localTriangleIndex == bvh->localTriangleIndex);
    snprintf(label, sizeof(label), "%s_t", name);
    assert_near(label, bvh->t, flat->t, 1e-9);
    snprintf(label, sizeof(label), "%s_object_id", name);
    assert_true(label, strcmp(flat->source.objectId, bvh->source.objectId) == 0);
}

static void test_bvh_matches_flat_trace(void) {
    RuntimeScene3D scene;
    Ray3D rays[4];
    RuntimeTriangleBVH3DTraceStats stats = {0};
    RuntimeTriangleBVH3DBuildStats build_stats = {0};
    RuntimeScene3D_Init(&scene);
    assert_true("bvh_build_test_scene", build_test_scene(&scene));
    assert_true("bvh_node_count", RuntimeTriangleMesh3D_BVHNodeCount(&scene.triangleMesh) > 1);
    assert_true("bvh_leaf_count", RuntimeTriangleMesh3D_BVHLeafCount(&scene.triangleMesh) > 1);
    assert_true("bvh_build_stats",
                RuntimeTriangleMesh3D_BVHBuildStats(&scene.triangleMesh, &build_stats));
    assert_true("bvh_build_stats_ready", build_stats.ready);
    assert_true("bvh_build_stats_node_count", build_stats.nodeCount > 1);
    assert_true("bvh_build_stats_leaf_count", build_stats.leafCount > 1);
    assert_true("bvh_build_stats_depth", build_stats.maxDepth > 1);
    assert_true("bvh_build_stats_memory", build_stats.totalBytes > 0u);
    assert_true("bvh_build_stats_centroid_scratch", build_stats.centroidBytes > 0u);
    assert_true("bvh_build_stats_triangle_bounds_min_scratch",
                build_stats.triangleBoundsMinBytes > 0u);
    assert_true("bvh_build_stats_triangle_bounds_max_scratch",
                build_stats.triangleBoundsMaxBytes > 0u);
    assert_true("bvh_build_stats_sort_scratch_removed", build_stats.sortScratchBytes == 0u);
    assert_true("bvh_build_stats_scratch_total",
                build_stats.buildScratchBytes ==
                    build_stats.centroidBytes + build_stats.triangleBoundsMinBytes +
                        build_stats.triangleBoundsMaxBytes + build_stats.sortScratchBytes);
    assert_true("bvh_build_stats_allocation_cpu", build_stats.allocationCpuMs >= 0.0);
    assert_true("bvh_build_stats_centroid_cpu", build_stats.centroidBuildCpuMs >= 0.0);
    assert_true("bvh_build_stats_tree_cpu", build_stats.treeBuildCpuMs >= 0.0);
    assert_true("bvh_build_stats_range_bounds_cpu", build_stats.rangeBoundsCpuMs >= 0.0);
    assert_true("bvh_build_stats_sort_cpu", build_stats.sortCpuMs >= 0.0);
    assert_true("bvh_build_stats_node_append_cpu", build_stats.nodeAppendCpuMs >= 0.0);
    assert_true("bvh_build_stats_unaccounted_cpu",
                build_stats.buildUnaccountedCpuMs >= 0.0);
    assert_true("bvh_build_stats_range_bounds_calls",
                build_stats.rangeBoundsCalls == (uint64_t)build_stats.nodeCount);
    assert_true("bvh_build_stats_sort_calls",
                build_stats.sortCalls == (uint64_t)(build_stats.leafCount - 1));
    assert_true("bvh_build_stats_node_append_calls",
                build_stats.nodeAppendCalls == (uint64_t)build_stats.nodeCount);
    assert_true("bvh_build_stats_max_range_bounds",
                build_stats.maxRangeBoundsCount == build_stats.triangleCount);
    assert_true("bvh_build_stats_max_sort", build_stats.maxSortCount == build_stats.triangleCount);

    rays[0] = RuntimeRay3D_Make(vec3(-3.5, 0.0, 1.0), vec3(0.0, 1.0, 0.0));
    rays[1] = RuntimeRay3D_Make(vec3(-0.5, 0.0, 1.0), vec3(0.0, 1.0, 0.0));
    rays[2] = RuntimeRay3D_Make(vec3(2.5, 0.0, 1.0), vec3(0.0, 1.0, 0.0));
    rays[3] = RuntimeRay3D_Make(vec3(8.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0));

    RuntimeTriangleBVH3D_ResetTraceStats();
    for (int i = 0; i < 4; ++i) {
        HitInfo3D flat = {0};
        HitInfo3D bvh = {0};
        bool flat_hit = false;
        bool bvh_hit = false;
        char label[96];

        RuntimeTriangleMesh3D_ClearBVH(&scene.triangleMesh);
        flat_hit = RuntimeRay3D_TraceSceneFirstHit(&scene, &rays[i], 0.001, 100.0, &flat);
        assert_true("bvh_rebuild", RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh));
        bvh_hit = RuntimeRay3D_TraceSceneFirstHit(&scene, &rays[i], 0.001, 100.0, &bvh);

        snprintf(label, sizeof(label), "bvh_hit_state_%d", i);
        assert_true(label, flat_hit == bvh_hit);
        if (flat_hit && bvh_hit) {
            snprintf(label, sizeof(label), "bvh_match_%d", i);
            assert_hit_matches(label, &flat, &bvh);
        }
    }
    RuntimeTriangleBVH3D_SnapshotTraceStats(&stats);
    assert_true("bvh_trace_stats_calls", stats.traceCalls > 0u);
    assert_true("bvh_trace_stats_node_visits", stats.nodeVisits > 0u);
    assert_true("bvh_trace_stats_aabb_tests", stats.aabbTests >= stats.nodeVisits);
    assert_true("bvh_trace_stats_triangle_tests", stats.triangleTests > 0u);
    assert_true("bvh_trace_stats_no_overflow", stats.traceOverflows == 0u);

    RuntimeScene3D_Free(&scene);
}

static void test_bvh_copy_preserves_trace_results(void) {
    RuntimeScene3D scene;
    RuntimeScene3D copy;
    RuntimeTriangleBVH3DBuildStats source_stats = {0};
    RuntimeTriangleBVH3DBuildStats copy_stats = {0};
    Ray3D ray;
    HitInfo3D source_hit = {0};
    HitInfo3D copy_hit = {0};
    bool source_hit_state = false;
    bool copy_hit_state = false;

    RuntimeScene3D_Init(&scene);
    RuntimeScene3D_Init(&copy);
    assert_true("bvh_copy_build_scene", build_test_scene(&scene));
    assert_true("bvh_copy_source_stats",
                RuntimeTriangleMesh3D_BVHBuildStats(&scene.triangleMesh, &source_stats));
    assert_true("bvh_copy_scene_geometry",
                RuntimeScene3D_CopyGeometryFrom(&copy, &scene));
    assert_true("bvh_copy_ready", RuntimeTriangleMesh3D_HasReadyBVH(&copy.triangleMesh));
    assert_true("bvh_copy_stats",
                RuntimeTriangleMesh3D_BVHBuildStats(&copy.triangleMesh, &copy_stats));
    assert_true("bvh_copy_triangle_count",
                copy.triangleMesh.triangleCount == scene.triangleMesh.triangleCount);
    assert_true("bvh_copy_node_count", copy_stats.nodeCount == source_stats.nodeCount);
    assert_true("bvh_copy_leaf_count", copy_stats.leafCount == source_stats.leafCount);
    assert_true("bvh_copy_total_bytes", copy_stats.totalBytes == source_stats.totalBytes);

    ray = RuntimeRay3D_Make(vec3(-0.5, 0.0, 1.0), vec3(0.0, 1.0, 0.0));
    source_hit_state =
        RuntimeTriangleBVH3D_TraceFirstHit(&scene.triangleMesh,
                                           &ray,
                                           0.001,
                                           100.0,
                                           &source_hit);
    copy_hit_state =
        RuntimeTriangleBVH3D_TraceFirstHit(&copy.triangleMesh,
                                           &ray,
                                           0.001,
                                           100.0,
                                           &copy_hit);
    assert_true("bvh_copy_hit_state", source_hit_state == copy_hit_state);
    if (source_hit_state && copy_hit_state) {
        assert_hit_matches("bvh_copy_match", &source_hit, &copy_hit);
    }

    RuntimeScene3D_Free(&copy);
    RuntimeScene3D_Free(&scene);
}

static void test_bvh_overflow_falls_back_to_flat_trace(void) {
    RuntimeScene3D scene;
    Ray3D ray;
    HitInfo3D flat = {0};
    HitInfo3D fallback = {0};
    RuntimeTriangleBVH3DTraceStats stats = {0};
    bool flat_hit = false;
    bool fallback_hit = false;

    RuntimeScene3D_Init(&scene);
    assert_true("bvh_overflow_build_scene", build_test_scene_with_half_size(&scene, 10.0));
    ray = RuntimeRay3D_Make(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0));

    RuntimeTriangleMesh3D_ClearBVH(&scene.triangleMesh);
    flat_hit = RuntimeRay3D_TraceSceneFirstHit(&scene, &ray, 0.001, 100.0, &flat);
    assert_true("bvh_overflow_rebuild", RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh));

    RuntimeTriangleBVH3D_SetTraversalStackLimitForTests(1);
    RuntimeTriangleBVH3D_ResetTraceStats();
    fallback_hit = RuntimeRay3D_TraceSceneFirstHit(&scene, &ray, 0.001, 100.0, &fallback);
    RuntimeTriangleBVH3D_SetTraversalStackLimitForTests(0);

    assert_true("bvh_overflow_hit_state", flat_hit == fallback_hit);
    if (flat_hit && fallback_hit) {
        assert_hit_matches("bvh_overflow_match", &flat, &fallback);
    }
    RuntimeTriangleBVH3D_SnapshotTraceStats(&stats);
    assert_true("bvh_overflow_trace_count", stats.traceCalls > 0u);
    assert_true("bvh_overflow_recorded", stats.traceOverflows > 0u);
    assert_true("bvh_overflow_flat_fallback", stats.flatFallbackCalls > 0u);
    assert_true("bvh_overflow_fallback_recorded", stats.overflowFallbackCalls > 0u);

    RuntimeTriangleBVH3D_ResetTraceStats();
    RuntimeScene3D_Free(&scene);
}

static void test_bvh_reports_nonfinite_centroid(void) {
    RuntimeScene3D scene;
    const char* diagnostics = NULL;

    RuntimeScene3D_Init(&scene);
    scene.triangleMesh.triangleCapacity = 1;
    scene.triangleMesh.triangles =
        (RuntimeTriangle3D*)calloc(1u, sizeof(*scene.triangleMesh.triangles));
    assert_true("bvh_nonfinite_alloc", scene.triangleMesh.triangles != NULL);
    if (!scene.triangleMesh.triangles) {
        RuntimeScene3D_Free(&scene);
        return;
    }

    scene.triangleMesh.triangleCount = 1;
    scene.triangleMesh.triangles[0] = make_triangle(vec3(NAN, 0.0, 0.0),
                                                    vec3(1.0, 0.0, 0.0),
                                                    vec3(0.0, 1.0, 0.0),
                                                    0,
                                                    0,
                                                    0);
    assert_true("bvh_nonfinite_build_fails",
                !RuntimeTriangleMesh3D_BuildBVH(&scene.triangleMesh));
    diagnostics = RuntimeTriangleMesh3D_BVHLastDiagnostics();
    assert_true("bvh_nonfinite_diag_present",
                diagnostics && strstr(diagnostics, "nonfinite") != NULL);
    assert_true("bvh_nonfinite_diag_index",
                diagnostics && strstr(diagnostics, "triangle_index=0") != NULL);

    RuntimeScene3D_Free(&scene);
}

int main(void) {
    test_bvh_matches_flat_trace();
    test_bvh_copy_preserves_trace_results();
    test_bvh_overflow_falls_back_to_flat_trace();
    test_bvh_reports_nonfinite_centroid();
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
