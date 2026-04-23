#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct RayTracingThemePalette {
    SDL_Color background_fill;
    SDL_Color panel_fill;
    SDL_Color panel_border;
    SDL_Color button_fill;
    SDL_Color button_active_fill;
    SDL_Color button_text;
    SDL_Color text_primary;
    SDL_Color text_muted;
    SDL_Color accent_primary;
} RayTracingThemePalette;

bool ray_tracing_shared_theme_resolve_palette(RayTracingThemePalette* out_palette);
SDL_Color ray_tracing_theme_resolve_button_active_fill(RayTracingThemePalette palette);
SDL_Color ray_tracing_theme_choose_button_text(SDL_Color fill, RayTracingThemePalette palette);
bool ray_tracing_shared_font_resolve_ui_regular(char* out_path, size_t out_path_size, int* out_point_size);
bool ray_tracing_shared_theme_cycle_next(void);
bool ray_tracing_shared_theme_cycle_prev(void);
bool ray_tracing_shared_theme_set_preset(const char* preset_name);
bool ray_tracing_shared_theme_current_preset(char* out_name, size_t out_name_size);
bool ray_tracing_shared_theme_load_persisted(void);
bool ray_tracing_shared_theme_save_persisted(void);
