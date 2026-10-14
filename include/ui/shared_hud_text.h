#ifndef RAY_TRACING_SHARED_HUD_TEXT_H
#define RAY_TRACING_SHARED_HUD_TEXT_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
void ray_tracing_shared_hud_text(SDL_Renderer *,TTF_Font *,SDL_Rect,const char *,SDL_Color);
#endif
