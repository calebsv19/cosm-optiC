#include "render/text_upload_policy.h"

#include "engine/Render/render_pipeline.h"

#include <math.h>
#if USE_VULKAN
#include <SDL2/SDL_vulkan.h>
#endif

static const float k_ray_tracing_max_text_raster_scale = 2.5f;

float ray_tracing_text_raster_scale(SDL_Renderer* renderer) {
    RenderContext* ctx = NULL;
    float logical_w = 0.0f;
    float logical_h = 0.0f;
    int drawable_w = 0;
    int drawable_h = 0;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float raster_scale = 1.0f;

    if (!renderer) {
        return 1.0f;
    }

    ctx = getRenderContext();
    if (!ctx || !ctx->window || ctx->renderer != renderer) {
        return 1.0f;
    }

    SDL_GetWindowSize(ctx->window, &drawable_w, &drawable_h);
#if USE_VULKAN
    SDL_Vulkan_GetDrawableSize(ctx->window, &drawable_w, &drawable_h);
#endif
    logical_w = (float)ctx->width;
    logical_h = (float)ctx->height;
    if (logical_w < 1.0f || logical_h < 1.0f) {
        SDL_GetWindowSize(ctx->window, &drawable_w, &drawable_h);
        logical_w = (float)drawable_w;
        logical_h = (float)drawable_h;
    }
    if (logical_w < 1.0f || logical_h < 1.0f) {
        return 1.0f;
    }

    if (logical_w > 0.0f) {
        scale_x = (float)drawable_w / logical_w;
    }
    if (logical_h > 0.0f) {
        scale_y = (float)drawable_h / logical_h;
    }

    raster_scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (!isfinite(raster_scale) || raster_scale < 1.0f) {
        raster_scale = 1.0f;
    }
    if (raster_scale > k_ray_tracing_max_text_raster_scale) {
        raster_scale = k_ray_tracing_max_text_raster_scale;
    }
    return raster_scale;
}

VkFilter ray_tracing_text_upload_filter(SDL_Renderer* renderer) {
    if (ray_tracing_text_raster_scale(renderer) > 1.0f) {
        return VK_FILTER_NEAREST;
    }
    return VK_FILTER_LINEAR;
}

int ray_tracing_text_raster_point_size(SDL_Renderer* renderer,
                                       int base_point_size,
                                       int min_point_size) {
    int raster_size = 0;
    int min_size = 0;
    float raster_scale = ray_tracing_text_raster_scale(renderer);

    if (min_point_size < 1) min_point_size = 1;
    if (base_point_size < min_point_size) base_point_size = min_point_size;

    raster_size = (int)lroundf((float)base_point_size * raster_scale);
    min_size = (int)lroundf((float)min_point_size * raster_scale);
    if (min_size < min_point_size) min_size = min_point_size;
    if (raster_size < min_size) raster_size = min_size;
    if (raster_size < 1) raster_size = 1;
    return raster_size;
}

int ray_tracing_text_logical_pixels(SDL_Renderer* renderer, int raster_pixels) {
    int logical_pixels = 0;
    float raster_scale = 0.0f;

    if (raster_pixels <= 0) return 0;
    raster_scale = ray_tracing_text_raster_scale(renderer);
    logical_pixels = (int)lroundf((float)raster_pixels / raster_scale);
    if (logical_pixels < 1) logical_pixels = 1;
    return logical_pixels;
}
