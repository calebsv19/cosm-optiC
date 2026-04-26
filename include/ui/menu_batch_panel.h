#ifndef RAY_TRACING_MENU_BATCH_PANEL_H
#define RAY_TRACING_MENU_BATCH_PANEL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stddef.h>

#include "ui/menu_layout.h"
#include "ui/sdl_menu_state.h"

typedef struct {
    SDL_Rect panelRect;
    SDL_Rect frameDirValueRect;
    SDL_Rect frameDirEditRect;
    SDL_Rect frameDirFolderRect;
    SDL_Rect frameDirApplyRect;
    SDL_Rect videoRootValueRect;
    SDL_Rect videoRootEditRect;
    SDL_Rect videoRootFolderRect;
    SDL_Rect videoRootApplyRect;
    SDL_Rect frameCountValueRect;
    SDL_Rect fpsValueRect;
    SDL_Rect clearFramesRect;
    SDL_Rect makeVideoRect;
} MenuBatchPanelLayout;

void menu_batch_panel_build_layout(TTF_Font* font,
                                   const MenuRuntimeState* state,
                                   const MenuScreenLayout* screen_layout,
                                   MenuBatchPanelLayout* out_layout);
void menu_batch_panel_refresh(MenuRuntimeState* state);
bool menu_batch_panel_edit_active(const MenuRuntimeState* state);
void menu_batch_panel_finish_edit(MenuRuntimeState* state, bool apply);
void menu_batch_panel_backspace_edit(MenuRuntimeState* state);
bool menu_batch_panel_append_text(MenuRuntimeState* state, const char* text);
bool menu_batch_panel_handle_click(const SDL_Event* event,
                                   SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   MenuRuntimeState* state,
                                   const MenuBatchPanelLayout* layout);
void menu_batch_panel_render(SDL_Renderer* renderer,
                             TTF_Font* font,
                             MenuRuntimeState* state,
                             const MenuBatchPanelLayout* layout);
void menu_batch_panel_build_frame_count_label(const MenuRuntimeState* state,
                                              char* out,
                                              size_t out_size);

#endif
