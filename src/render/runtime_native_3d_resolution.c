#include "render/runtime_native_3d_resolution.h"

#include "config/config_manager.h"
#include "render/runtime_native_3d_render.h"

#include <math.h>

static int runtime_native_3d_resolution_clamp_index(int value, int max_value) {
    if (value < 0) return 0;
    if (value > max_value) return max_value;
    return value;
}

static uint8_t runtime_native_3d_resolution_sample_bilinear_channel(const uint8_t* src,
                                                                    int src_width,
                                                                    int src_height,
                                                                    double src_x,
                                                                    double src_y,
                                                                    int channel) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    double tx = 0.0;
    double ty = 0.0;
    double top = 0.0;
    double bottom = 0.0;
    size_t index00 = 0u;
    size_t index10 = 0u;
    size_t index01 = 0u;
    size_t index11 = 0u;

    x0 = runtime_native_3d_resolution_clamp_index((int)floor(src_x), src_width - 1);
    y0 = runtime_native_3d_resolution_clamp_index((int)floor(src_y), src_height - 1);
    x1 = runtime_native_3d_resolution_clamp_index(x0 + 1, src_width - 1);
    y1 = runtime_native_3d_resolution_clamp_index(y0 + 1, src_height - 1);
    tx = src_x - (double)x0;
    ty = src_y - (double)y0;
    if (tx < 0.0) tx = 0.0;
    if (tx > 1.0) tx = 1.0;
    if (ty < 0.0) ty = 0.0;
    if (ty > 1.0) ty = 1.0;

    index00 = (((size_t)y0 * (size_t)src_width) + (size_t)x0) *
              (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES + (size_t)channel;
    index10 = (((size_t)y0 * (size_t)src_width) + (size_t)x1) *
              (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES + (size_t)channel;
    index01 = (((size_t)y1 * (size_t)src_width) + (size_t)x0) *
              (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES + (size_t)channel;
    index11 = (((size_t)y1 * (size_t)src_width) + (size_t)x1) *
              (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES + (size_t)channel;

    top = ((1.0 - tx) * (double)src[index00]) + (tx * (double)src[index10]);
    bottom = ((1.0 - tx) * (double)src[index01]) + (tx * (double)src[index11]);
    return (uint8_t)lround(((1.0 - ty) * top) + (ty * bottom));
}

int RuntimeNative3DClampRenderScale(int value) {
    if (value == RUNTIME_3D_RENDER_SCALE_HIDPI) {
        return RUNTIME_3D_RENDER_SCALE_HIDPI;
    }
    if (value < 1) {
        value = RUNTIME_3D_RENDER_SCALE_DEFAULT;
    }
    if (value > RUNTIME_3D_RENDER_SCALE_MAX) {
        value = RUNTIME_3D_RENDER_SCALE_MAX;
    }
    return value;
}

bool RuntimeNative3DRenderScaleUsesHiDPI(int value) {
    return RuntimeNative3DClampRenderScale(value) == RUNTIME_3D_RENDER_SCALE_HIDPI;
}

bool RuntimeNative3DResolveHostDimensions(int logical_width,
                                          int logical_height,
                                          int drawable_width,
                                          int drawable_height,
                                          int scale,
                                          int* out_width,
                                          int* out_height) {
    if (!out_width || !out_height || logical_width <= 0 || logical_height <= 0) {
        return false;
    }

    if (RuntimeNative3DRenderScaleUsesHiDPI(scale) &&
        drawable_width > logical_width &&
        drawable_height > logical_height) {
        *out_width = drawable_width;
        *out_height = drawable_height;
        return true;
    }

    *out_width = logical_width;
    *out_height = logical_height;
    return true;
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

    if (clamped_scale == RUNTIME_3D_RENDER_SCALE_HIDPI) {
        clamped_scale = 1;
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

void RuntimeNative3DUpscaleBilinearABGR(const uint8_t* src,
                                        int src_width,
                                        int src_height,
                                        uint8_t* dst,
                                        int dst_width,
                                        int dst_height) {
    if (!src || !dst || src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0) {
        return;
    }

    for (int y = 0; y < dst_height; ++y) {
        double src_y = (((double)y + 0.5) * (double)src_height / (double)dst_height) - 0.5;
        size_t dst_row = (size_t)y * (size_t)dst_width * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        for (int x = 0; x < dst_width; ++x) {
            double src_x = (((double)x + 0.5) * (double)src_width / (double)dst_width) - 0.5;
            size_t dst_index =
                dst_row + (size_t)x * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            for (int c = 0; c < RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES; ++c) {
                dst[dst_index + (size_t)c] = runtime_native_3d_resolution_sample_bilinear_channel(
                    src,
                    src_width,
                    src_height,
                    src_x,
                    src_y,
                    c);
            }
        }
    }
}

void RuntimeNative3DUpscaleNearestABGRRect(const uint8_t* src,
                                           int src_width,
                                           int src_height,
                                           uint8_t* dst,
                                           int dst_width,
                                           int dst_height,
                                           int dst_x,
                                           int dst_y,
                                           int dst_rect_width,
                                           int dst_rect_height) {
    if (!src || !dst || src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0) {
        return;
    }
    if (dst_rect_width <= 0 || dst_rect_height <= 0) {
        return;
    }
    if (dst_x < 0 || dst_y < 0 || dst_x + dst_rect_width > dst_width ||
        dst_y + dst_rect_height > dst_height) {
        return;
    }

    for (int y = dst_y; y < dst_y + dst_rect_height; ++y) {
        int src_y = ((int64_t)y * (int64_t)src_height) / (int64_t)dst_height;
        size_t dst_row = (size_t)y * (size_t)dst_width * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        size_t src_row = (size_t)src_y * (size_t)src_width * RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        for (int x = dst_x; x < dst_x + dst_rect_width; ++x) {
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
