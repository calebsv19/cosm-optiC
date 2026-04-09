#ifndef SDL_MENU_INPUT_H
#define SDL_MENU_INPUT_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "ui/sdl_menu_render.h"
#include "ui/sdl_menu_state.h"

void menu_input_handle_key(SDL_Event* event,
                           bool* running,
                           TTF_Font** font,
                           MenuRuntimeState* state);

void menu_input_handle_mouse_motion(SDL_Event* event, MenuRuntimeState* state);
void menu_input_handle_mouse_wheel(SDL_Event* event, MenuRuntimeState* state);

void menu_input_handle_mouse_click(SDL_Event* event,
                                   bool* running,
                                   bool* menuExitedNormally,
                                   SDL_Renderer* renderer,
                                   TTF_Font** font,
                                   MenuRuntimeState* state);

#endif
