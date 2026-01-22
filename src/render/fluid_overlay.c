#include "render/fluid_overlay.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "camera/camera.h"
#include "vk_renderer.h"

static float clamp01f(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

bool fluid_overlay_draw(SDL_Renderer *renderer,
                        const FluidFrame *frame,
                        const Camera *camera,
                        int screen_w,
                        int screen_h) {
    if (!renderer || !frame || !camera || frame->w <= 0 || frame->h <= 0) return false;
#if USE_VULKAN
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, frame->w, frame->h, 32,
                                                          SDL_PIXELFORMAT_ARGB8888);
    if (!surface) return false;
    uint8_t *dst = (uint8_t *)surface->pixels;
    int pitch = surface->pitch;
#else
    SDL_Texture *tex = SDL_CreateTexture(renderer,
                                         SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         frame->w,
                                         frame->h);
    if (!tex) return false;

    // Build a small texture where alpha encodes density.
    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch) != 0) {
        SDL_DestroyTexture(tex);
        return false;
    }
    uint8_t *dst = (uint8_t *)pixels;
#endif
    size_t count = (size_t)frame->w * (size_t)frame->h;
    const float *density = frame->density;
    float max_d = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        if (density[i] > max_d) max_d = density[i];
    }
    float inv = (max_d > 1e-6f) ? (1.0f / max_d) : 1.0f;
    for (int y = 0; y < frame->h; ++y) {
        uint32_t *row = (uint32_t *)(dst + y * pitch);
        for (int x = 0; x < frame->w; ++x) {
            float d = density[(size_t)y * (size_t)frame->w + (size_t)x];
            float a = clamp01f(d * inv);
            uint8_t alpha = (uint8_t)lroundf(a * 180.0f);
            row[x] = ((uint32_t)alpha << 24) | 0x00FFFFFF; // white with alpha
        }
    }
#if !USE_VULKAN
    SDL_UnlockTexture(tex);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
#endif

    // Compute screen-space rectangle for the fluid grid
    float origin_x = frame->meta.origin_x;
    float origin_y = frame->meta.origin_y;
    float cell = frame->meta.cell_size > 0.0f ? frame->meta.cell_size : 1.0f;
    float world_min_x = origin_x;
    float world_min_y = origin_y;
    float world_max_x = origin_x + cell * (float)(frame->w);
    float world_max_y = origin_y + cell * (float)(frame->h);

    CameraPoint p0 = CameraWorldToScreen(camera, world_min_x, world_min_y, screen_w, screen_h);
    CameraPoint p1 = CameraWorldToScreen(camera, world_max_x, world_max_y, screen_w, screen_h);

    SDL_Rect dst_rect = {
        (int)lroundf(fminf((float)p0.x, (float)p1.x)),
        (int)lroundf(fminf((float)p0.y, (float)p1.y)),
        (int)lroundf(fabsf((float)p1.x - (float)p0.x)),
        (int)lroundf(fabsf((float)p1.y - (float)p0.y))
    };
    if (dst_rect.w < 1) dst_rect.w = 1;
    if (dst_rect.h < 1) dst_rect.h = 1;

#if USE_VULKAN
    VkRendererTexture texture;
    VkResult result = vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)renderer, surface, &texture,
                                                                 VK_FILTER_LINEAR);
    SDL_FreeSurface(surface);
    if (result != VK_SUCCESS) {
        return false;
    }
    vk_renderer_draw_texture((VkRenderer*)renderer, &texture, NULL, &dst_rect);
    vk_renderer_queue_texture_destroy((VkRenderer*)renderer, &texture);
#else
    SDL_RenderCopy(renderer, tex, NULL, &dst_rect);
    SDL_DestroyTexture(tex);
#endif
    return true;
}
