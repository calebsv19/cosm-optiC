#ifndef RAY_TRACING_MENU_PANEL_CHROME_H
#define RAY_TRACING_MENU_PANEL_CHROME_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#define MENU_PANEL_CHROME_TITLE_BAND 28
#define MENU_PANEL_CHROME_INSET 12

void menu_panel_chrome_draw(SDL_Renderer* renderer,
                            TTF_Font* font,
                            const SDL_Rect* rect,
                            const char* title,
                            bool accent_title);

#endif
