#include "render/font_runtime.h"

#include "config/config_manager.h"
#include "render/text_draw.h"
#include "render/text_font_cache.h"

#include <stdio.h>

typedef struct RayTracingFontRuntimeState {
    SDL_Renderer* attached_renderer;
    TTF_Font* active_font;
    int active_point_size;
    uint32_t generation;
    uint32_t consumer_count;
    bool ready;
} RayTracingFontRuntimeState;

static RayTracingFontRuntimeState g_font_runtime = {
    .attached_renderer = NULL,
    .active_font = NULL,
    .active_point_size = 16,
    .generation = 1u,
    .consumer_count = 0u,
    .ready = false,
};

static const int kBasePointSize = 16;
static const int kMinPointSize = 6;

static bool ray_tracing_font_runtime_active_font_is_usable(TTF_Font* font) {
    return ray_tracing_text_font_is_current(font);
}

static void ray_tracing_font_runtime_bump_generation(void) {
    if (g_font_runtime.generation == UINT32_MAX) {
        g_font_runtime.generation = 1u;
        return;
    }
    g_font_runtime.generation += 1u;
}

static void ray_tracing_font_runtime_reset_active_slot(void) {
    g_font_runtime.active_font = NULL;
    g_font_runtime.active_point_size = kBasePointSize;
}

TTF_Font* ray_tracing_font_runtime_get_ui_regular(SDL_Renderer* renderer,
                                                  int logical_point_size,
                                                  int min_point_size) {
    if (!ray_tracing_font_runtime_ensure_ttf()) {
        return NULL;
    }
    return ray_tracing_text_font_cache_get_ui_regular(renderer, logical_point_size, min_point_size);
}

int ray_tracing_font_runtime_ui_regular_base_point_size(int fallback_point_size) {
    return ray_tracing_text_font_cache_ui_regular_base_point_size(fallback_point_size);
}

static bool ray_tracing_font_runtime_load_default_font(void) {
    TTF_Font* cached_font = NULL;
    int base_point_size = kBasePointSize;
    int requested_point_size = 0;
    SDL_Renderer* renderer = g_font_runtime.attached_renderer;

    if (!ray_tracing_font_runtime_ensure_ttf()) {
        g_font_runtime.ready = false;
        return false;
    }
    if (ray_tracing_font_runtime_active_font_is_usable(g_font_runtime.active_font)) {
        g_font_runtime.ready = true;
        return true;
    }

    ray_tracing_font_runtime_reset_active_slot();
    base_point_size = ray_tracing_font_runtime_ui_regular_base_point_size(kBasePointSize);
    requested_point_size = animation_config_scale_text_point_size(&animSettings,
                                                                  base_point_size,
                                                                  kMinPointSize);
    cached_font = ray_tracing_font_runtime_get_ui_regular(renderer,
                                                          requested_point_size,
                                                          kMinPointSize);
    if (!cached_font) {
        fprintf(stderr, "[TimerHUD] Failed to open runtime font: %s\n", TTF_GetError());
        g_font_runtime.ready = false;
        return false;
    }

    g_font_runtime.active_font = cached_font;
    g_font_runtime.active_point_size = requested_point_size;
    g_font_runtime.ready = true;
    return true;
}

bool ray_tracing_font_runtime_init(void) {
    if (g_font_runtime.consumer_count == 0u) {
        g_font_runtime.ready = ray_tracing_font_runtime_ensure_ttf();
    } else if (!ray_tracing_font_runtime_ensure_ttf()) {
        g_font_runtime.ready = false;
        return false;
    }
    g_font_runtime.consumer_count += 1u;
    return true;
}

bool ray_tracing_font_runtime_ensure_ttf(void) {
    if (TTF_WasInit() != 0) {
        return true;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "[font_runtime] TTF_Init failed: %s\n", TTF_GetError());
        return false;
    }
    return true;
}

void ray_tracing_font_runtime_invalidate_active_font(void) {
    ray_tracing_font_runtime_reset_active_slot();
}

void ray_tracing_font_runtime_attach_renderer(SDL_Renderer* renderer) {
    if (g_font_runtime.attached_renderer == renderer) {
        return;
    }
    if (g_font_runtime.attached_renderer) {
        ray_tracing_text_reset_renderer(g_font_runtime.attached_renderer);
    }
    g_font_runtime.attached_renderer = renderer;
}

void ray_tracing_font_runtime_detach_renderer(SDL_Renderer* renderer) {
    if (!g_font_runtime.attached_renderer) {
        return;
    }
    if (renderer && g_font_runtime.attached_renderer != renderer) {
        return;
    }
    ray_tracing_text_reset_renderer(g_font_runtime.attached_renderer);
    g_font_runtime.attached_renderer = NULL;
}

SDL_Renderer* ray_tracing_font_runtime_attached_renderer(void) {
    return g_font_runtime.attached_renderer;
}

bool ray_tracing_font_runtime_is_ready(void) {
    return g_font_runtime.ready && TTF_WasInit() != 0;
}

uint32_t ray_tracing_font_runtime_generation(void) {
    return g_font_runtime.generation;
}

void ray_tracing_font_runtime_invalidate_all(void) {
    ray_tracing_font_runtime_reset_active_slot();
    if (g_font_runtime.attached_renderer) {
        ray_tracing_text_reset_renderer(g_font_runtime.attached_renderer);
    }
    ray_tracing_text_font_cache_shutdown();
    ray_tracing_text_reset_font_system();
    g_font_runtime.ready = (TTF_WasInit() != 0);
    ray_tracing_font_runtime_bump_generation();
}

void ray_tracing_font_runtime_shutdown(void) {
    if (g_font_runtime.consumer_count == 0u) {
        return;
    }
    g_font_runtime.consumer_count -= 1u;
    if (g_font_runtime.consumer_count > 0u) {
        return;
    }

    g_font_runtime.ready = false;
    ray_tracing_font_runtime_invalidate_all();
    ray_tracing_font_runtime_detach_renderer(NULL);
    if (TTF_WasInit() != 0) {
        TTF_Quit();
    }
}

TTF_Font* ray_tracing_font_runtime_get_active_font(void) {
    if (!ray_tracing_font_runtime_active_font_is_usable(g_font_runtime.active_font)) {
        ray_tracing_font_runtime_reset_active_slot();
        if (!ray_tracing_font_runtime_load_default_font()) {
            return NULL;
        }
    }
    return g_font_runtime.active_font;
}

bool ray_tracing_font_runtime_activate_default_font(void) {
    return ray_tracing_font_runtime_load_default_font();
}

bool ray_tracing_font_runtime_refresh_active_font(void) {
    int base_point_size = kBasePointSize;
    int scaled_point_size = 0;
    TTF_Font* refreshed = NULL;
    SDL_Renderer* renderer = g_font_runtime.attached_renderer;

    base_point_size = ray_tracing_font_runtime_ui_regular_base_point_size(kBasePointSize);
    scaled_point_size = animation_config_scale_text_point_size(&animSettings,
                                                               base_point_size,
                                                               kMinPointSize);
    if (scaled_point_size <= 0) {
        scaled_point_size = kMinPointSize;
    }
    if (!ray_tracing_font_runtime_ensure_ttf()) {
        g_font_runtime.ready = false;
        return false;
    }
    if (ray_tracing_font_runtime_active_font_is_usable(g_font_runtime.active_font) &&
        scaled_point_size == g_font_runtime.active_point_size) {
        g_font_runtime.ready = true;
        return true;
    }

    refreshed = ray_tracing_font_runtime_get_ui_regular(renderer,
                                                        scaled_point_size,
                                                        kMinPointSize);
    if (!refreshed) {
        fprintf(stderr, "[TimerHUD] Failed to reload runtime font (%d): %s\n",
                scaled_point_size,
                TTF_GetError());
        g_font_runtime.ready = false;
        return false;
    }

    g_font_runtime.active_font = refreshed;
    g_font_runtime.active_point_size = scaled_point_size;
    g_font_runtime.ready = true;
    return true;
}

int ray_tracing_font_runtime_active_point_size(void) {
    return g_font_runtime.active_point_size;
}
