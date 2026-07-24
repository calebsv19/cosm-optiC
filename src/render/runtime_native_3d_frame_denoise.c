#include "render/runtime_native_3d_frame_denoise.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static bool runtime_native_3d_frame_denoise_dimensions_valid(int width,
                                                             int height,
                                                             size_t* out_pixels) {
    size_t pixel_count = 0u;
    if (width <= 0 || height <= 0) {
        return false;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return false;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS) {
        return false;
    }
    if (out_pixels) {
        *out_pixels = pixel_count;
    }
    return true;
}

static void runtime_native_3d_frame_denoise_record_stats(
    RuntimeNative3DRenderStats* stats,
    const RuntimeNative3DDenoiseDiagnostics* diagnostics) {
    if (!stats || !diagnostics) return;
    stats->denoiseTemporalFrameCount = diagnostics->temporalFrameCount;
    stats->denoiseRawPixelCount = diagnostics->rawPixelCount;
    stats->denoiseReconstructedPixelCount = diagnostics->reconstructedPixelCount;
    stats->denoiseStableInteriorSampleCount = diagnostics->stableInteriorSampleCount;
    stats->denoiseRejectedEdgeSampleCount = diagnostics->rejectedEdgeSampleCount;
    stats->denoisePreservedTransparentPixelCount =
        diagnostics->preservedTransparentPixelCount;
    stats->denoisePreservedMirrorGlossyPixelCount =
        diagnostics->preservedMirrorGlossyPixelCount;
    stats->denoiseSkippedUnstableTemporalPixelCount =
        diagnostics->skippedUnstableTemporalPixelCount;
    stats->denoiseSkippedInvalidSurfacePixelCount =
        diagnostics->skippedInvalidSurfacePixelCount;
    stats->denoiseRawRadianceLumaTotal = diagnostics->rawRadianceLumaTotal;
    stats->denoiseReconstructedRadianceLumaTotal =
        diagnostics->reconstructedRadianceLumaTotal;
}

void RuntimeNative3DFrameDenoise_Init(RuntimeNative3DFrameDenoise* frame_denoise) {
    if (!frame_denoise) return;
    memset(frame_denoise, 0, sizeof(*frame_denoise));
    RuntimeNative3DFeatureBuffer_Init(&frame_denoise->featureBuffer);
}

void RuntimeNative3DFrameDenoise_Free(RuntimeNative3DFrameDenoise* frame_denoise) {
    if (!frame_denoise) return;
    free(frame_denoise->radianceBuffer);
    free(frame_denoise->temporalActivityBuffer);
    RuntimeNative3DFeatureBuffer_Free(&frame_denoise->featureBuffer);
    memset(frame_denoise, 0, sizeof(*frame_denoise));
}

bool RuntimeNative3DFrameDenoise_Prepare(RuntimeNative3DFrameDenoise* frame_denoise,
                                         int width,
                                         int height,
                                         RayTracing3DIntegratorId integrator_id,
                                         int temporal_frames) {
    size_t pixel_count = 0u;
    if (!frame_denoise ||
        !runtime_native_3d_frame_denoise_dimensions_valid(width, height, &pixel_count) ||
        temporal_frames <= 1) {
        return false;
    }

    RuntimeNative3DFrameDenoise_Free(frame_denoise);
    RuntimeNative3DFrameDenoise_Init(frame_denoise);
    frame_denoise->radianceBuffer =
        (float*)calloc(pixel_count * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
                       sizeof(*frame_denoise->radianceBuffer));
    frame_denoise->temporalActivityBuffer =
        (float*)calloc(pixel_count, sizeof(*frame_denoise->temporalActivityBuffer));
    if (!frame_denoise->radianceBuffer || !frame_denoise->temporalActivityBuffer ||
        !RuntimeNative3DFeatureBuffer_Ensure(&frame_denoise->featureBuffer, width, height)) {
        RuntimeNative3DFrameDenoise_Free(frame_denoise);
        return false;
    }
    RuntimeNative3DFeatureBuffer_Clear(&frame_denoise->featureBuffer);
    frame_denoise->integratorId = integrator_id;
    frame_denoise->width = width;
    frame_denoise->height = height;
    frame_denoise->temporalFrames = temporal_frames;
    return true;
}

bool RuntimeNative3DFrameDenoise_GatherUnit(RuntimeNative3DFrameDenoise* frame_denoise,
                                            RuntimeNative3DRenderUnit* unit) {
    if (!frame_denoise || !unit || frame_denoise->applied || !unit->useDenoise ||
        unit->integratorId != frame_denoise->integratorId ||
        unit->temporalFrames != frame_denoise->temporalFrames ||
        unit->committedSubpasses <= 0 ||
        unit->committedSubpasses > frame_denoise->temporalFrames ||
        unit->startX < 0 || unit->startY < 0 ||
        unit->endX > frame_denoise->width || unit->endY > frame_denoise->height ||
        unit->width != unit->endX - unit->startX ||
        unit->height != unit->endY - unit->startY ||
        unit->width <= 0 || unit->height <= 0 || !unit->resolvedRadiance ||
        !unit->accumulation.activityBuffer || !unit->featureBuffer.normalBuffer ||
        !unit->featureBuffer.depthBuffer || !unit->featureBuffer.reflectivityBuffer ||
        !unit->featureBuffer.roughnessBuffer || !unit->featureBuffer.transparencyBuffer ||
        !unit->featureBuffer.hitMaskBuffer ||
        !unit->featureBuffer.directLightVisibilityOutcomeBuffer ||
        !unit->featureBuffer.triangleIndexBuffer ||
        !unit->featureBuffer.sceneObjectIndexBuffer) {
        return false;
    }

    if (!RuntimeNative3DTemporalAccumulation_ResolveRegionToRadianceBuffer(
            &unit->accumulation,
            unit->resolvedRadiance,
            unit->width,
            0,
            0,
            unit->width,
            unit->height)) {
        return false;
    }

    for (int y = 0; y < unit->height; ++y) {
        const size_t src = (size_t)y * (size_t)unit->width;
        const size_t dst =
            (size_t)(unit->startY + y) * (size_t)frame_denoise->width +
            (size_t)unit->startX;
        const size_t row_pixels = (size_t)unit->width;
        memcpy(frame_denoise->radianceBuffer +
                   dst * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
               unit->resolvedRadiance +
                   src * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
               row_pixels * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS *
                   sizeof(*frame_denoise->radianceBuffer));
        memcpy(frame_denoise->temporalActivityBuffer + dst,
               unit->accumulation.activityBuffer + src,
               row_pixels * sizeof(*frame_denoise->temporalActivityBuffer));
        memcpy(frame_denoise->featureBuffer.normalBuffer + dst * 3u,
               unit->featureBuffer.normalBuffer + src * 3u,
               row_pixels * 3u * sizeof(*frame_denoise->featureBuffer.normalBuffer));
        memcpy(frame_denoise->featureBuffer.depthBuffer + dst,
               unit->featureBuffer.depthBuffer + src,
               row_pixels * sizeof(*frame_denoise->featureBuffer.depthBuffer));
        memcpy(frame_denoise->featureBuffer.reflectivityBuffer + dst,
               unit->featureBuffer.reflectivityBuffer + src,
               row_pixels * sizeof(*frame_denoise->featureBuffer.reflectivityBuffer));
        memcpy(frame_denoise->featureBuffer.roughnessBuffer + dst,
               unit->featureBuffer.roughnessBuffer + src,
               row_pixels * sizeof(*frame_denoise->featureBuffer.roughnessBuffer));
        memcpy(frame_denoise->featureBuffer.transparencyBuffer + dst,
               unit->featureBuffer.transparencyBuffer + src,
               row_pixels * sizeof(*frame_denoise->featureBuffer.transparencyBuffer));
        memcpy(frame_denoise->featureBuffer.hitMaskBuffer + dst,
               unit->featureBuffer.hitMaskBuffer + src,
               row_pixels * sizeof(*frame_denoise->featureBuffer.hitMaskBuffer));
        memcpy(frame_denoise->featureBuffer.directLightVisibilityOutcomeBuffer + dst,
               unit->featureBuffer.directLightVisibilityOutcomeBuffer + src,
               row_pixels *
                   sizeof(*frame_denoise->featureBuffer.directLightVisibilityOutcomeBuffer));
        memcpy(frame_denoise->featureBuffer.triangleIndexBuffer + dst,
               unit->featureBuffer.triangleIndexBuffer + src,
               row_pixels * sizeof(*frame_denoise->featureBuffer.triangleIndexBuffer));
        memcpy(frame_denoise->featureBuffer.sceneObjectIndexBuffer + dst,
               unit->featureBuffer.sceneObjectIndexBuffer + src,
               row_pixels * sizeof(*frame_denoise->featureBuffer.sceneObjectIndexBuffer));
    }
    return true;
}

bool RuntimeNative3DFrameDenoise_Apply(RuntimeNative3DFrameDenoise* frame_denoise,
                                       RuntimeNative3DRenderStats* out_stats) {
    RuntimeNative3DDenoiseDiagnostics diagnostics = {0};
    if (!frame_denoise || frame_denoise->applied || !frame_denoise->radianceBuffer ||
        !frame_denoise->temporalActivityBuffer) {
        return false;
    }
    if (!RuntimeNative3DDenoise_ApplyForIntegrator(
            frame_denoise->radianceBuffer,
            frame_denoise->width,
            &frame_denoise->featureBuffer,
            frame_denoise->integratorId,
            frame_denoise->temporalFrames,
            frame_denoise->temporalActivityBuffer,
            frame_denoise->width,
            out_stats ? &diagnostics : NULL)) {
        return false;
    }
    frame_denoise->applied = true;
    runtime_native_3d_frame_denoise_record_stats(out_stats, &diagnostics);
    return true;
}

bool RuntimeNative3DFrameDenoise_ResolveUnitToPixels(
    const RuntimeNative3DFrameDenoise* frame_denoise,
    RuntimeNative3DRenderUnit* unit,
    uint8_t* pixel_buffer,
    int pixel_width) {
    if (!frame_denoise || !frame_denoise->applied || !unit || !pixel_buffer ||
        pixel_width <= 0 || !unit->resolvedRadiance ||
        unit->startX < 0 || unit->startY < 0 ||
        unit->endX > frame_denoise->width || unit->endY > frame_denoise->height) {
        return false;
    }
    for (int y = 0; y < unit->height; ++y) {
        const size_t src =
            (size_t)(unit->startY + y) * (size_t)frame_denoise->width +
            (size_t)unit->startX;
        const size_t dst = (size_t)y * (size_t)unit->width;
        memcpy(unit->resolvedRadiance +
                   dst * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
               frame_denoise->radianceBuffer +
                   src * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS,
               (size_t)unit->width * (size_t)RUNTIME_NATIVE_3D_RADIANCE_CHANNELS *
                   sizeof(*unit->resolvedRadiance));
    }
    RuntimeNative3DResolveRadianceRegionToPixels(pixel_buffer,
                                                 pixel_width,
                                                 unit->resolvedRadiance,
                                                 unit->width,
                                                 unit->startX,
                                                 unit->startY,
                                                 unit->endX,
                                                 unit->endY);
    return true;
}
