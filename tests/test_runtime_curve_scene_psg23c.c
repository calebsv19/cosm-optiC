#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render/runtime_curve_blas_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_ray_3d.h"
#include "render/runtime_scene_accel_3d.h"
#include "render/runtime_scene_curve_3d.h"

static int g_failures = 0;

static void check_true(const char* name, bool condition) {
    if (!condition) {
        fprintf(stderr, "FAIL %s\n", name);
        g_failures += 1;
    }
}

static void check_near(const char* name,
                       double actual,
                       double expected,
                       double epsilon) {
    if (!isfinite(actual) || fabs(actual - expected) > epsilon) {
        fprintf(stderr,
                "FAIL %s actual=%.12f expected=%.12f epsilon=%.12f\n",
                name,
                actual,
                expected,
                epsilon);
        g_failures += 1;
    }
}

static bool build_curve_asset(RuntimeCurveAsset3D* asset) {
    const Vec3 points[3] = {
        {0.0, -1.0, 0.0},
        {0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}};
    const double radii[3] = {0.20, 0.15, 0.10};
    RuntimeCurveAsset3D_Init(asset);
    return RuntimeCurveAsset3D_BuildFromPolylineStrands(
               asset, points, radii, 1u, 3u) &&
           RuntimeCurveAsset3D_BuildBLAS(asset);
}

static bool add_curve(RuntimeScene3D* scene,
                      const RuntimeCurveAsset3D* asset,
                      Vec3 position,
                      Vec3 rotation,
                      double scale,
                      int scene_object_index) {
    const RuntimeCurveSceneInstance3DDescriptor descriptor = {
        .assetId = "psg23c_curve_asset",
        .objectId = "psg23c_curve_object",
        .sceneObjectIndex = scene_object_index,
        .position = position,
        .rotation = rotation,
        .uniformScale = scale};
    return RuntimeScene3D_AddCurveInstance(scene, asset, &descriptor);
}

static bool add_triangle(RuntimeScene3D* scene) {
    RuntimeTriangle3D* triangle = NULL;
    scene->primitiveCount = 1;
    scene->primitiveCapacity = 1;
    scene->primitives = calloc(1u, sizeof(*scene->primitives));
    scene->triangleMesh.triangleCount = 1;
    scene->triangleMesh.triangleCapacity = 1;
    scene->triangleMesh.triangles =
        calloc(1u, sizeof(*scene->triangleMesh.triangles));
    if (!scene->primitives || !scene->triangleMesh.triangles) return false;
    scene->primitives[0].kind = RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.kind =
        RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH;
    scene->primitives[0].source.sceneObjectIndex = 5;
    snprintf(scene->primitives[0].source.objectId,
             sizeof(scene->primitives[0].source.objectId),
             "%s",
             "psg23c_triangle_object");
    triangle = &scene->triangleMesh.triangles[0];
    triangle->p0 = vec3(3.0, -4.0, -4.0);
    triangle->p1 = vec3(3.0, 4.0, -4.0);
    triangle->p2 = vec3(3.0, 0.0, 4.0);
    triangle->normal = vec3(-1.0, 0.0, 0.0);
    triangle->twoSided = true;
    triangle->primitiveIndex = 0;
    triangle->sceneObjectIndex = 5;
    triangle->localTriangleIndex = 0;
    return true;
}

static void test_curve_only_scene_and_material_dispatch(void) {
    RuntimeCurveAsset3D source_asset;
    RuntimeScene3D scene;
    RuntimeScene3D copy;
    RuntimeSceneAcceleration3DDiagnostics diagnostics =
        RuntimeSceneAcceleration3DDiagnostics_Disabled();
    RuntimeSceneAcceleration3DTraceStats stats = {0};
    RuntimeMaterialPayload3D material;
    Ray3D ray;
    HitInfo3D hit;
    Vec3 bounds_min;
    Vec3 bounds_max;
    const uint64_t signature_before = 0u;
    uint64_t signature = signature_before;

    RuntimeScene3D_Init(&scene);
    RuntimeScene3D_Init(&copy);
    check_true("curve_asset_build", build_curve_asset(&source_asset));
    check_true("curve_scene_add",
               add_curve(&scene,
                         &source_asset,
                         vec3(1.0, 0.0, 0.0),
                         vec3(0.0, 0.0, 0.0),
                         2.0,
                         3));
    RuntimeCurveAsset3D_Free(&source_asset);
    check_true("curve_scene_owns_deep_copy",
               scene.curveInstanceCount == 1 &&
                   RuntimeCurveAsset3D_HasReadyBLAS(
                       &scene.curveInstances[0].asset));
    check_true("curve_scene_bounds",
               RuntimeSceneCurve3D_InstanceWorldBounds(
                   &scene.curveInstances[0], &bounds_min, &bounds_max));
    check_true("curve_scene_bounds_x",
               bounds_min.x <= 0.60 && bounds_max.x >= 1.40);
    signature = RuntimeSceneCurve3D_GeometrySignature(&scene);
    check_true("curve_scene_signature_nonzero", signature != 0u);
    check_true("curve_scene_signature_repeat",
               signature == RuntimeSceneCurve3D_GeometrySignature(&scene));

    check_true("curve_scene_copy", RuntimeScene3D_CopyGeometryFrom(&copy, &scene));
    RuntimeScene3D_Free(&scene);
    check_true("curve_scene_copy_independent",
               copy.curveInstanceCount == 1 &&
                   RuntimeCurveAsset3D_HasReadyBLAS(
                       &copy.curveInstances[0].asset));

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeSceneAcceleration3D_ResetTraceStats();
    check_true("curve_only_tlas_build",
               RuntimeSceneAcceleration3D_RebuildTLASFromScene(&copy));
    ray = RuntimeRay3D_Make(
        vec3(-3.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0));
    check_true("curve_only_tlas_hit",
               RuntimeSceneAcceleration3D_TraceFirstHit(
                   &copy, &ray, 0.001, 100.0, &hit) ==
                   RUNTIME_SCENE_ACCEL_3D_TRACE_HIT);
    check_near("curve_only_hit_t", hit.t, 3.70, 1.0e-9);
    check_true("curve_only_hit_kind",
               hit.source.kind == RUNTIME_PRIMITIVE_3D_KIND_CURVE);
    check_true("curve_only_hit_object",
               strcmp(hit.source.objectId, "psg23c_curve_object") == 0);
    check_true("curve_only_hit_scene_object", hit.sceneObjectIndex == 3);
    check_true("curve_only_hit_instance", hit.curveSceneInstanceIndex == 0);
    check_true("curve_only_hit_payload",
               hit.hasCurveTangent &&
                   hit.curvePrimitiveIndex >= 0 &&
                   hit.curveStrandIndex == 0 &&
                   hit.curveSegmentIndex >= 0);
    check_near("curve_only_tangent_unit",
               vec3_length(hit.curveTangent),
               1.0,
               1.0e-12);
    check_near("curve_only_scaled_radius", hit.curveRadius, 0.30, 1.0e-9);
    check_true("curve_material_dispatch",
               RuntimeSceneCurve3D_ResolveMaterial(&hit, &material));
    check_true("curve_material_scene_identity",
               material.valid &&
                   material.sceneObjectIndex == 3 &&
                   material.materialId == 1003);

    RuntimeSceneAcceleration3D_AppendTLASDiagnostics(&diagnostics);
    RuntimeSceneAcceleration3D_SnapshotTraceStats(&stats);
    check_true("curve_tlas_instance_diagnostics",
               diagnostics.tlasInstanceCount == 1u &&
                   diagnostics.tlasCurveInstanceCount == 1u);
    check_true("curve_blas_trace_stats",
               stats.curveBlasTraceCalls == 1u &&
                   stats.curveBlasTraceHits == 1u);

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&copy);
}

static void test_mixed_curve_triangle_tlas_and_flat_parity(void) {
    RuntimeCurveAsset3D asset;
    RuntimeScene3D scene;
    RuntimeRay3DTraceContext flat_context;
    RuntimeRay3DTraceContext tlas_context;
    RuntimeRay3DTraceContext parity_context;
    RuntimeRay3DRouteStats parity_stats;
    Ray3D center_ray;
    Ray3D triangle_ray;
    HitInfo3D flat_hit;
    HitInfo3D tlas_hit;
    HitInfo3D triangle_hit;
    HitInfo3D parity_hit;

    RuntimeScene3D_Init(&scene);
    check_true("mixed_curve_asset_build", build_curve_asset(&asset));
    check_true("mixed_triangle_add", add_triangle(&scene));
    check_true("mixed_curve_add",
               add_curve(&scene,
                         &asset,
                         vec3(1.0, 0.0, 0.0),
                         vec3(0.0, 0.0, 0.0),
                         1.0,
                         2));
    RuntimeCurveAsset3D_Free(&asset);
    check_true("mixed_tlas_build",
               RuntimeSceneAcceleration3D_RebuildTLASFromScene(&scene));

    RuntimeRay3DTraceContext_Init(&flat_context);
    RuntimeRay3DTraceContext_SetTraceRoute(
        &flat_context, RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
    RuntimeRay3DTraceContext_Init(&tlas_context);
    RuntimeRay3DTraceContext_SetTraceRoute(
        &tlas_context, RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS);
    RuntimeRay3DTraceContext_SetSceneAccelerationTraceFirstHit(
        &tlas_context,
        (RuntimeRay3DSceneAccelerationTraceFirstHitFn)
            RuntimeSceneAcceleration3D_TraceFirstHit);
    RuntimeRay3DTraceContext_Init(&parity_context);
    RuntimeRay3DTraceContext_SetTraceRoute(
        &parity_context, RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY);
    RuntimeRay3DTraceContext_SetSceneAccelerationTraceFirstHit(
        &parity_context,
        (RuntimeRay3DSceneAccelerationTraceFirstHitFn)
            RuntimeSceneAcceleration3D_TraceFirstHit);

    center_ray = RuntimeRay3D_Make(
        vec3(-3.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0));
    check_true("mixed_flat_curve_hit",
               RuntimeRay3D_TraceSceneFirstHitWithContext(
                   &flat_context,
                   &scene,
                   &center_ray,
                   0.001,
                   100.0,
                   &flat_hit));
    check_true("mixed_tlas_curve_hit",
               RuntimeRay3D_TraceSceneFirstHitWithContext(
                   &tlas_context,
                   &scene,
                   &center_ray,
                   0.001,
                   100.0,
                   &tlas_hit));
    check_true("mixed_curve_wins",
               flat_hit.source.kind == RUNTIME_PRIMITIVE_3D_KIND_CURVE &&
                   tlas_hit.source.kind == RUNTIME_PRIMITIVE_3D_KIND_CURVE);
    check_near("mixed_flat_tlas_t", flat_hit.t, tlas_hit.t, 1.0e-12);
    check_true("mixed_flat_tlas_identity",
               flat_hit.sceneObjectIndex == tlas_hit.sceneObjectIndex &&
                   flat_hit.curvePrimitiveIndex ==
                       tlas_hit.curvePrimitiveIndex);
    check_true("mixed_parity_curve_hit",
               RuntimeRay3D_TraceSceneFirstHitWithContext(
                   &parity_context,
                   &scene,
                   &center_ray,
                   0.001,
                   100.0,
                   &parity_hit));
    RuntimeRay3DTraceContext_SnapshotRouteStats(
        &parity_context, &parity_stats);
    check_true("mixed_parity_curve_identity",
               parity_hit.curveSceneInstanceIndex ==
                       tlas_hit.curveSceneInstanceIndex &&
                   parity_hit.curvePrimitiveIndex ==
                       tlas_hit.curvePrimitiveIndex);
    check_true("mixed_parity_no_mismatch",
               parity_stats.parityCheckedRays == 1u &&
                   parity_stats.parityMismatches == 0u);

    triangle_ray = RuntimeRay3D_Make(
        vec3(-3.0, 2.0, -1.0), vec3(1.0, 0.0, 0.0));
    check_true("mixed_triangle_fallback_hit",
               RuntimeRay3D_TraceSceneFirstHitWithContext(
                   &tlas_context,
                   &scene,
                   &triangle_ray,
                   0.001,
                   100.0,
                   &triangle_hit));
    check_true("mixed_triangle_identity",
               triangle_hit.source.kind ==
                       RUNTIME_PRIMITIVE_3D_KIND_TRIANGLE_MESH &&
                   triangle_hit.sceneObjectIndex == 5 &&
                   !triangle_hit.hasCurveTangent);
    check_near("mixed_triangle_t", triangle_hit.t, 6.0, 1.0e-12);

    RuntimeSceneAcceleration3D_ResetTLASForTests();
    RuntimeScene3D_Free(&scene);
}

static void test_curve_instance_validation_and_rotation(void) {
    RuntimeCurveAsset3D asset;
    RuntimeScene3D scene;
    RuntimeCurveSceneInstance3DDescriptor invalid = {
        .assetId = "invalid",
        .objectId = "invalid",
        .sceneObjectIndex = 1,
        .position = {0.0, 0.0, 0.0},
        .rotation = {0.0, 0.0, 0.0},
        .uniformScale = 0.0};
    Ray3D ray;
    HitInfo3D hit;

    RuntimeScene3D_Init(&scene);
    check_true("rotation_curve_asset_build", build_curve_asset(&asset));
    check_true("curve_zero_scale_rejected",
               !RuntimeScene3D_AddCurveInstance(&scene, &asset, &invalid));
    check_true("rotated_curve_add",
               add_curve(&scene,
                         &asset,
                         vec3(0.0, 1.0, 0.0),
                         vec3(0.0, 0.0, 1.5707963267948966),
                         1.0,
                         4));
    ray = RuntimeRay3D_Make(
        vec3(0.0, -2.0, 0.0), vec3(0.0, 1.0, 0.0));
    check_true("rotated_curve_direct_hit",
               RuntimeSceneCurve3D_TraceAllInstances(
                   &scene, &ray, 0.001, 100.0, &hit));
    check_near("rotated_curve_tangent_x",
               fabs(hit.curveTangent.x),
               1.0,
               1.0e-12);
    check_near("rotated_curve_tangent_y",
               hit.curveTangent.y,
               0.0,
               1.0e-12);
    RuntimeCurveAsset3D_Free(&asset);
    RuntimeScene3D_Free(&scene);
}

int main(void) {
    test_curve_only_scene_and_material_dispatch();
    test_mixed_curve_triangle_tlas_and_flat_parity();
    test_curve_instance_validation_and_rotation();
    if (g_failures != 0) {
        fprintf(stderr, "PSG-23C failures=%d\n", g_failures);
        return 1;
    }
    printf("PSG-23C curve scene/TLAS/material dispatch passed\n");
    return 0;
}
