#ifndef RUNTIME_NATIVE_3D_PIXEL_PROBE_H
#define RUNTIME_NATIVE_3D_PIXEL_PROBE_H
#include "render/runtime_disney_v2_3d.h"
#include "render/runtime_native_3d_feature_buffer.h"

/* Opt-in stderr records for at most 64 native coordinates. No frame storage. */
bool RuntimeNative3DPixelProbe_IsSelected(int x, int y);
bool RuntimeNative3DPixelProbe_ShouldShade(int x, int y);
void RuntimeNative3DPixelProbe_Receiver(int x, int y, const HitInfo3D* hit,
    const RuntimeMaterialPayload3D* payload, const Ray3D* ray);
void RuntimeNative3DPixelProbe_Shade(int x, int y,
    const RuntimeNative3DSamplingContext* sampling,
    const RuntimeDisneyV2_3DResult* result,
    double ambient_r, double ambient_g, double ambient_b);
void RuntimeNative3DPixelProbe_Denoise(int x, int y,
    const RuntimeNative3DFeatureBuffer* features, size_t pixel,
    const float* before, const float* after, float blend,
    int accepted_neighbors, float total_weight, const char* decision);
#endif
