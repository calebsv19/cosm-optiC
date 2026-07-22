#include "test_runtime_caustic_photon_ppm28_3d.h"

#include <math.h>
#include <string.h>

#include "render/runtime_caustic_photon_map_3d.h"
#include "render/runtime_caustic_photon_receiver_patch_3d.h"
#include "render/runtime_caustic_photon_surface_kernel_3d.h"
#include "test_support.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static RuntimeCausticPhotonMapRecord3D ppm28_record(uint64_t photon_id,
                                                    double x,
                                                    int primitive,
                                                    int triangle) {
    RuntimeCausticPhotonMapRecord3D record;
    memset(&record, 0, sizeof(record));
    record.photonId = photon_id;
    record.position = vec3(x, 0.0, 0.0);
    record.normal = vec3(0.0, 1.0, 0.0);
    record.incidentDirection = vec3(0.0, -1.0, 0.0);
    record.flux = vec3(1.0, 1.0, 1.0);
    record.pathPdf = 1.0;
    record.queryRadius = 9.0;
    record.sceneObjectIndex = 11;
    record.materialId = 12;
    record.primitiveIndex = primitive;
    record.triangleIndex = triangle;
    return record;
}

static RuntimeCausticPhotonMapQuery3D ppm28_query(void) {
    RuntimeCausticPhotonMapQuery3D query;
    RuntimeCausticPhotonMap3D_DefaultQuery(&query);
    query.position = vec3(0.0, 0.0, 0.0);
    query.normal = vec3(0.0, 1.0, 0.0);
    query.radius = 0.10;
    query.sceneObjectIndex = 11;
    query.materialId = 12;
    query.primitiveIndex = 20;
    query.triangleIndex = 30;
    query.estimator.estimator = RUNTIME_CAUSTIC_PHOTON_ESTIMATOR_K_NEAREST;
    query.estimator.neighborLimit = 4u;
    query.estimator.minimumEffectiveSamples = 4u;
    return query;
}

static int test_ppm28_receiver_patch_and_material_discontinuity(void) {
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapQuery3D query = ppm28_query();
    RuntimeCausticPhotonMapQueryResult3D patch;
    RuntimeCausticPhotonMapQueryResult3D material_reject;

    query.position = vec3(0.025, 0.0, 0.0);

    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("ppm28_receiver_patch_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 4u));
    for (uint64_t i = 0u; i < 4u; ++i) {
        RuntimeCausticPhotonMapRecord3D record =
            ppm28_record(i + 1u, 0.01 * (double)(i + 1u), 20 + (int)i, 30 + (int)i);
        assert_true("ppm28_receiver_patch_store",
                    RuntimeCausticPhotonMap3D_StoreRecord(&map, &record));
    }
    assert_true("ppm28_receiver_patch_prepare_sample_supports",
                RuntimeCausticPhotonMap3D_PrepareSampleCenteredSupports(
                    &map, 3u));
    assert_true("ppm28_receiver_patch_crosses_retessellation",
                RuntimeCausticPhotonMap3D_Query(&map, &query, &patch) &&
                    patch.effectiveSampleCount == 4u &&
                    patch.receiverRejectCount == 0u && patch.supportAdaptive &&
                    patch.supportRadius < query.radius &&
                    patch.kernelBoundaryWeight == 0.0 &&
                    patch.densityEstimate > 0.0 && !patch.undersampled &&
                    !patch.fallbackUsed);

    query.materialId = 99;
    assert_true("ppm28_receiver_patch_material_discontinuity",
                !RuntimeCausticPhotonMap3D_Query(&map, &query, &material_reject) &&
                    material_reject.receiverMaterialRejectCount == 4u);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

static int test_ppm28_query_radius_not_record_radius_and_undersampled(void) {
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapQuery3D query = ppm28_query();
    RuntimeCausticPhotonMapQueryResult3D result;

    RuntimeCausticPhotonMap3D_Init(&map);
    assert_true("ppm28_bounded_support_allocate",
                RuntimeCausticPhotonMap3D_Allocate(&map, 4u));
    for (uint64_t i = 0u; i < 4u; ++i) {
        RuntimeCausticPhotonMapRecord3D record =
            ppm28_record(i + 1u, 0.01 * (double)(i + 1u), 20, 30);
        assert_true("ppm28_bounded_support_store",
                    RuntimeCausticPhotonMap3D_StoreRecord(&map, &record));
    }
    assert_true("ppm28_bounded_support_prepare",
                RuntimeCausticPhotonMap3D_PrepareSampleCenteredSupports(
                    &map, 3u));
    query.radius = 0.025;
    assert_true("ppm28_record_radius_cannot_expand_query_support",
                RuntimeCausticPhotonMap3D_Query(&map, &query, &result) &&
                    result.effectiveSampleCount == 2u && result.radiusRejectCount == 2u &&
                    result.supportRadius == query.radius && result.undersampled &&
                    result.physicalFlux.x > 0.0);
    RuntimeCausticPhotonMap3D_Free(&map);
    return 0;
}

static int test_ppm28_compact_kernel_contract(void) {
    const double radius = 0.5;
    const double center =
        RuntimeCausticPhotonSurfaceKernel3D_Weight(0.0, radius);
    const double interior =
        RuntimeCausticPhotonSurfaceKernel3D_Weight(0.25, radius);
    const double boundary =
        RuntimeCausticPhotonSurfaceKernel3D_Weight(radius, radius);
    assert_close("ppm28_kernel_center_normalization",
                 center,
                 2.0 / (M_PI * radius * radius),
                 1e-12);
    assert_true("ppm28_kernel_compact_support",
                center > interior && interior > 0.0 && boundary == 0.0 &&
                    RuntimeCausticPhotonSurfaceKernel3D_Weight(0.6, radius) == 0.0);
    return 0;
}

static double ppm28_uniform_density_estimate(uint64_t side) {
    RuntimeCausticPhotonMap3D map;
    RuntimeCausticPhotonMapQuery3D query = ppm28_query();
    RuntimeCausticPhotonMapQueryResult3D result;
    const uint64_t count = side * side;
    RuntimeCausticPhotonMap3D_Init(&map);
    if (!RuntimeCausticPhotonMap3D_Allocate(&map, count)) return 0.0;
    for (uint64_t y = 0u; y < side; ++y) {
        for (uint64_t x = 0u; x < side; ++x) {
            RuntimeCausticPhotonMapRecord3D record = ppm28_record(
                y * side + x + 1u, 0.0, 20, 30);
            record.position = vec3(-1.0 + ((double)x + 0.5) * 2.0 / (double)side,
                                   0.0,
                                   -1.0 + ((double)y + 0.5) * 2.0 / (double)side);
            record.flux = vec3(4.0 / (double)count,
                               4.0 / (double)count,
                               4.0 / (double)count);
            if (!RuntimeCausticPhotonMap3D_StoreRecord(&map, &record)) {
                RuntimeCausticPhotonMap3D_Free(&map);
                return 0.0;
            }
        }
    }
    query.radius = 1.0;
    query.estimator.neighborLimit =
        RuntimeCausticPhotonEstimator3D_ConvergentNeighborLimit(
            count, 1u, 256u);
    query.estimator.minimumEffectiveSamples = 1u;
    if (!RuntimeCausticPhotonMap3D_PrepareSampleCenteredSupports(
            &map, query.estimator.neighborLimit)) {
        RuntimeCausticPhotonMap3D_Free(&map);
        return 0.0;
    }
    (void)RuntimeCausticPhotonMap3D_Query(&map, &query, &result);
    RuntimeCausticPhotonMap3D_Free(&map);
    return result.physicalFlux.x;
}

static int test_ppm28_convergent_uniform_density_contract(void) {
    const double coarse = ppm28_uniform_density_estimate(8u);
    const double fine = ppm28_uniform_density_estimate(64u);
    assert_true("ppm28_convergent_uniform_density_improves",
                fabs(fine - 1.0) < fabs(coarse - 1.0));
    assert_close("ppm28_convergent_uniform_density_limit", fine, 1.0, 0.02);
    return 0;
}

int run_test_runtime_caustic_photon_ppm28_3d_tests(void) {
    int failures = 0;
    failures += test_ppm28_receiver_patch_and_material_discontinuity();
    failures += test_ppm28_query_radius_not_record_radius_and_undersampled();
    failures += test_ppm28_compact_kernel_contract();
    failures += test_ppm28_convergent_uniform_density_contract();
    return failures;
}
