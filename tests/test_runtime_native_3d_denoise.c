#include <string.h>

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
    return 0;
}

static int test_runtime_native_3d_denoise_respects_normal_breaks(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    float radiance[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f
    };
    bool ok = RuntimeNative3DFeatureBuffer_Ensure(&features, 3, 1);
    assert_true("runtime_native_3d_denoise_normal_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

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
    assert_true("runtime_native_3d_denoise_normal_center_lifts", radiance[3] > 0.05f);
    assert_true("runtime_native_3d_denoise_normal_discontinuity_stays_dark", radiance[6] < 0.01f);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

static int test_runtime_native_3d_denoise_respects_depth_breaks(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    float radiance[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f
    };
    bool ok = RuntimeNative3DFeatureBuffer_Ensure(&features, 3, 1);
    assert_true("runtime_native_3d_denoise_depth_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

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
    assert_true("runtime_native_3d_denoise_depth_center_lifts", radiance[3] > 0.05f);
    assert_true("runtime_native_3d_denoise_depth_far_stays_dark", radiance[6] < 0.01f);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

int run_test_runtime_native_3d_denoise_tests(void) {
    test_runtime_native_3d_denoise_apply_policy();
    test_runtime_native_3d_denoise_respects_normal_breaks();
    test_runtime_native_3d_denoise_respects_depth_breaks();
    return test_support_failures();
}
