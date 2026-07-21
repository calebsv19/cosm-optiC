#include "render/runtime_native_3d_denoise.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "render/runtime_native_3d_render.h"

enum {
    RUNTIME_NATIVE_3D_DENOISE_RADIUS = 2
};

static const float kRuntimeNative3DDisneyV2DenoiseNormalDot = 0.985f;
static const float kRuntimeNative3DDisneyV2DenoiseDepthDelta = 0.02f;
static const float kRuntimeNative3DDisneyV2DenoiseTransparencyPreserve = 0.02f;
static const float kRuntimeNative3DDisneyV2DenoiseMirrorReflectivityPreserve = 0.70f;
static const float kRuntimeNative3DDisneyV2DenoiseMirrorRoughnessPreserve = 0.20f;
static const float kRuntimeNative3DDisneyV2DenoiseGlossReflectivityPreserve = 0.20f;
static const float kRuntimeNative3DDisneyV2DenoiseGlossRoughnessPreserve = 0.32f;
static const float kRuntimeNative3DDisneyV2DenoiseSharpGlossReflectivityPreserve = 0.05f;
static const float kRuntimeNative3DDisneyV2DenoiseSharpGlossRoughnessPreserve = 0.08f;
static const float kRuntimeNative3DDisneyV2DenoiseTemporalActivityLimit = 0.35f;

void RuntimeNative3DDenoiseDiagnostics_Reset(RuntimeNative3DDenoiseDiagnostics* diagnostics) {
    if (!diagnostics) return;
    memset(diagnostics, 0, sizeof(*diagnostics));
}

void RuntimeNative3DDenoiseDiagnostics_Accumulate(
    RuntimeNative3DDenoiseDiagnostics* dst,
    const RuntimeNative3DDenoiseDiagnostics* src) {
    if (!dst || !src) return;
    if (src->temporalFrameCount > dst->temporalFrameCount) {
        dst->temporalFrameCount = src->temporalFrameCount;
    }
    dst->rawPixelCount += src->rawPixelCount;
    dst->reconstructedPixelCount += src->reconstructedPixelCount;
    dst->stableInteriorSampleCount += src->stableInteriorSampleCount;
    dst->rejectedEdgeSampleCount += src->rejectedEdgeSampleCount;
    dst->preservedTransparentPixelCount += src->preservedTransparentPixelCount;
    dst->preservedMirrorGlossyPixelCount += src->preservedMirrorGlossyPixelCount;
    dst->skippedUnstableTemporalPixelCount += src->skippedUnstableTemporalPixelCount;
    dst->skippedInvalidSurfacePixelCount += src->skippedInvalidSurfacePixelCount;
    dst->rawRadianceLumaTotal += src->rawRadianceLumaTotal;
    dst->reconstructedRadianceLumaTotal += src->reconstructedRadianceLumaTotal;
}

static float runtime_native_3d_denoise_spatial_weight(int dx, int dy) {
    const float sigma = 1.25f;
    const float distance_sq = (float)(dx * dx + dy * dy);
    const float denom = 2.0f * sigma * sigma;
    return expf(-distance_sq / denom);
}

static float runtime_native_3d_denoise_depth_relative_delta(float center_depth,
                                                            float sample_depth) {
    const float denom = fmaxf(fmaxf(center_depth, sample_depth), 1e-3f);
    return fabsf(sample_depth - center_depth) / denom;
}

static float runtime_native_3d_denoise_luma(const float* radiance_buffer, size_t base) {
    return 0.2126f * radiance_buffer[base] +
           0.7152f * radiance_buffer[base + 1u] +
           0.0722f * radiance_buffer[base + 2u];
}

static double runtime_native_3d_denoise_sum_luma(const float* radiance_buffer,
                                                 int radiance_stride,
                                                 int width,
                                                 int height) {
    double total = 0.0;
    if (!radiance_buffer || radiance_stride < width || width <= 0 || height <= 0) {
        return 0.0;
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t base = ((size_t)y * (size_t)radiance_stride + (size_t)x) *
                                (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            total += (double)runtime_native_3d_denoise_luma(radiance_buffer, base);
        }
    }
    return total;
}

static void runtime_native_3d_denoise_copy_pixel(float* dst,
                                                 const float* src,
                                                 size_t radiance_base) {
    dst[radiance_base] = src[radiance_base];
    dst[radiance_base + 1u] = src[radiance_base + 1u];
    dst[radiance_base + 2u] = src[radiance_base + 2u];
    dst[radiance_base + RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL] =
        src[radiance_base + RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL];
}

static bool runtime_native_3d_denoise_validate_base(
    float* radiance_buffer,
    int radiance_stride,
    const RuntimeNative3DFeatureBuffer* features,
    size_t* out_filtered_count) {
    if (out_filtered_count) *out_filtered_count = 0u;
    if (!radiance_buffer || radiance_stride <= 0 || !features || !features->normalBuffer ||
        !features->depthBuffer || !features->hitMaskBuffer || features->width <= 0 ||
        features->height <= 0 || radiance_stride < features->width) {
        return false;
    }
    if (out_filtered_count) {
        *out_filtered_count = (size_t)features->height * (size_t)radiance_stride *
                              (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
    }
    return true;
}

static bool runtime_native_3d_denoise_disney_v2_features_valid(
    const RuntimeNative3DFeatureBuffer* features) {
    return features &&
           features->normalBuffer &&
           features->depthBuffer &&
           features->reflectivityBuffer &&
           features->roughnessBuffer &&
           features->transparencyBuffer &&
           features->hitMaskBuffer &&
           features->triangleIndexBuffer &&
           features->sceneObjectIndexBuffer &&
           features->width > 0 &&
           features->height > 0;
}

static bool runtime_native_3d_denoise_disney_v2_preserves_transparent(
    const RuntimeNative3DFeatureBuffer* features,
    size_t index) {
    return features->transparencyBuffer[index] > kRuntimeNative3DDisneyV2DenoiseTransparencyPreserve;
}

static bool runtime_native_3d_denoise_disney_v2_preserves_mirror_glossy(
    const RuntimeNative3DFeatureBuffer* features,
    size_t index) {
    const float reflectivity = fmaxf(features->reflectivityBuffer[index], 0.0f);
    const float roughness = fmaxf(features->roughnessBuffer[index], 0.0f);
    return (reflectivity >= kRuntimeNative3DDisneyV2DenoiseMirrorReflectivityPreserve &&
            roughness <= kRuntimeNative3DDisneyV2DenoiseMirrorRoughnessPreserve) ||
           (reflectivity >= kRuntimeNative3DDisneyV2DenoiseGlossReflectivityPreserve &&
            roughness <= kRuntimeNative3DDisneyV2DenoiseGlossRoughnessPreserve) ||
           (reflectivity >= kRuntimeNative3DDisneyV2DenoiseSharpGlossReflectivityPreserve &&
            roughness <= kRuntimeNative3DDisneyV2DenoiseSharpGlossRoughnessPreserve);
}

static bool runtime_native_3d_denoise_disney_v2_rejects_visual_edge(
    const float* radiance_buffer,
    size_t center_base,
    size_t sample_base) {
    const float center_luma = runtime_native_3d_denoise_luma(radiance_buffer, center_base);
    const float sample_luma = runtime_native_3d_denoise_luma(radiance_buffer, sample_base);
    const float luma_delta = fabsf(sample_luma - center_luma);
    const float luma_scale = fmaxf(fmaxf(center_luma, sample_luma), 0.1f);
    const float max_channel_delta = fmaxf(
        fabsf(radiance_buffer[sample_base] - radiance_buffer[center_base]),
        fmaxf(fabsf(radiance_buffer[sample_base + 1u] - radiance_buffer[center_base + 1u]),
              fabsf(radiance_buffer[sample_base + 2u] - radiance_buffer[center_base + 2u])));
    return luma_delta > fmaxf(0.22f, 0.45f * luma_scale) || max_channel_delta > 0.40f;
}

bool RuntimeNative3DDenoise_ShouldApply(RayTracing3DIntegratorId integrator_id,
                                        int temporal_frames,
                                        bool denoise_enabled) {
    return denoise_enabled &&
           (integrator_id == RAY_TRACING_3D_INTEGRATOR_DISNEY ||
            integrator_id == RAY_TRACING_3D_INTEGRATOR_DISNEY_V2) &&
           temporal_frames > 1;
}

bool RuntimeNative3DDenoise_Apply(float* radiance_buffer,
                                  int radiance_stride,
                                  const RuntimeNative3DFeatureBuffer* features) {
    float* filtered = NULL;
    size_t filtered_count = 0;
    if (!runtime_native_3d_denoise_validate_base(radiance_buffer,
                                                 radiance_stride,
                                                 features,
                                                 &filtered_count)) {
        return false;
    }

    filtered = (float*)calloc(filtered_count, sizeof(*filtered));
    if (!filtered) return false;

    for (int y = 0; y < features->height; ++y) {
        for (int x = 0; x < features->width; ++x) {
            const size_t center_index = (size_t)y * (size_t)features->width + (size_t)x;
            const size_t center_normal_base = center_index * 3u;
            const size_t center_radiance_base =
                ((size_t)y * (size_t)radiance_stride + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            float total_weight = 0.0f;
            float accum_r = 0.0f;
            float accum_g = 0.0f;
            float accum_b = 0.0f;

            if (!features->hitMaskBuffer[center_index]) {
                runtime_native_3d_denoise_copy_pixel(filtered, radiance_buffer, center_radiance_base);
                continue;
            }

            {
                const float center_depth = features->depthBuffer[center_index];
                const float center_nx = features->normalBuffer[center_normal_base];
                const float center_ny = features->normalBuffer[center_normal_base + 1u];
                const float center_nz = features->normalBuffer[center_normal_base + 2u];

                for (int dy = -RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                     dy <= RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                     ++dy) {
                    const int ny = y + dy;
                    if (ny < 0 || ny >= features->height) continue;
                    for (int dx = -RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                         dx <= RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                         ++dx) {
                        const int nx = x + dx;
                        size_t sample_index = 0u;
                        size_t sample_normal_base = 0u;
                        size_t sample_radiance_base = 0u;
                        float ndot = 0.0f;
                        float depth_delta = 0.0f;
                        float weight = 0.0f;
                        if (nx < 0 || nx >= features->width) continue;
                        sample_index = (size_t)ny * (size_t)features->width + (size_t)nx;
                        sample_normal_base = sample_index * 3u;
                        sample_radiance_base =
                            ((size_t)ny * (size_t)radiance_stride + (size_t)nx) *
                            (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
                        if (!features->hitMaskBuffer[sample_index]) continue;
                        ndot = center_nx * features->normalBuffer[sample_normal_base] +
                               center_ny * features->normalBuffer[sample_normal_base + 1u] +
                               center_nz * features->normalBuffer[sample_normal_base + 2u];
                        if (ndot < 0.9f) continue;
                        depth_delta = runtime_native_3d_denoise_depth_relative_delta(
                            center_depth,
                            features->depthBuffer[sample_index]);
                        if (depth_delta > 0.08f) continue;
                        weight = runtime_native_3d_denoise_spatial_weight(dx, dy) *
                                 powf(fminf(ndot, 1.0f), 12.0f) *
                                 expf(-(depth_delta * depth_delta) / (2.0f * 0.03f * 0.03f));
                        accum_r += radiance_buffer[sample_radiance_base] * weight;
                        accum_g += radiance_buffer[sample_radiance_base + 1u] * weight;
                        accum_b += radiance_buffer[sample_radiance_base + 2u] * weight;
                        total_weight += weight;
                    }
                }
            }

            if (total_weight > 1e-6f) {
                filtered[center_radiance_base] = accum_r / total_weight;
                filtered[center_radiance_base + 1u] = accum_g / total_weight;
                filtered[center_radiance_base + 2u] = accum_b / total_weight;
                filtered[center_radiance_base + RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL] =
                    radiance_buffer[center_radiance_base +
                                    RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL];
            } else {
                runtime_native_3d_denoise_copy_pixel(filtered, radiance_buffer, center_radiance_base);
            }
        }
    }

    memcpy(radiance_buffer, filtered, filtered_count * sizeof(*filtered));
    free(filtered);
    return true;
}

static bool runtime_native_3d_denoise_apply_disney_v2_edge_safe(
    float* radiance_buffer,
    int radiance_stride,
    const RuntimeNative3DFeatureBuffer* features,
    int temporal_frames,
    const float* temporal_activity_buffer,
    int temporal_activity_stride,
    RuntimeNative3DDenoiseDiagnostics* out_diagnostics) {
    RuntimeNative3DDenoiseDiagnostics diagnostics = {0};
    float* filtered = NULL;
    size_t filtered_count = 0u;

    diagnostics.temporalFrameCount = temporal_frames;
    if (!runtime_native_3d_denoise_validate_base(radiance_buffer,
                                                 radiance_stride,
                                                 features,
                                                 &filtered_count) ||
        !runtime_native_3d_denoise_disney_v2_features_valid(features)) {
        return false;
    }
    if (temporal_activity_buffer && temporal_activity_stride < features->width) {
        return false;
    }

    filtered = (float*)calloc(filtered_count, sizeof(*filtered));
    if (!filtered) return false;

    for (int y = 0; y < features->height; ++y) {
        for (int x = 0; x < features->width; ++x) {
            const size_t center_index = (size_t)y * (size_t)features->width + (size_t)x;
            const size_t center_normal_base = center_index * 3u;
            const size_t center_radiance_base =
                ((size_t)y * (size_t)radiance_stride + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            const float center_luma =
                runtime_native_3d_denoise_luma(radiance_buffer, center_radiance_base);
            float total_weight = 1.0f;
            float accum_r = radiance_buffer[center_radiance_base];
            float accum_g = radiance_buffer[center_radiance_base + 1u];
            float accum_b = radiance_buffer[center_radiance_base + 2u];
            int accepted_neighbor_count = 0;

            diagnostics.rawPixelCount += 1;
            diagnostics.rawRadianceLumaTotal += center_luma;

            if (!features->hitMaskBuffer[center_index]) {
                runtime_native_3d_denoise_copy_pixel(filtered, radiance_buffer, center_radiance_base);
                diagnostics.reconstructedRadianceLumaTotal += center_luma;
                diagnostics.skippedInvalidSurfacePixelCount += 1;
                continue;
            }
            if (features->triangleIndexBuffer[center_index] < 0 ||
                features->sceneObjectIndexBuffer[center_index] < 0) {
                runtime_native_3d_denoise_copy_pixel(filtered, radiance_buffer, center_radiance_base);
                diagnostics.reconstructedRadianceLumaTotal += center_luma;
                diagnostics.skippedInvalidSurfacePixelCount += 1;
                continue;
            }
            if (runtime_native_3d_denoise_disney_v2_preserves_transparent(features,
                                                                          center_index)) {
                runtime_native_3d_denoise_copy_pixel(filtered, radiance_buffer, center_radiance_base);
                diagnostics.reconstructedRadianceLumaTotal += center_luma;
                diagnostics.preservedTransparentPixelCount += 1;
                continue;
            }
            if (runtime_native_3d_denoise_disney_v2_preserves_mirror_glossy(features,
                                                                            center_index)) {
                runtime_native_3d_denoise_copy_pixel(filtered, radiance_buffer, center_radiance_base);
                diagnostics.reconstructedRadianceLumaTotal += center_luma;
                diagnostics.preservedMirrorGlossyPixelCount += 1;
                continue;
            }
            if (temporal_activity_buffer) {
                const size_t activity_index =
                    (size_t)y * (size_t)temporal_activity_stride + (size_t)x;
                if (temporal_activity_buffer[activity_index] >
                    kRuntimeNative3DDisneyV2DenoiseTemporalActivityLimit) {
                    runtime_native_3d_denoise_copy_pixel(filtered,
                                                         radiance_buffer,
                                                         center_radiance_base);
                    diagnostics.reconstructedRadianceLumaTotal += center_luma;
                    diagnostics.skippedUnstableTemporalPixelCount += 1;
                    continue;
                }
            }

            {
                const float center_depth = features->depthBuffer[center_index];
                const float center_nx = features->normalBuffer[center_normal_base];
                const float center_ny = features->normalBuffer[center_normal_base + 1u];
                const float center_nz = features->normalBuffer[center_normal_base + 2u];
                const int center_object = features->sceneObjectIndexBuffer[center_index];

                for (int dy = -RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                     dy <= RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                     ++dy) {
                    const int sample_y = y + dy;
                    if (sample_y < 0 || sample_y >= features->height) continue;
                    for (int dx = -RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                         dx <= RUNTIME_NATIVE_3D_DENOISE_RADIUS;
                         ++dx) {
                        const int sample_x = x + dx;
                        size_t sample_index = 0u;
                        size_t sample_normal_base = 0u;
                        size_t sample_radiance_base = 0u;
                        float ndot = 0.0f;
                        float depth_delta = 0.0f;
                        float weight = 0.0f;

                        if (dx == 0 && dy == 0) continue;
                        if (sample_x < 0 || sample_x >= features->width) continue;
                        sample_index =
                            (size_t)sample_y * (size_t)features->width + (size_t)sample_x;
                        sample_normal_base = sample_index * 3u;
                        sample_radiance_base =
                            ((size_t)sample_y * (size_t)radiance_stride + (size_t)sample_x) *
                            (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
                        if (!features->hitMaskBuffer[sample_index] ||
                            features->sceneObjectIndexBuffer[sample_index] != center_object ||
                            runtime_native_3d_denoise_disney_v2_preserves_transparent(
                                features,
                                sample_index) ||
                            runtime_native_3d_denoise_disney_v2_preserves_mirror_glossy(
                                features,
                                sample_index) ||
                            runtime_native_3d_denoise_disney_v2_rejects_visual_edge(
                                radiance_buffer,
                                center_radiance_base,
                                sample_radiance_base)) {
                            diagnostics.rejectedEdgeSampleCount += 1;
                            continue;
                        }

                        ndot = center_nx * features->normalBuffer[sample_normal_base] +
                               center_ny * features->normalBuffer[sample_normal_base + 1u] +
                               center_nz * features->normalBuffer[sample_normal_base + 2u];
                        depth_delta = runtime_native_3d_denoise_depth_relative_delta(
                            center_depth,
                            features->depthBuffer[sample_index]);
                        if (ndot < kRuntimeNative3DDisneyV2DenoiseNormalDot ||
                            depth_delta > kRuntimeNative3DDisneyV2DenoiseDepthDelta) {
                            diagnostics.rejectedEdgeSampleCount += 1;
                            continue;
                        }

                        weight = runtime_native_3d_denoise_spatial_weight(dx, dy) *
                                 powf(fminf(ndot, 1.0f), 16.0f) *
                                 expf(-(depth_delta * depth_delta) / (2.0f * 0.01f * 0.01f));
                        accum_r += radiance_buffer[sample_radiance_base] * weight;
                        accum_g += radiance_buffer[sample_radiance_base + 1u] * weight;
                        accum_b += radiance_buffer[sample_radiance_base + 2u] * weight;
                        total_weight += weight;
                        accepted_neighbor_count += 1;
                        diagnostics.stableInteriorSampleCount += 1;
                    }
                }
            }

            if (accepted_neighbor_count > 0 && total_weight > 1.0f + 1e-6f) {
                filtered[center_radiance_base] = accum_r / total_weight;
                filtered[center_radiance_base + 1u] = accum_g / total_weight;
                filtered[center_radiance_base + 2u] = accum_b / total_weight;
                filtered[center_radiance_base + RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL] =
                    radiance_buffer[center_radiance_base +
                                    RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL];
                diagnostics.reconstructedPixelCount += 1;
                diagnostics.reconstructedRadianceLumaTotal += runtime_native_3d_denoise_luma(
                    filtered,
                    center_radiance_base);
            } else {
                runtime_native_3d_denoise_copy_pixel(filtered, radiance_buffer, center_radiance_base);
                diagnostics.reconstructedRadianceLumaTotal += center_luma;
            }
        }
    }

    memcpy(radiance_buffer, filtered, filtered_count * sizeof(*filtered));
    free(filtered);
    if (out_diagnostics) {
        *out_diagnostics = diagnostics;
    }
    return true;
}

bool RuntimeNative3DDenoise_ApplyForIntegrator(
    float* radiance_buffer,
    int radiance_stride,
    const RuntimeNative3DFeatureBuffer* features,
    RayTracing3DIntegratorId integrator_id,
    int temporal_frames,
    const float* temporal_activity_buffer,
    int temporal_activity_stride,
    RuntimeNative3DDenoiseDiagnostics* out_diagnostics) {
    bool ok = false;
    if (out_diagnostics) {
        RuntimeNative3DDenoiseDiagnostics_Reset(out_diagnostics);
        out_diagnostics->temporalFrameCount = temporal_frames;
    }
    if (integrator_id == RAY_TRACING_3D_INTEGRATOR_DISNEY_V2) {
        return runtime_native_3d_denoise_apply_disney_v2_edge_safe(radiance_buffer,
                                                                  radiance_stride,
                                                                  features,
                                                                  temporal_frames,
                                                                  temporal_activity_buffer,
                                                                  temporal_activity_stride,
                                                                  out_diagnostics);
    }
    if (integrator_id == RAY_TRACING_3D_INTEGRATOR_DISNEY) {
        const double raw_luma = (out_diagnostics && features)
                                    ? runtime_native_3d_denoise_sum_luma(radiance_buffer,
                                                                         radiance_stride,
                                                                         features->width,
                                                                         features->height)
                                    : 0.0;
        ok = RuntimeNative3DDenoise_Apply(radiance_buffer, radiance_stride, features);
        if (ok && out_diagnostics) {
            out_diagnostics->rawPixelCount = features ? features->width * features->height : 0;
            out_diagnostics->reconstructedPixelCount = out_diagnostics->rawPixelCount;
            out_diagnostics->rawRadianceLumaTotal = raw_luma;
            out_diagnostics->reconstructedRadianceLumaTotal =
                features ? runtime_native_3d_denoise_sum_luma(radiance_buffer,
                                                              radiance_stride,
                                                              features->width,
                                                              features->height)
                         : 0.0;
        }
        return ok;
    }
    return false;
}
