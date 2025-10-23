#ifndef SDL_MENU_H
#define SDL_MENU_H

#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

// Functions to manage menu and settings
bool RunMenu(void);
void LoadConfig(void);
void SaveConfig(void);

void RenderButton(SDL_Renderer *renderer, TTF_Font *font, int x, int y, int width, int height, const char *text, bool active);
void RenderText(SDL_Renderer *renderer, TTF_Font *font, int x, int y, const char *format, ...);

#endif // SDL_MENU_H
