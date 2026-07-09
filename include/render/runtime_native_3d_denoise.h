#ifndef RENDER_RUNTIME_NATIVE_3D_DENOISE_H
#define RENDER_RUNTIME_NATIVE_3D_DENOISE_H

#include <stdbool.h>

#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_native_3d_feature_buffer.h"

typedef struct {
    int temporalFrameCount;
    int rawPixelCount;
    int reconstructedPixelCount;
    int stableInteriorSampleCount;
    int rejectedEdgeSampleCount;
    int preservedTransparentPixelCount;
    int preservedMirrorGlossyPixelCount;
    int skippedUnstableTemporalPixelCount;
    int skippedInvalidSurfacePixelCount;
    double rawRadianceLumaTotal;
    double reconstructedRadianceLumaTotal;
} RuntimeNative3DDenoiseDiagnostics;

void RuntimeNative3DDenoiseDiagnostics_Reset(RuntimeNative3DDenoiseDiagnostics* diagnostics);
void RuntimeNative3DDenoiseDiagnostics_Accumulate(
    RuntimeNative3DDenoiseDiagnostics* dst,
    const RuntimeNative3DDenoiseDiagnostics* src);
bool RuntimeNative3DDenoise_ShouldApply(RayTracing3DIntegratorId integrator_id,
                                        int temporal_frames,
                                        bool denoise_enabled);
bool RuntimeNative3DDenoise_Apply(float* radiance_buffer,
                                  int radiance_stride,
                                  const RuntimeNative3DFeatureBuffer* features);
bool RuntimeNative3DDenoise_ApplyForIntegrator(
    float* radiance_buffer,
    int radiance_stride,
    const RuntimeNative3DFeatureBuffer* features,
    RayTracing3DIntegratorId integrator_id,
    int temporal_frames,
    const float* temporal_activity_buffer,
    int temporal_activity_stride,
    RuntimeNative3DDenoiseDiagnostics* out_diagnostics);

#endif
