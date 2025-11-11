#ifndef ENGINE_RENDER_FONT_H
#define ENGINE_RENDER_FONT_H

#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

typedef enum {
    FONT_DEFAULT = 0
} FontID;

bool initFontSystem(void);
void shutdownFontSystem(void);
bool loadFontByID(FontID id);
TTF_Font* getActiveFont(void);

#endif
