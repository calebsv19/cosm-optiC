#ifndef RAY_TRACING_RENDER_TEXT_DRAW_H
#define RAY_TRACING_RENDER_TEXT_DRAW_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void ray_tracing_text_register_font_source(TTF_Font* font,
                                           const char* path,
                                           int logical_point_size,
                                           int loaded_point_size,
                                           int kerning_enabled);

void ray_tracing_text_unregister_font_source(TTF_Font* font);
int ray_tracing_text_has_font_source(TTF_Font* font);
int ray_tracing_text_font_is_current(TTF_Font* font);

void ray_tracing_text_reset_font_system(void);
void ray_tracing_text_reset_renderer(SDL_Renderer* renderer);

int ray_tracing_text_measure_utf8(SDL_Renderer* renderer,
                                  TTF_Font* font,
                                  const char* text,
                                  int* out_w,
                                  int* out_h);
int ray_tracing_text_line_height(SDL_Renderer* renderer,
                                 TTF_Font* font,
                                 int* out_h);

int ray_tracing_text_draw_utf8(SDL_Renderer* renderer,
                               TTF_Font* font,
                               const char* text,
                               SDL_Color color,
                               SDL_Rect* io_dst);

int ray_tracing_text_draw_utf8_at(SDL_Renderer* renderer,
                                  TTF_Font* font,
                                  const char* text,
                                  int x,
                                  int y,
                                  SDL_Color color);

int ray_tracing_text_draw_utf8_wrapped(SDL_Renderer* renderer,
                                       TTF_Font* font,
                                       const char* text,
                                       int wrap_width,
                                       SDL_Color color,
                                       SDL_Rect* io_dst);

#endif
