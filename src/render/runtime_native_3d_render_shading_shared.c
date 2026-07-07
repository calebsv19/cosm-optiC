#include "render/runtime_native_3d_render_shading_internal.h"

#include <math.h>

#include "render/runtime_volume_3d_integrate.h"

RuntimeNative3DPrimaryTrace runtime_native_3d_render_trace_primary(
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

void runtime_native_3d_render_record_disney_v2_emissive_area_stats(
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

void runtime_native_3d_render_record_disney_v2_mirror_stats(
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

void runtime_native_3d_render_apply_surface_caustic_cache(
    RuntimeCausticSurfaceCache3D* surface_cache,
    const HitInfo3D* hit,
    double* io_r,
    double* io_g,
    double* io_b,
    double* io_luma,
    bool* io_visible,
    RuntimeNative3DRenderStats* stats) {
    Vec3 radiance = vec3(0.0, 0.0, 0.0);
    double luma = 0.0;

    if (!surface_cache || !hit || !io_r || !io_g || !io_b || !io_luma || !stats) {
        return;
    }
    stats->causticSurfaceCacheSampleLookupCount += 1;
    if (!RuntimeCausticSurfaceCache3D_SampleAtHit(surface_cache, hit, &radiance)) {
        return;
    }
    luma = fmax(fmax(radiance.x, radiance.y), radiance.z);
    if (!(luma > 0.0)) return;
    *io_r += radiance.x;
    *io_g += radiance.y;
    *io_b += radiance.z;
    *io_luma = fmax(fmax(*io_r, *io_g), *io_b);
    if (io_visible) *io_visible = true;
    stats->causticSurfaceCacheSampleContributingCount += 1;
    stats->totalCausticSurfaceRadianceR += radiance.x;
    stats->totalCausticSurfaceRadianceG += radiance.y;
    stats->totalCausticSurfaceRadianceB += radiance.z;
    if (luma > stats->maxCausticSurfaceCacheRadiance) {
        stats->maxCausticSurfaceCacheRadiance = luma;
    }
}

void runtime_native_3d_render_apply_transmitted_surface_caustic_cache(
    RuntimeCausticSurfaceCache3D* surface_cache,
    const RuntimeDisneyV2_3DResult* result,
    double* io_r,
    double* io_g,
    double* io_b,
    double* io_luma,
    bool* io_visible,
    RuntimeNative3DRenderStats* stats) {
    Vec3 radiance = vec3(0.0, 0.0, 0.0);
    double throughput_r = 0.0;
    double throughput_g = 0.0;
    double throughput_b = 0.0;
    double luma = 0.0;

    if (!surface_cache || !result || !io_r || !io_g || !io_b || !io_luma || !stats) {
        return;
    }
    if (!result->primaryTransmissionContinued ||
        !result->primaryTransmissionPathState.valid ||
        !result->primaryTransmissionPathState.hit) {
        return;
    }
    stats->causticSurfaceCacheSampleLookupCount += 1;
    if (!RuntimeCausticSurfaceCache3D_SampleAtHit(surface_cache,
                                                  &result->primaryTransmissionPathState.hitInfo,
                                                  &radiance)) {
        return;
    }
    throughput_r = fmax(result->primaryTransmissionCameraThroughputR,
                        result->primaryTransmissionPathState.throughputR);
    throughput_g = fmax(result->primaryTransmissionCameraThroughputG,
                        result->primaryTransmissionPathState.throughputG);
    throughput_b = fmax(result->primaryTransmissionCameraThroughputB,
                        result->primaryTransmissionPathState.throughputB);
    radiance.x *= fmin(fmax(throughput_r, 0.0), 1.0);
    radiance.y *= fmin(fmax(throughput_g, 0.0), 1.0);
    radiance.z *= fmin(fmax(throughput_b, 0.0), 1.0);
    luma = fmax(fmax(radiance.x, radiance.y), radiance.z);
    if (!(luma > 0.0)) return;
    *io_r += radiance.x;
    *io_g += radiance.y;
    *io_b += radiance.z;
    *io_luma = fmax(fmax(*io_r, *io_g), *io_b);
    if (io_visible) *io_visible = true;
    stats->causticSurfaceCacheSampleContributingCount += 1;
    stats->totalCausticSurfaceRadianceR += radiance.x;
    stats->totalCausticSurfaceRadianceG += radiance.y;
    stats->totalCausticSurfaceRadianceB += radiance.z;
    if (luma > stats->maxCausticSurfaceCacheRadiance) {
        stats->maxCausticSurfaceCacheRadiance = luma;
    }
}
