#include "render/runtime_native_3d_render_internal.h"

#include <math.h>
#include <stddef.h>

#include "render/runtime_disney_3d.h"
#include "render/runtime_disney_v2_3d.h"
#include "render/runtime_disney_v2_caustic_sidecar_3d.h"
#include "render/runtime_direct_light_3d.h"
#include "render/runtime_diffuse_bounce_3d.h"
#include "render/runtime_emission_transparency_3d.h"
#include "render/runtime_light_emitter_3d.h"
#include "render/runtime_material_response_3d.h"
#include "render/runtime_volume_3d_integrate.h"

typedef struct RuntimeNative3DPrimaryTrace {
    RuntimePrimaryHit3DResult primary;
    RuntimeLightEmitterHit3DResult emitterHit;
    RuntimeMaterialPayload3D payload;
    bool payloadResolved;
    bool emitterWins;
} RuntimeNative3DPrimaryTrace;

static RuntimeNative3DPrimaryTrace runtime_native_3d_render_trace_primary(
    const RuntimeScene3D* scene,
    const RuntimeCameraProjector3D* projector,
    double pixel_x,
    double pixel_y) {
    RuntimeNative3DPrimaryTrace trace = {0};
    RuntimeLightEmitterHit3DResult emitter_hit = {0};

    if (!scene || !projector) return trace;

    RuntimeDirectLight3D_TracePrimaryHit(scene,
                                         projector,
                                         pixel_x,
                                         pixel_y,
                                         &trace.primary);
    if (trace.primary.hit) {
        trace.payloadResolved =
            RuntimeMaterialPayload3D_ResolveFromHit(&trace.primary.hitInfo, &trace.payload);
    }
    if (RuntimeLightEmitter3D_IntersectRay(scene,
                                           &trace.primary.primaryRay,
                                           projector->nearPlane,
                                           HUGE_VAL,
                                           &emitter_hit) &&
        (!trace.primary.hit || emitter_hit.t < trace.primary.hitInfo.t)) {
        const RuntimeVisibility3DTransmittance transmittance =
            RuntimeVolume3D_TransmittanceAlongRayRGB(&scene->volume,
                                                     &trace.primary.primaryRay,
                                                     projector->nearPlane,
                                                     emitter_hit.t);
        emitter_hit.radiance *= transmittance.luma;
        trace.emitterHit = emitter_hit;
        trace.emitterWins = true;
    }

    return trace;
}

static void runtime_native_3d_render_record_disney_v2_emissive_area_stats(
    RuntimeNative3DRenderStats* stats,
    const RuntimeDisneyV2_3DResult* result) {
    if (!stats || !result) return;
    if (result->emissiveAreaCandidateCount > stats->emissiveAreaCandidateCount) {
        stats->emissiveAreaCandidateCount = result->emissiveAreaCandidateCount;
    }
    stats->emissiveAreaSelectedCandidateCount +=
        result->emissiveAreaSelectedCandidateCount;
    stats->emissiveAreaVisibilityRayCount += result->emissiveAreaVisibilityRayCount;
    stats->emissiveAreaPrimarySampleCount += result->emissiveAreaPrimarySampleCount;
    stats->emissiveAreaRecursiveSampleCount += result->emissiveAreaRecursiveSampleCount;
    stats->emissiveAreaRecursivePolicySkipCount +=
        result->emissiveAreaRecursivePolicySkipCount;
    stats->emissiveAreaRecursiveCandidateCapSkipCount +=
        result->emissiveAreaRecursiveCandidateCapSkipCount;
    stats->emissiveAreaRecursiveTriangleCapSkipCount +=
        result->emissiveAreaRecursiveTriangleCapSkipCount;
    if (result->emissiveAreaRecursiveCandidateCap >
        stats->emissiveAreaRecursiveCandidateCap) {
        stats->emissiveAreaRecursiveCandidateCap =
            result->emissiveAreaRecursiveCandidateCap;
    }
    if (result->emissiveAreaRecursiveTriangleCap >
        stats->emissiveAreaRecursiveTriangleCap) {
        stats->emissiveAreaRecursiveTriangleCap =
            result->emissiveAreaRecursiveTriangleCap;
    }
    stats->emissiveAreaFullScanFallbackCount += result->emissiveAreaFullScanFallbackCount;
}

static void runtime_native_3d_render_record_disney_v2_mirror_stats(
    RuntimeNative3DRenderStats* stats,
    const RuntimeDisneyV2_3DResult* result) {
    if (!stats || !result) return;

    if (result->mirrorDominance > stats->maxMirrorDominance) {
        stats->maxMirrorDominance = result->mirrorDominance;
    }
    if (result->mirrorDominance >= 0.75) {
        stats->mirrorDominantPixelCount += 1;
    }
    if (result->mirrorBaseRadianceBeforeAttenuation > 1e-9 &&
        result->mirrorBaseRadianceAfterAttenuation <
            result->mirrorBaseRadianceBeforeAttenuation * 0.75) {
        stats->mirrorBaseAttenuatedPixelCount += 1;
    }
    if (result->specularReflectionHitCount > 0) {
        stats->mirrorReflectionHitPixelCount += 1;
    }
    if (result->specularReflectionEmitterHitCount > 0) {
        stats->mirrorEmitterReflectionPixelCount += 1;
    }
    if (result->specularReflectionGeometryHitCount > 0) {
        stats->mirrorGeometryReflectionPixelCount += 1;
    }
    if (result->specularReflectionRadiance > stats->maxMirrorSpecularReflectionRadiance) {
        stats->maxMirrorSpecularReflectionRadiance = result->specularReflectionRadiance;
    }
    if (result->mirrorBaseRadianceBeforeAttenuation >
        stats->maxMirrorBaseRadianceBeforeAttenuation) {
        stats->maxMirrorBaseRadianceBeforeAttenuation =
            result->mirrorBaseRadianceBeforeAttenuation;
    }
    if (result->mirrorBaseRadianceAfterAttenuation >
        stats->maxMirrorBaseRadianceAfterAttenuation) {
        stats->maxMirrorBaseRadianceAfterAttenuation =
            result->mirrorBaseRadianceAfterAttenuation;
    }
    stats->totalMirrorSpecularReflectionRadiance += result->specularReflectionRadiance;
    stats->totalMirrorBaseRadianceBeforeAttenuation +=
        result->mirrorBaseRadianceBeforeAttenuation;
    stats->totalMirrorBaseRadianceAfterAttenuation +=
        result->mirrorBaseRadianceAfterAttenuation;
}

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
                                                                   sampling);
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
                                                                   sampling);
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
                                                                   sampling);
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
                                                                   sampling);
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
                                                                   sampling);
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

static bool runtime_native_3d_render_shade_disney_v2(float* radiance_buffer,
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
    RuntimeDisneyV2CausticSidecarProbe3D caustic_probe = {0};
    const bool caustic_sidecar_active =
        RuntimeDisneyV2_3D_BuildCausticSidecarProbe(scene, &caustic_probe);

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
                                                                             &stats);
                continue;
            }
            if (!RuntimeDisneyV2_3D_ShadePrimaryHitWithPayload(
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
                                                                   sampling);
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
            if (caustic_sidecar_active) {
                RuntimeDisneyV2CausticSidecarContribution3D caustic = {0};
                stats.causticSidecarSampleCount += 1;
                if (RuntimeDisneyV2_3D_EvaluateCausticSidecar(&caustic_probe,
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
        case RAY_TRACING_3D_INTEGRATOR_DISNEY_V2:
            return runtime_native_3d_render_shade_disney_v2(radiance_buffer,
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
