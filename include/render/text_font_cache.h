#ifndef RAY_TRACING_TEXT_FONT_CACHE_H
#define RAY_TRACING_TEXT_FONT_CACHE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

TTF_Font* ray_tracing_text_font_cache_get_ui_regular(SDL_Renderer* renderer,
                                                     int logical_point_size,
                                                     int min_point_size);
int ray_tracing_text_font_cache_ui_regular_base_point_size(int fallback_point_size);
int ray_tracing_text_font_cache_contains(TTF_Font* font);
void ray_tracing_text_font_cache_shutdown(void);

#endif
