#ifndef MENU_SCROLL_H
#define MENU_SCROLL_H
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "kit_ui_sdl.h"

enum { MENU_SCROLL_SCENE, MENU_SCROLL_VOLUME, MENU_SCROLL_SETTINGS, MENU_SCROLL_CONTROLS, MENU_SCROLL_COUNT };
typedef struct {
    SDL_Rect viewport;
    KitUiSdlScrollbarLayout layout;
    float *offset;
    float maximum, dragOffset;
    int dragY;
    bool visible, dragging;
} MenuScroll;

void menu_scroll_setup(MenuScroll *scroll, SDL_Rect viewport, int content_height, float *offset);
void menu_scroll_draw(SDL_Renderer *renderer, const MenuScroll *scroll);
bool menu_scroll_event(MenuScroll *scroll, const SDL_Event *event);
#endif
