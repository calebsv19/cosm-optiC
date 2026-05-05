#include "render/timer_hud_adapter.h"

#include "timer_hud/time_scope.h"
#include "timer_hud/timer_hud_backend.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_font.h"
#include "render/text_draw.h"
#include "render/text_upload_policy.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#define TIMER_HUD_DEFAULT_SETTINGS_PATH "config/timer_hud_settings.json"
#define TIMER_HUD_RUNTIME_SETTINGS_PATH "data/runtime/timer_hud_settings.json"

static int path_exists(const char* path) {
    struct stat st = {0};
    return (path && path[0] && stat(path, &st) == 0);
}

static int ensure_runtime_lane(void) {
    if (mkdir("data", 0755) != 0 && errno != EEXIST) return 0;
    if (mkdir("data/runtime", 0755) != 0 && errno != EEXIST) return 0;
    return 1;
}

static void seed_runtime_timer_hud_settings(void) {
    if (path_exists(TIMER_HUD_RUNTIME_SETTINGS_PATH)) return;
    if (!path_exists(TIMER_HUD_DEFAULT_SETTINGS_PATH)) return;
    if (!ensure_runtime_lane()) return;

    FILE* in = fopen(TIMER_HUD_DEFAULT_SETTINGS_PATH, "rb");
    if (!in) return;
    FILE* out = fopen(TIMER_HUD_RUNTIME_SETTINGS_PATH, "wb");
    if (!out) {
        fclose(in);
        return;
    }

    char buffer[4096];
    size_t n = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, n, out) != n) {
            break;
        }
    }
    fclose(out);
    fclose(in);
}

static void timer_hud_backend_init(void) {
    if (!initFontSystem()) {
        fprintf(stderr, "[TimerHUD] Failed to initialise font system.\n");
    }
}

static int timer_hud_get_screen_size(int* out_w, int* out_h) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return 0;
    if (out_w) *out_w = ctx->logical_width > 0 ? ctx->logical_width : ctx->width;
    if (out_h) *out_h = ctx->logical_height > 0 ? ctx->logical_height : ctx->height;
    return 1;
}

static int timer_hud_measure_text(const char* text, int* out_w, int* out_h) {
    TTF_Font* font = getActiveFont();
    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = (ctx ? ctx->renderer : NULL);
    if (!text || !font) return 0;
    return ray_tracing_text_measure_utf8(renderer, font, text, out_w, out_h);
}

static int timer_hud_line_height(void) {
    TTF_Font* font = getActiveFont();
    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = (ctx ? ctx->renderer : NULL);
    int raster_h = 0;
    if (!font) return 0;
    raster_h = TTF_FontHeight(font);
    return ray_tracing_text_logical_pixels(renderer, raster_h);
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

static void timer_hud_draw_line(int x1, int y1, int x2, int y2, TimerHUDColor color) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
#if !USE_VULKAN
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
#endif
    SDL_SetRenderDrawColor(ctx->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(ctx->renderer, x1, y1, x2, y2);
}

static void timer_hud_draw_text(const char* text, int x, int y, int align_flags, TimerHUDColor color) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    TTF_Font* font = getActiveFont();
    int logical_w = 0;
    int logical_h = 0;
    if (!text || !font) return;

    if (!ray_tracing_text_measure_utf8(ctx->renderer, font, text, &logical_w, &logical_h)) {
        return;
    }
    SDL_Rect dst = {x, y, logical_w, logical_h};

    if (align_flags & TIMER_HUD_ALIGN_CENTER)  dst.x -= logical_w / 2;
    if (align_flags & TIMER_HUD_ALIGN_RIGHT)   dst.x -= logical_w;
    if (align_flags & TIMER_HUD_ALIGN_MIDDLE)  dst.y -= logical_h / 2;
    if (align_flags & TIMER_HUD_ALIGN_BOTTOM)  dst.y -= logical_h;

    (void)ray_tracing_text_draw_utf8(ctx->renderer,
                                     font,
                                     text,
                                     (SDL_Color){color.r, color.g, color.b, color.a},
                                     &dst);
}

static const TimerHUDBackend g_timer_hud_backend = {
    .init = timer_hud_backend_init,
    .shutdown = NULL,
    .get_screen_size = timer_hud_get_screen_size,
    .measure_text = timer_hud_measure_text,
    .get_line_height = timer_hud_line_height,
    .draw_rect = timer_hud_draw_rect,
    .draw_line = timer_hud_draw_line,
    .draw_text = timer_hud_draw_text,
    .hud_padding = 6,
    .hud_spacing = 4,
    .hud_bg_alpha = 180
};

void timer_hud_register_backend(void) {
    ts_register_backend(&g_timer_hud_backend);
    seed_runtime_timer_hud_settings();
    ts_set_settings_path(TIMER_HUD_RUNTIME_SETTINGS_PATH);
}
