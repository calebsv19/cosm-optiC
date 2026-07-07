#include "render/runtime_native_3d_render_shading_internal.h"

#include <math.h>
#include <stddef.h>

bool runtime_native_3d_render_shade_direct_light(float* radiance_buffer,
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
                                                        RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderStats stats = {0};

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
    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            RuntimeDirectLight3DResult result = {0};
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
            if (!RuntimeDirectLight3D_ShadePrimaryHitWithPayload(
                    scene,
                    &primary_trace.primary,
                    primary_trace.payloadResolved ? &primary_trace.payload : NULL,
                    sampling,
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
            scatter = runtime_native_3d_render_primary_scatter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               result.hitInfo.t,
                                                               sampling,
                                                               caustic_cache);
            runtime_native_3d_render_record_scatter_stats(&stats, &scatter, x, y);
            stats.hitPixelCount += 1;
            if (result.visible || scatter.active) {
                stats.visiblePixelCount += 1;
            }
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

bool runtime_native_3d_render_shade_diffuse_bounce(float* radiance_buffer,
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
                                                          RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderStats stats = {0};

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

    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            RuntimeDiffuseBounce3DResult result = {0};
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
            if (!RuntimeDiffuseBounce3D_ShadePrimaryHitWithPayload(
                    scene,
                    &primary_trace.primary,
                    primary_trace.payloadResolved ? &primary_trace.payload : NULL,
                    sampling,
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
            if (result.visible) {
                stats.visiblePixelCount += 1;
            }
            if (result.bounceRadiance > 0.0) {
                stats.bouncePixelCount += 1;
                stats.totalBounceRadiance += result.bounceRadiance;
            }
            stats.secondaryRayCount += result.secondaryRayCount;
            stats.secondaryHitCount += result.secondaryHitCount;
            stats.secondaryContributingHitCount += result.secondaryContributingHitCount;
            if (result.radiance > stats.maxRadiance) {
                stats.maxRadiance = result.radiance;
            }
            if (result.bounceRadiance > stats.maxBounceRadiance) {
                stats.maxBounceRadiance = result.bounceRadiance;
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

bool runtime_native_3d_render_shade_material(float* radiance_buffer,
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
                                                    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderStats stats = {0};

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
    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            RuntimeMaterialResponse3DResult result = {0};
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
            if (!RuntimeMaterialResponse3D_ShadePrimaryHitWithPayload(
                    scene,
                    &primary_trace.primary,
                    primary_trace.payloadResolved ? &primary_trace.payload : NULL,
                    sampling,
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
            if (result.visible) {
                stats.visiblePixelCount += 1;
            }
            if (result.bounceRadiance > 0.0) {
                stats.bouncePixelCount += 1;
                stats.totalBounceRadiance += result.bounceRadiance;
            }
            stats.secondaryRayCount += result.secondaryRayCount;
            stats.secondaryHitCount += result.secondaryHitCount;
            stats.secondaryContributingHitCount += result.secondaryContributingHitCount;
            if (result.radiance > stats.maxRadiance) {
                stats.maxRadiance = result.radiance;
            }
            if (result.bounceRadiance > stats.maxBounceRadiance) {
                stats.maxBounceRadiance = result.bounceRadiance;
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

bool runtime_native_3d_render_shade_emission_transparency(
    float* radiance_buffer,
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
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderStats stats = {0};

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
    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            RuntimeEmissionTransparency3DResult result = {0};
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
            if (!RuntimeEmissionTransparency3D_ShadePrimaryHitWithPayload(
                    scene,
                    &primary_trace.primary,
                    primary_trace.payloadResolved ? &primary_trace.payload : NULL,
                    sampling,
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
            if (result.visible || result.radiance > 1e-9) {
                stats.visiblePixelCount += 1;
            }
            if (result.bounceRadiance > 0.0) {
                stats.bouncePixelCount += 1;
                stats.totalBounceRadiance += result.bounceRadiance;
            }
            stats.secondaryRayCount += result.secondaryRayCount;
            stats.secondaryHitCount += result.secondaryHitCount;
            stats.secondaryContributingHitCount += result.secondaryContributingHitCount;
            if (result.radiance > stats.maxRadiance) {
                stats.maxRadiance = result.radiance;
            }
            if (result.bounceRadiance > stats.maxBounceRadiance) {
                stats.maxBounceRadiance = result.bounceRadiance;
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

bool runtime_native_3d_render_shade_disney(float* radiance_buffer,
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
                                                  RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderStats stats = {0};

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
    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            RuntimeDisney3DResult result = {0};
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
            if (!RuntimeDisney3D_ShadePrimaryHitWithPayload(
                    scene,
                    &primary_trace.primary,
                    primary_trace.payloadResolved ? &primary_trace.payload : NULL,
                    sampling,
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
            if (result.visible) {
                stats.visiblePixelCount += 1;
            }
            if (result.bounceRadiance > 0.0) {
                stats.bouncePixelCount += 1;
                stats.totalBounceRadiance += result.bounceRadiance;
            }
            stats.secondaryRayCount += result.secondaryRayCount;
            stats.secondaryHitCount += result.secondaryHitCount;
            stats.secondaryContributingHitCount += result.secondaryContributingHitCount;
            if (result.radiance > stats.maxRadiance) {
                stats.maxRadiance = result.radiance;
            }
            if (result.bounceRadiance > stats.maxBounceRadiance) {
                stats.maxBounceRadiance = result.bounceRadiance;
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
