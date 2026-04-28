#include "render/runtime_native_3d_adaptive_sampling.h"

#include <stdlib.h>
#include <string.h>

#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_ray_3d.h"

static bool runtime_native_3d_adaptive_sampling_is_stable_emitter_pixel(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y) {
    RuntimeLightEmitterTrace3DResult trace = {0};
    RuntimeMaterialPayload3D payload = {0};
    Ray3D primary_ray = {0};

    if (!scene || !projector) return false;

    primary_ray = RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
    if (!RuntimeLightEmitter3D_ResolveFirstHit(scene,
                                               &primary_ray,
                                               projector->nearPlane,
                                               1.0e30,
                                               &trace)) {
        return false;
    }
    if (trace.emitterWins) {
        return true;
    }
    if (!trace.geometryHit) {
        return false;
    }
    if (!RuntimeMaterialPayload3D_ResolveFromHit(&trace.geometryHitInfo, &payload)) {
        return false;
    }
    return payload.valid && payload.emissive > 0.0 && payload.transparency <= 0.0;
}

void RuntimeNative3DAdaptiveSamplingMask_Init(RuntimeNative3DAdaptiveSamplingMask* mask) {
    if (!mask) return;
    memset(mask, 0, sizeof(*mask));
}

void RuntimeNative3DAdaptiveSamplingMask_Free(RuntimeNative3DAdaptiveSamplingMask* mask) {
    if (!mask) return;
    free(mask->stableEmitterMask);
    free(mask->activeSampleMask);
    memset(mask, 0, sizeof(*mask));
}

bool RuntimeNative3DAdaptiveSamplingMask_Ensure(RuntimeNative3DAdaptiveSamplingMask* mask,
                                                int width,
                                                int height) {
    uint8_t* stable = NULL;
    uint8_t* active = NULL;
    size_t count = 0;

    if (!mask || width <= 0 || height <= 0) return false;
    if (mask->stableEmitterMask && mask->activeSampleMask &&
        mask->width == width && mask->height == height) {
        return true;
    }

    count = (size_t)width * (size_t)height;
    stable = (uint8_t*)calloc(count, sizeof(*stable));
    active = (uint8_t*)calloc(count, sizeof(*active));
    if (!stable || !active) {
        free(stable);
        free(active);
        return false;
    }

    free(mask->stableEmitterMask);
    free(mask->activeSampleMask);
    mask->stableEmitterMask = stable;
    mask->activeSampleMask = active;
    mask->width = width;
    mask->height = height;
    return true;
}

void RuntimeNative3DAdaptiveSamplingMask_Clear(RuntimeNative3DAdaptiveSamplingMask* mask) {
    size_t count = 0;
    if (!mask || !mask->stableEmitterMask || !mask->activeSampleMask ||
        mask->width <= 0 || mask->height <= 0) {
        return;
    }
    count = (size_t)mask->width * (size_t)mask->height;
    memset(mask->stableEmitterMask, 0, count * sizeof(*mask->stableEmitterMask));
    memset(mask->activeSampleMask, 0, count * sizeof(*mask->activeSampleMask));
}

bool RuntimeNative3DAdaptiveSampling_ShouldUse(RayTracing3DIntegratorId integrator_id,
                                               int temporal_frames) {
    return temporal_frames > 1 &&
           (integrator_id == RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY ||
            integrator_id == RAY_TRACING_3D_INTEGRATOR_DISNEY);
}

bool RuntimeNative3DAdaptiveSampling_BuildStableEmitterMask(
    RuntimeNative3DAdaptiveSamplingMask* mask,
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    int start_x,
    int start_y,
    int end_x,
    int end_y) {
    if (!mask || !mask->stableEmitterMask || !mask->activeSampleMask || !scene || !projector) {
        return false;
    }
    if (start_x >= end_x || start_y >= end_y ||
        end_x - start_x != mask->width ||
        end_y - start_y != mask->height) {
        return false;
    }

    RuntimeNative3DAdaptiveSamplingMask_Clear(mask);
    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        for (int x = start_x; x < end_x; ++x) {
            const int local_x = x - start_x;
            const size_t idx = (size_t)local_y * (size_t)mask->width + (size_t)local_x;
            const bool stable = runtime_native_3d_adaptive_sampling_is_stable_emitter_pixel(
                scene,
                projector,
                (double)x,
                (double)y);
            mask->stableEmitterMask[idx] = stable ? 1u : 0u;
            mask->activeSampleMask[idx] = stable ? 0u : 1u;
        }
    }
    return true;
}

bool RuntimeNative3DAdaptiveSampling_HasActiveSamples(
    const RuntimeNative3DAdaptiveSamplingMask* mask) {
    size_t count = 0;
    if (!mask || !mask->activeSampleMask || mask->width <= 0 || mask->height <= 0) {
        return false;
    }
    count = (size_t)mask->width * (size_t)mask->height;
    for (size_t i = 0; i < count; ++i) {
        if (mask->activeSampleMask[i]) {
            return true;
        }
    }
    return false;
}

bool RuntimeNative3DAdaptiveSampling_RenderPreparedRegionRadianceRGBMasked(
    float* radiance_buffer,
    int radiance_stride,
    RayTracing3DIntegratorId integrator_id,
    const RuntimeNative3DPreparedFrame* frame,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    const uint8_t* active_mask,
    int active_mask_stride,
    RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DRenderStats stats = {0};

    if (out_stats) {
        memset(out_stats, 0, sizeof(*out_stats));
    }
    if (!active_mask) {
        return RuntimeNative3DRenderPreparedRegionRadianceRGB(radiance_buffer,
                                                              radiance_stride,
                                                              integrator_id,
                                                              frame,
                                                              start_x,
                                                              start_y,
                                                              end_x,
                                                              end_y,
                                                              out_stats);
    }
    if (!radiance_buffer || radiance_stride <= 0 || active_mask_stride <= 0 ||
        !frame || !frame->valid || start_x >= end_x || start_y >= end_y) {
        return false;
    }

    for (int y = start_y; y < end_y; ++y) {
        const int local_y = y - start_y;
        int run_start = -1;
        for (int x = start_x; x <= end_x; ++x) {
            const int local_x = x - start_x;
            const bool active =
                x < end_x &&
                active_mask[(size_t)local_y * (size_t)active_mask_stride +
                            (size_t)local_x] != 0u;
            if (active && run_start < 0) {
                run_start = x;
            }
            if ((!active || x == end_x) && run_start >= 0) {
                RuntimeNative3DRenderStats run_stats = {0};
                float* run_buffer = radiance_buffer +
                                    (((size_t)local_y * (size_t)radiance_stride +
                                      (size_t)(run_start - start_x)) *
                                     RUNTIME_NATIVE_3D_RADIANCE_CHANNELS);
                if (!RuntimeNative3DRenderPreparedRegionRadianceRGB(run_buffer,
                                                                    radiance_stride,
                                                                    integrator_id,
                                                                    frame,
                                                                    run_start,
                                                                    y,
                                                                    x,
                                                                    y + 1,
                                                                    &run_stats)) {
                    return false;
                }
                RuntimeNative3DRenderStats_Accumulate(&stats, &run_stats);
                run_start = -1;
            }
        }
    }

    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}
