#ifndef RAY_TRACING_MENU_LAYOUT_H
#define RAY_TRACING_MENU_LAYOUT_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "ui/sdl_menu_state.h"

typedef struct MenuButtonLayout MenuButtonLayout;

typedef struct {
    SDL_Rect menuRect;
    SDL_Rect leftPanelRect;
    MenuWorkspaceLayout workspace;
    SDL_Rect centerControlsRect;
    SDL_Rect centerBatchRect;
    SDL_Rect centerResumeRect;
    SDL_Rect sliderPanelRect;
    SDL_Rect renderInfoRect;
    SDL_Rect routeStackRect;
    SDL_Rect bottomActionRowRect;
    SDL_Rect manifestReserveRect;
} MenuScreenLayout;

typedef struct MenuRuntimeRouteActionLayout {
    SDL_Rect spaceModeRect;
    SDL_Rect sceneModeRect;
    SDL_Rect sceneEditorRect;
    SDL_Rect previewRect;
    SDL_Rect startRect;
} MenuRuntimeRouteActionLayout;

void menu_layout_build_base(TTF_Font* font,
                            MenuRuntimeState* state,
                            int window_width,
                            int window_height,
                            MenuScreenLayout* out_layout);
void menu_layout_finalize_with_buttons(MenuScreenLayout* layout,
                                       const MenuButtonLayout* buttons,
                                       const MenuRuntimeState* state);
bool menu_layout_build_runtime_route_actions(
    const SDL_Rect* route_stack_rect,
    MenuRuntimeRouteActionLayout* out_layout);

#endif
