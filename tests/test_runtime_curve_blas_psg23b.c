#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "procedural/procedural_imported_surface_strand_curve.h"
#include "render/runtime_curve_blas_3d.h"
#include "render/runtime_triangle_bvh_3d.h"

#define PSG23B_PI 3.14159265358979323846

static int g_failures = 0;
static size_t g_parity_ray_count = 0u;
static size_t g_parity_mismatch_count = 0u;
static double g_maximum_t_delta = 0.0;

static void assert_true(const char *name, bool condition) {
    if (!condition) {
        fprintf(stderr, "FAIL %-52s condition=false\n", name);
        g_failures += 1;
    }
}

static void assert_near(const char *name,
                        double actual,
                        double expected,
                        double epsilon) {
    if (!isfinite(actual) || fabs(actual - expected) > epsilon) {
        fprintf(stderr,
                "FAIL %-52s actual=%.12f expected=%.12f epsilon=%.12f\n",
                name,
                actual,
                expected,
                epsilon);
        g_failures += 1;
    }
}

static bool trace_curve_flat(const RuntimeCurveAsset3D *asset,
                             const Ray3D *ray,
                             double t_min,
                             double t_max,
                             HitInfo3D *out_hit) {
    HitInfo3D best;
    bool found = false;
    HitInfo3D_Reset(&best);
    for (size_t i = 0u; i < asset->primitiveCount; ++i) {
        HitInfo3D hit;
        if (!RuntimeRay3D_IntersectCurvePrimitive(
                ray,
                &asset->primitives[i],
                (int)i,
                t_min,
                found ? best.t : t_max,
                &hit)) {
            continue;
        }
        if (!found || hit.t < best.t ||
            (hit.t == best.t &&
             hit.curvePrimitiveIndex < best.curvePrimitiveIndex)) {
            best = hit;
            found = true;
        }
    }
    if (!found) {
        HitInfo3D_Reset(out_hit);
        return false;
    }
    *out_hit = best;
    return true;
}

static RuntimeTriangle3D make_triangle(Vec3 p0,
                                       Vec3 p1,
                                       Vec3 p2,
                                       int local_index) {
    RuntimeTriangle3D triangle;
    memset(&triangle, 0, sizeof(triangle));
    triangle.p0 = p0;
    triangle.p1 = p1;
    triangle.p2 = p2;
    triangle.normal =
        vec3_normalize(vec3_cross(vec3_sub(p1, p0), vec3_sub(p2, p0)));
    triangle.twoSided = true;
    triangle.localTriangleIndex = local_index;
    return triangle;
}

static bool build_straight_tapered_tube(RuntimeTriangleMesh3D *mesh,
                                        Vec3 p0,
                                        Vec3 p1,
                                        double radius0,
                                        double radius1,
                                        size_t sides) {
    Vec3 *ring0 = NULL;
    Vec3 *ring1 = NULL;
    size_t triangle = 0u;
    if (!mesh || sides < 8u || sides > (size_t)INT_MAX / 4u) return false;
    RuntimeTriangleMesh3D_Init(mesh);
    mesh->triangleCapacity = (int)(sides * 4u);
    mesh->triangles = calloc(
        (size_t)mesh->triangleCapacity, sizeof(*mesh->triangles));
    ring0 = calloc(sides, sizeof(*ring0));
    ring1 = calloc(sides, sizeof(*ring1));
    if (!mesh->triangles || !ring0 || !ring1) goto fail;
    for (size_t side = 0u; side < sides; ++side) {
        const double angle = 2.0 * PSG23B_PI * (double)side / (double)sides;
        const Vec3 direction = vec3(cos(angle), 0.0, sin(angle));
        ring0[side] = vec3_add(p0, vec3_scale(direction, radius0));
        ring1[side] = vec3_add(p1, vec3_scale(direction, radius1));
    }
    for (size_t side = 0u; side < sides; ++side) {
        const size_t next = (side + 1u) % sides;
        mesh->triangles[triangle] =
            make_triangle(ring0[side],
                          ring1[side],
                          ring1[next],
                          (int)triangle);
        triangle += 1u;
        mesh->triangles[triangle] =
            make_triangle(ring0[side],
                          ring1[next],
                          ring0[next],
                          (int)triangle);
        triangle += 1u;
        mesh->triangles[triangle] =
            make_triangle(p0,
                          ring0[side],
                          ring0[next],
                          (int)triangle);
        triangle += 1u;
        mesh->triangles[triangle] =
            make_triangle(p1,
                          ring1[next],
                          ring1[side],
                          (int)triangle);
        triangle += 1u;
    }
    mesh->triangleCount = (int)triangle;
    mesh->bvhDirty = true;
    free(ring0);
    free(ring1);
    return RuntimeTriangleMesh3D_BuildBVH(mesh);

fail:
    free(ring0);
    free(ring1);
    RuntimeTriangleMesh3D_Free(mesh);
    return false;
}

static void compare_curve_and_tube(const RuntimeCurveAsset3D *curve,
                                   const RuntimeTriangleMesh3D *tube,
                                   Ray3D ray,
                                   double t_epsilon) {
    HitInfo3D curve_hit;
    HitInfo3D tube_hit;
    const bool curve_found = RuntimeCurveBLAS3D_TraceFirstHit(
        curve, &ray, 1.0e-6, 10.0, &curve_hit);
    const bool tube_found = RuntimeTriangleBVH3D_TraceFirstHit(
        tube, &ray, 1.0e-6, 10.0, &tube_hit);
    g_parity_ray_count += 1u;
    if (curve_found != tube_found) {
        g_parity_mismatch_count += 1u;
        return;
    }
    if (curve_found) {
        const double delta = fabs(curve_hit.t - tube_hit.t);
        if (delta > g_maximum_t_delta) g_maximum_t_delta = delta;
        if (delta > t_epsilon) g_parity_mismatch_count += 1u;
    }
}

static void test_psg23a_adapter_and_curve_blas(void) {
    ProceduralImportedSurfaceStrandAsset strands;
    RuntimeCurveAsset3D curve;
    RuntimeCurveBLAS3DBuildStats build_stats;
    RuntimeCurveBLAS3DTraceStats trace_stats;
    CoreObjectVec3 points[8] = {
        {0.0, 0.0, 0.0},
        {0.0, 0.7, 0.0},
        {0.18, 1.4, 0.0},
        {0.35, 2.0, 0.12},
        {1.0, 0.0, 0.0},
        {1.0, 0.6, 0.0},
        {0.9, 1.2, 0.15},
        {0.8, 1.8, 0.25}};
    double radii[8] = {0.20, 0.17, 0.13, 0.07, 0.18, 0.15, 0.11, 0.06};

    memset(&strands, 0, sizeof(strands));
    RuntimeCurveAsset3D_Init(&curve);
    strands.strand_count = 2u;
    strands.points_per_strand = 4u;
    strands.points = points;
    strands.radii = radii;
    assert_true("psg23b_adapter_build",
                ProceduralImportedSurfaceStrands_BuildCurveAsset(
                    &strands, &curve));
    assert_true("psg23b_adapter_segment_count", curve.primitiveCount == 6u);
    assert_true("psg23b_adapter_root_cap",
                curve.primitives[0].hasRootCap &&
                    !curve.primitives[0].hasTipCap);
    assert_true("psg23b_adapter_tip_cap",
                curve.primitives[2].hasTipCap &&
                    !curve.primitives[2].hasRootCap);
    assert_true("psg23b_adapter_provenance",
                curve.primitives[4].strandIndex == 1 &&
                    curve.primitives[4].segmentIndex == 1);
    assert_true("psg23b_blas_build", RuntimeCurveAsset3D_BuildBLAS(&curve));
    assert_true("psg23b_blas_ready", RuntimeCurveAsset3D_HasReadyBLAS(&curve));
    assert_true("psg23b_blas_stats",
                RuntimeCurveAsset3D_BLASBuildStats(&curve, &build_stats));
    assert_true("psg23b_blas_stats_nodes",
                build_stats.ready && build_stats.nodeCount >= 3u &&
                    build_stats.leafCount >= 2u &&
                    build_stats.totalBytes > 0u);

    RuntimeCurveBLAS3D_ResetTraceStats();
    for (int ray_index = 0; ray_index < 64; ++ray_index) {
        const double y = 0.03 + 1.90 * (double)ray_index / 63.0;
        Ray3D ray = RuntimeRay3D_Make(
            vec3(-1.0, y, 0.0), vec3(1.0, 0.0, 0.0));
        HitInfo3D flat;
        HitInfo3D accelerated;
        const bool flat_found =
            trace_curve_flat(&curve, &ray, 1.0e-6, 10.0, &flat);
        const bool accelerated_found = RuntimeCurveBLAS3D_TraceFirstHit(
            &curve, &ray, 1.0e-6, 10.0, &accelerated);
        assert_true("psg23b_flat_blas_hit_parity",
                    flat_found == accelerated_found);
        if (flat_found && accelerated_found) {
            assert_near(
                "psg23b_flat_blas_t_parity", accelerated.t, flat.t, 1.0e-12);
            assert_true("psg23b_flat_blas_primitive_parity",
                        accelerated.curvePrimitiveIndex ==
                            flat.curvePrimitiveIndex);
            assert_true("psg23b_tangent_payload_present",
                        accelerated.hasCurveTangent);
            assert_near("psg23b_tangent_payload_unit",
                        vec3_length(accelerated.curveTangent),
                        1.0,
                        1.0e-12);
            assert_true("psg23b_tangent_payload_finite_u",
                        isfinite(accelerated.curveU) &&
                            accelerated.curveU >= 0.0 &&
                            accelerated.curveU <= 1.0);
            assert_true("psg23b_tangent_payload_positive_radius",
                        accelerated.curveRadius > 0.0);
        }
    }
    {
        const Ray3D cap_ray = RuntimeRay3D_Make(
            vec3(0.0, -1.0, 0.0), vec3(0.0, 1.0, 0.0));
        HitInfo3D hit;
        assert_true("psg23b_root_cap_hit",
                    RuntimeCurveBLAS3D_TraceFirstHit(
                        &curve, &cap_ray, 1.0e-6, 10.0, &hit));
        assert_near("psg23b_root_cap_t", hit.t, 1.0, 1.0e-12);
        assert_near("psg23b_root_cap_u", hit.curveU, 0.0, 1.0e-12);
        assert_true("psg23b_root_cap_normal",
                    vec3_dot(hit.geometricNormal, vec3(0.0, -1.0, 0.0)) >
                        0.999999);
    }
    RuntimeCurveBLAS3D_SnapshotTraceStats(&trace_stats);
    assert_true("psg23b_trace_stats",
                trace_stats.traceCalls > 0u &&
                    trace_stats.traceHits > 0u &&
                    trace_stats.nodeVisits > 0u &&
                    trace_stats.aabbTests > 0u &&
                    trace_stats.primitiveTests > 0u &&
                    trace_stats.traceOverflows == 0u);
    RuntimeCurveAsset3D_Free(&curve);
}

static void test_triangle_tube_parity(void) {
    const Vec3 points[2] = {
        {0.0, 0.0, 0.0},
        {0.0, 2.0, 0.0}};
    const double radii[2] = {0.24, 0.12};
    RuntimeCurveAsset3D curve;
    RuntimeTriangleMesh3D tube;

    RuntimeCurveAsset3D_Init(&curve);
    RuntimeTriangleMesh3D_Init(&tube);
    assert_true("psg23b_parity_curve_build",
                RuntimeCurveAsset3D_BuildFromPolylineStrands(
                    &curve, points, radii, 1u, 2u));
    assert_true("psg23b_parity_curve_blas",
                RuntimeCurveAsset3D_BuildBLAS(&curve));
    assert_true("psg23b_parity_tube_build",
                build_straight_tapered_tube(
                    &tube, points[0], points[1], radii[0], radii[1], 64u));

    for (int y_index = 0; y_index < 19; ++y_index) {
        const double y = 0.1 + 1.8 * (double)y_index / 18.0;
        const double radius = radii[0] + (radii[1] - radii[0]) * (y / 2.0);
        const double radial_fractions[7] = {
            -1.20, -0.80, -0.35, 0.0, 0.35, 0.80, 1.20};
        for (size_t z_index = 0u; z_index < 7u; ++z_index) {
            const Ray3D ray = RuntimeRay3D_Make(
                vec3(-1.0, y, radius * radial_fractions[z_index]),
                vec3(1.0, 0.0, 0.0));
            compare_curve_and_tube(&curve, &tube, ray, 5.0e-4);
        }
    }
    {
        const double radial_fractions[6] = {
            -1.20, -0.75, -0.25, 0.25, 0.75, 1.20};
        for (size_t x_index = 0u; x_index < 6u; ++x_index) {
            const Ray3D root_ray = RuntimeRay3D_Make(
                vec3(radii[0] * radial_fractions[x_index], -1.0, 0.0),
                vec3(0.0, 1.0, 0.0));
            const Ray3D tip_ray = RuntimeRay3D_Make(
                vec3(radii[1] * radial_fractions[x_index], 3.0, 0.0),
                vec3(0.0, -1.0, 0.0));
            compare_curve_and_tube(&curve, &tube, root_ray, 1.0e-12);
            compare_curve_and_tube(&curve, &tube, tip_ray, 1.0e-12);
        }
    }
    assert_true("psg23b_triangle_tube_parity_rays",
                g_parity_ray_count == 145u);
    assert_true("psg23b_triangle_tube_parity_mismatches",
                g_parity_mismatch_count == 0u);
    assert_true("psg23b_triangle_tube_parity_t_delta",
                g_maximum_t_delta <= 5.0e-4);
    RuntimeTriangleMesh3D_Free(&tube);
    RuntimeCurveAsset3D_Free(&curve);
}

static bool write_parity_grid(const char *path) {
    const Vec3 points[2] = {
        {0.0, 0.0, 0.0},
        {0.0, 2.0, 0.0}};
    const double radii[2] = {0.24, 0.12};
    const int width = 360;
    const int height = 220;
    RuntimeCurveAsset3D curve;
    RuntimeTriangleMesh3D tube;
    FILE *file = NULL;
    bool ok = false;

    RuntimeCurveAsset3D_Init(&curve);
    RuntimeTriangleMesh3D_Init(&tube);
    if (!RuntimeCurveAsset3D_BuildFromPolylineStrands(
            &curve, points, radii, 1u, 2u) ||
        !RuntimeCurveAsset3D_BuildBLAS(&curve) ||
        !build_straight_tapered_tube(
            &tube, points[0], points[1], radii[0], radii[1], 64u)) {
        goto cleanup;
    }
    file = fopen(path, "w");
    if (!file) goto cleanup;
    fprintf(file,
            "pixel_x,pixel_y,sample_y,sample_z,curve_hit,tube_hit,"
            "curve_t,tube_t,curve_u,curve_radius,tangent_x,tangent_y,tangent_z\n");
    for (int pixel_y = 0; pixel_y < height; ++pixel_y) {
        const double sample_z =
            0.35 - 0.70 * (double)pixel_y / (double)(height - 1);
        for (int pixel_x = 0; pixel_x < width; ++pixel_x) {
            const double sample_y =
                -0.15 + 2.30 * (double)pixel_x / (double)(width - 1);
            const Ray3D ray = RuntimeRay3D_Make(
                vec3(-1.0, sample_y, sample_z), vec3(1.0, 0.0, 0.0));
            HitInfo3D curve_hit;
            HitInfo3D tube_hit;
            const bool curve_found = RuntimeCurveBLAS3D_TraceFirstHit(
                &curve, &ray, 1.0e-6, 10.0, &curve_hit);
            const bool tube_found = RuntimeTriangleBVH3D_TraceFirstHit(
                &tube, &ray, 1.0e-6, 10.0, &tube_hit);
            fprintf(file,
                    "%d,%d,%.12g,%.12g,%d,%d,%.12g,%.12g,%.12g,%.12g,"
                    "%.12g,%.12g,%.12g\n",
                    pixel_x,
                    pixel_y,
                    sample_y,
                    sample_z,
                    curve_found ? 1 : 0,
                    tube_found ? 1 : 0,
                    curve_found ? curve_hit.t : -1.0,
                    tube_found ? tube_hit.t : -1.0,
                    curve_found ? curve_hit.curveU : -1.0,
                    curve_found ? curve_hit.curveRadius : -1.0,
                    curve_found ? curve_hit.curveTangent.x : 0.0,
                    curve_found ? curve_hit.curveTangent.y : 0.0,
                    curve_found ? curve_hit.curveTangent.z : 0.0);
        }
    }
    ok = fclose(file) == 0;
    file = NULL;

cleanup:
    if (file) fclose(file);
    RuntimeTriangleMesh3D_Free(&tube);
    RuntimeCurveAsset3D_Free(&curve);
    return ok;
}

int main(int argc, char **argv) {
    test_psg23a_adapter_and_curve_blas();
    test_triangle_tube_parity();
    if (g_failures != 0) {
        fprintf(stderr,
                "PSG-23B curve lane failed: failures=%d parity_mismatches=%zu\n",
                g_failures,
                g_parity_mismatch_count);
        return 1;
    }
    if (argc == 3 && strcmp(argv[1], "--dump-grid") == 0) {
        if (!write_parity_grid(argv[2])) {
            fprintf(stderr, "PSG-23B failed to write parity grid: %s\n", argv[2]);
            return 1;
        }
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [--dump-grid <path>]\n", argv[0]);
        return 2;
    }
    printf("{\"schema\":\"ray_tracing.psg23b_native_curve_contract\","
           "\"schema_version\":1,\"passed\":true,"
           "\"curve_primitive\":\"tapered_piecewise_linear\","
           "\"curve_blas\":true,\"tangent_hit_payload\":true,"
           "\"triangle_tube_parity_ray_count\":%zu,"
           "\"triangle_tube_parity_mismatch_count\":%zu,"
           "\"maximum_t_delta\":%.12g}\n",
           g_parity_ray_count,
           g_parity_mismatch_count,
           g_maximum_t_delta);
    return 0;
}
