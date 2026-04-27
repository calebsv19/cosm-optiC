#include "render/runtime_native_3d_render.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "config/config_manager.h"
#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_native_3d_temporal_accum.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_scene_3d_samples.h"

static double runtime_native_3d_render_resolve_default_light_radius(
    const RuntimeScene3D* scene) {
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    bool seeded = false;
    double span_max = 0.0;
    double radius = 0.0;

    if (!scene || scene->triangleMesh.triangleCount <= 0) {
        return 0.12;
    }

    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* tri = &scene->triangleMesh.triangles[i];
        const Vec3 points[3] = {tri->p0, tri->p1, tri->p2};

        for (int p = 0; p < 3; ++p) {
            const Vec3 point = points[p];
            if (!seeded) {
                min_x = max_x = point.x;
                min_y = max_y = point.y;
                min_z = max_z = point.z;
                seeded = true;
            } else {
                if (point.x < min_x) min_x = point.x;
                if (point.x > max_x) max_x = point.x;
                if (point.y < min_y) min_y = point.y;
                if (point.y > max_y) max_y = point.y;
                if (point.z < min_z) min_z = point.z;
                if (point.z > max_z) max_z = point.z;
            }
        }
    }

    if (!seeded) {
        return 0.12;
    }

    span_max = fmax(max_x - min_x, fmax(max_y - min_y, max_z - min_z));
    if (!(span_max > 0.0) || !isfinite(span_max)) {
        return 0.12;
    }

    radius = span_max * 0.015;
    if (radius < 0.05) radius = 0.05;
    if (radius > 0.25) radius = 0.25;
    return radius;
}

static void runtime_native_3d_render_apply_live_light(RuntimeScene3D* scene,
                                                      double live_light_x,
                                                      double live_light_y) {
    RuntimeLight3D light = {0};
    if (!scene) return;

    if (scene->hasLight) {
        light = scene->light;
    }
    light.position = vec3(live_light_x, live_light_y, animSettings.lightHeight);
    light.radius = (light.radius > 0.0) ? light.radius
                                        : runtime_native_3d_render_resolve_default_light_radius(scene);
    light.intensity = animSettings.lightIntensity;
    light.falloffDistance = animSettings.forwardDecay;
    light.falloffMode = animSettings.forwardFalloffMode;
    scene->light = light;
    scene->hasLight = true;
}

static void runtime_native_3d_render_apply_live_camera(RuntimeScene3D* scene,
                                                       double normalized_t) {
    RuntimeCamera3D camera = {0};
    RuntimeCamera3D sampled = {0};
    if (!scene) return;

    if (scene->hasCamera) {
        camera = scene->camera;
    }
    camera.position = vec3(sceneSettings.camera.x, sceneSettings.camera.y, sceneSettings.cameraZ);
    camera.rotation = sceneSettings.camera.rotation;
    camera.zoom = (sceneSettings.camera.zoom > 0.0) ? sceneSettings.camera.zoom : 1.0;
    camera.nearPlane = (camera.nearPlane > 0.0) ? camera.nearPlane : 0.1;
    camera.lookPitch = 0.0;
    if (!animSettings.interactiveMode &&
        RuntimeScene3DSampleAuthoredCamera(normalized_t, &sampled)) {
        camera.lookPitch = sampled.lookPitch;
    }

    scene->camera = camera;
    scene->hasCamera = true;
}

static bool runtime_native_3d_render_build_live_scene(RuntimeScene3D* scene,
                                                      double normalized_t,
                                                      double live_light_x,
                                                      double live_light_y) {
    if (!scene) return false;
    if (!RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(scene, normalized_t)) {
        return false;
    }

    runtime_native_3d_render_apply_live_light(scene, live_light_x, live_light_y);
    runtime_native_3d_render_apply_live_camera(scene, normalized_t);
    return scene->primitiveCount > 0 &&
           scene->triangleMesh.triangleCount > 0 &&
           scene->hasLight &&
           scene->hasCamera;
}

static bool runtime_native_3d_render_trace_visible_emitter(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y,
    RuntimeLightEmitterHit3DResult* out_emitter_hit) {
    RuntimeLightEmitterTrace3DResult trace = {0};
    Ray3D primary_ray = {0};

    if (!scene || !projector || !out_emitter_hit) return false;

    primary_ray = RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
    if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                               &primary_ray,
                                               projector->nearPlane,
                                               HUGE_VAL,
                                               &trace) ||
        !trace.emitterWins) {
        return false;
    }

    *out_emitter_hit = trace.emitterHitInfo;
    return true;
}

static void runtime_native_3d_render_write_emitter_luminance(float* luminance_buffer,
                                                             size_t pixel_index,
                                                             const RuntimeLightEmitterHit3DResult* hit,
                                                             RuntimeNative3DRenderStats* io_stats) {
    if (!luminance_buffer || !hit || !io_stats) return;

    io_stats->hitPixelCount += 1;
    io_stats->visiblePixelCount += 1;
    if (hit->radiance > io_stats->maxRadiance) {
        io_stats->maxRadiance = hit->radiance;
    }
    luminance_buffer[pixel_index] = (float)hit->radiance;
}

static void runtime_native_3d_render_resolve_luminance_region_to_pixels(
    uint8_t* pixel_buffer,
    int pixel_stride,
    const float* luminance_buffer,
    int luminance_stride,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    if (!pixel_buffer || !luminance_buffer || pixel_stride <= 0 || luminance_stride <= 0) return;
    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        for (int x = start_x; x < end_x; ++x) {
            const int local_x = x - start_x;
            const size_t pixel_index = (size_t)y * (size_t)pixel_stride + (size_t)x;
            const size_t luminance_index =
                (size_t)local_y * (size_t)luminance_stride + (size_t)local_x;
            pixel_buffer[pixel_index] =
                (uint8_t)(TonemapCurve(luminance_buffer[luminance_index]) * 255.0f);
        }
    }
}

static bool runtime_native_3d_render_shade_direct_light(float* luminance_buffer,
                                                        int luminance_stride,
                                                        int width,
                                                        int height,
                                                        int start_x,
                                                        int start_y,
                                                        int end_x,
                                                        int end_y,
                                                        const RuntimeScene3D* scene,
                                                        const RuntimeCameraProjector3D* projector,
                                                        RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderStats stats = {0};

    if (!luminance_buffer || luminance_stride <= 0 || width <= 0 || height <= 0 || !scene ||
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
            const int local_y = y - start_y;
            const int local_x = x - start_x;
            size_t idx = (size_t)local_y * (size_t)luminance_stride + (size_t)local_x;
            if (runtime_native_3d_render_trace_visible_emitter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               &emitter_hit)) {
                runtime_native_3d_render_write_emitter_luminance(luminance_buffer,
                                                                 idx,
                                                                 &emitter_hit,
                                                                 &stats);
                continue;
            }
            if (!RuntimeDirectLight3D_ShadePixel(scene,
                                                 projector,
                                                 (double)x,
                                                 (double)y,
                                                 &result)) {
                continue;
            }
            stats.hitPixelCount += 1;
            if (result.visible) {
                stats.visiblePixelCount += 1;
            }
            if (result.radiance > stats.maxRadiance) {
                stats.maxRadiance = result.radiance;
            }
            luminance_buffer[idx] = (float)result.radiance;
        }
    }

    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}

static bool runtime_native_3d_render_shade_diffuse_bounce(float* luminance_buffer,
                                                          int luminance_stride,
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

    if (!luminance_buffer || luminance_stride <= 0 || width <= 0 || height <= 0 || !scene ||
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
            const int local_y = y - start_y;
            const int local_x = x - start_x;
            size_t idx = (size_t)local_y * (size_t)luminance_stride + (size_t)local_x;
            if (runtime_native_3d_render_trace_visible_emitter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               &emitter_hit)) {
                runtime_native_3d_render_write_emitter_luminance(luminance_buffer,
                                                                 idx,
                                                                 &emitter_hit,
                                                                 &stats);
                continue;
            }
            if (!RuntimeDiffuseBounce3D_ShadePixel(scene,
                                                   projector,
                                                   (double)x,
                                                   (double)y,
                                                   sampling,
                                                   &result)) {
                continue;
            }
            stats.hitPixelCount += 1;
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
            luminance_buffer[idx] = (float)result.radiance;
        }
    }

    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}

static bool runtime_native_3d_render_shade_material(float* luminance_buffer,
                                                    int luminance_stride,
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

    if (!luminance_buffer || luminance_stride <= 0 || width <= 0 || height <= 0 || !scene ||
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
            const int local_y = y - start_y;
            const int local_x = x - start_x;
            size_t idx = (size_t)local_y * (size_t)luminance_stride + (size_t)local_x;
            if (runtime_native_3d_render_trace_visible_emitter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               &emitter_hit)) {
                runtime_native_3d_render_write_emitter_luminance(luminance_buffer,
                                                                 idx,
                                                                 &emitter_hit,
                                                                 &stats);
                continue;
            }
            if (!RuntimeMaterialResponse3D_ShadePixel(scene,
                                                      projector,
                                                      (double)x,
                                                      (double)y,
                                                      sampling,
                                                      &result)) {
                continue;
            }
            stats.hitPixelCount += 1;
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
            luminance_buffer[idx] = (float)result.radiance;
        }
    }

    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}

static bool runtime_native_3d_render_shade_emission_transparency(
    float* luminance_buffer,
    int luminance_stride,
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

    if (!luminance_buffer || luminance_stride <= 0 || width <= 0 || height <= 0 || !scene ||
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
            const int local_y = y - start_y;
            const int local_x = x - start_x;
            size_t idx = (size_t)local_y * (size_t)luminance_stride + (size_t)local_x;
            if (runtime_native_3d_render_trace_visible_emitter(scene,
                                                               projector,
                                                               (double)x,
                                                               (double)y,
                                                               &emitter_hit)) {
                runtime_native_3d_render_write_emitter_luminance(luminance_buffer,
                                                                 idx,
                                                                 &emitter_hit,
                                                                 &stats);
                continue;
            }
            if (!RuntimeEmissionTransparency3D_ShadePixel(scene,
                                                          projector,
                                                          (double)x,
                                                          (double)y,
                                                          sampling,
                                                          &result)) {
                continue;
            }
            stats.hitPixelCount += 1;
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
            luminance_buffer[idx] = (float)result.radiance;
        }
    }

    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}

static bool runtime_native_3d_render_dispatch_integrator(float* luminance_buffer,
                                                         int luminance_stride,
                                                         int integrator_id,
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
    /* Keep the native renderer dispatch explicit so later 3D tiers can add
     * focused shader paths without overloading the entrypoint itself. */
    switch ((RayTracing3DIntegratorId)integrator_id) {
        case RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT:
            return runtime_native_3d_render_shade_direct_light(luminance_buffer,
                                                               luminance_stride,
                                                               width,
                                                               height,
                                                               start_x,
                                                               start_y,
                                                               end_x,
                                                               end_y,
                                                               scene,
                                                               projector,
                                                               out_stats);
        case RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE:
            return runtime_native_3d_render_shade_diffuse_bounce(luminance_buffer,
                                                                 luminance_stride,
                                                                 width,
                                                                 height,
                                                                 start_x,
                                                                 start_y,
                                                                 end_x,
                                                                 end_y,
                                                                 scene,
                                                                 projector,
                                                                 sampling,
                                                                 out_stats);
        case RAY_TRACING_3D_INTEGRATOR_MATERIAL:
            return runtime_native_3d_render_shade_material(luminance_buffer,
                                                           luminance_stride,
                                                           width,
                                                           height,
                                                           start_x,
                                                           start_y,
                                                           end_x,
                                                           end_y,
                                                           scene,
                                                           projector,
                                                           sampling,
                                                           out_stats);
        case RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY:
            return runtime_native_3d_render_shade_emission_transparency(luminance_buffer,
                                                                        luminance_stride,
                                                                        width,
                                                                        height,
                                                                        start_x,
                                                                        start_y,
                                                                        end_x,
                                                                        end_y,
                                                                        scene,
                                                                        projector,
                                                                        sampling,
                                                                        out_stats);
        case RAY_TRACING_3D_INTEGRATOR_DISNEY:
        default:
            return false;
    }
}

void RuntimeNative3DRenderStats_Accumulate(RuntimeNative3DRenderStats* dst,
                                           const RuntimeNative3DRenderStats* src) {
    if (!dst || !src) return;
    dst->hitPixelCount += src->hitPixelCount;
    dst->visiblePixelCount += src->visiblePixelCount;
    dst->bouncePixelCount += src->bouncePixelCount;
    dst->secondaryRayCount += src->secondaryRayCount;
    dst->secondaryHitCount += src->secondaryHitCount;
    dst->secondaryContributingHitCount += src->secondaryContributingHitCount;
    if (src->maxRadiance > dst->maxRadiance) {
        dst->maxRadiance = src->maxRadiance;
    }
    if (src->maxBounceRadiance > dst->maxBounceRadiance) {
        dst->maxBounceRadiance = src->maxBounceRadiance;
    }
    dst->totalBounceRadiance += src->totalBounceRadiance;
}

bool RuntimeNative3DPrepareFrame(RuntimeNative3DPreparedFrame* out_frame,
                                 int width,
                                 int height,
                                 double normalized_t,
                                 double live_light_x,
                                 double live_light_y) {
    return RuntimeNative3DPrepareFrameWithSampling(out_frame,
                                                   width,
                                                   height,
                                                   normalized_t,
                                                   live_light_x,
                                                   live_light_y,
                                                   NULL);
}

bool RuntimeNative3DPrepareFrameWithSampling(RuntimeNative3DPreparedFrame* out_frame,
                                             int width,
                                             int height,
                                             double normalized_t,
                                             double live_light_x,
                                             double live_light_y,
                                             const RuntimeNative3DSamplingContext* sampling) {
    RuntimeNative3DPreparedFrame frame = {0};

    if (!out_frame || width <= 0 || height <= 0) return false;

    RuntimeNative3DTileOccupancy_Init(&frame.tileOccupancy);
    RuntimeScene3D_Init(&frame.scene);
    if (!runtime_native_3d_render_build_live_scene(&frame.scene,
                                                   normalized_t,
                                                   live_light_x,
                                                   live_light_y)) {
        RuntimeScene3D_Free(&frame.scene);
        return false;
    }

    if (!RuntimeCameraProjector3D_Build(&frame.scene.camera, width, height, &frame.projector)) {
        RuntimeScene3D_Free(&frame.scene);
        return false;
    }

    frame.width = width;
    frame.height = height;
    if (sampling) {
        frame.sampling = *sampling;
    }
    frame.valid = true;
    *out_frame = frame;
    return true;
}

void RuntimeNative3DPreparedFrame_Free(RuntimeNative3DPreparedFrame* frame) {
    if (!frame) return;
    RuntimeNative3DTileOccupancy_Free(&frame->tileOccupancy);
    RuntimeScene3D_Free(&frame->scene);
    memset(frame, 0, sizeof(*frame));
}

bool RuntimeNative3DRenderPreparedRegion(uint8_t* pixel_buffer,
                                         RayTracing3DIntegratorId integrator_id,
                                         const RuntimeNative3DPreparedFrame* frame,
                                         int start_x,
                                         int start_y,
                                         int end_x,
                                         int end_y,
                                         RuntimeNative3DRenderStats* out_stats) {
    float* luminance_buffer = NULL;
    const int region_width = end_x - start_x;
    const int region_height = end_y - start_y;
    bool ok = false;

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!pixel_buffer || !frame || !frame->valid) return false;
    if (region_width <= 0 || region_height <= 0) return true;
    luminance_buffer =
        (float*)calloc((size_t)region_width * (size_t)region_height, sizeof(*luminance_buffer));
    if (!luminance_buffer) return false;
    ok = RuntimeNative3DRenderPreparedRegionLuminance(luminance_buffer,
                                                      region_width,
                                                      integrator_id,
                                                      frame,
                                                      start_x,
                                                      start_y,
                                                      end_x,
                                                      end_y,
                                                      out_stats);
    if (ok) {
        runtime_native_3d_render_resolve_luminance_region_to_pixels(pixel_buffer,
                                                                    frame->width,
                                                                    luminance_buffer,
                                                                    region_width,
                                                                    start_x,
                                                                    start_y,
                                                                    end_x,
                                                                    end_y);
    }
    free(luminance_buffer);
    return ok;
}

bool RuntimeNative3DRenderPreparedRegionLuminance(float* luminance_buffer,
                                                  int luminance_stride,
                                                  RayTracing3DIntegratorId integrator_id,
                                                  const RuntimeNative3DPreparedFrame* frame,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y,
                                                  RuntimeNative3DRenderStats* out_stats) {
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!luminance_buffer || luminance_stride <= 0 || !frame || !frame->valid) return false;
    return runtime_native_3d_render_dispatch_integrator(luminance_buffer,
                                                        luminance_stride,
                                                        integrator_id,
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
}

bool RuntimeNative3DPrepareFrameTileOccupancy(RuntimeNative3DPreparedFrame* frame, int tile_size) {
    if (!frame || !frame->valid) return false;
    return RuntimeNative3DTileOccupancy_Build(&frame->tileOccupancy,
                                              &frame->scene,
                                              &frame->projector,
                                              tile_size);
}

bool RuntimeNative3DPreparedRegionMayContainGeometry(const RuntimeNative3DPreparedFrame* frame,
                                                     int start_x,
                                                     int start_y,
                                                     int end_x,
                                                     int end_y) {
    if (!frame || !frame->valid) return true;
    return RuntimeNative3DTileOccupancy_RegionMayContainGeometry(&frame->tileOccupancy,
                                                                 start_x,
                                                                 start_y,
                                                                 end_x,
                                                                 end_y);
}

bool RuntimeNative3DRenderToPixelBuffer(uint8_t* pixel_buffer,
                                        RayTracing3DIntegratorId integrator_id,
                                        int width,
                                        int height,
                                        double normalized_t,
                                        double live_light_x,
                                        double live_light_y,
                                        RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(pixel_buffer,
                                                                  integrator_id,
                                                                  width,
                                                                  height,
                                                                  normalized_t,
                                                                  live_light_x,
                                                                  live_light_y,
                                                                  NULL,
                                                                  1,
                                                                  out_stats);
}

bool RuntimeNative3DRenderToPixelBufferWithSampling(uint8_t* pixel_buffer,
                                                    RayTracing3DIntegratorId integrator_id,
                                                    int width,
                                                    int height,
                                                    double normalized_t,
                                                    double live_light_x,
                                                    double live_light_y,
                                                    const RuntimeNative3DSamplingContext* sampling,
                                                    RuntimeNative3DRenderStats* out_stats) {
    return RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(pixel_buffer,
                                                                  integrator_id,
                                                                  width,
                                                                  height,
                                                                  normalized_t,
                                                                  live_light_x,
                                                                  live_light_y,
                                                                  sampling,
                                                                  1,
                                                                  out_stats);
}

static RuntimeNative3DSamplingContext runtime_native_3d_render_resolve_subpass_sampling(
    const RuntimeNative3DSamplingContext* sampling,
    uint32_t sequence_offset) {
    RuntimeNative3DSamplingContext resolved = {0};
    if (sampling) {
        resolved = *sampling;
    }
    resolved.sampleSequence += sequence_offset;
    if (resolved.sampleSequence == 0U) {
        resolved.sampleSequence = sequence_offset + 1U;
    }
    return resolved;
}

bool RuntimeNative3DRenderToPixelBufferWithSamplingTemporal(
    uint8_t* pixel_buffer,
    RayTracing3DIntegratorId integrator_id,
    int width,
    int height,
    double normalized_t,
    double live_light_x,
    double live_light_y,
    const RuntimeNative3DSamplingContext* sampling,
    int temporal_frames,
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DPreparedFrame frame = {0};
    RuntimeNative3DTemporalAccumulation accumulation = {0};
    float* subpass_luminance = NULL;
    bool ok = false;
    const int effective_temporal_frames =
        (integrator_id == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT || temporal_frames <= 1)
            ? 1
            : temporal_frames;

    if (!pixel_buffer || width <= 0 || height <= 0) return false;
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }

    memset(pixel_buffer, 0, (size_t)width * (size_t)height);
    subpass_luminance = (float*)calloc((size_t)width * (size_t)height, sizeof(*subpass_luminance));
    if (!subpass_luminance) {
        return false;
    }
    ok = RuntimeNative3DPrepareFrameWithSampling(&frame,
                                                 width,
                                                 height,
                                                 normalized_t,
                                                 live_light_x,
                                                 live_light_y,
                                                 sampling);
    if (!ok) {
        free(subpass_luminance);
        return false;
    }
    RuntimeNative3DTemporalAccumulation_Init(&accumulation);
    ok = RuntimeNative3DTemporalAccumulation_Ensure(&accumulation, width, height);
    if (!ok) {
        free(subpass_luminance);
        RuntimeNative3DPreparedFrame_Free(&frame);
        return false;
    }
    RuntimeNative3DTemporalAccumulation_Clear(&accumulation);

    for (int subpass = 0; subpass < effective_temporal_frames; ++subpass) {
        RuntimeNative3DPreparedFrame subpass_frame = frame;
        RuntimeNative3DRenderStats subpass_stats = {0};
        memset(subpass_luminance, 0, (size_t)width * (size_t)height * sizeof(*subpass_luminance));
        subpass_frame.sampling =
            runtime_native_3d_render_resolve_subpass_sampling(sampling, (uint32_t)subpass);
        ok = RuntimeNative3DRenderPreparedRegionLuminance(subpass_luminance,
                                                          width,
                                                          integrator_id,
                                                          &subpass_frame,
                                                          0,
                                                          0,
                                                          width,
                                                          height,
                                                          &subpass_stats);
        if (!ok) {
            break;
        }
        ok = RuntimeNative3DTemporalAccumulation_AddRegion(&accumulation,
                                                           subpass_luminance,
                                                           width,
                                                           0,
                                                           0,
                                                           width,
                                                           height);
        if (!ok) {
            break;
        }
        RuntimeNative3DTemporalAccumulation_CommitSubpass(&accumulation);
        if (out_stats) {
            RuntimeNative3DRenderStats_Accumulate(out_stats, &subpass_stats);
        }
    }

    if (ok) {
        RuntimeNative3DTemporalAccumulation_ResolveRegionToPixelBuffer(&accumulation,
                                                                       pixel_buffer,
                                                                       width,
                                                                       0,
                                                                       0,
                                                                       width,
                                                                       height);
    }
    free(subpass_luminance);
    RuntimeNative3DTemporalAccumulation_Free(&accumulation);
    RuntimeNative3DPreparedFrame_Free(&frame);
    return ok;
}
