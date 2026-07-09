#include "render/runtime_native_3d_preview_reconstruction.h"

#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_resolution.h"

#include <math.h>
#include <stddef.h>

static int runtime_native_3d_preview_reconstruction_clamp_index(int value, int max_value) {
    if (value < 0) return 0;
    if (value > max_value) return max_value;
    return value;
}

static Runtime3DUpscaleMode runtime_native_3d_preview_reconstruction_clamp_mode(
    Runtime3DUpscaleMode upscale_mode) {
    if (upscale_mode < RUNTIME_3D_UPSCALE_MODE_MIN) {
        return RUNTIME_3D_UPSCALE_MODE_DEFAULT;
    }
    if (upscale_mode > RUNTIME_3D_UPSCALE_MODE_MAX) {
        return RUNTIME_3D_UPSCALE_MODE_DEFAULT;
    }
    return upscale_mode;
}

static uint8_t runtime_native_3d_preview_reconstruction_sample_nearest_channel(
    const uint8_t* render_buffer,
    int render_width,
    int render_height,
    double render_x,
    double render_y,
    int channel) {
    int sample_x = runtime_native_3d_preview_reconstruction_clamp_index((int)floor(render_x + 0.5),
                                                                        render_width - 1);
    int sample_y = runtime_native_3d_preview_reconstruction_clamp_index((int)floor(render_y + 0.5),
                                                                        render_height - 1);
    const size_t index =
        (((size_t)sample_y * (size_t)render_width) + (size_t)sample_x) *
            (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES +
        (size_t)channel;
    return render_buffer[index];
}

static uint8_t runtime_native_3d_preview_reconstruction_sample_bilinear_channel(
    const uint8_t* render_buffer,
    int render_width,
    int render_height,
    double render_x,
    double render_y,
    int channel) {
    int x0 = runtime_native_3d_preview_reconstruction_clamp_index((int)floor(render_x),
                                                                  render_width - 1);
    int y0 = runtime_native_3d_preview_reconstruction_clamp_index((int)floor(render_y),
                                                                  render_height - 1);
    int x1 = runtime_native_3d_preview_reconstruction_clamp_index(x0 + 1, render_width - 1);
    int y1 = runtime_native_3d_preview_reconstruction_clamp_index(y0 + 1, render_height - 1);
    double tx = render_x - (double)x0;
    double ty = render_y - (double)y0;
    const size_t index00 =
        (((size_t)y0 * (size_t)render_width) + (size_t)x0) *
            (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES +
        (size_t)channel;
    const size_t index10 =
        (((size_t)y0 * (size_t)render_width) + (size_t)x1) *
            (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES +
        (size_t)channel;
    const size_t index01 =
        (((size_t)y1 * (size_t)render_width) + (size_t)x0) *
            (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES +
        (size_t)channel;
    const size_t index11 =
        (((size_t)y1 * (size_t)render_width) + (size_t)x1) *
            (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES +
        (size_t)channel;
    if (tx < 0.0) tx = 0.0;
    if (tx > 1.0) tx = 1.0;
    if (ty < 0.0) ty = 0.0;
    if (ty > 1.0) ty = 1.0;
    const double top =
        ((1.0 - tx) * (double)render_buffer[index00]) +
        (tx * (double)render_buffer[index10]);
    const double bottom =
        ((1.0 - tx) * (double)render_buffer[index01]) +
        (tx * (double)render_buffer[index11]);
    return (uint8_t)lround(((1.0 - ty) * top) + (ty * bottom));
}

static bool runtime_native_3d_preview_reconstruction_validate_rect(const SDL_Rect* rect,
                                                                   int host_width,
                                                                   int host_height) {
    return rect && rect->w > 0 && rect->h > 0 &&
           rect->x >= 0 && rect->y >= 0 &&
           rect->x + rect->w <= host_width &&
           rect->y + rect->h <= host_height;
}

bool RuntimeNative3DPreviewResolveDirtyHostRect(int render_x,
                                                int render_y,
                                                int render_rect_width,
                                                int render_rect_height,
                                                int render_width,
                                                int render_height,
                                                int host_width,
                                                int host_height,
                                                SDL_Rect* out_host_rect) {
    int expanded_x = 0;
    int expanded_y = 0;
    int expanded_w = 0;
    int expanded_h = 0;

    if (!out_host_rect ||
        render_width <= 0 || render_height <= 0 ||
        host_width <= 0 || host_height <= 0 ||
        render_rect_width <= 0 || render_rect_height <= 0 ||
        render_x < 0 || render_y < 0 ||
        render_x + render_rect_width > render_width ||
        render_y + render_rect_height > render_height) {
        return false;
    }

    expanded_x = render_x;
    expanded_y = render_y;
    expanded_w = render_rect_width;
    expanded_h = render_rect_height;

    if (expanded_x > 0) {
        expanded_x -= 1;
        expanded_w += 1;
    }
    if (expanded_y > 0) {
        expanded_y -= 1;
        expanded_h += 1;
    }
    if (expanded_x + expanded_w < render_width) {
        expanded_w += 1;
    }
    if (expanded_y + expanded_h < render_height) {
        expanded_h += 1;
    }

    return RuntimeNative3DResolveUpscaledRect(expanded_x,
                                              expanded_y,
                                              expanded_w,
                                              expanded_h,
                                              render_width,
                                              render_height,
                                              host_width,
                                              host_height,
                                              &out_host_rect->x,
                                              &out_host_rect->y,
                                              &out_host_rect->w,
                                              &out_host_rect->h);
}

bool RuntimeNative3DPreviewReconstructABGRRectWithMode(const uint8_t* render_buffer,
                                                       int render_width,
                                                       int render_height,
                                                       uint8_t* host_buffer,
                                                       int host_width,
                                                       int host_height,
                                                       const SDL_Rect* host_rect,
                                                       Runtime3DUpscaleMode upscale_mode) {
    SDL_Rect rect = {0, 0, host_width, host_height};
    Runtime3DUpscaleMode resolved_mode =
        runtime_native_3d_preview_reconstruction_clamp_mode(upscale_mode);

    if (!render_buffer || !host_buffer ||
        render_width <= 0 || render_height <= 0 ||
        host_width <= 0 || host_height <= 0) {
        return false;
    }
    if (host_rect) {
        if (!runtime_native_3d_preview_reconstruction_validate_rect(host_rect,
                                                                    host_width,
                                                                    host_height)) {
            return false;
        }
        rect = *host_rect;
    }

    for (int y = rect.y; y < rect.y + rect.h; ++y) {
        double render_y =
            (((double)y + 0.5) * (double)render_height / (double)host_height) - 0.5;
        size_t host_row =
            (size_t)y * (size_t)host_width * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
        for (int x = rect.x; x < rect.x + rect.w; ++x) {
            double render_x =
                (((double)x + 0.5) * (double)render_width / (double)host_width) - 0.5;
            size_t host_index =
                host_row +
                (size_t)x * (size_t)RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES;
            for (int channel = 0; channel < RUNTIME_NATIVE_3D_PIXEL_STRIDE_BYTES; ++channel) {
                if (resolved_mode == RUNTIME_3D_UPSCALE_MODE_NEAREST) {
                    host_buffer[host_index + (size_t)channel] =
                        runtime_native_3d_preview_reconstruction_sample_nearest_channel(
                            render_buffer,
                            render_width,
                            render_height,
                            render_x,
                            render_y,
                            channel);
                } else {
                    host_buffer[host_index + (size_t)channel] =
                        runtime_native_3d_preview_reconstruction_sample_bilinear_channel(
                            render_buffer,
                            render_width,
                            render_height,
                            render_x,
                            render_y,
                            channel);
                }
            }
        }
    }

    return true;
}

bool RuntimeNative3DPreviewReconstructABGRWithMode(const uint8_t* render_buffer,
                                                   int render_width,
                                                   int render_height,
                                                   uint8_t* host_buffer,
                                                   int host_width,
                                                   int host_height,
                                                   Runtime3DUpscaleMode upscale_mode) {
    return RuntimeNative3DPreviewReconstructABGRRectWithMode(render_buffer,
                                                             render_width,
                                                             render_height,
                                                             host_buffer,
                                                             host_width,
                                                             host_height,
                                                             NULL,
                                                             upscale_mode);
}
