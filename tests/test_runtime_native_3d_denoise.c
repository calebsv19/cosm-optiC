#include <string.h>

#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "test_runtime_native_3d_denoise.h"
#include "test_support.h"

static int test_runtime_native_3d_denoise_apply_policy(void) {
    assert_true("runtime_native_3d_denoise_policy_disney_temporal",
                RuntimeNative3DDenoise_ShouldApply(RAY_TRACING_3D_INTEGRATOR_DISNEY,
                                                   12,
                                                   true));
    assert_true("runtime_native_3d_denoise_policy_disney_single_disabled",
                !RuntimeNative3DDenoise_ShouldApply(RAY_TRACING_3D_INTEGRATOR_DISNEY,
                                                    1,
                                                    true));
    assert_true("runtime_native_3d_denoise_policy_disabled_toggle_blocks",
                !RuntimeNative3DDenoise_ShouldApply(RAY_TRACING_3D_INTEGRATOR_DISNEY,
                                                    12,
                                                    false));
    assert_true("runtime_native_3d_denoise_policy_material_disabled",
                !RuntimeNative3DDenoise_ShouldApply(RAY_TRACING_3D_INTEGRATOR_MATERIAL,
                                                    12,
                                                    true));
    assert_true("runtime_native_3d_denoise_policy_disney_v2_edge_safe",
                RuntimeNative3DDenoise_ShouldApply(RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                   12,
                                                   true));
    return 0;
}

static bool test_runtime_native_3d_denoise_setup_flat_features(
    RuntimeNative3DFeatureBuffer* features,
    int width) {
    bool ok = RuntimeNative3DFeatureBuffer_Ensure(features, width, 1);
    if (!ok) return false;

    memset(features->hitMaskBuffer, 1, (size_t)width * sizeof(*features->hitMaskBuffer));
    for (int i = 0; i < width; ++i) {
        const size_t normal_base = (size_t)i * 3u;
        features->depthBuffer[i] = 1.0f;
        features->normalBuffer[normal_base] = 0.0f;
        features->normalBuffer[normal_base + 1u] = 0.0f;
        features->normalBuffer[normal_base + 2u] = 1.0f;
        features->reflectivityBuffer[i] = 0.0f;
        features->roughnessBuffer[i] = 1.0f;
        features->transparencyBuffer[i] = 0.0f;
        features->triangleIndexBuffer[i] = 7;
        features->sceneObjectIndexBuffer[i] = 3;
    }
    return true;
}

static void test_runtime_native_3d_denoise_set_gray(float* radiance,
                                                    int index,
                                                    float value) {
    const size_t base = (size_t)index * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    radiance[base] = value;
    radiance[base + 1u] = value;
    radiance[base + 2u] = value;
}

static int test_runtime_native_3d_denoise_respects_normal_breaks(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    float radiance[3 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    bool ok = RuntimeNative3DFeatureBuffer_Ensure(&features, 3, 1);
    assert_true("runtime_native_3d_denoise_normal_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

    radiance[0] = 1.0f;

    memset(features.hitMaskBuffer, 1, 3u);
    features.depthBuffer[0] = 1.0f;
    features.depthBuffer[1] = 1.0f;
    features.depthBuffer[2] = 1.0f;
    features.normalBuffer[0] = 0.0f;
    features.normalBuffer[1] = 0.0f;
    features.normalBuffer[2] = 1.0f;
    features.normalBuffer[3] = 0.0f;
    features.normalBuffer[4] = 0.0f;
    features.normalBuffer[5] = 1.0f;
    features.normalBuffer[6] = 1.0f;
    features.normalBuffer[7] = 0.0f;
    features.normalBuffer[8] = 0.0f;

    ok = RuntimeNative3DDenoise_Apply(radiance, 3, &features);
    assert_true("runtime_native_3d_denoise_normal_apply_ok", ok);
    assert_true("runtime_native_3d_denoise_normal_center_lifts",
                radiance[RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] > 0.05f);
    assert_true("runtime_native_3d_denoise_normal_discontinuity_stays_dark",
                radiance[2 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] < 0.01f);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

static int test_runtime_native_3d_denoise_respects_depth_breaks(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    float radiance[3 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    bool ok = RuntimeNative3DFeatureBuffer_Ensure(&features, 3, 1);
    assert_true("runtime_native_3d_denoise_depth_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

    radiance[0] = 1.0f;

    memset(features.hitMaskBuffer, 1, 3u);
    features.depthBuffer[0] = 1.0f;
    features.depthBuffer[1] = 1.0f;
    features.depthBuffer[2] = 5.0f;
    for (int i = 0; i < 3; ++i) {
        const size_t base = (size_t)i * 3u;
        features.normalBuffer[base] = 0.0f;
        features.normalBuffer[base + 1u] = 0.0f;
        features.normalBuffer[base + 2u] = 1.0f;
    }

    ok = RuntimeNative3DDenoise_Apply(radiance, 3, &features);
    assert_true("runtime_native_3d_denoise_depth_apply_ok", ok);
    assert_true("runtime_native_3d_denoise_depth_center_lifts",
                radiance[RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] > 0.05f);
    assert_true("runtime_native_3d_denoise_depth_far_stays_dark",
                radiance[2 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] < 0.01f);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

static int test_runtime_native_3d_denoise_disney_v2_blurs_stable_same_triangle(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    RuntimeNative3DDenoiseDiagnostics diagnostics = {0};
    float radiance[3 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    float temporal_activity[3] = {0.01f, 0.01f, 0.01f};
    bool ok = test_runtime_native_3d_denoise_setup_flat_features(&features, 3);
    assert_true("runtime_native_3d_denoise_v2_stable_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

    test_runtime_native_3d_denoise_set_gray(radiance, 0, 0.50f);
    test_runtime_native_3d_denoise_set_gray(radiance, 1, 0.40f);
    test_runtime_native_3d_denoise_set_gray(radiance, 2, 0.50f);

    ok = RuntimeNative3DDenoise_ApplyForIntegrator(radiance,
                                                   3,
                                                   &features,
                                                   RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                   8,
                                                   temporal_activity,
                                                   3,
                                                   &diagnostics);
    assert_true("runtime_native_3d_denoise_v2_stable_apply_ok", ok);
    assert_true("runtime_native_3d_denoise_v2_stable_center_blurs",
                radiance[RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] > 0.42f &&
                    radiance[RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] < 0.50f);
    assert_true("runtime_native_3d_denoise_v2_stable_diag_records_temporal",
                diagnostics.temporalFrameCount == 8 && diagnostics.rawPixelCount == 3);
    assert_true("runtime_native_3d_denoise_v2_stable_diag_records_samples",
                diagnostics.reconstructedPixelCount > 0 &&
                    diagnostics.stableInteriorSampleCount > 0);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

static int test_runtime_native_3d_denoise_disney_v2_blurs_rough_reflective_interiors(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    RuntimeNative3DDenoiseDiagnostics diagnostics = {0};
    float radiance[3 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    bool ok = test_runtime_native_3d_denoise_setup_flat_features(&features, 3);
    assert_true("runtime_native_3d_denoise_v2_rough_reflective_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

    test_runtime_native_3d_denoise_set_gray(radiance, 0, 0.50f);
    test_runtime_native_3d_denoise_set_gray(radiance, 1, 0.40f);
    test_runtime_native_3d_denoise_set_gray(radiance, 2, 0.50f);
    for (int i = 0; i < 3; ++i) {
        features.reflectivityBuffer[i] = 0.12f;
        features.roughnessBuffer[i] = 0.65f;
    }

    ok = RuntimeNative3DDenoise_ApplyForIntegrator(radiance,
                                                   3,
                                                   &features,
                                                   RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                   12,
                                                   NULL,
                                                   0,
                                                   &diagnostics);
    assert_true("runtime_native_3d_denoise_v2_rough_reflective_apply_ok", ok);
    assert_true("runtime_native_3d_denoise_v2_rough_reflective_center_blurs",
                radiance[RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] > 0.42f &&
                    radiance[RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] < 0.50f);
    assert_true("runtime_native_3d_denoise_v2_rough_reflective_not_preserved",
                diagnostics.preservedMirrorGlossyPixelCount == 0);
    assert_true("runtime_native_3d_denoise_v2_rough_reflective_records_samples",
                diagnostics.reconstructedPixelCount > 0 &&
                    diagnostics.stableInteriorSampleCount > 0);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

static int test_runtime_native_3d_denoise_disney_v2_rejects_clean_visual_edges(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    RuntimeNative3DDenoiseDiagnostics diagnostics = {0};
    float radiance[3 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    bool ok = test_runtime_native_3d_denoise_setup_flat_features(&features, 3);
    assert_true("runtime_native_3d_denoise_v2_visual_edge_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

    test_runtime_native_3d_denoise_set_gray(radiance, 0, 1.00f);
    test_runtime_native_3d_denoise_set_gray(radiance, 1, 0.00f);
    test_runtime_native_3d_denoise_set_gray(radiance, 2, 0.00f);

    ok = RuntimeNative3DDenoise_ApplyForIntegrator(radiance,
                                                   3,
                                                   &features,
                                                   RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                   8,
                                                   NULL,
                                                   0,
                                                   &diagnostics);
    assert_true("runtime_native_3d_denoise_v2_visual_edge_apply_ok", ok);
    assert_true("runtime_native_3d_denoise_v2_visual_edge_center_preserved",
                radiance[RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] < 0.05f);
    assert_true("runtime_native_3d_denoise_v2_visual_edge_rejected_samples",
                diagnostics.rejectedEdgeSampleCount > 0);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

static int test_runtime_native_3d_denoise_disney_v2_requires_same_surface_identity(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    RuntimeNative3DDenoiseDiagnostics diagnostics = {0};
    float radiance[3 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    bool ok = test_runtime_native_3d_denoise_setup_flat_features(&features, 3);
    assert_true("runtime_native_3d_denoise_v2_identity_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

    test_runtime_native_3d_denoise_set_gray(radiance, 0, 0.20f);
    test_runtime_native_3d_denoise_set_gray(radiance, 1, 0.00f);
    test_runtime_native_3d_denoise_set_gray(radiance, 2, 0.20f);
    features.triangleIndexBuffer[0] = 8;
    features.triangleIndexBuffer[2] = 8;
    features.sceneObjectIndexBuffer[2] = 4;

    ok = RuntimeNative3DDenoise_ApplyForIntegrator(radiance,
                                                   3,
                                                   &features,
                                                   RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                   8,
                                                   NULL,
                                                   0,
                                                   &diagnostics);
    assert_true("runtime_native_3d_denoise_v2_identity_apply_ok", ok);
    assert_true("runtime_native_3d_denoise_v2_identity_center_preserved",
                radiance[RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] < 0.01f);
    assert_true("runtime_native_3d_denoise_v2_identity_rejected_samples",
                diagnostics.rejectedEdgeSampleCount > 0);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

static int test_runtime_native_3d_denoise_disney_v2_preserves_special_materials(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    RuntimeNative3DDenoiseDiagnostics diagnostics = {0};
    float radiance[5 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    bool ok = test_runtime_native_3d_denoise_setup_flat_features(&features, 5);
    assert_true("runtime_native_3d_denoise_v2_special_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

    test_runtime_native_3d_denoise_set_gray(radiance, 0, 0.20f);
    test_runtime_native_3d_denoise_set_gray(radiance, 1, 0.30f);
    test_runtime_native_3d_denoise_set_gray(radiance, 2, 0.20f);
    test_runtime_native_3d_denoise_set_gray(radiance, 3, 0.60f);
    test_runtime_native_3d_denoise_set_gray(radiance, 4, 0.20f);
    features.transparencyBuffer[1] = 0.65f;
    features.reflectivityBuffer[3] = 1.0f;
    features.roughnessBuffer[3] = 0.0f;

    ok = RuntimeNative3DDenoise_ApplyForIntegrator(radiance,
                                                   5,
                                                   &features,
                                                   RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                   8,
                                                   NULL,
                                                   0,
                                                   &diagnostics);
    assert_true("runtime_native_3d_denoise_v2_special_apply_ok", ok);
    assert_true("runtime_native_3d_denoise_v2_transparent_preserved",
                radiance[1u * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] == 0.30f);
    assert_true("runtime_native_3d_denoise_v2_mirror_glossy_preserved",
                radiance[3u * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] == 0.60f);
    assert_true("runtime_native_3d_denoise_v2_special_diag_preserved",
                diagnostics.preservedTransparentPixelCount == 1 &&
                    diagnostics.preservedMirrorGlossyPixelCount == 1);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

static int test_runtime_native_3d_denoise_disney_v2_preserves_temporally_unstable_pixels(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    RuntimeNative3DDenoiseDiagnostics diagnostics = {0};
    float radiance[3 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    float temporal_activity[3] = {0.01f, 0.60f, 0.01f};
    bool ok = test_runtime_native_3d_denoise_setup_flat_features(&features, 3);
    assert_true("runtime_native_3d_denoise_v2_temporal_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

    test_runtime_native_3d_denoise_set_gray(radiance, 0, 0.20f);
    test_runtime_native_3d_denoise_set_gray(radiance, 1, 0.10f);
    test_runtime_native_3d_denoise_set_gray(radiance, 2, 0.20f);

    ok = RuntimeNative3DDenoise_ApplyForIntegrator(radiance,
                                                   3,
                                                   &features,
                                                   RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                   8,
                                                   temporal_activity,
                                                   3,
                                                   &diagnostics);
    assert_true("runtime_native_3d_denoise_v2_temporal_apply_ok", ok);
    assert_true("runtime_native_3d_denoise_v2_temporal_center_preserved",
                radiance[RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] == 0.10f);
    assert_true("runtime_native_3d_denoise_v2_temporal_diag_skip",
                diagnostics.skippedUnstableTemporalPixelCount == 1);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

int run_test_runtime_native_3d_denoise_tests(void) {
    int before = test_support_failures();

    test_runtime_native_3d_denoise_apply_policy();
    test_runtime_native_3d_denoise_respects_normal_breaks();
    test_runtime_native_3d_denoise_respects_depth_breaks();
    test_runtime_native_3d_denoise_disney_v2_blurs_stable_same_triangle();
    test_runtime_native_3d_denoise_disney_v2_blurs_rough_reflective_interiors();
    test_runtime_native_3d_denoise_disney_v2_rejects_clean_visual_edges();
    test_runtime_native_3d_denoise_disney_v2_requires_same_surface_identity();
    test_runtime_native_3d_denoise_disney_v2_preserves_special_materials();
    test_runtime_native_3d_denoise_disney_v2_preserves_temporally_unstable_pixels();
    return test_support_failures() - before;
}
