#ifndef RUNTIME_NATIVE_3D_DENOISE_GUIDANCE_H
#define RUNTIME_NATIVE_3D_DENOISE_GUIDANCE_H
#include <stddef.h>
#include "render/runtime_native_3d_feature_buffer.h"
bool RuntimeNative3DDenoiseGuidance_HasMaterial(
    const RuntimeNative3DFeatureBuffer* features, size_t pixel);
float RuntimeNative3DDenoiseGuidance_Blend(
    const RuntimeNative3DFeatureBuffer* features, size_t pixel, float luma);
float RuntimeNative3DDenoiseGuidance_NeighborWeight(
    const RuntimeNative3DFeatureBuffer* features, size_t center, size_t sample);
/* A beauty contrast within measured sampling error is not a proven edge. */
bool RuntimeNative3DDenoiseGuidance_RadianceDifferenceUncertain(
    const RuntimeNative3DFeatureBuffer* features, size_t center, size_t sample,
    const float* center_rgb, const float* sample_rgb);
#endif
