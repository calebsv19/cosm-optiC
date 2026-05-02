#include "render/runtime_native_3d_render.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "import/fluid_volume_import_3d.h"
#include "render/integrators/hybrid/integrator_tonemap.h"
#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_native_3d_render_internal.h"
#include "render/runtime_camera_3d_rays.h"
#include "render/runtime_native_3d_denoise.h"
#include "render/runtime_native_3d_feature_buffer.h"
#include "render/runtime_native_3d_adaptive_sampling.h"
#include "render/runtime_native_3d_temporal_accum.h"
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

static RuntimeVolume3DSourceKind runtime_native_3d_render_map_volume_source_kind(
    int config_kind) {
    switch (config_kind) {
        case VOLUME_SOURCE_MANIFEST:
            return RUNTIME_VOLUME_3D_SOURCE_MANIFEST;
        case VOLUME_SOURCE_RAW_VF3D:
            return RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D;
        case VOLUME_SOURCE_PACK:
            return RUNTIME_VOLUME_3D_SOURCE_PACK;
        case VOLUME_SOURCE_NONE:
        default:
            return RUNTIME_VOLUME_3D_SOURCE_NONE;
    }
}

static bool runtime_native_3d_render_attach_configured_volume(RuntimeScene3D* scene) {
    RuntimeVolume3DSourceKind source_kind = RUNTIME_VOLUME_3D_SOURCE_NONE;
    char diagnostics[128] = {0};

    if (!scene) return false;

    scene->volume.affectsLighting = animSettings.volumeAffectsLighting;
    scene->volume.debugOverlayEnabled = animSettings.volumeDebugOverlayEnabled;
    if (!animSettings.volumeInteractionEnabled) {
        return true;
    }

    source_kind = runtime_native_3d_render_map_volume_source_kind(animSettings.volumeSourceKind);
    if (source_kind == RUNTIME_VOLUME_3D_SOURCE_NONE ||
        animSettings.volumeSourcePath[0] == '\0') {
        return false;
    }

    if (!fluid_volume_import_3d_load_source(animSettings.volumeSourcePath,
                                            source_kind,
                                            &scene->volume,
                                            diagnostics,
                                            sizeof(diagnostics))) {
        RuntimeVolumeAttachment3D_Reset(&scene->volume);
        return false;
    }

    scene->volume.affectsLighting = animSettings.volumeAffectsLighting;
    scene->volume.debugOverlayEnabled = animSettings.volumeDebugOverlayEnabled;
    return true;
}

static double runtime_native_3d_render_clamp_unit(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

uint8_t RuntimeNative3DResolveEnvironmentByte(void) {
    double value = animSettings.environmentBrightness;
    if (value < 0.0) value = 0.0;
    if (value > 255.0) value = 255.0;
    return (uint8_t)lround(value);
}

void RuntimeNative3DFillPixelBufferEnvironment(uint8_t* pixel_buffer, size_t pixel_count) {
    const uint8_t environment = RuntimeNative3DResolveEnvironmentByte();
    if (!pixel_buffer) return;

    for (size_t i = 0; i < pixel_count; ++i) {
        const size_t base = i * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        pixel_buffer[base] = environment;
        pixel_buffer[base + 1u] = environment;
        pixel_buffer[base + 2u] = environment;
        pixel_buffer[base + 3u] = 0xFFu;
    }
}

void RuntimeNative3DResolveRadianceRegionToPixels(
    uint8_t* pixel_buffer,
    int pixel_width,
    const float* radiance_buffer,
    int radiance_stride,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    if (!pixel_buffer || !radiance_buffer || pixel_width <= 0 || radiance_stride <= 0) return;
    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        for (int x = start_x; x < end_x; ++x) {
            const int local_x = x - start_x;
            const size_t pixel_base =
                ((size_t)y * (size_t)pixel_width + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            const size_t radiance_base =
                ((size_t)local_y * (size_t)radiance_stride + (size_t)local_x) *
                (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
            const uint8_t environment = (uint8_t)lround(
                runtime_native_3d_render_clamp_unit(
                    radiance_buffer[radiance_base +
                                    RUNTIME_NATIVE_3D_RADIANCE_BACKGROUND_FLOOR_CHANNEL]) *
                255.0);
            pixel_buffer[pixel_base] = TonemapCurveToByteWithFloor(
                radiance_buffer[radiance_base], environment);
            pixel_buffer[pixel_base + 1u] = TonemapCurveToByteWithFloor(
                radiance_buffer[radiance_base + 1u], environment);
            pixel_buffer[pixel_base + 2u] = TonemapCurveToByteWithFloor(
                radiance_buffer[radiance_base + 2u], environment);
            pixel_buffer[pixel_base + 3u] = 0xFFu;
        }
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
    if (!runtime_native_3d_render_attach_configured_volume(&frame.scene)) {
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
    float* radiance_buffer = NULL;
    const int region_width = end_x - start_x;
    const int region_height = end_y - start_y;
    bool ok = false;

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!pixel_buffer || !frame || !frame->valid) return false;
    if (region_width <= 0 || region_height <= 0) return true;
    radiance_buffer = (float*)calloc((size_t)region_width * (size_t)region_height *
                                         RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                     sizeof(*radiance_buffer));
    if (!radiance_buffer) return false;
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance_buffer,
                                                        region_width,
                                                        integrator_id,
                                                        frame,
                                                        start_x,
                                                        start_y,
                                                        end_x,
                                                        end_y,
                                                        out_stats);
    if (ok) {
        RuntimeNative3DResolveRadianceRegionToPixels(pixel_buffer,
                                                     frame->width,
                                                     radiance_buffer,
                                                     region_width,
                                                     start_x,
                                                     start_y,
                                                     end_x,
                                                     end_y);
    }
    free(radiance_buffer);
    return ok;
}

bool RuntimeNative3DRenderPreparedRegionRadianceRGB(float* radiance_buffer,
                                                    int radiance_stride,
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
    if (!radiance_buffer || radiance_stride <= 0 || !frame || !frame->valid) return false;
    return runtime_native_3d_render_dispatch_integrator(radiance_buffer,
                                                        radiance_stride,
                                                        integrator_id,
                                                        frame,
                                                        start_x,
                                                        start_y,
                                                        end_x,
                                                        end_y,
                                                        out_stats);
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
    float* radiance_buffer = NULL;
    const int region_width = end_x - start_x;
    const int region_height = end_y - start_y;
    bool ok = false;
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!luminance_buffer || luminance_stride <= 0 || !frame || !frame->valid) return false;
    if (region_width <= 0 || region_height <= 0) return true;
    radiance_buffer = (float*)calloc((size_t)region_width * (size_t)region_height *
                                         RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                     sizeof(*radiance_buffer));
    if (!radiance_buffer) return false;
    ok = RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance_buffer,
                                                        region_width,
                                                        integrator_id,
                                                        frame,
                                                        start_x,
                                                        start_y,
                                                        end_x,
                                                        end_y,
                                                        out_stats);
    if (ok) {
        for (int y = 0; y < region_height; ++y) {
            for (int x = 0; x < region_width; ++x) {
                const size_t dst_index = (size_t)y * (size_t)luminance_stride + (size_t)x;
                const size_t src_base =
                    ((size_t)y * (size_t)region_width + (size_t)x) *
                    (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS;
                luminance_buffer[dst_index] = 0.2126f * radiance_buffer[src_base] +
                                              0.7152f * radiance_buffer[src_base + 1u] +
                                              0.0722f * radiance_buffer[src_base + 2u];
            }
        }
    }
    free(radiance_buffer);
    return ok;
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
    if (RuntimeVolume3D_HasActiveExtinction(&frame->scene.volume)) {
        return true;
    }
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
    uint32_t sequence_offset,
    int total_subpasses) {
    RuntimeNative3DSamplingContext resolved = {0};
    if (sampling) {
        resolved = *sampling;
    }
    resolved.sampleSequence += sequence_offset;
    if (resolved.sampleSequence == 0U) {
        resolved.sampleSequence = sequence_offset + 1U;
    }
    resolved.temporalSubpassIndex = (uint16_t)((sequence_offset > 65535u) ? 65535u : sequence_offset);
    resolved.temporalSubpassCount =
        (uint16_t)((total_subpasses <= 1) ? 1 : ((total_subpasses > 65535) ? 65535 : total_subpasses));
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
    RuntimeNative3DAdaptiveSamplingMask adaptive_mask = {0};
    RuntimeNative3DFeatureBuffer feature_buffer = {0};
    float* subpass_radiance = NULL;
    float* resolved_radiance = NULL;
    bool ok = false;
    const int effective_temporal_frames =
        (integrator_id == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT || temporal_frames <= 1)
            ? 1
            : temporal_frames;
    const bool use_adaptive_sampling =
        RuntimeNative3DAdaptiveSampling_ShouldUse(integrator_id, effective_temporal_frames);
    const bool use_denoise =
        RuntimeNative3DDenoise_ShouldApply(integrator_id,
                                           effective_temporal_frames,
                                           animSettings.disneyDenoiseEnabled);

    if (!pixel_buffer || width <= 0 || height <= 0) return false;
    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }

    RuntimeNative3DFillPixelBufferEnvironment(pixel_buffer, (size_t)width * (size_t)height);
    subpass_radiance = (float*)calloc((size_t)width * (size_t)height *
                                          RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                      sizeof(*subpass_radiance));
    resolved_radiance = (float*)calloc((size_t)width * (size_t)height *
                                           RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                                       sizeof(*resolved_radiance));
    if (!subpass_radiance || !resolved_radiance) {
        free(subpass_radiance);
        free(resolved_radiance);
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
        free(subpass_radiance);
        free(resolved_radiance);
        return false;
    }
    RuntimeNative3DTemporalAccumulation_Init(&accumulation);
    ok = RuntimeNative3DTemporalAccumulation_Ensure(&accumulation, width, height);
    if (!ok) {
        free(subpass_radiance);
        free(resolved_radiance);
        RuntimeNative3DPreparedFrame_Free(&frame);
        return false;
    }
    RuntimeNative3DTemporalAccumulation_Clear(&accumulation);
    RuntimeNative3DFeatureBuffer_Init(&feature_buffer);
    if (use_denoise &&
        (!RuntimeNative3DFeatureBuffer_Ensure(&feature_buffer, width, height) ||
         !RuntimeNative3DFeatureBuffer_RenderRegion(&feature_buffer,
                                                   &frame.scene,
                                                   &frame.projector,
                                                   0,
                                                   0,
                                                   width,
                                                   height))) {
        free(subpass_radiance);
        free(resolved_radiance);
        RuntimeNative3DFeatureBuffer_Free(&feature_buffer);
        RuntimeNative3DTemporalAccumulation_Free(&accumulation);
        RuntimeNative3DPreparedFrame_Free(&frame);
        return false;
    }
    if (use_adaptive_sampling) {
        RuntimeNative3DAdaptiveSamplingMask_Init(&adaptive_mask);
        ok = RuntimeNative3DAdaptiveSamplingMask_Ensure(&adaptive_mask, width, height) &&
             RuntimeNative3DAdaptiveSampling_BuildStableEmitterMask(&adaptive_mask,
                                                                    &frame.scene,
                                                                    &frame.projector,
                                                                    0,
                                                                    0,
                                                                    width,
                                                                    height);
        if (!ok) {
            free(subpass_radiance);
            free(resolved_radiance);
            RuntimeNative3DFeatureBuffer_Free(&feature_buffer);
            RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
            RuntimeNative3DTemporalAccumulation_Free(&accumulation);
            RuntimeNative3DPreparedFrame_Free(&frame);
            return false;
        }
    }

    for (int subpass = 0; subpass < effective_temporal_frames; ++subpass) {
        RuntimeNative3DPreparedFrame subpass_frame = frame;
        RuntimeNative3DRenderStats subpass_stats = {0};
        const uint8_t* active_mask =
            (use_adaptive_sampling && subpass > 0) ? adaptive_mask.activeSampleMask : NULL;
        const int active_mask_stride =
            (use_adaptive_sampling && subpass > 0) ? adaptive_mask.width : 0;
        if (active_mask && !RuntimeNative3DAdaptiveSampling_HasActiveSamples(&adaptive_mask)) {
            break;
        }
        memset(subpass_radiance,
               0,
               (size_t)width * (size_t)height * RUNTIME_NATIVE_3D_RADIANCE_CHANNELS *
                   sizeof(*subpass_radiance));
        subpass_frame.sampling =
            runtime_native_3d_render_resolve_subpass_sampling(sampling,
                                                              (uint32_t)subpass,
                                                              effective_temporal_frames);
        ok = RuntimeNative3DAdaptiveSampling_RenderPreparedRegionRadianceRGBMasked(
            subpass_radiance,
            width,
            integrator_id,
            &subpass_frame,
            0,
            0,
            width,
            height,
            active_mask,
            active_mask_stride,
            &subpass_stats);
        if (!ok) {
            break;
        }
        ok = RuntimeNative3DTemporalAccumulation_AddRegionSamples(&accumulation,
                                                                  subpass_radiance,
                                                                  width,
                                                                  0,
                                                                  0,
                                                                  width,
                                                                  height,
                                                                  active_mask,
                                                                  active_mask_stride);
        if (!ok) {
            break;
        }
        RuntimeNative3DTemporalAccumulation_CommitSubpass(&accumulation);
        if (out_stats) {
            RuntimeNative3DRenderStats_Accumulate(out_stats, &subpass_stats);
        }
    }

    if (ok) {
        ok = RuntimeNative3DTemporalAccumulation_ResolveRegionToRadianceBuffer(&accumulation,
                                                                               resolved_radiance,
                                                                               width,
                                                                               0,
                                                                               0,
                                                                               width,
                                                                               height);
    }
    if (ok && use_denoise) {
        ok = RuntimeNative3DDenoise_Apply(resolved_radiance, width, &feature_buffer);
    }
    if (ok) {
        RuntimeNative3DResolveRadianceRegionToPixels(pixel_buffer,
                                                     width,
                                                     resolved_radiance,
                                                     width,
                                                     0,
                                                     0,
                                                     width,
                                                     height);
    }
    free(subpass_radiance);
    free(resolved_radiance);
    RuntimeNative3DFeatureBuffer_Free(&feature_buffer);
    RuntimeNative3DAdaptiveSamplingMask_Free(&adaptive_mask);
    RuntimeNative3DTemporalAccumulation_Free(&accumulation);
    RuntimeNative3DPreparedFrame_Free(&frame);
    return ok;
}
