#include "render/timer_hud_adapter.h"

#include "timer_hud/time_scope.h"
#include "timer_hud/timer_hud_backend.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_font.h"
#include "render/text_draw.h"
#include "render/text_upload_policy.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define TIMER_HUD_DEFAULT_SETTINGS_PATH "config/timer_hud_settings.json"
#define TIMER_HUD_RUNTIME_SETTINGS_PATH "data/runtime/timer_hud_settings.json"

static TimerHUDSession* g_timer_hud_session = NULL;

TimerHUDSession* timer_hud_session(void) {
    if (!g_timer_hud_session) {
        g_timer_hud_session = ts_session_create();
    }
    return g_timer_hud_session;
}

static int timer_hud_resolve_abs_from_cwd(const char* relative_path,
                                          char* out,
                                          size_t out_cap) {
    char cwd[PATH_MAX] = {0};
    int written = 0;
    if (!relative_path || !relative_path[0] || !out || out_cap == 0) {
        return 0;
    }
    if (relative_path[0] == '/') {
        written = snprintf(out, out_cap, "%s", relative_path);
        return written > 0 && (size_t)written < out_cap;
    }
    if (!getcwd(cwd, sizeof(cwd))) {
        return 0;
    }
    written = snprintf(out, out_cap, "%s/%s", cwd, relative_path);
    return written > 0 && (size_t)written < out_cap;
}

static bool timer_hud_parse_bool_token(const char* value, bool* out_enabled) {
    if (!value || !out_enabled) {
        return false;
    }
    if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "on") == 0 || strcasecmp(value, "yes") == 0) {
        *out_enabled = true;
        return true;
    }
    if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "off") == 0 || strcasecmp(value, "no") == 0) {
        *out_enabled = false;
        return true;
    }
    return false;
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
    TimerHUDSession* session = timer_hud_session();
    char default_settings_path[PATH_MAX] = {0};
    char runtime_settings_path[PATH_MAX] = {0};
    TimerHUDInitConfig init_config = {
        .program_name = "ray_tracing",
        .output_root = ".",
        .settings_path = TIMER_HUD_RUNTIME_SETTINGS_PATH,
        .default_settings_path = NULL,
        .seed_settings_if_missing = false,
    };
    const char* output_root = NULL;
    const char* override_path = NULL;

    if (!session) {
        fprintf(stderr, "[TimerHUD] failed to allocate ray_tracing session.\n");
        return;
    }

    ts_session_register_backend(session, &g_timer_hud_backend);

    if (timer_hud_resolve_abs_from_cwd(TIMER_HUD_DEFAULT_SETTINGS_PATH,
                                       default_settings_path,
                                       sizeof(default_settings_path)) &&
        timer_hud_resolve_abs_from_cwd(TIMER_HUD_RUNTIME_SETTINGS_PATH,
                                       runtime_settings_path,
                                       sizeof(runtime_settings_path)) &&
        access(runtime_settings_path, F_OK) != 0 &&
        access(default_settings_path, F_OK) == 0) {
        init_config.default_settings_path = default_settings_path;
        init_config.seed_settings_if_missing = true;
    }

    output_root = getenv("TIMERHUD_OUTPUT_ROOT");
    if (output_root && output_root[0]) {
        init_config.output_root = output_root;
    }

    override_path = getenv("RAY_TRACING_TIMER_HUD_SETTINGS");
    if (override_path && override_path[0]) {
        init_config.settings_path = override_path;
    }

    (void)ts_session_apply_init_config(session, &init_config);
}

void timer_hud_apply_startup_env_overrides(void) {
    TimerHUDSession* session = timer_hud_session();
    const char* hud_env = getenv("RAY_TRACING_TIMER_HUD");
    const char* overlay_env = getenv("RAY_TRACING_TIMER_HUD_OVERLAY");
    const char* visual_mode_env = getenv("RAY_TRACING_TIMER_HUD_VISUAL_MODE");
    bool enabled = false;
    bool set_hybrid_by_default = false;
    TimerHUDVisualMode visual_mode = TIMER_HUD_VISUAL_MODE_INVALID;

    if (!session) {
        return;
    }

    if (hud_env && hud_env[0]) {
        if (timer_hud_parse_bool_token(hud_env, &enabled)) {
            ts_session_set_hud_enabled(session, enabled);
            set_hybrid_by_default = enabled;
        } else {
            fprintf(stderr,
                    "[TimerHUD] ignoring invalid RAY_TRACING_TIMER_HUD=%s\n",
                    hud_env);
        }
    }

    if (overlay_env && overlay_env[0]) {
        if (timer_hud_parse_bool_token(overlay_env, &enabled)) {
            ts_session_set_hud_enabled(session, enabled);
            set_hybrid_by_default = enabled;
        } else {
            fprintf(stderr,
                    "[TimerHUD] ignoring invalid RAY_TRACING_TIMER_HUD_OVERLAY=%s\n",
                    overlay_env);
        }
    }

    if (visual_mode_env && visual_mode_env[0]) {
        visual_mode = ts_visual_mode_from_string(visual_mode_env);
        if (visual_mode != TIMER_HUD_VISUAL_MODE_INVALID) {
            (void)ts_session_set_hud_visual_mode_kind(session, visual_mode);
        } else {
            fprintf(stderr,
                    "[TimerHUD] ignoring invalid RAY_TRACING_TIMER_HUD_VISUAL_MODE=%s\n",
                    visual_mode_env);
        }
    } else if (set_hybrid_by_default) {
        (void)ts_session_set_hud_visual_mode_kind(session, TIMER_HUD_VISUAL_MODE_HYBRID);
    }

    fprintf(stderr,
            "[TimerHUD] ray_tracing startup hud_enabled=%d mode=%s log_enabled=%d log_file=%s\n",
            ts_session_is_hud_enabled(session) ? 1 : 0,
            ts_session_get_hud_visual_mode(session),
            ts_session_is_log_enabled(session) ? 1 : 0,
            ts_session_get_log_filepath(session));
}

void timer_hud_shutdown_session(void) {
    if (!g_timer_hud_session) {
        return;
    }
    ts_session_destroy(g_timer_hud_session);
    g_timer_hud_session = NULL;
}
