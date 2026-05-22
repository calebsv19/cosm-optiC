#include "render/text_draw.h"

#include "kit_render_external_text.h"
#include "render/text_font_cache.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

static int ray_tracing_text_font_is_usable(TTF_Font* font) {
    return font &&
           ray_tracing_text_has_font_source(font) &&
           ray_tracing_text_font_cache_contains(font);
}

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

int ray_tracing_text_has_font_source(TTF_Font* font) {
    return kit_render_external_text_has_font_source(font);
}

void ray_tracing_text_reset_renderer(SDL_Renderer* renderer) {
    kit_render_external_text_reset_renderer(renderer);
}

int ray_tracing_text_measure_utf8(SDL_Renderer* renderer,
                                  TTF_Font* font,
                                  const char* text,
                                  int* out_w,
                                  int* out_h) {
    if (!ray_tracing_text_font_is_usable(font)) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return 0;
    }
    return kit_render_external_text_measure_utf8(renderer, font, text, out_w, out_h);
}

int ray_tracing_text_line_height(SDL_Renderer* renderer,
                                 TTF_Font* font,
                                 int* out_h) {
    return ray_tracing_text_measure_utf8(renderer, font, "", NULL, out_h);
}

int ray_tracing_text_draw_utf8(SDL_Renderer* renderer,
                               TTF_Font* font,
                               const char* text,
                               SDL_Color color,
                               SDL_Rect* io_dst) {
    if (!ray_tracing_text_font_is_usable(font)) {
        return 0;
    }
    return kit_render_external_text_draw_utf8(renderer, font, text, color, io_dst);
}

int ray_tracing_text_draw_utf8_at(SDL_Renderer* renderer,
                                  TTF_Font* font,
                                  const char* text,
                                  int x,
                                  int y,
                                  SDL_Color color) {
    if (!ray_tracing_text_font_is_usable(font)) {
        return 0;
    }
    return kit_render_external_text_draw_utf8_at(renderer, font, text, x, y, color);
}

int ray_tracing_text_draw_utf8_wrapped(SDL_Renderer* renderer,
                                       TTF_Font* font,
                                       const char* text,
                                       int wrap_width,
                                       SDL_Color color,
                                       SDL_Rect* io_dst) {
    if (!ray_tracing_text_font_is_usable(font)) {
        return 0;
    }
    return kit_render_external_text_draw_utf8_wrapped(renderer,
                                                      font,
                                                      text,
                                                      wrap_width,
                                                      color,
                                                      io_dst);
}
