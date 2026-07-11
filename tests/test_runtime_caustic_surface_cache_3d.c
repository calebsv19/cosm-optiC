#include "test_runtime_caustic_surface_cache_3d.h"

#include "render/runtime_caustic_surface_cache_3d.h"
#include "test_support.h"

static HitInfo3D test_surface_cache_hit(Vec3 position) {
    HitInfo3D hit = {0};
    hit.position = position;
    hit.normal = vec3(0.0, 1.0, 0.0);
    hit.sceneObjectIndex = 2;
    hit.primitiveIndex = 3;
    hit.triangleIndex = 4;
    return hit;
}

static int test_runtime_caustic_surface_cache_empty_allocation(void) {
    RuntimeCausticSurfaceCache3D cache;
    RuntimeCausticSurfaceCacheDiagnostics3D diagnostics;

    RuntimeCausticSurfaceCache3D_Init(&cache);
    assert_true("runtime_caustic_surface_cache_allocate",
                RuntimeCausticSurfaceCache3D_Allocate(&cache, 8u));
    assert_true("runtime_caustic_surface_cache_allocated",
                RuntimeCausticSurfaceCache3D_IsAllocated(&cache));
    RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_surface_cache_empty_state",
                diagnostics.state == RUNTIME_CAUSTIC_CACHE_STATE_ALLOCATED_EMPTY);
    assert_true("runtime_caustic_surface_cache_empty_capacity",
                diagnostics.recordCapacity == 8u);
    assert_true("runtime_caustic_surface_cache_empty_count",
                diagnostics.recordCount == 0u);

    RuntimeCausticSurfaceCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_surface_cache_deposit_and_sample(void) {
    RuntimeCausticSurfaceCache3D cache;
    RuntimeCausticSurfaceCacheDiagnostics3D diagnostics;
    HitInfo3D hit = test_surface_cache_hit(vec3(0.0, 0.0, 0.0));
    HitInfo3D sample_hit = test_surface_cache_hit(vec3(0.02, 0.0, 0.0));
    Vec3 sample = vec3(0.0, 0.0, 0.0);

    RuntimeCausticSurfaceCache3D_Init(&cache);
    assert_true("runtime_caustic_surface_cache_deposit_allocate",
                RuntimeCausticSurfaceCache3D_Allocate(&cache, 8u));
    assert_true("runtime_caustic_surface_cache_deposit_ok",
                RuntimeCausticSurfaceCache3D_DepositAtHit(&cache,
                                                          &hit,
                                                          0.10,
                                                          1.0,
                                                          0.5,
                                                          0.25));
    assert_true("runtime_caustic_surface_cache_sample_ok",
                RuntimeCausticSurfaceCache3D_SampleAtHit(&cache, &sample_hit, &sample));
    assert_true("runtime_caustic_surface_cache_sample_positive", sample.x > 0.0);
    RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_surface_cache_sample_state",
                diagnostics.state == RUNTIME_CAUSTIC_CACHE_STATE_SAMPLED);
    assert_true("runtime_caustic_surface_cache_sample_count",
                diagnostics.sampleContributingCount == 1u);
    assert_true("runtime_caustic_surface_cache_record_count",
                diagnostics.recordCount == 1u);
    assert_close("runtime_caustic_surface_cache_total_r",
                 diagnostics.totalRadianceR,
                 1.0,
                 1e-6);

    RuntimeCausticSurfaceCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_surface_cache_normalizes_broad_footprints(void) {
    RuntimeCausticSurfaceCache3D compact_cache;
    RuntimeCausticSurfaceCache3D broad_cache;
    HitInfo3D hit = test_surface_cache_hit(vec3(0.0, 0.0, 0.0));
    Vec3 compact_sample = vec3(0.0, 0.0, 0.0);
    Vec3 broad_sample = vec3(0.0, 0.0, 0.0);

    RuntimeCausticSurfaceCache3D_Init(&compact_cache);
    RuntimeCausticSurfaceCache3D_Init(&broad_cache);
    assert_true("runtime_caustic_surface_cache_compact_allocate",
                RuntimeCausticSurfaceCache3D_Allocate(&compact_cache, 8u));
    assert_true("runtime_caustic_surface_cache_broad_allocate",
                RuntimeCausticSurfaceCache3D_Allocate(&broad_cache, 8u));
    assert_true("runtime_caustic_surface_cache_compact_deposit",
                RuntimeCausticSurfaceCache3D_DepositAtHit(&compact_cache,
                                                          &hit,
                                                          0.10,
                                                          1.0,
                                                          1.0,
                                                          1.0));
    assert_true("runtime_caustic_surface_cache_broad_deposit",
                RuntimeCausticSurfaceCache3D_DepositAtHit(&broad_cache,
                                                          &hit,
                                                          1.00,
                                                          1.0,
                                                          1.0,
                                                          1.0));
    assert_true("runtime_caustic_surface_cache_compact_center_sample",
                RuntimeCausticSurfaceCache3D_SampleAtHit(&compact_cache,
                                                         &hit,
                                                         &compact_sample));
    assert_true("runtime_caustic_surface_cache_broad_center_sample",
                RuntimeCausticSurfaceCache3D_SampleAtHit(&broad_cache,
                                                         &hit,
                                                         &broad_sample));
    assert_true("runtime_caustic_surface_cache_broad_density_lower",
                broad_sample.x < compact_sample.x * 0.25);
    assert_true("runtime_caustic_surface_cache_broad_still_positive",
                broad_sample.x > 0.0);

    RuntimeCausticSurfaceCache3D_Free(&compact_cache);
    RuntimeCausticSurfaceCache3D_Free(&broad_cache);
    return 0;
}

static int test_runtime_caustic_surface_cache_rejects_capacity_overflow(void) {
    RuntimeCausticSurfaceCache3D cache;
    RuntimeCausticSurfaceCacheDiagnostics3D diagnostics;
    HitInfo3D hit = test_surface_cache_hit(vec3(0.0, 0.0, 0.0));

    RuntimeCausticSurfaceCache3D_Init(&cache);
    assert_true("runtime_caustic_surface_cache_capacity_allocate",
                RuntimeCausticSurfaceCache3D_Allocate(&cache, 1u));
    assert_true("runtime_caustic_surface_cache_capacity_first",
                RuntimeCausticSurfaceCache3D_DepositAtHit(&cache,
                                                          &hit,
                                                          0.10,
                                                          1.0,
                                                          1.0,
                                                          1.0));
    hit.position = vec3(0.5, 0.0, 0.0);
    assert_true("runtime_caustic_surface_cache_capacity_second_rejected",
                !RuntimeCausticSurfaceCache3D_DepositAtHit(&cache,
                                                           &hit,
                                                           0.10,
                                                           1.0,
                                                           1.0,
                                                           1.0));
    RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_surface_cache_capacity_count",
                diagnostics.recordCount == 1u);
    assert_true("runtime_caustic_surface_cache_capacity_rejects",
                diagnostics.depositRejectedCount == 1u);

    RuntimeCausticSurfaceCache3D_Free(&cache);
    return 0;
}

static int test_runtime_caustic_surface_cache_clear_reset(void) {
    RuntimeCausticSurfaceCache3D cache;
    RuntimeCausticSurfaceCacheDiagnostics3D diagnostics;
    HitInfo3D hit = test_surface_cache_hit(vec3(0.0, 0.0, 0.0));

    RuntimeCausticSurfaceCache3D_Init(&cache);
    assert_true("runtime_caustic_surface_cache_clear_allocate",
                RuntimeCausticSurfaceCache3D_Allocate(&cache, 8u));
    assert_true("runtime_caustic_surface_cache_clear_deposit",
                RuntimeCausticSurfaceCache3D_DepositAtHit(&cache,
                                                          &hit,
                                                          0.10,
                                                          1.0,
                                                          1.0,
                                                          1.0));
    RuntimeCausticSurfaceCache3D_Clear(&cache);
    RuntimeCausticSurfaceCache3D_SnapshotDiagnostics(&cache, &diagnostics);
    assert_true("runtime_caustic_surface_cache_clear_still_allocated",
                diagnostics.allocated);
    assert_true("runtime_caustic_surface_cache_clear_empty",
                diagnostics.recordCount == 0u);
    assert_true("runtime_caustic_surface_cache_clear_state",
                diagnostics.state == RUNTIME_CAUSTIC_CACHE_STATE_ALLOCATED_EMPTY);

    RuntimeCausticSurfaceCache3D_Free(&cache);
    return 0;
}

int run_test_runtime_caustic_surface_cache_3d_tests(void) {
    int before = test_support_failures();

    test_runtime_caustic_surface_cache_empty_allocation();
    test_runtime_caustic_surface_cache_deposit_and_sample();
    test_runtime_caustic_surface_cache_normalizes_broad_footprints();
    test_runtime_caustic_surface_cache_rejects_capacity_overflow();
    test_runtime_caustic_surface_cache_clear_reset();

    return test_support_failures() - before;
}
