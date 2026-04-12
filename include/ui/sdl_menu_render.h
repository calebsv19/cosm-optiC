#ifndef SDL_MENU_RENDER_H
#define SDL_MENU_RENDER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "ui/sdl_menu_state.h"

#define SDL_MENU_MAX_SLIDERS 24

typedef struct {
    int *value;
    int min;
    int max;
    SDL_Rect trackRect;
    SDL_Rect hitRect;
    int labelX;
    int labelY;
    int valueX;
    int valueY;
    const char *label;
} MenuSlider;

typedef struct {
    MenuSlider items[SDL_MENU_MAX_SLIDERS];
    size_t count;
    int nextY;
    int trackHeight;
    int knobWidth;
    int knobHeight;
    SDL_Rect panelRect;
    int contentBottomY;
    float maxScroll;
    float scroll;
} SliderLayout;

typedef struct {
    SDL_Rect interactiveRect;
    SDL_Rect deepRenderRect;
    SDL_Rect bounceRect;
    SDL_Rect autoMp4Rect;
    SDL_Rect integratorRect;
    SDL_Rect pathRouletteRect;
    SDL_Rect pathBsdfRect;
    SDL_Rect loadSceneRect;
    SDL_Rect inputRootValueRect;
    SDL_Rect inputRootEditRect;
    SDL_Rect inputRootFolderRect;
    SDL_Rect inputRootApplyRect;
    SDL_Rect outputRootValueRect;
    SDL_Rect outputRootEditRect;
    SDL_Rect outputRootFolderRect;
    SDL_Rect outputRootApplyRect;
    SDL_Rect falloffRect;
    SDL_Rect tileRect;
    SDL_Rect tilePreviewRect;
    SDL_Rect lightHeightRect;
    SDL_Rect sceneEditorRect;
    SDL_Rect sceneModeRect;
    SDL_Rect spaceModeRect;
    SDL_Rect saveRect;
    SDL_Rect restoreRect;
    SDL_Rect previewRect;
    SDL_Rect exitRect;
    SDL_Rect startRect;
    bool showLightHeight;
    bool showPathToggles;
} MenuButtonLayout;

const char* menu_space_mode_button_label(void);

void menu_render_build_button_layout(TTF_Font* font,
                                     MenuRuntimeState* state,
                                     MenuButtonLayout* out_layout);
void menu_render_build_slider_layout(TTF_Font* font,
                                     MenuRuntimeState* state,
                                     const MenuButtonLayout* buttons,
                                     SliderLayout* out_layout);
void menu_render_draw_sliders(SDL_Renderer* renderer,
                              TTF_Font* font,
                              MenuRuntimeState* state,
                              const SliderLayout* layout);

void menu_render_frame(SDL_Renderer* renderer, TTF_Font* font, MenuRuntimeState* state);

void RenderText(SDL_Renderer *renderer, TTF_Font *font, int x, int y, const char *format, ...);
void RenderButton(SDL_Renderer *renderer, TTF_Font *font, int x, int y, int width, int height, const char *text, bool active);

#endif
