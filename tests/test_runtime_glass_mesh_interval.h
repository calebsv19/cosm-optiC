#pragma once
#include "render/runtime_scene_accel_3d.h"
/* A conservative local BLAS interval may return a candidate just before the
 * world lower bound. Rejecting that candidate must not hide a solid's exit. */
static void test_runtime_glass_mesh_interval(void) {
    RayTracingRuntimeMeshAssetSet *set = calloc(1, sizeof(*set));
    assert_true("glass_interval_asset_set", set != NULL);
    if (!set)
        return;
    RayTracingRuntimeMeshAssetInstance instance;
    CoreMeshAssetRuntimeVertex vertices[6] = {0};
    CoreMeshAssetRuntimeTriangle triangles[2] = {0};
    test_runtime_scene_3d_builder_init_asymmetric_mesh_set(set, &instance, vertices, triangles);
    set->assets[0].document.vertex_count = 6;
    set->assets[0].document.triangle_count = 2;
    set->assets[0].document.contract.local_bounds.min.z = -.5;
    for (int i = 0; i < 3; ++i) {
        vertices[i + 3] = vertices[i];
        vertices[i + 3].position.z = -.5;
    }
    triangles[1].a = 3;
    triangles[1].b = 5;
    triangles[1].c = 4;
    set->instances[0].rotation_z = .23;
    set->instances[0].scale_x = 1.7;
    set->instances[0].scale_y = .8;
    set->instances[0].scale_z = 1.3;
    RuntimeScene3D scene;
    RuntimeScene3D_Init(&scene);
    assert_true("glass_interval_append", RuntimeScene3DBuilder_AppendMeshAssetSet(&scene, set));
    assert_true("glass_interval_prepare",
                RuntimeSceneAcceleration3D_RebuildPreparedFromSceneAndMeshAssets(&scene, set));
    const RuntimeTriangle3D *entry = &scene.triangleMesh.triangles[0];
    Vec3 center = vec3_scale(vec3_add(vec3_add(entry->p0, entry->p1), entry->p2), 1. / 3);
    Ray3D ray = RuntimeRay3D_Make(vec3_add(center, vec3(0, 0, 1)), vec3(0, 0, -1));
    HitInfo3D first = {0}, second = {0};
    RuntimeSceneAcceleration3D_ResetTraceStats();
    assert_true("glass_interval_entry",
                RuntimeSceneAcceleration3D_TraceFirstHit(&scene, &ray, 0, 10, &first) ==
                    RUNTIME_SCENE_ACCEL_3D_TRACE_HIT);
    assert_true("glass_interval_exit", RuntimeSceneAcceleration3D_TraceFirstHit(
                                           &scene, &ray, nextafter(first.t, INFINITY), 10,
                                           &second) == RUNTIME_SCENE_ACCEL_3D_TRACE_HIT);
    assert_true("glass_interval_distinct_surface",
                first.localTriangleIndex != second.localTriangleIndex);
    assert_close("glass_interval_thickness", second.t - first.t, .65, 1e-10);
    RuntimeSceneAcceleration3DTraceStats stats={0};
    RuntimeSceneAcceleration3D_SnapshotTraceStats(&stats);
    assert_true("glass_interval_uses_blas",stats.blasTraceCalls>=2);
    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
    free(set); /* arrays are borrowed */
}
