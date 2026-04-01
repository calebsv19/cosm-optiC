#include "engine/Render/render_font.h"
#include "config/config_manager.h"
#include "ui/shared_theme_font_adapter.h"
#include <stdio.h>
#include <string.h>

static TTF_Font* activeFont = NULL;
static int activePointSize = 16;
static const int kBasePointSize = 16;
static const int kMinPointSize = 8;
static const char* kSystemFontPath = "/System/Library/Fonts/Supplemental/Arial.ttf";
static const char* kBundledFontPath = "config/default.ttf";

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
    char shared_font_path[256];
    const char* font_path = kSystemFontPath;
    TTF_Font* opened_font = NULL;
    int requested_point_size = 0;

    if (activeFont) return true;
    if (!ensureTTF()) return false;
    requested_point_size = animation_config_scale_text_point_size(&animSettings, kBasePointSize, kMinPointSize);
    if (ray_tracing_shared_font_resolve_ui_regular(
            shared_font_path, sizeof(shared_font_path), &requested_point_size)) {
        font_path = shared_font_path;
    }
    opened_font = TTF_OpenFont(font_path, requested_point_size);
    if (!opened_font && strcmp(font_path, kSystemFontPath) != 0) {
        opened_font = TTF_OpenFont(kSystemFontPath, requested_point_size);
        font_path = kSystemFontPath;
    }
    if (!opened_font && strcmp(font_path, kBundledFontPath) != 0) {
        opened_font = TTF_OpenFont(kBundledFontPath, requested_point_size);
        font_path = kBundledFontPath;
    }
    if (!opened_font) {
        fprintf(stderr, "[TimerHUD] Failed to open runtime font: %s\n", TTF_GetError());
        return false;
    }
    activeFont = opened_font;
    activePointSize = requested_point_size;
    return true;
}

bool initFontSystem(void) {
    return loadDefaultFont();
}

void shutdownFontSystem(void) {
    if (activeFont) {
        TTF_CloseFont(activeFont);
        activeFont = NULL;
    }
}

bool loadFontByID(FontID id) {
    (void)id;
    return loadDefaultFont();
}

TTF_Font* getActiveFont(void) {
    if (!activeFont) {
        loadDefaultFont();
    }
    return activeFont;
}

bool refreshActiveFontFromAnimationConfig(void) {
    char shared_font_path[256];
    const char* font_path = kSystemFontPath;
    int scaled_point_size = animation_config_scale_text_point_size(&animSettings, kBasePointSize, kMinPointSize);
    TTF_Font* refreshed = NULL;

    if (scaled_point_size <= 0) {
        scaled_point_size = kMinPointSize;
    }
    if (!ensureTTF()) {
        return false;
    }
    if (activeFont && scaled_point_size == activePointSize) {
        return true;
    }

    if (ray_tracing_shared_font_resolve_ui_regular(
            shared_font_path, sizeof(shared_font_path), &scaled_point_size)) {
        font_path = shared_font_path;
    }
    refreshed = TTF_OpenFont(font_path, scaled_point_size);
    if (!refreshed && strcmp(font_path, kSystemFontPath) != 0) {
        refreshed = TTF_OpenFont(kSystemFontPath, scaled_point_size);
        font_path = kSystemFontPath;
    }
    if (!refreshed && strcmp(font_path, kBundledFontPath) != 0) {
        refreshed = TTF_OpenFont(kBundledFontPath, scaled_point_size);
        font_path = kBundledFontPath;
    }
    if (!refreshed) {
        fprintf(stderr, "[TimerHUD] Failed to reload runtime font (%d): %s\n",
                scaled_point_size, TTF_GetError());
        return false;
    }

    if (activeFont) {
        TTF_CloseFont(activeFont);
    }
    activeFont = refreshed;
    activePointSize = scaled_point_size;
    return true;
}

int getActiveFontPointSize(void) {
    return activePointSize;
}
