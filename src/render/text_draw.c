#include "render/text_draw.h"

#include "kit_render_external_text.h"
#include "render/font_runtime.h"
#include "render/text_font_cache.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>
#include <string.h>

typedef struct RayTracingTextFontGenerationEntry {
    TTF_Font* font;
    uint32_t generation;
} RayTracingTextFontGenerationEntry;

enum {
    RAY_TRACING_TEXT_FONT_GENERATION_CAPACITY = 256
};

static RayTracingTextFontGenerationEntry
    g_font_generations[RAY_TRACING_TEXT_FONT_GENERATION_CAPACITY];

static RayTracingTextFontGenerationEntry* ray_tracing_text_find_generation_slot(TTF_Font* font) {
    size_t i = 0;
    RayTracingTextFontGenerationEntry* empty_slot = NULL;

    if (!font) {
        return NULL;
    }
    for (i = 0; i < RAY_TRACING_TEXT_FONT_GENERATION_CAPACITY; ++i) {
        RayTracingTextFontGenerationEntry* entry = &g_font_generations[i];
        if (entry->font == font) {
            return entry;
        }
        if (!entry->font && !empty_slot) {
            empty_slot = entry;
        }
    }
    return empty_slot;
}

static int ray_tracing_text_font_generation_matches(TTF_Font* font) {
    size_t i = 0;
    uint32_t current_generation = ray_tracing_font_runtime_generation();

    if (!font) {
        return 0;
    }
    for (i = 0; i < RAY_TRACING_TEXT_FONT_GENERATION_CAPACITY; ++i) {
        const RayTracingTextFontGenerationEntry* entry = &g_font_generations[i];
        if (entry->font == font) {
            return entry->generation == current_generation;
        }
    }
    return 0;
}

int ray_tracing_text_font_is_current(TTF_Font* font) {
    return font &&
           ray_tracing_font_runtime_is_ready() &&
           ray_tracing_text_font_generation_matches(font) &&
           ray_tracing_text_has_font_source(font) &&
           ray_tracing_text_font_cache_contains(font);
}

void ray_tracing_text_register_font_source(TTF_Font* font,
                                           const char* path,
                                           int logical_point_size,
                                           int loaded_point_size,
                                           int kerning_enabled) {
    RayTracingTextFontGenerationEntry* entry = ray_tracing_text_find_generation_slot(font);
    if (entry) {
        entry->font = font;
        entry->generation = ray_tracing_font_runtime_generation();
    }
    kit_render_external_text_register_font_source(font,
                                                  path,
                                                  logical_point_size,
                                                  loaded_point_size,
                                                  kerning_enabled);
}

void ray_tracing_text_unregister_font_source(TTF_Font* font) {
    RayTracingTextFontGenerationEntry* entry = ray_tracing_text_find_generation_slot(font);
    if (entry && entry->font == font) {
        memset(entry, 0, sizeof(*entry));
    }
    kit_render_external_text_unregister_font_source(font);
}

int ray_tracing_text_has_font_source(TTF_Font* font) {
    return kit_render_external_text_has_font_source(font);
}

void ray_tracing_text_reset_font_system(void) {
    memset(g_font_generations, 0, sizeof(g_font_generations));
    kit_render_external_text_reset_font_system();
}

void ray_tracing_text_reset_renderer(SDL_Renderer* renderer) {
    kit_render_external_text_reset_renderer(renderer);
}

int ray_tracing_text_measure_utf8(SDL_Renderer* renderer,
                                  TTF_Font* font,
                                  const char* text,
    int* out_w,
    int* out_h) {
    if (!ray_tracing_text_font_is_current(font)) {
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
    if (!ray_tracing_text_font_is_current(font)) {
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
    if (!ray_tracing_text_font_is_current(font)) {
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
    if (!ray_tracing_text_font_is_current(font)) {
        return 0;
    }
    return kit_render_external_text_draw_utf8_wrapped(renderer,
                                                      font,
                                                      text,
                                                      wrap_width,
                                                      color,
                                                      io_dst);
}
