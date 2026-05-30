#include "render/runtime_native_3d_render_internal.h"

#include <math.h>
#include <stddef.h>

#include "render/runtime_disney_3d.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_material_response_3d.h"

static bool runtime_native_3d_render_shade_direct_light(float* radiance_buffer,
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
            RuntimeLightEmitterHit3DResult emitter_hit = {0};
            RuntimeVolume3DScatterResult scatter = {0};
            const int local_y = y - start_y;
            const int local_x = x - start_x;
            size_t idx = (size_t)local_y * (size_t)radiance_stride + (size_t)local_x;
            if (runtime_native_3d_render_trace_visible_emitter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               &emitter_hit)) {
                runtime_native_3d_render_write_emitter_radiance_with_scatter(radiance_buffer,
                                                                             idx,
                                                                             scene,
                                                                             projector,
                                                                             (double)x,
                                                                             (double)y,
                                                                             &emitter_hit,
                                                                             sampling,
                                                                             &stats);
                continue;
            }
            if (!RuntimeDirectLight3D_ShadePixel(scene,
                                                 projector,
                                                 (double)x,
                                                 (double)y,
                                                 sampling,
                                                 &result)) {
                scatter = runtime_native_3d_render_primary_scatter(scene,
                                                                   projector,
                                                                   (double)x,
                                                                   (double)y,
                                                                   HUGE_VAL,
                                                                   sampling);
                runtime_native_3d_render_write_background_radiance(radiance_buffer,
                                                                   idx,
                                                                   scene,
                                                                   projector,
                                                                   &result.primaryRay,
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
                                                               sampling);
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

static bool runtime_native_3d_render_shade_diffuse_bounce(float* radiance_buffer,
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
            RuntimeLightEmitterHit3DResult emitter_hit = {0};
            RuntimeVolume3DScatterResult scatter = {0};
            const int local_y = y - start_y;
            const int local_x = x - start_x;
            size_t idx = (size_t)local_y * (size_t)radiance_stride + (size_t)local_x;
            if (runtime_native_3d_render_trace_visible_emitter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               &emitter_hit)) {
                runtime_native_3d_render_write_emitter_radiance_with_scatter(radiance_buffer,
                                                                             idx,
                                                                             scene,
                                                                             projector,
                                                                             (double)x,
                                                                             (double)y,
                                                                             &emitter_hit,
                                                                             sampling,
                                                                             &stats);
                continue;
            }
            if (!RuntimeDiffuseBounce3D_ShadePixel(scene,
                                                   projector,
                                                   (double)x,
                                                   (double)y,
                                                   sampling,
                                                   &result)) {
                scatter = runtime_native_3d_render_primary_scatter(scene,
                                                                   projector,
                                                                   (double)x,
                                                                   (double)y,
                                                                   HUGE_VAL,
                                                                   sampling);
                runtime_native_3d_render_write_background_radiance(radiance_buffer,
                                                                   idx,
                                                                   scene,
                                                                   projector,
                                                                   &result.primaryRay,
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
                                                               sampling);
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

static bool runtime_native_3d_render_shade_material(float* radiance_buffer,
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
            RuntimeLightEmitterHit3DResult emitter_hit = {0};
            RuntimeVolume3DScatterResult scatter = {0};
            const int local_y = y - start_y;
            const int local_x = x - start_x;
            size_t idx = (size_t)local_y * (size_t)radiance_stride + (size_t)local_x;
            if (runtime_native_3d_render_trace_visible_emitter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               &emitter_hit)) {
                runtime_native_3d_render_write_emitter_radiance_with_scatter(radiance_buffer,
                                                                             idx,
                                                                             scene,
                                                                             projector,
                                                                             (double)x,
                                                                             (double)y,
                                                                             &emitter_hit,
                                                                             sampling,
                                                                             &stats);
                continue;
            }
            if (!RuntimeMaterialResponse3D_ShadePixel(scene,
                                                      projector,
                                                      (double)x,
                                                      (double)y,
                                                      sampling,
                                                      &result)) {
                scatter = runtime_native_3d_render_primary_scatter(scene,
                                                                   projector,
                                                                   (double)x,
                                                                   (double)y,
                                                                   HUGE_VAL,
                                                                   sampling);
                runtime_native_3d_render_write_background_radiance(radiance_buffer,
                                                                   idx,
                                                                   scene,
                                                                   projector,
                                                                   &result.primaryRay,
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
                                                               sampling);
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

static bool runtime_native_3d_render_shade_emission_transparency(
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
            RuntimeLightEmitterHit3DResult emitter_hit = {0};
            RuntimeVolume3DScatterResult scatter = {0};
            const int local_y = y - start_y;
            const int local_x = x - start_x;
            size_t idx = (size_t)local_y * (size_t)radiance_stride + (size_t)local_x;
            if (runtime_native_3d_render_trace_visible_emitter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               &emitter_hit)) {
                runtime_native_3d_render_write_emitter_radiance_with_scatter(radiance_buffer,
                                                                             idx,
                                                                             scene,
                                                                             projector,
                                                                             (double)x,
                                                                             (double)y,
                                                                             &emitter_hit,
                                                                             sampling,
                                                                             &stats);
                continue;
            }
            if (!RuntimeEmissionTransparency3D_ShadePixel(scene,
                                                          projector,
                                                          (double)x,
                                                          (double)y,
                                                          sampling,
                                                          &result)) {
                scatter = runtime_native_3d_render_primary_scatter(scene,
                                                                   projector,
                                                                   (double)x,
                                                                   (double)y,
                                                                   HUGE_VAL,
                                                                   sampling);
                runtime_native_3d_render_write_background_radiance(radiance_buffer,
                                                                   idx,
                                                                   scene,
                                                                   projector,
                                                                   &result.primaryRay,
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
                                                               sampling);
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

static bool runtime_native_3d_render_shade_disney(float* radiance_buffer,
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
            RuntimeLightEmitterHit3DResult emitter_hit = {0};
            RuntimeVolume3DScatterResult scatter = {0};
            const int local_y = y - start_y;
            const int local_x = x - start_x;
            size_t idx = (size_t)local_y * (size_t)radiance_stride + (size_t)local_x;
            if (runtime_native_3d_render_trace_visible_emitter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               &emitter_hit)) {
                runtime_native_3d_render_write_emitter_radiance_with_scatter(radiance_buffer,
                                                                             idx,
                                                                             scene,
                                                                             projector,
                                                                             (double)x,
                                                                             (double)y,
                                                                             &emitter_hit,
                                                                             sampling,
                                                                             &stats);
                continue;
            }
            if (!RuntimeDisney3D_ShadePixel(scene,
                                            projector,
                                            (double)x,
                                            (double)y,
                                            sampling,
                                            &result)) {
                scatter = runtime_native_3d_render_primary_scatter(scene,
                                                                   projector,
                                                                   (double)x,
                                                                   (double)y,
                                                                   HUGE_VAL,
                                                                   sampling);
                runtime_native_3d_render_write_background_radiance(radiance_buffer,
                                                                   idx,
                                                                   scene,
                                                                   projector,
                                                                   &result.primaryRay,
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
                                                               sampling);
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

bool runtime_native_3d_render_dispatch_integrator(float* radiance_buffer,
                                                  int radiance_stride,
                                                  RayTracing3DIntegratorId integrator_id,
                                                  const RuntimeNative3DPreparedFrame* frame,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y,
                                                  RuntimeNative3DRenderStats* out_stats) {
    if (!frame || !frame->valid) {
        return false;
    }

    /* Keep the native renderer dispatch explicit so later 3D tiers can add
     * focused shader paths without overloading the entrypoint itself. */
    switch (integrator_id) {
        case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
            return runtime_native_3d_render_shade_direct_light(radiance_buffer,
                                                               radiance_stride,
                                                               frame->width,
                                                               frame->height,
                                                               start_x,
                                                               start_y,
                                                               end_x,
                                                               end_y,
                                                               &frame->scene,
                                                               &frame->projector,
                                                               &frame->sampling,
                                                               out_stats);
        case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
            return runtime_native_3d_render_shade_diffuse_bounce(radiance_buffer,
                                                                 radiance_stride,
                                                                 frame->width,
                                                                 frame->height,
                                                                 start_x,
                                                                 start_y,
                                                                 end_x,
                                                                 end_y,
                                                                 &frame->scene,
                                                                 &frame->projector,
                                                                 &frame->sampling,
                                                                 out_stats);
        case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
            return runtime_native_3d_render_shade_material(radiance_buffer,
                                                           radiance_stride,
                                                           frame->width,
                                                           frame->height,
                                                           start_x,
                                                           start_y,
                                                           end_x,
                                                           end_y,
                                                           &frame->scene,
                                                           &frame->projector,
                                                           &frame->sampling,
                                                           out_stats);
        case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            return runtime_native_3d_render_shade_emission_transparency(radiance_buffer,
                                                                        radiance_stride,
                                                                        frame->width,
                                                                        frame->height,
                                                                        start_x,
                                                                        start_y,
                                                                        end_x,
                                                                        end_y,
                                                                        &frame->scene,
                                                                        &frame->projector,
                                                                        &frame->sampling,
                                                                        out_stats);
        case RAY_TRACING_3D_INTEGRATOR_DISNEY:
            return runtime_native_3d_render_shade_disney(radiance_buffer,
                                                         radiance_stride,
                                                         frame->width,
                                                         frame->height,
                                                         start_x,
                                                         start_y,
                                                         end_x,
                                                         end_y,
                                                         &frame->scene,
                                                         &frame->projector,
                                                         &frame->sampling,
                                                         out_stats);
        default:
            return false;
    }
}
