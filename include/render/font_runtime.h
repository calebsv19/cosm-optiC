#ifndef RAY_TRACING_RENDER_FONT_RUNTIME_H
#define RAY_TRACING_RENDER_FONT_RUNTIME_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>

bool ray_tracing_font_runtime_init(void);
void ray_tracing_font_runtime_shutdown(void);
bool ray_tracing_font_runtime_ensure_ttf(void);

void ray_tracing_font_runtime_attach_renderer(SDL_Renderer* renderer);
void ray_tracing_font_runtime_detach_renderer(SDL_Renderer* renderer);
SDL_Renderer* ray_tracing_font_runtime_attached_renderer(void);

bool ray_tracing_font_runtime_is_ready(void);
uint32_t ray_tracing_font_runtime_generation(void);

void ray_tracing_font_runtime_invalidate_active_font(void);
void ray_tracing_font_runtime_invalidate_all(void);

TTF_Font* ray_tracing_font_runtime_get_active_font(void);
bool ray_tracing_font_runtime_activate_default_font(void);
bool ray_tracing_font_runtime_refresh_active_font(void);
int ray_tracing_font_runtime_active_point_size(void);

TTF_Font* ray_tracing_font_runtime_get_ui_regular(SDL_Renderer* renderer,
                                                  int logical_point_size,
                                                  int min_point_size);
int ray_tracing_font_runtime_ui_regular_base_point_size(int fallback_point_size);

#endif
