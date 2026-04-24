#include "render/text_draw.h"

#include "kit_render_external_text.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void ray_tracing_text_register_font_source(TTF_Font* font,
                                           const char* path,
                                           int logical_point_size,
                                           int loaded_point_size,
                                           int kerning_enabled) {
    kit_render_external_text_register_font_source(font,
                                                  path,
                                                  logical_point_size,
                                                  loaded_point_size,
                                                  kerning_enabled);
}

void ray_tracing_text_unregister_font_source(TTF_Font* font) {
    kit_render_external_text_unregister_font_source(font);
}

void ray_tracing_text_reset_renderer(SDL_Renderer* renderer) {
    kit_render_external_text_reset_renderer(renderer);
}

int ray_tracing_text_measure_utf8(SDL_Renderer* renderer,
                                  TTF_Font* font,
                                  const char* text,
                                  int* out_w,
                                  int* out_h) {
    return kit_render_external_text_measure_utf8(renderer, font, text, out_w, out_h);
}

int ray_tracing_text_draw_utf8(SDL_Renderer* renderer,
                               TTF_Font* font,
                               const char* text,
                               SDL_Color color,
                               SDL_Rect* io_dst) {
    return kit_render_external_text_draw_utf8(renderer, font, text, color, io_dst);
}

int ray_tracing_text_draw_utf8_at(SDL_Renderer* renderer,
                                  TTF_Font* font,
                                  const char* text,
                                  int x,
                                  int y,
                                  SDL_Color color) {
    return kit_render_external_text_draw_utf8_at(renderer, font, text, x, y, color);
}

int ray_tracing_text_draw_utf8_wrapped(SDL_Renderer* renderer,
                                       TTF_Font* font,
                                       const char* text,
                                       int wrap_width,
                                       SDL_Color color,
                                       SDL_Rect* io_dst) {
    return kit_render_external_text_draw_utf8_wrapped(renderer,
                                                      font,
                                                      text,
                                                      wrap_width,
                                                      color,
                                                      io_dst);
}
