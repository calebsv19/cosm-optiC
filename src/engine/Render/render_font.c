#include "engine/Render/render_font.h"
#include "config/config_manager.h"
#include <stdio.h>

static TTF_Font* activeFont = NULL;
static int activePointSize = 16;
static const int kBasePointSize = 16;
static const int kMinPointSize = 8;

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
    if (activeFont) return true;
    if (!ensureTTF()) return false;
    activePointSize = animation_config_scale_text_point_size(&animSettings, kBasePointSize, kMinPointSize);
    activeFont = TTF_OpenFont("config/default.ttf", activePointSize);
    if (!activeFont) {
        fprintf(stderr, "[TimerHUD] Failed to open font config/default.ttf: %s\n", TTF_GetError());
        return false;
    }
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

    refreshed = TTF_OpenFont("config/default.ttf", scaled_point_size);
    if (!refreshed) {
        fprintf(stderr, "[TimerHUD] Failed to reload font config/default.ttf (%d): %s\n",
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
