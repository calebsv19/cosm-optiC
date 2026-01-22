#include "render/timer_hud_adapter.h"

#include "timer_hud/time_scope.h"
#include "timer_hud/timer_hud_backend.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_font.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include "vk_renderer.h"

static void timer_hud_backend_init(void) {
    if (!initFontSystem()) {
        fprintf(stderr, "[TimerHUD] Failed to initialise font system.\n");
    }
}

static int timer_hud_get_screen_size(int* out_w, int* out_h) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return 0;
    if (out_w) *out_w = ctx->width;
    if (out_h) *out_h = ctx->height;
    return 1;
}

static int timer_hud_measure_text(const char* text, int* out_w, int* out_h) {
    TTF_Font* font = getActiveFont();
    if (!text || !font) return 0;
    if (TTF_SizeUTF8(font, text, out_w, out_h) != 0) return 0;
    return 1;
}

static int timer_hud_line_height(void) {
    TTF_Font* font = getActiveFont();
    if (!font) return 0;
    return TTF_FontHeight(font);
}

static void timer_hud_draw_rect(int x, int y, int w, int h, TimerHUDColor color) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
#endif
    SDL_SetRenderDrawColor(ctx->renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(ctx->renderer, &rect);
}

static void timer_hud_draw_text(const char* text, int x, int y, int align_flags, TimerHUDColor color) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    TTF_Font* font = getActiveFont();
    if (!text || !font) return;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text,
                                                  (SDL_Color){color.r, color.g, color.b, color.a});
    if (!surface) return;

    SDL_Rect dst = {x, y, surface->w, surface->h};

    if (align_flags & TIMER_HUD_ALIGN_CENTER)  dst.x -= surface->w / 2;
    if (align_flags & TIMER_HUD_ALIGN_RIGHT)   dst.x -= surface->w;
    if (align_flags & TIMER_HUD_ALIGN_MIDDLE)  dst.y -= surface->h / 2;
    if (align_flags & TIMER_HUD_ALIGN_BOTTOM)  dst.y -= surface->h;

#if USE_VULKAN
    VkRendererTexture texture;
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)ctx->renderer, surface, &texture,
                                                   VK_FILTER_LINEAR) == VK_SUCCESS) {
        vk_renderer_draw_texture((VkRenderer*)ctx->renderer, &texture, NULL, &dst);
        vk_renderer_queue_texture_destroy((VkRenderer*)ctx->renderer, &texture);
    }
#else
    SDL_Texture* texture = SDL_CreateTextureFromSurface(ctx->renderer, surface);
    if (texture) {
        SDL_RenderCopy(ctx->renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
#endif

    SDL_FreeSurface(surface);
}

static const TimerHUDBackend g_timer_hud_backend = {
    .init = timer_hud_backend_init,
    .shutdown = NULL,
    .get_screen_size = timer_hud_get_screen_size,
    .measure_text = timer_hud_measure_text,
    .get_line_height = timer_hud_line_height,
    .draw_rect = timer_hud_draw_rect,
    .draw_text = timer_hud_draw_text,
    .hud_padding = 6,
    .hud_spacing = 4,
    .hud_bg_alpha = 180
};

void timer_hud_register_backend(void) {
    ts_register_backend(&g_timer_hud_backend);
    ts_set_settings_path("Configs/timer_hud_settings.json");
}
