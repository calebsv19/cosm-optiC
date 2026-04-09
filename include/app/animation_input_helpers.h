#ifndef ANIMATION_INPUT_HELPERS_H
#define ANIMATION_INPUT_HELPERS_H

#include <SDL2/SDL.h>
#include <stdbool.h>

void animation_handle_fluid_overlay_key(SDL_Keycode key);
bool animation_handle_text_zoom_shortcut(const SDL_KeyboardEvent* key_event);

#endif
