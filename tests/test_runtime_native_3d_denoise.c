#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_frame_denoise.h"
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

static int test_runtime_native_3d_denoise_disney_v2_blurs_across_coplanar_triangles(void) {
    RuntimeNative3DFeatureBuffer features = {0};
    RuntimeNative3DDenoiseDiagnostics diagnostics = {0};
    float radiance[3 * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] = {0};
    bool ok = test_runtime_native_3d_denoise_setup_flat_features(&features, 3);
    assert_true("runtime_native_3d_denoise_v2_coplanar_features_alloc", ok);
    if (!ok) {
        RuntimeNative3DFeatureBuffer_Free(&features);
        return 0;
    }

    test_runtime_native_3d_denoise_set_gray(radiance, 0, 0.20f);
    test_runtime_native_3d_denoise_set_gray(radiance, 1, 0.00f);
    test_runtime_native_3d_denoise_set_gray(radiance, 2, 0.20f);
    features.triangleIndexBuffer[0] = 8;
    features.triangleIndexBuffer[2] = 8;

    ok = RuntimeNative3DDenoise_ApplyForIntegrator(radiance,
                                                   3,
                                                   &features,
                                                   RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                   8,
                                                   NULL,
                                                   0,
                                                   &diagnostics);
    assert_true("runtime_native_3d_denoise_v2_coplanar_apply_ok", ok);
    assert_true("runtime_native_3d_denoise_v2_coplanar_center_blurs",
                radiance[RUNTIME_NATIVE_3D_RADIANCE_CHANNELS] > 0.02f);
    assert_true("runtime_native_3d_denoise_v2_coplanar_records_samples",
                diagnostics.stableInteriorSampleCount > 0);

    RuntimeNative3DFeatureBuffer_Free(&features);
    return 0;
}

static int test_runtime_native_3d_denoise_disney_v2_requires_same_object_identity(void) {
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
    features.sceneObjectIndexBuffer[0] = 4;
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

static bool test_runtime_native_3d_frame_denoise_setup_unit(
    RuntimeNative3DRenderUnit* unit,
    int start_x,
    int end_x,
    const float* source_radiance,
    bool guided) {
    const int width = end_x - start_x;
    if (!unit || width <= 0 || !source_radiance) return false;

    RuntimeNative3DRenderUnit_Init(unit);
    unit->integratorId = RAY_TRACING_3D_INTEGRATOR_DISNEY_V2;
    unit->startX = start_x;
    unit->startY = 0;
    unit->endX = end_x;
    unit->endY = 1;
    unit->width = width;
    unit->height = 1;
    unit->temporalFrames = 8;
    unit->committedSubpasses = 2 + (start_x % 7);
    unit->useDenoise = true;
    unit->featuresPrepared = true;
    unit->resolvedRadiance =
        (float*)calloc((size_t)width * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                       sizeof(*unit->resolvedRadiance));
    if (!unit->resolvedRadiance ||
        !RuntimeNative3DTemporalAccumulation_Ensure(&unit->accumulation, width, 1) ||
        !RuntimeNative3DFeatureBuffer_EnsureWithGuides(&unit->featureBuffer, width, 1,
            guided ? RUNTIME_NATIVE_3D_GUIDES_BORROWED_STATISTICS : RUNTIME_NATIVE_3D_GUIDES_NONE)) {
        return false;
    }

    RuntimeNative3DFeatureBuffer_BorrowSamplingStatistics(&unit->featureBuffer,
        unit->accumulation.rawM2Buffer, unit->accumulation.sampleCountBuffer);
    memset(unit->featureBuffer.hitMaskBuffer,
           1,
           (size_t)width * sizeof(*unit->featureBuffer.hitMaskBuffer));
    for (int x = 0; x < width; ++x) {
        const size_t local = (size_t)x;
        const size_t radiance_base =
            local * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
        const size_t normal_base = local * 3u;
        const float value = source_radiance[start_x + x];
        unit->accumulation.accumulationBuffer[radiance_base] = value;
        unit->accumulation.accumulationBuffer[radiance_base + 1u] = value;
        unit->accumulation.accumulationBuffer[radiance_base + 2u] = value;
        unit->accumulation.sampleCountBuffer[local] = 1u;
        unit->accumulation.activityBuffer[local] = 0.01f;
        unit->featureBuffer.normalBuffer[normal_base + 2u] = 1.0f;
        unit->featureBuffer.depthBuffer[local] = 1.0f;
        unit->featureBuffer.roughnessBuffer[local] = 1.0f;
        unit->featureBuffer.triangleIndexBuffer[local] = x;
        unit->featureBuffer.sceneObjectIndexBuffer[local] = 9;
        if (guided) {
            static const float moments[] = {0.24f, 0.40f, 0.60f, 0.84f};
            const int offset = (start_x + x) % 4;
            unit->committedSubpasses = 8;
            unit->accumulation.sampleCountBuffer[local] = (uint16_t)(4 + offset);
            unit->featureBuffer.materialGuideMaskBuffer[local] = 1;
            unit->featureBuffer.shadingNormalBuffer[normal_base + 2u] = 1;
            for (size_t c = 0; c < 3u; ++c) {
                unit->featureBuffer.albedoBuffer[normal_base + c] = 0.5f;
                unit->accumulation.rawM2Buffer[normal_base + c] = moments[offset];
            }
        }
    }
    return true;
}

static bool test_runtime_native_3d_frame_denoise_run_partition(
    int tile_size,
    float* output,
    bool guided) {
    enum { kWidth = 24 };
    static const float source[kWidth] = {
        0.20f, 0.21f, 0.19f, 0.20f, 0.22f, 0.18f,
        0.20f, 0.21f, 0.19f, 0.20f, 0.22f, 0.18f,
        0.20f, 0.21f, 0.19f, 0.20f, 0.22f, 0.18f,
        0.20f, 0.21f, 0.19f, 0.20f, 0.22f, 0.18f
    };
    RuntimeNative3DFrameDenoise frame_denoise = {0};
    RuntimeNative3DRenderUnit* units = NULL;
    const int unit_count = (kWidth + tile_size - 1) / tile_size;
    bool ok = tile_size > 0 && output;

    RuntimeNative3DFrameDenoise_Init(&frame_denoise);
    if (ok) {
        units = (RuntimeNative3DRenderUnit*)calloc((size_t)unit_count, sizeof(*units));
        ok = units != NULL;
    }
    for (int i = 0; ok && i < unit_count; ++i) {
        const int start_x = i * tile_size;
        const int end_x = start_x + tile_size < kWidth ? start_x + tile_size : kWidth;
        ok = test_runtime_native_3d_frame_denoise_setup_unit(&units[i],
                                                             start_x,
                                                             end_x,
                                                             source, guided);
    }
    if (ok) {
        ok = RuntimeNative3DFrameDenoise_PrepareWithGuides(&frame_denoise,
                                                 kWidth,
                                                 1,
                                                 RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,
                                                 8, guided);
    }
    for (int i = unit_count - 1; ok && i >= 0; --i) {
        ok = RuntimeNative3DFrameDenoise_GatherUnit(&frame_denoise, &units[i]);
    }
    if (ok) ok = RuntimeNative3DFrameDenoise_Apply(&frame_denoise, NULL);
    if (ok && guided) {
        assert_true("guided_frame_no_array_copies",
            !frame_denoise.featureBuffer.albedoBuffer && !frame_denoise.featureBuffer.shadingNormalBuffer &&
            !frame_denoise.featureBuffer.sampleCountBuffer && !frame_denoise.featureBuffer.varianceOfMeanBuffer &&
            RuntimeNative3DFeatureBuffer_AllocatedBytes(&frame_denoise.featureBuffer) == kWidth * 38u);
        for (int x = 0; x < kWidth; ++x) {
            size_t local;
            const RuntimeNative3DFeatureBuffer* f = RuntimeNative3DFeatureBuffer_GuideSource(
                &frame_denoise.featureBuffer, (size_t)x, &local);
            assert_true("guided_frame_source_borrowed", f != NULL);
            if (!f) continue;
            assert_close("guided_frame_material_transport", f->albedoBuffer[local * 3], 0.5, 1e-7);
            assert_close("guided_frame_variance_transport", RuntimeNative3DFeatureBuffer_VarianceAt(f,local), 0.02, 1e-7);
            assert_true("guided_frame_unequal_count_transport", f->sampleCountBuffer[local] == 4 + x % 4);
        }
    }
    if (ok) {
        for (int x = 0; x < kWidth; ++x) {
            output[x] = frame_denoise.radianceBuffer[
                (size_t)x * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS];
        }
    }

    for (int i = 0; i < unit_count; ++i) {
        RuntimeNative3DRenderUnit_Free(&units[i]);
    }
    free(units);
    RuntimeNative3DFrameDenoise_Free(&frame_denoise);
    return ok;
}

static int test_runtime_native_3d_frame_denoise_tile_size_invariance(void) {
    enum { kWidth = 24 };
    float tile_8[kWidth] = {0};
    float tile_16[kWidth] = {0};
    float tile_32[kWidth] = {0};
    bool ok_8 = test_runtime_native_3d_frame_denoise_run_partition(8, tile_8, false);
    bool ok_16 = test_runtime_native_3d_frame_denoise_run_partition(16, tile_16, false);
    bool ok_32 = test_runtime_native_3d_frame_denoise_run_partition(32, tile_32, false);

    assert_true("runtime_native_3d_frame_denoise_tile_8_ok", ok_8);
    assert_true("runtime_native_3d_frame_denoise_tile_16_ok", ok_16);
    assert_true("runtime_native_3d_frame_denoise_tile_32_ok", ok_32);
    if (!ok_8 || !ok_16 || !ok_32) return 0;

    for (int x = 0; x < kWidth; ++x) {
        assert_close("runtime_native_3d_frame_denoise_8_16_invariant",
                     tile_8[x],
                     tile_16[x],
                     1e-7);
        assert_close("runtime_native_3d_frame_denoise_16_32_invariant",
                     tile_16[x],
                     tile_32[x],
                     1e-7);
    }
    assert_true("runtime_native_3d_frame_denoise_filters_across_tile_8_boundary",
                tile_8[7] != 0.21f && tile_8[8] != 0.19f);
    assert_true("runtime_native_3d_frame_denoise_filters_across_tile_16_boundary",
                tile_16[15] != 0.20f && tile_16[16] != 0.22f);
    return 0;
}


/* Weak texture must survive even when measured lighting noise requests filtering.
 * The flat case keeps the same noise realization and proves useful reduction. */
static void test_runtime_native_3d_denoise_material_uncertainty(void) {
    enum { width = 128, height = 16, pixels = width * height };
    RuntimeNative3DFeatureBuffer f = {0};
    float rgb[pixels * 4], m2[pixels * 3];
    uint16_t counts[pixels];
    assert_true("guided_allocate", RuntimeNative3DFeatureBuffer_EnsureWithGuides(&f, width, height, RUNTIME_NATIVE_3D_GUIDES_OWNED_STATISTICS));
    if (!f.normalBuffer) return;
    for (int mode = 0; mode < 4; ++mode) {
        unsigned int rng = 1907;
        double before = 0, after = 0, projection = 0, pattern_energy = 0;
        for (int i = 0; i < pixels; ++i) {
            float pattern = mode == 1 ? 0 : ((i / 2) % 2 ? 0.04f : -0.04f);
            float truth = 0.5f + pattern;
            rng = 1664525u * rng + 1013904223u;
            float noise = mode < 2 ? (((rng >> 8) & 65535u) / 65535.0f - 0.5f) * 0.16f : 0;
            f.hitMaskBuffer[i] = f.materialGuideMaskBuffer[i] = 1;
            f.normalBuffer[i * 3 + 2] = f.shadingNormalBuffer[i * 3 + 2] = 1;
            f.depthBuffer[i] = 1; f.roughnessBuffer[i] = 0.7f;
            f.reflectivityBuffer[i] = 0.04f; f.triangleIndexBuffer[i] = i;
            f.sceneObjectIndexBuffer[i] = 0;
            counts[i] = mode == 3 ? 1 : 16;
            for (int c = 0; c < 3; ++c) {
                f.albedoBuffer[i * 3 + c] = truth;
                m2[i * 3 + c] = mode < 2 ? 0.0021333333f * 16 * 15 : 0;
                rgb[i * 4 + c] = truth + noise;
            }
            rgb[i * 4 + 3] = 0.2f;
            if (i % width >= 2 && i % width < width - 2 && i / width >= 2 && i / width < height - 2) before += noise * noise;
        }
        RuntimeNative3DFeatureBuffer_RecordSamplingStatistics(&f, m2, counts);
        assert_true("guided_apply", RuntimeNative3DDenoise_ApplyForIntegrator(
            rgb, width, &f, RAY_TRACING_3D_INTEGRATOR_DISNEY_V2, 16, NULL, 0, NULL));
        for (int i = 0; i < pixels; ++i) {
            if (i % width < 2 || i % width >= width - 2 || i / width < 2 || i / width >= height - 2) continue;
            float pattern = mode == 1 ? 0 : ((i / 2) % 2 ? 0.04f : -0.04f);
            double error = rgb[i * 4] - (0.5 + pattern);
            after += error * error;
            projection += pattern * (rgb[i * 4] - 0.5);
            pattern_energy += pattern * pattern;
            assert_true("guided_aux_channel_preserved", fabs(rgb[i * 4 + 3] - 0.2) < 1e-7);
        }
        if (mode == 1) assert_true("guided_flat_noise_rmse_halved", after < before * 0.25);
        else if (mode == 0) assert_true("guided_noisy_texture_contrast", projection / pattern_energy > 0.90);
        else assert_true("guided_clean_and_insufficient_passthrough", after < 1e-12);
    }
    for (int i = 0; i < pixels; ++i) counts[i] = 4;
    m2[0] = NAN;
    RuntimeNative3DFeatureBuffer_RecordSamplingStatistics(&f, m2, counts);
    assert_true("guided_nonfinite_variance_unknown", isinf(f.varianceOfMeanBuffer[0]));
    counts[0] = 8; m2[0] = 56;
    RuntimeNative3DFeatureBuffer_RecordSamplingStatistics(&f, m2, counts);
    assert_true("guided_variance_of_mean_count_contract", fabs(f.varianceOfMeanBuffer[0] - 1) < 1e-7);
    RuntimeNative3DFeatureBuffer_Free(&f);
}

/* Compatible repeated grain outside 5x5 may contribute only at high uncertainty. */
static void test_runtime_native_3d_denoise_sparse_material_neighbors(void) {
    RuntimeNative3DFeatureBuffer f = {0};
    float rgb[9 * 4];
    assert_true("sparse_material_allocate", RuntimeNative3DFeatureBuffer_EnsureWithGuides(&f, 9, 1, RUNTIME_NATIVE_3D_GUIDES_OWNED_STATISTICS));
    if (!f.normalBuffer) return;
    for (int mode = 0; mode < 5; ++mode) {
        RuntimeNative3DFeatureBuffer_Clear(&f);
        for (int x = 0; x < 9; ++x) {
            f.hitMaskBuffer[x] = f.materialGuideMaskBuffer[x] = 1;
            f.sampleCountBuffer[x] = 8;
            f.normalBuffer[x * 3 + 2] = f.shadingNormalBuffer[x * 3 + 2] = 1;
            f.depthBuffer[x] = 1;
            f.roughnessBuffer[x] = 0.7f;
            f.reflectivityBuffer[x] = 0.04f;
            f.triangleIndexBuffer[x] = x;
            f.sceneObjectIndexBuffer[x] = mode == 1 && x != 4 ? 1 : 0;
            const bool compatible = x == 0 || x == 4 || x == 8;
            for (int c = 0; c < 3; ++c) {
                f.albedoBuffer[x * 3 + c] = compatible ? 0.5f : 0.9f;
                f.varianceOfMeanBuffer[x] = mode == 4 ? 0.000025f : 0.01f;
                rgb[x * 4 + c] = x == 4 ? 0.6f : 0.5f;
            }
            rgb[x * 4 + 3] = 0.2f;
        }
        if (mode == 2) f.depthBuffer[0] = f.depthBuffer[8] = 2;
        if (mode == 3) {
            for (int c = 0; c < 3; ++c)
                f.albedoBuffer[c] = f.albedoBuffer[8 * 3 + c] = 0.53f;
        }
        assert_true("sparse_material_apply", RuntimeNative3DDenoise_ApplyForIntegrator(
            rgb, 9, &f, RAY_TRACING_3D_INTEGRATOR_DISNEY_V2, 8, NULL, 0, NULL));
        if (mode == 0) assert_true("sparse_material_noise_reduced", rgb[4 * 4] < 0.58f);
        else assert_close("sparse_material_preserves_incompatible_or_low_uncertainty",
                          rgb[4 * 4], 0.6, 1e-5);
        assert_close("sparse_material_aux_preserved", rgb[4 * 4 + 3], 0.2, 1e-7);
    }
    RuntimeNative3DFeatureBuffer_Free(&f);
}

static void test_runtime_native_3d_frame_denoise_guided_partition(void) {
    float a[24], b[24], c[24];
    bool ok = test_runtime_native_3d_frame_denoise_run_partition(8, a, true) &&
        test_runtime_native_3d_frame_denoise_run_partition(16, b, true) &&
        test_runtime_native_3d_frame_denoise_run_partition(32, c, true);
    assert_true("guided_partition_runs", ok);
    if (ok) for (int x = 0; x < 24; ++x) {
        assert_close("guided_partition_8_16", a[x], b[x], 1e-7);
        assert_close("guided_partition_16_32", b[x], c[x], 1e-7);
    }
}

static void test_guidance_render_modes(void) {
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeScene3D_Init(&frame.scene);
    frame.scene.hasCamera = true;
    frame.scene.camera.position = vec3(0,-4,1.5);
    frame.scene.camera.zoom = 1; frame.scene.camera.nearPlane = 0.1;
    frame.width = frame.height = 4; frame.valid = true;
    assert_true("mode_projector", RuntimeCameraProjector3D_Build(&frame.scene.camera,4,4,&frame.projector));
    RuntimeNative3DRenderUnit unit = {0};
    const RayTracing3DIntegratorId ids[] = {RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,RAY_TRACING_3D_INTEGRATOR_DISNEY,RAY_TRACING_3D_INTEGRATOR_MATERIAL,RAY_TRACING_3D_INTEGRATOR_DISNEY_V2};
    for(int mode=0;mode<4;++mode) {
        assert_true("mode_setup", RuntimeNative3DRenderUnit_Setup(&unit,ids[mode],&frame,0,0,4,4,NULL,8,mode!=0));
        assert_true("mode_render", RuntimeNative3DRenderUnit_RenderSubpass(&unit,0,NULL));
        assert_true("mode_guides_only_v2_denoised", (unit.featureBuffer.albedoBuffer != NULL)==(mode==3));
        assert_true("mode_owned_bytes", RuntimeNative3DFeatureBuffer_AllocatedBytes(&unit.featureBuffer)==16u*(mode==3?63u:38u));
    }
    RuntimeNative3DRenderUnit_Free(&unit);
    RuntimeNative3DPreparedFrame_Free(&frame);
}

static void test_guidance_storage_lifecycle(void) {
    RuntimeNative3DFeatureBuffer f = {0};
    float moments[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9}; uint16_t counts[3] = {4, 6, 8};
    assert_true("legacy_allocate", RuntimeNative3DFeatureBuffer_Ensure(&f, 3, 1));
    assert_true("legacy_bytes_38", RuntimeNative3DFeatureBuffer_AllocatedBytes(&f) == 114);
    assert_true("legacy_guides_null", !f.albedoBuffer && !f.sampleCountBuffer);
    float* base = f.normalBuffer;
    assert_true("owned_allocate", RuntimeNative3DFeatureBuffer_EnsureWithGuides(&f, 3, 1, RUNTIME_NATIVE_3D_GUIDES_OWNED_STATISTICS));
    assert_true("owned_bytes_69", RuntimeNative3DFeatureBuffer_AllocatedBytes(&f) == 207);
    RuntimeNative3DFeatureBuffer_RecordSamplingStatistics(&f, moments, counts);
    float expected[3]; memcpy(expected, f.varianceOfMeanBuffer, sizeof(expected));
    assert_true("borrowed_allocate", RuntimeNative3DFeatureBuffer_EnsureWithGuides(&f, 3, 1, RUNTIME_NATIVE_3D_GUIDES_BORROWED_STATISTICS));
    RuntimeNative3DFeatureBuffer_BorrowSamplingStatistics(&f, moments, counts);
    assert_true("borrowed_bytes_63", RuntimeNative3DFeatureBuffer_AllocatedBytes(&f) == 189 && !f.varianceOfMeanBuffer && f.sampleCountBuffer == counts);
    RuntimeNative3DFeatureBuffer_Clear(&f);
    assert_true("borrowed_clear_preserves_counts", counts[0] == 4 && counts[1] == 6 && counts[2] == 8);
    for (size_t i=0; i<3; ++i) assert_true("borrowed_variance_exact", RuntimeNative3DFeatureBuffer_VarianceAt(&f,i) == expected[i]);
    moments[1]=NAN; assert_true("nan_invalid", isinf(RuntimeNative3DFeatureBuffer_VarianceAt(&f,0)));
    moments[1]=-1; assert_true("negative_invalid", isinf(RuntimeNative3DFeatureBuffer_VarianceAt(&f,0)));
    moments[1]=INFINITY; assert_true("infinite_invalid", isinf(RuntimeNative3DFeatureBuffer_VarianceAt(&f,0)));
    counts[0]=1; assert_true("insufficient_invalid", isinf(RuntimeNative3DFeatureBuffer_VarianceAt(&f,0)));
    assert_true("disable_releases", RuntimeNative3DFeatureBuffer_Ensure(&f,3,1));
    assert_true("disable_preserves_base", f.normalBuffer==base && !f.albedoBuffer && !f.sampleCountBuffer);
    RuntimeNative3DFeatureBuffer_Free(&f);
    assert_true("borrowed_storage_not_freed", counts[1]==6 && moments[3]==4);
    RuntimeNative3DFrameDenoise frame={0};
    assert_true("legacy_frame_prepare", RuntimeNative3DFrameDenoise_PrepareWithGuides(&frame,3,1,RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,8,false));
    assert_true("legacy_frame_no_guides", RuntimeNative3DFeatureBuffer_AllocatedBytes(&frame.featureBuffer)==114 && !frame.featureBuffer.albedoBuffer);
    RuntimeNative3DFrameDenoise_Free(&frame);
    RuntimeNative3DRenderUnit unit={0};
    RuntimeNative3DRenderUnit_Init(&unit);
    assert_true("cache_guides_allocate", RuntimeNative3DFeatureBuffer_EnsureWithGuides(&unit.featureBuffer,3,1,RUNTIME_NATIVE_3D_GUIDES_BORROWED_STATISTICS));
    RuntimeNative3DFeatureBuffer_BorrowSamplingStatistics(&unit.featureBuffer,moments,counts);
    RuntimeNative3DRenderUnit_ReturnReusable(&unit);
    RuntimeNative3DRenderUnit_TakeReusable(&unit);
    assert_true("cache_drops_guides", !unit.featureBuffer.albedoBuffer && !unit.featureBuffer.sampleCountBuffer && RuntimeNative3DFeatureBuffer_AllocatedBytes(&unit.featureBuffer)==114);
    RuntimeNative3DRenderUnit_Free(&unit);
}

/* Exact float comparison with a contiguous reference, including horizontal and
 * vertical seams, irregular edge tiles, reverse completion and invalid guides. */
static void test_runtime_native_3d_shared_guides_2d(void) {
    enum { W = 23, H = 9, N = W * H };
    const int sizes[] = {3, 8, 32};
    RuntimeNative3DFeatureBuffer reference = {0};
    float raw[N * 4], expected[N * 4], activity[N], m2[N * 3];
    uint16_t counts[N];
    assert_true("shared_reference_allocate", RuntimeNative3DFeatureBuffer_EnsureWithGuides(
        &reference, W, H, RUNTIME_NATIVE_3D_GUIDES_OWNED_STATISTICS));
    if (!reference.normalBuffer) return;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        size_t i = (size_t)y * W + x;
        counts[i] = (uint16_t)(4 + (x + y) % 5);
        activity[i] = 0;
        reference.hitMaskBuffer[i] = reference.materialGuideMaskBuffer[i] = 1;
        reference.sceneObjectIndexBuffer[i] = x < 12 ? 1 : 2;
        reference.triangleIndexBuffer[i] = 0;
        reference.depthBuffer[i] = 2;
        reference.roughnessBuffer[i] = 0.8f;
        reference.normalBuffer[i * 3 + 2] = reference.shadingNormalBuffer[i * 3 + 2] = 1;
        for (size_t c = 0; c < 3; ++c) {
            reference.albedoBuffer[i * 3 + c] = 0.5f + 0.002f * ((x + y) % 3);
            m2[i * 3 + c] = 0.02f * counts[i] * (counts[i] - 1);
            raw[i * 4 + c] = 0.2f + 0.02f * ((x * 7 + y * 3) % 5 - 2);
        }
        raw[i * 4 + 3] = 0.4f;
    }
    m2[5 * 3] = NAN; counts[17] = 2; reference.materialGuideMaskBuffer[48] = 0;
    RuntimeNative3DFeatureBuffer_RecordSamplingStatistics(&reference, m2, counts);
    memcpy(expected, raw, sizeof raw);
    assert_true("shared_reference_filter", RuntimeNative3DDenoise_ApplyForIntegrator(
        expected, W, &reference, RAY_TRACING_3D_INTEGRATOR_DISNEY_V2, 8, activity, W, NULL));
    for (size_t mode = 0; mode < sizeof sizes / sizeof sizes[0]; ++mode) {
        int size = sizes[mode], nx = (W + size - 1) / size, ny = (H + size - 1) / size;
        int total = nx * ny;
        RuntimeNative3DRenderUnit* units = calloc((size_t)total, sizeof(*units));
        RuntimeNative3DFrameDenoise frame = {0};
        bool ok = units && RuntimeNative3DFrameDenoise_PrepareWithGuides(
            &frame, W, H, RAY_TRACING_3D_INTEGRATOR_DISNEY_V2, 8, true);
        for (int j = 0; ok && j < total; ++j) {
            RuntimeNative3DRenderUnit* u = &units[j];
            u->startX = (j % nx) * size; u->startY = (j / nx) * size;
            u->endX = u->startX + size < W ? u->startX + size : W;
            u->endY = u->startY + size < H ? u->startY + size : H;
            u->width = u->endX - u->startX; u->height = u->endY - u->startY;
            u->useDenoise = true; u->temporalFrames = u->committedSubpasses = 8;
            u->integratorId = RAY_TRACING_3D_INTEGRATOR_DISNEY_V2;
            size_t n = (size_t)u->width * u->height;
            u->resolvedRadiance = calloc(n * 4, sizeof(float));
            ok = u->resolvedRadiance && RuntimeNative3DTemporalAccumulation_Ensure(
                &u->accumulation, u->width, u->height) &&
                RuntimeNative3DFeatureBuffer_EnsureWithGuides(&u->featureBuffer,
                    u->width, u->height, RUNTIME_NATIVE_3D_GUIDES_BORROWED_STATISTICS);
            if (!ok) break;
            RuntimeNative3DFeatureBuffer_BorrowSamplingStatistics(&u->featureBuffer,
                u->accumulation.rawM2Buffer,u->accumulation.sampleCountBuffer);
            for (int y = 0; y < u->height; ++y) for (int x = 0; x < u->width; ++x) {
                size_t local = (size_t)y * u->width + x;
                size_t global = (size_t)(u->startY + y) * W + u->startX + x;
#define COPY(field, channels) memcpy(u->featureBuffer.field + local * channels, reference.field + global * channels, channels * sizeof(*reference.field))
                COPY(normalBuffer,3); COPY(albedoBuffer,3); COPY(shadingNormalBuffer,3);
                COPY(depthBuffer,1); COPY(reflectivityBuffer,1); COPY(roughnessBuffer,1);
                COPY(transparencyBuffer,1); COPY(hitMaskBuffer,1); COPY(materialGuideMaskBuffer,1);
                COPY(directLightVisibilityOutcomeBuffer,1); COPY(triangleIndexBuffer,1); COPY(sceneObjectIndexBuffer,1);
#undef COPY
                u->accumulation.sampleCountBuffer[local] = counts[global];
                memcpy(u->accumulation.rawM2Buffer + local * 3, m2 + global * 3, 3 * sizeof(float));
                /* Resolve divides the sum by the sample count. */
                for (int c = 0; c < 4; ++c)
                    u->accumulation.accumulationBuffer[local * 4 + c] = raw[global * 4 + c] * counts[global];
            }
        }
        if (ok && total > 1) {
            RuntimeNative3DFrameDenoise sparse = {0};
            bool sparse_ok = RuntimeNative3DFrameDenoise_PrepareWithGuides(
                &sparse,W,H,RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,8,true);
            sparse.allowSparseGuides = true;
            sparse_ok = sparse_ok && RuntimeNative3DFrameDenoise_GatherUnit(&sparse,&units[total-1]);
            assert_true("shared_sparse_apply", sparse_ok && RuntimeNative3DFrameDenoise_Apply(&sparse,NULL));
            size_t local = 0;
            assert_true("shared_empty_row_has_no_guide", !RuntimeNative3DFeatureBuffer_GuideSource(
                &sparse.featureBuffer,0,&local));
            RuntimeNative3DFrameDenoise_Free(&sparse);
            assert_true("shared_view_free_retains_owner", units[total-1].featureBuffer.albedoBuffer != NULL);
        }
        for (int j = total - 1; ok && j > 0; --j)
            ok = RuntimeNative3DFrameDenoise_GatherUnit(&frame, &units[j]);
        if (ok) {
            assert_true("shared_incomplete_frame_refused", !RuntimeNative3DFrameDenoise_Apply(&frame,NULL));
            ok = RuntimeNative3DFrameDenoise_GatherUnit(&frame,&units[0]);
            size_t saved = frame.guideSpanCount;
            ok = ok && RuntimeNative3DFrameDenoise_GatherUnit(&frame,&units[0]);
            assert_true("shared_overlapping_frame_refused", !RuntimeNative3DFrameDenoise_Apply(&frame,NULL));
            /* Apply sorts, so rebuild the descriptors after intentional overlap. */
            frame.guideSpanCount = 0;
            for (int j = total - 1; ok && j >= 0; --j)
                ok = RuntimeNative3DFrameDenoise_GatherUnit(&frame,&units[j]);
            assert_true("shared_descriptor_count_restored", frame.guideSpanCount == saved);
        }
        /* Compare against exactly the gathered radiance (sum/count rounding). */
        float gathered_expected[N * 4];
        if (ok) {
            memcpy(gathered_expected, frame.radianceBuffer, sizeof gathered_expected);
            ok = RuntimeNative3DDenoise_ApplyForIntegrator(gathered_expected, W, &reference,
                RAY_TRACING_3D_INTEGRATOR_DISNEY_V2,8,activity,W,NULL);
        }
        assert_true("shared_2d_apply", ok && RuntimeNative3DFrameDenoise_Apply(&frame,NULL));
        if (ok && frame.applied) assert_true("shared_2d_bit_identical",
            memcmp(gathered_expected,frame.radianceBuffer,sizeof gathered_expected)==0);
        assert_true("shared_2d_no_frame_guides", !frame.featureBuffer.albedoBuffer &&
            !frame.featureBuffer.shadingNormalBuffer && !frame.featureBuffer.varianceOfMeanBuffer &&
            !frame.featureBuffer.sampleCountBuffer && !frame.featureBuffer.materialGuideMaskBuffer);
        RuntimeNative3DFrameDenoise_Free(&frame);
        for (int j = 0; units && j < total; ++j) RuntimeNative3DRenderUnit_Free(&units[j]);
        free(units);
    }
    RuntimeNative3DFeatureBuffer_Free(&reference);
}

int run_test_runtime_native_3d_denoise_tests(void) {
    int before = test_support_failures();

    test_runtime_native_3d_shared_guides_2d();
    test_runtime_native_3d_frame_denoise_guided_partition();
    test_guidance_render_modes();
    test_guidance_storage_lifecycle();
    test_runtime_native_3d_denoise_material_uncertainty();
    test_runtime_native_3d_denoise_sparse_material_neighbors();
    test_runtime_native_3d_denoise_apply_policy();
    test_runtime_native_3d_denoise_respects_normal_breaks();
    test_runtime_native_3d_denoise_respects_depth_breaks();
    test_runtime_native_3d_denoise_disney_v2_blurs_stable_same_triangle();
    test_runtime_native_3d_denoise_disney_v2_blurs_rough_reflective_interiors();
    test_runtime_native_3d_denoise_disney_v2_rejects_clean_visual_edges();
    test_runtime_native_3d_denoise_disney_v2_blurs_across_coplanar_triangles();
    test_runtime_native_3d_denoise_disney_v2_requires_same_object_identity();
    test_runtime_native_3d_denoise_disney_v2_preserves_special_materials();
    test_runtime_native_3d_denoise_disney_v2_preserves_temporally_unstable_pixels();
    test_runtime_native_3d_frame_denoise_tile_size_invariance();
    return test_support_failures() - before;
}
