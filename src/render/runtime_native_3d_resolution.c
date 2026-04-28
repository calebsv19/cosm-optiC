#include "render/runtime_native_3d_resolution.h"

#include "config/config_manager.h"
#include "render/runtime_native_3d_render.h"

int RuntimeNative3DClampRenderScale(int value) {
    if (value < RUNTIME_3D_RENDER_SCALE_MIN) {
        value = RUNTIME_3D_RENDER_SCALE_MIN;
    }
    if (value > RUNTIME_3D_RENDER_SCALE_MAX) {
        value = RUNTIME_3D_RENDER_SCALE_MAX;
    }
    return value;
}

bool RuntimeNative3DResolveScaledDimensions(int host_width,
                                            int host_height,
                                            int scale,
                                            int* out_width,
                                            int* out_height) {
    int clamped_scale = RuntimeNative3DClampRenderScale(scale);
    int resolved_width = 0;
    int resolved_height = 0;

    if (!out_width || !out_height || host_width <= 0 || host_height <= 0) {
        return false;
    }

    resolved_width = host_width / clamped_scale;
    resolved_height = host_height / clamped_scale;
    if (resolved_width < 1) resolved_width = 1;
    if (resolved_height < 1) resolved_height = 1;

    *out_width = resolved_width;
    *out_height = resolved_height;
    return true;
}

bool RuntimeNative3DResolveUpscaledRect(int src_x,
                                        int src_y,
                                        int src_width,
                                        int src_height,
                                        int src_frame_width,
                                        int src_frame_height,
                                        int dst_frame_width,
                                        int dst_frame_height,
                                        int* out_x,
                                        int* out_y,
                                        int* out_width,
                                        int* out_height) {
    int mapped_x0 = 0;
    int mapped_y0 = 0;
    int mapped_x1 = 0;
    int mapped_y1 = 0;

    if (!out_x || !out_y || !out_width || !out_height ||
        src_frame_width <= 0 || src_frame_height <= 0 ||
        dst_frame_width <= 0 || dst_frame_height <= 0 ||
        src_width <= 0 || src_height <= 0 ||
        src_x < 0 || src_y < 0 ||
        src_x + src_width > src_frame_width ||
        src_y + src_height > src_frame_height) {
        return false;
    }

    mapped_x0 = (int)(((int64_t)src_x * (int64_t)dst_frame_width) / (int64_t)src_frame_width);
    mapped_y0 = (int)(((int64_t)src_y * (int64_t)dst_frame_height) / (int64_t)src_frame_height);
    mapped_x1 = (int)((((int64_t)src_x + (int64_t)src_width) * (int64_t)dst_frame_width) /
                      (int64_t)src_frame_width);
    mapped_y1 = (int)((((int64_t)src_y + (int64_t)src_height) * (int64_t)dst_frame_height) /
                      (int64_t)src_frame_height);

    if (mapped_x1 <= mapped_x0) mapped_x1 = mapped_x0 + 1;
    if (mapped_y1 <= mapped_y0) mapped_y1 = mapped_y0 + 1;
    if (mapped_x1 > dst_frame_width) mapped_x1 = dst_frame_width;
    if (mapped_y1 > dst_frame_height) mapped_y1 = dst_frame_height;

    *out_x = mapped_x0;
    *out_y = mapped_y0;
    *out_width = mapped_x1 - mapped_x0;
    *out_height = mapped_y1 - mapped_y0;
    return true;
}

void RuntimeNative3DUpscaleNearest(const uint8_t* src,
                                   int src_width,
                                   int src_height,
                                   uint8_t* dst,
                                   int dst_width,
                                   int dst_height) {
    if (!src || !dst || src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0) {
        return;
    }

    for (int y = 0; y < dst_height; ++y) {
        int src_y = ((int64_t)y * (int64_t)src_height) / (int64_t)dst_height;
        size_t dst_row = (size_t)y * (size_t)dst_width;
        size_t src_row = (size_t)src_y * (size_t)src_width;
        for (int x = 0; x < dst_width; ++x) {
            int src_x = ((int64_t)x * (int64_t)src_width) / (int64_t)dst_width;
            dst[dst_row + (size_t)x] = src[src_row + (size_t)src_x];
        }
    }
}

void RuntimeNative3DUpscaleNearestABGR(const uint8_t* src,
                                       int src_width,
                                       int src_height,
                                       uint8_t* dst,
                                       int dst_width,
                                       int dst_height) {
    if (!src || !dst || src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0) {
        return;
    }

    for (int y = 0; y < dst_height; ++y) {
        int src_y = ((int64_t)y * (int64_t)src_height) / (int64_t)dst_height;
        size_t dst_row = (size_t)y * (size_t)dst_width * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        size_t src_row = (size_t)src_y * (size_t)src_width * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        for (int x = 0; x < dst_width; ++x) {
            int src_x = ((int64_t)x * (int64_t)src_width) / (int64_t)dst_width;
            size_t dst_index =
                dst_row + (size_t)x * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            size_t src_index =
                src_row + (size_t)src_x * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            for (int c = 0; c < RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES; ++c) {
                dst[dst_index + (size_t)c] = src[src_index + (size_t)c];
            }
        }
    }
}
