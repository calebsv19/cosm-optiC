#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "render/runtime_curve_blas_3d.h"

#define PSG23F_STRANDS 2048u
#define PSG23F_POINTS 8u

static int g_failures;

static void require_true(const char *name, bool condition) {
    if (!condition) {
        fprintf(stderr, "FAIL %-52s condition=false\n", name);
        g_failures += 1;
    }
}

static bool build_dense_asset(RuntimeCurveAsset3D *asset) {
    Vec3 *points = calloc(PSG23F_STRANDS * PSG23F_POINTS, sizeof(*points));
    double *radii = calloc(
        PSG23F_STRANDS * PSG23F_POINTS, sizeof(*radii));
    bool ok = false;
    if (!points || !radii) goto cleanup;
    for (size_t strand = 0u; strand < PSG23F_STRANDS; ++strand) {
        const size_t column = strand % 64u;
        const size_t row = strand / 64u;
        const double root_x = -1.5 + 3.0 * (double)column / 63.0;
        const double root_y = -0.75 + 1.5 * (double)row / 31.0;
        const double phase = 0.37 * (double)(strand % 17u);
        for (size_t point = 0u; point < PSG23F_POINTS; ++point) {
            const size_t index = strand * PSG23F_POINTS + point;
            const double t = (double)point / (double)(PSG23F_POINTS - 1u);
            const double envelope = sin(t * 3.141592653589793);
            points[index] = vec3(
                root_x +
                    envelope * 0.018 *
                        sin(phase + t * 3.141592653589793),
                root_y +
                    envelope * 0.014 *
                        cos(phase + t * 3.141592653589793),
                0.08 + 0.82 * t);
            radii[index] = 0.0030 * (1.0 - t) + 0.00045 * t;
        }
    }
    ok = RuntimeCurveAsset3D_BuildFromPolylineStrands(
             asset, points, radii, PSG23F_STRANDS, PSG23F_POINTS) &&
         RuntimeCurveAsset3D_BuildBLAS(asset);

cleanup:
    free(points);
    free(radii);
    return ok;
}

int main(void) {
    RuntimeCurveAsset3D first;
    RuntimeCurveAsset3D second;
    RuntimeCurveBLAS3DBuildStats build_stats;
    RuntimeCurveBLAS3DTraceStats trace_stats;
    size_t hit_count = 0u;
    RuntimeCurveAsset3D_Init(&first);
    RuntimeCurveAsset3D_Init(&second);

    require_true("psg23f_dense_first_build", build_dense_asset(&first));
    require_true("psg23f_dense_second_build", build_dense_asset(&second));
    require_true(
        "psg23f_dense_primitive_count",
        first.primitiveCount ==
            PSG23F_STRANDS * (PSG23F_POINTS - 1u));
    require_true(
        "psg23f_dense_blas_stats",
        RuntimeCurveAsset3D_BLASBuildStats(&first, &build_stats));
    require_true(
        "psg23f_dense_blas_shape",
        build_stats.ready &&
            build_stats.primitiveCount == first.primitiveCount &&
            build_stats.nodeCount > 1u &&
            build_stats.leafCount > 1u &&
            build_stats.maxDepth <= 32u &&
            build_stats.totalBytes > 0u);

    RuntimeCurveBLAS3D_ResetTraceStats();
    for (size_t strand = 0u; strand < PSG23F_STRANDS; ++strand) {
        const size_t column = strand % 64u;
        const size_t row = strand / 64u;
        const double root_x = -1.5 + 3.0 * (double)column / 63.0;
        const double root_y = -0.75 + 1.5 * (double)row / 31.0;
        const Ray3D ray = RuntimeRay3D_Make(
            vec3(root_x, root_y, 1.5), vec3(0.0, 0.0, -1.0));
        HitInfo3D first_hit;
        HitInfo3D second_hit;
        const bool first_found = RuntimeCurveBLAS3D_TraceFirstHit(
            &first, &ray, 1.0e-6, 3.0, &first_hit);
        const bool second_found = RuntimeCurveBLAS3D_TraceFirstHit(
            &second, &ray, 1.0e-6, 3.0, &second_hit);
        require_true("psg23f_dense_repeat_hit_state",
                     first_found == second_found);
        if (first_found && second_found) {
            hit_count += 1u;
            require_true(
                "psg23f_dense_repeat_primitive_identity",
                first_hit.curvePrimitiveIndex ==
                    second_hit.curvePrimitiveIndex);
            require_true(
                "psg23f_dense_repeat_depth",
                fabs(first_hit.t - second_hit.t) <= 1.0e-12);
        }
    }
    RuntimeCurveBLAS3D_SnapshotTraceStats(&trace_stats);
    require_true("psg23f_dense_hit_coverage",
                 hit_count >= PSG23F_STRANDS * 9u / 10u);
    require_true(
        "psg23f_dense_trace_accounting",
        trace_stats.traceCalls == PSG23F_STRANDS * 2u &&
            trace_stats.traceHits >= hit_count * 2u &&
            trace_stats.traceOverflows == 0u &&
            trace_stats.maxStackDepth <= 32u);
    require_true(
        "psg23f_dense_acceleration_efficiency",
        trace_stats.primitiveTests <
            trace_stats.traceCalls * 256u &&
            trace_stats.primitiveTests <
                trace_stats.traceCalls * first.primitiveCount / 32u);

    if (g_failures != 0) {
        fprintf(stderr, "PSG-23F dense curve lane failed: %d\n", g_failures);
        RuntimeCurveAsset3D_Free(&second);
        RuntimeCurveAsset3D_Free(&first);
        return 1;
    }
    printf(
        "{\"schema\":\"ray_tracing.psg23f_dense_curve_blas\","
        "\"passed\":true,\"strand_count\":%u,\"primitive_count\":%zu,"
        "\"node_count\":%zu,\"max_depth\":%zu,"
        "\"trace_calls\":%llu,\"primitive_tests\":%llu,"
        "\"max_stack_depth\":%llu}\n",
        PSG23F_STRANDS,
        first.primitiveCount,
        build_stats.nodeCount,
        build_stats.maxDepth,
        (unsigned long long)trace_stats.traceCalls,
        (unsigned long long)trace_stats.primitiveTests,
        (unsigned long long)trace_stats.maxStackDepth);
    RuntimeCurveAsset3D_Free(&second);
    RuntimeCurveAsset3D_Free(&first);
    return 0;
}
