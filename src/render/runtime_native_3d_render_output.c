#include "render/runtime_native_3d_render_internal_host.h"

double runtime_native_3d_render_clamp_unit(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

uint8_t RuntimeNative3DResolveEnvironmentByte(void) {
    RuntimeEnvironment3D environment;
    double value = 0.0;
    RuntimeEnvironment3D_ResolveFromAnimationConfig(&environment, &animSettings);
    value = RuntimeEnvironment3D_BackgroundBrightness(&environment) * 255.0;
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

void RuntimeNative3DFillPixelBufferBackground(uint8_t* pixel_buffer,
                                              int width,
                                              int height,
                                              const RuntimeScene3D* scene,
                                              const RuntimeCameraProjector3D* projector) {
    if (!pixel_buffer || width <= 0 || height <= 0) return;
    if (!scene || !projector) {
        RuntimeNative3DFillPixelBufferEnvironment(pixel_buffer, (size_t)width * (size_t)height);
        return;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double pixel_x = (double)x;
            const double pixel_y = (double)y;
            const Ray3D primary_ray =
                RuntimeCameraProjector3D_MakePrimaryRay(projector, pixel_x, pixel_y);
            double radiance_r = 0.0;
            double radiance_g = 0.0;
            double radiance_b = 0.0;
            const size_t base =
                ((size_t)y * (size_t)width + (size_t)x) *
                (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;

            runtime_native_3d_render_background_rgb(scene,
                                                    &primary_ray,
                                                    &radiance_r,
                                                    &radiance_g,
                                                    &radiance_b);
            pixel_buffer[base] = TonemapCurveToByteWithFloor((float)radiance_r, 0u);
            pixel_buffer[base + 1u] = TonemapCurveToByteWithFloor((float)radiance_g, 0u);
            pixel_buffer[base + 2u] = TonemapCurveToByteWithFloor((float)radiance_b, 0u);
            pixel_buffer[base + 3u] = 0xFFu;
        }
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
