#ifndef RAY_TRACING_TEXT_ZOOM_SHORTCUTS_H
#define RAY_TRACING_TEXT_ZOOM_SHORTCUTS_H

#include <SDL2/SDL.h>
#include <stdbool.h>

bool ray_tracing_text_zoom_apply_shortcut(SDL_Keycode key,
                                          SDL_Keymod mod,
                                          bool* out_changed,
                                          int* out_step,
                                          int* out_percent);

#endif
