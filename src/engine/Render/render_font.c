#include "engine/Render/render_font.h"
#include <stdio.h>

static TTF_Font* activeFont = NULL;
static int activePointSize = 16;

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
