#include "ui/menu_panel_chrome.h"

#include "ui/sdl_menu_render.h"
#include "ui/shared_theme_font_adapter.h"

static Uint8 menu_panel_alpha(Uint8 base_alpha) {
    return (base_alpha == 0u) ? 236u : base_alpha;
}

void menu_panel_chrome_draw(SDL_Renderer* renderer,
                            TTF_Font* font,
                            const SDL_Rect* rect,
                            const char* title,
                            bool accent_title) {
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    SDL_Rect title_band;
    SDL_Rect divider;
    SDL_Color title_color;
    if (!renderer || !rect || rect->w <= 0 || rect->h <= 0) return;

    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r,
                               palette.panel_fill.g,
                               palette.panel_fill.b,
                               menu_panel_alpha(palette.panel_fill.a));
    } else {
        SDL_SetRenderDrawColor(renderer, 22, 24, 28, 238);
    }
    SDL_RenderFillRect(renderer, rect);

    if (title && title[0]) {
        title_band = (SDL_Rect){rect->x, rect->y, rect->w, MENU_PANEL_CHROME_TITLE_BAND};
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.button_fill.r,
                                   palette.button_fill.g,
                                   palette.button_fill.b,
                                   248);
        } else {
            SDL_SetRenderDrawColor(renderer, 30, 32, 38, 250);
        }
        SDL_RenderFillRect(renderer, &title_band);

        divider = (SDL_Rect){
            rect->x + MENU_PANEL_CHROME_INSET,
            rect->y + MENU_PANEL_CHROME_TITLE_BAND - 1,
            rect->w - MENU_PANEL_CHROME_INSET * 2,
            1
        };
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_border.r,
                                   palette.panel_border.g,
                                   palette.panel_border.b,
                                   220);
        } else {
            SDL_SetRenderDrawColor(renderer, 86, 92, 104, 220);
        }
        SDL_RenderFillRect(renderer, &divider);

        if (has_shared_palette) {
            title_color = accent_title ? palette.accent_primary : palette.text_primary;
        } else {
            title_color = accent_title ? (SDL_Color){210, 220, 240, 255}
                                       : (SDL_Color){226, 228, 232, 255};
        }
        menu_render_draw_text_color(renderer,
                                    font,
                                    rect->x + MENU_PANEL_CHROME_INSET,
                                    rect->y + 7,
                                    title_color,
                                    title);
    }

    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r,
                               palette.panel_border.g,
                               palette.panel_border.b,
                               palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 78, 84, 96, 255);
    }
    SDL_RenderDrawRect(renderer, rect);
}
