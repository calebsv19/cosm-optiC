#ifndef MENU_NUMERIC_CONTROLS_H
#define MENU_NUMERIC_CONTROLS_H
#include "ui/sdl_menu_render.h"

MenuNumericSpec menu_numeric_spec(const MenuRuntimeState *state, int *target, int min, int max);
void menu_numeric_apply(MenuRuntimeState *state, int *target, int value);
void menu_numeric_layout(MenuSlider *slider, int text_height, int right);
bool menu_numeric_click(SDL_Event *event, const SliderLayout *layout, MenuRuntimeState *state, TTF_Font *font);
bool menu_numeric_key(MenuRuntimeState *state, const SDL_Event *event);
bool menu_numeric_finish(MenuRuntimeState *state, bool apply);
void menu_numeric_draw(SDL_Renderer *renderer, TTF_Font *font, MenuRuntimeState *state, const MenuSlider *slider);

#endif
