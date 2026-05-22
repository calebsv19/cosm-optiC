#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "config/config_manager.h"
#include "render/text_draw.h"
#include "render/text_font_cache.h"
#include <stdio.h>

static TTF_Font* activeFont = NULL;
static int activePointSize = 16;
static const int kBasePointSize = 16;
static const int kMinPointSize = 6;

static bool active_font_is_usable(TTF_Font* font) {
    return font &&
           ray_tracing_text_has_font_source(font) &&
           ray_tracing_text_font_cache_contains(font);
}

void invalidateActiveFontHandle(void) {
    activeFont = NULL;
    activePointSize = kBasePointSize;
}

static bool ensureTTF(void) {
    if (TTF_WasInit() == 0) {
        if (TTF_Init() == -1) {
            fprintf(stderr, "[TimerHUD] TTF_Init failed: %s\n", TTF_GetError());
            return false;
        }
    }
    return true;
}

static bool loadDefaultFont(void) {
    TTF_Font* cached_font = NULL;
    int base_point_size = kBasePointSize;
    int requested_point_size = 0;
    SDL_Renderer* renderer = NULL;

    if (!ensureTTF()) return false;
    if (active_font_is_usable(activeFont)) {
        return true;
    }
    activeFont = NULL;
    renderer = getRenderContext() ? getRenderContext()->renderer : NULL;
    base_point_size = ray_tracing_text_font_cache_ui_regular_base_point_size(kBasePointSize);
    requested_point_size = animation_config_scale_text_point_size(&animSettings, base_point_size, kMinPointSize);
    cached_font = ray_tracing_text_font_cache_get_ui_regular(renderer, requested_point_size, kMinPointSize);
    if (!cached_font) {
        fprintf(stderr, "[TimerHUD] Failed to open runtime font: %s\n", TTF_GetError());
        return false;
    }
    activeFont = cached_font;
    activePointSize = requested_point_size;
    return true;
}

bool initFontSystem(void) {
    return loadDefaultFont();
}

void shutdownFontSystem(void) {
    invalidateActiveFontHandle();
    ray_tracing_text_reset_renderer(getRenderContext() ? getRenderContext()->renderer : NULL);
    ray_tracing_text_font_cache_shutdown();
}

bool loadFontByID(FontID id) {
    (void)id;
    return loadDefaultFont();
}

TTF_Font* getActiveFont(void) {
    if (!active_font_is_usable(activeFont)) {
        activeFont = NULL;
        activePointSize = kBasePointSize;
        if (!loadDefaultFont()) {
            return NULL;
        }
    }
    return activeFont;
}

bool refreshActiveFontFromAnimationConfig(void) {
    int base_point_size = kBasePointSize;
    int scaled_point_size = animation_config_scale_text_point_size(&animSettings, kBasePointSize, kMinPointSize);
    TTF_Font* refreshed = NULL;
    SDL_Renderer* renderer = getRenderContext() ? getRenderContext()->renderer : NULL;

    base_point_size = ray_tracing_text_font_cache_ui_regular_base_point_size(kBasePointSize);
    scaled_point_size = animation_config_scale_text_point_size(&animSettings, base_point_size, kMinPointSize);
    if (scaled_point_size <= 0) {
        scaled_point_size = kMinPointSize;
    }
    if (!ensureTTF()) {
        return false;
    }
    if (active_font_is_usable(activeFont) &&
        scaled_point_size == activePointSize) {
        return true;
    }

    refreshed = ray_tracing_text_font_cache_get_ui_regular(renderer, scaled_point_size, kMinPointSize);
    if (!refreshed) {
        fprintf(stderr, "[TimerHUD] Failed to reload runtime font (%d): %s\n",
                scaled_point_size, TTF_GetError());
        return false;
    }
    activeFont = refreshed;
    activePointSize = scaled_point_size;
    return true;
}

int getActiveFontPointSize(void) {
    return activePointSize;
}
