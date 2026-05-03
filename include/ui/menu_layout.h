#ifndef RAY_TRACING_MENU_LAYOUT_H
#define RAY_TRACING_MENU_LAYOUT_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "ui/sdl_menu_state.h"

typedef struct MenuButtonLayout MenuButtonLayout;

typedef struct {
    SDL_Rect menuRect;
    SDL_Rect leftPanelRect;
    SDL_Rect centerControlsRect;
    SDL_Rect centerBatchRect;
    SDL_Rect sliderPanelRect;
    SDL_Rect routeStackRect;
    SDL_Rect bottomActionRowRect;
    SDL_Rect manifestReserveRect;
} MenuScreenLayout;

void menu_layout_build_base(TTF_Font* font,
                            const MenuRuntimeState* state,
                            int window_width,
                            int window_height,
                            MenuScreenLayout* out_layout);
void menu_layout_finalize_with_buttons(MenuScreenLayout* layout,
                                       const MenuButtonLayout* buttons,
                                       const MenuRuntimeState* state);

#endif
