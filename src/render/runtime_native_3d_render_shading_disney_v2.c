#include "render/runtime_native_3d_render_shading_internal.h"

#include <math.h>
#include <stddef.h>

bool runtime_native_3d_render_shade_disney_v2(float* radiance_buffer,
                                                     int radiance_stride,
                                                     int width,
                                                     int height,
                                                     int start_x,
                                                     int start_y,
                                                     int end_x,
                                                     int end_y,
                                                     const RuntimeScene3D* scene,
                                                     const RuntimeCameraProjector3D* projector,
                                                     const RuntimeNative3DSamplingContext* sampling,
                                                     RuntimeCausticVolumeCache3D* caustic_cache,
                                                     RuntimeCausticSurfaceCache3D* surface_cache,
                                                     const RuntimeDisneyV2CausticSidecarProbe3D*
                                                         caustic_probe,
                                                     RuntimeNative3DFeatureBuffer* feature_buffer,
                                                     int feature_start_x,
                                                     int feature_start_y,
                                                     RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderStats stats = {0};
    const bool caustic_sidecar_active = caustic_probe && caustic_probe->valid;

    if (!radiance_buffer || radiance_stride <= 0 || width <= 0 || height <= 0 || !scene ||
        !projector) {
        return false;
    }
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;
    if (end_x > width) end_x = width;
    if (end_y > height) end_y = height;
    if (start_x >= end_x || start_y >= end_y) {
        if (out_stats) {
            *out_stats = stats;
        }
        return true;
    }
    if (scene->emissiveLightSet.valid) {
        stats.emissiveAreaCandidateCount = scene->emissiveLightSet.candidateCount;
    }
    if (caustic_sidecar_active) {
        stats.causticSidecarEnabled = 1;
    }

    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            RuntimeDisneyV2_3DResult result = {0};
            RuntimeNative3DPrimaryTrace primary_trace = {0};
            RuntimeVolume3DScatterResult scatter = {0};
            const int local_y = y - start_y;
            const int local_x = x - start_x;
            size_t idx = (size_t)local_y * (size_t)radiance_stride + (size_t)local_x;
            primary_trace = runtime_native_3d_render_trace_primary(scene,
                                                                   projector,
                                                                   (double)x,
                                                                   (double)y);
            if (primary_trace.emitterWins) {
                runtime_native_3d_render_write_emitter_radiance_with_scatter(radiance_buffer,
                                                                             idx,
                                                                             scene,
                                                                             projector,
                                                                             (double)x,
                                                                             (double)y,
                                                                             &primary_trace.emitterHit,
                                                                             sampling,
                                                                             caustic_cache,
                                                                             &stats);
                continue;
            }
            if (!RuntimeDisneyV2_3D_ShadePrimaryHitWithPayloadAndTraceContext(
                    scene,
                    &primary_trace.primary,
                    primary_trace.payloadResolved ? &primary_trace.payload : NULL,
                    sampling,
                    local_x,
                    local_y,
                    end_x - start_x,
                    end_y - start_y,
                    &result)) {
                scatter = runtime_native_3d_render_primary_scatter(scene,
                                                                   projector,
                                                                   (double)x,
                                                                   (double)y,
                                                                   HUGE_VAL,
                                                                   sampling,
                                                                   caustic_cache);
                runtime_native_3d_render_record_scatter_stats(&stats, &scatter, x, y);
                runtime_native_3d_render_write_background_radiance(radiance_buffer,
                                                                   idx,
                                                                   scene,
                                                                   projector,
                                                                   &primary_trace.primary.primaryRay,
                                                                   &scatter);
                if (scatter.active && scatter.radiance > stats.maxRadiance) {
                    stats.maxRadiance = scatter.radiance;
                }
                continue;
            }
            if (feature_buffer) {
                RuntimeNative3DFeatureBuffer_RecordDirectLightVisibilityOutcome(
                    feature_buffer,
                    x - feature_start_x,
                    y - feature_start_y,
                    RuntimeNative3DFeatureBuffer_ResolveDirectLightVisibilityOutcome(
                        result.directVisibilityOutcomeNoTraceCount,
                        result.directVisibilityOutcomeClearVisibleCount,
                        result.directVisibilityOutcomeClearBlockedCount,
                        result.directVisibilityOutcomeStablePartialCount,
                        result.directVisibilityOutcomeMixedPartialCount));
            }
            scatter = runtime_native_3d_render_primary_scatter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               result.hitInfo.t,
                                                               sampling,
                                                               caustic_cache);
            runtime_native_3d_render_record_scatter_stats(&stats, &scatter, x, y);
            stats.hitPixelCount += 1;
            runtime_native_3d_render_apply_scatter_rgb(&result.radianceR,
                                                       &result.radianceG,
                                                       &result.radianceB,
                                                       &result.radiance,
                                                       &result.visible,
                                                       &scatter);
            runtime_native_3d_render_apply_ambient_hit_lighting(scene,
                                                                &result.hitInfo,
                                                                &result.radianceR,
                                                                &result.radianceG,
                                                                &result.radianceB,
                                                                &result.radiance,
                                                                &result.visible);
            runtime_native_3d_render_apply_surface_caustic_cache(surface_cache,
                                                                 &result.hitInfo,
                                                                 &result.radianceR,
                                                                 &result.radianceG,
                                                                 &result.radianceB,
                                                                 &result.radiance,
                                                                 &result.visible,
                                                                 &stats);
            runtime_native_3d_render_apply_transmitted_surface_caustic_cache(
                surface_cache,
                &result,
                &result.radianceR,
                &result.radianceG,
                &result.radianceB,
                &result.radiance,
                &result.visible,
                &stats);
            if (caustic_sidecar_active) {
                RuntimeDisneyV2CausticSidecarContribution3D caustic = {0};
                stats.causticSidecarSampleCount += 1;
                if (RuntimeDisneyV2_3D_EvaluateCausticSidecar(caustic_probe,
                                                               &result.hitInfo,
                                                               &caustic)) {
                    result.radianceR += caustic.r;
                    result.radianceG += caustic.g;
                    result.radianceB += caustic.b;
                    result.radiance =
                        fmax(fmax(result.radianceR, result.radianceG), result.radianceB);
                    stats.causticSidecarContributingSampleCount += 1;
                    stats.totalCausticSidecarRadiance += caustic.luma;
                    if (caustic.luma > stats.maxCausticSidecarRadiance) {
                        stats.maxCausticSidecarRadiance = caustic.luma;
                    }
                }
            }
            if (result.visible) {
                stats.visiblePixelCount += 1;
            }
            stats.secondaryRayCount += result.secondaryRayCount;
            stats.secondaryHitCount += result.secondaryHitCount;
            stats.secondaryContributingHitCount += result.secondaryContributingHitCount;
            runtime_native_3d_render_record_disney_v2_emissive_area_stats(&stats, &result);
            runtime_native_3d_render_record_disney_v2_mirror_stats(&stats, &result);
            if (result.radiance > stats.maxRadiance) {
                stats.maxRadiance = result.radiance;
            }
            runtime_native_3d_render_write_radiance_rgb(radiance_buffer,
                                                        idx,
                                                        result.radianceR,
                                                        result.radianceG,
                                                        result.radianceB,
                                                        0.0);
        }
    }

    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}
