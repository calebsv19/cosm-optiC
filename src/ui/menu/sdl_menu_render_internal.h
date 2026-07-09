#ifndef SDL_MENU_RENDER_INTERNAL_H
#define SDL_MENU_RENDER_INTERNAL_H

#include "ui/sdl_menu_render.h"

#define SDL_MENU_RENDER_MANIFEST_PANEL_MIN_HEIGHT 140
#define SDL_MENU_RENDER_MANIFEST_PANEL_MAX_HEIGHT 340
#define SDL_MENU_RENDER_MANIFEST_ITEM_PADDING 6
#define SDL_MENU_RENDER_MANIFEST_SCROLLBAR_WIDTH 10
#define SDL_MENU_RENDER_MANIFEST_FALLBACK_BOTTOM_Y 770

SDL_Color menu_render_ensure_highlight_fill_contrast(SDL_Color fill,
                                                     SDL_Color preferred_text,
                                                     SDL_Color darker_anchor);
SDL_Color menu_render_choose_readable_text(SDL_Color background,
                                           SDL_Color preferred_text);

void menu_render_format_manifest_button_label(MenuRuntimeState* state,
                                              char *out,
                                              size_t out_size);
void menu_render_format_volume_button_label(MenuRuntimeState* state,
                                            char *out,
                                            size_t out_size);
void menu_render_draw_manifest_dropdown(SDL_Renderer *renderer,
                                        TTF_Font *font,
                                        MenuRuntimeState* state,
                                        const MenuButtonLayout* buttons,
                                        const MenuScreenLayout* screen_layout);
void menu_render_draw_volume_dropdown(SDL_Renderer *renderer,
                                      TTF_Font *font,
                                      MenuRuntimeState* state,
                                      const MenuButtonLayout* buttons,
                                      const MenuScreenLayout* screen_layout);

#endif
