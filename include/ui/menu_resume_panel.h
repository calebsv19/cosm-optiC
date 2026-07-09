#ifndef RAY_TRACING_MENU_RESUME_PANEL_H
#define RAY_TRACING_MENU_RESUME_PANEL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "ui/menu_layout.h"
#include "ui/sdl_menu_state.h"

typedef struct {
    SDL_Rect panelRect;
    SDL_Rect statusRect;
    SDL_Rect resumeToggleRect;
    SDL_Rect startFrameRect;
    SDL_Rect nextExistingRect;
} MenuResumePanelLayout;

void menu_resume_panel_build_layout(TTF_Font* font,
                                    const MenuRuntimeState* state,
                                    const MenuScreenLayout* screen_layout,
                                    MenuResumePanelLayout* out_layout);
bool menu_resume_panel_handle_click(const SDL_Event* event,
                                    MenuRuntimeState* state,
                                    const MenuResumePanelLayout* layout);
void menu_resume_panel_render(SDL_Renderer* renderer,
                              TTF_Font* font,
                              MenuRuntimeState* state,
                              const MenuResumePanelLayout* layout);

#endif
