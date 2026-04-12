#include "ui/sdl_menu_render.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "app/animation.h"
#include "editor/editor_mode_router.h"
#include "engine/Render/render_font.h"
#include "render/render_helper.h"
#include "engine/Render/render_pipeline.h"
#include "ui/shared_theme_font_adapter.h"

#define MENU_WIDTH 1000
#define MENU_HEIGHT 900
#define MENU_MARGIN_X 30
#define MENU_MARGIN_Y 30
#define TOGGLE_BUTTON_WIDTH 200
#define TOGGLE_BUTTON_HEIGHT 50
#define TOGGLE_BUTTON_MARGIN_X MENU_MARGIN_X
#define TOGGLE_BUTTON_MARGIN_Y MENU_MARGIN_Y
#define TOGGLE_BUTTON_SPACING 10
#define SUBSETTING_BUTTON_WIDTH 175
#define SUBSETTING_BUTTON_HEIGHT 40
#define SUBSETTING_BUTTON_MARGIN_X (MENU_MARGIN_X + 10)
#define BOTTOM_BUTTON_SPACING 10
#define BOTTOM_BUTTON_WIDTH_START 200
#define BOTTOM_BUTTON_HEIGHT_START 50
#define BOTTOM_BUTTON_MARGIN_Y_START (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_START)
#define BOTTOM_BUTTON_WIDTH_EXIT 180
#define BOTTOM_BUTTON_HEIGHT_EXIT 40
#define BOTTOM_BUTTON_MARGIN_X_EXIT (MENU_MARGIN_X)
#define BOTTOM_BUTTON_MARGIN_Y_EXIT (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_EXIT)
#define BOTTOM_BUTTON_WIDTH_PREVIEW 180
#define BOTTOM_BUTTON_HEIGHT_PREVIEW 40
#define BOTTOM_BUTTON_MARGIN_X_PREVIEW (MENU_MARGIN_X)
#define BOTTOM_BUTTON_MARGIN_Y_PREVIEW (BOTTOM_BUTTON_MARGIN_Y_EXIT - BOTTOM_BUTTON_HEIGHT_PREVIEW - BOTTOM_BUTTON_SPACING)
#define BOTTOM_BUTTON_WIDTH_RESTORE 180
#define BOTTOM_BUTTON_HEIGHT_RESTORE 40
#define BOTTOM_BUTTON_MARGIN_Y_RESTORE (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_RESTORE)
#define BOTTOM_BUTTON_WIDTH_SAVE 100
#define BOTTOM_BUTTON_HEIGHT_SAVE 40
#define BOTTOM_BUTTON_MARGIN_Y_SAVE (MENU_HEIGHT -MENU_MARGIN_Y - BOTTOM_BUTTON_HEIGHT_RESTORE)
#define SLIDER_WIDTH 250
#define SLIDER_HEIGHT 8
#define SLIDER_SPACING 10
#define SLIDER_MARGIN_X (MENU_WIDTH - SLIDER_WIDTH - MENU_MARGIN_X - 40)
#define SLIDER_MARGIN_Y MENU_MARGIN_Y
#define FORWARD_FALLOFF_BUTTON_WIDTH 200
#define FORWARD_FALLOFF_BUTTON_HEIGHT 40
#define FORWARD_FALLOFF_BUTTON_SPACING 10
#define SLIDER_SECTION_GAP 30
#define TILE_BUTTON_WIDTH 200
#define TILE_BUTTON_HEIGHT 40
#define INTEGRATOR_BUTTON_WIDTH 220
#define INTEGRATOR_BUTTON_HEIGHT 40
#define PATH_TOGGLE_WIDTH 180
#define PATH_TOGGLE_HEIGHT 35
#define PATH_TOGGLE_SPACING 10
#define LOAD_SCENE_BUTTON_WIDTH INTEGRATOR_BUTTON_WIDTH
#define LOAD_SCENE_BUTTON_HEIGHT 36
#define LOAD_SCENE_BUTTON_X TOGGLE_BUTTON_MARGIN_X
#define LOAD_SCENE_BUTTON_SPACING 20
#define ROOT_ROW_HEIGHT 34
#define ROOT_ROW_SPACING 8
#define ROOT_CTRL_BUTTON_W 56
#define MANIFEST_PANEL_EXTRA_WIDTH 40
#define MANIFEST_PANEL_MIN_HEIGHT 140
#define MANIFEST_PANEL_MAX_HEIGHT 260
#define MANIFEST_ITEM_PADDING 6
#define MANIFEST_SCROLLBAR_WIDTH 10

static void render_surface(SDL_Renderer* renderer, SDL_Surface* surface, const SDL_Rect* dst) {
    if (!renderer || !surface || !dst) return;
#if USE_VULKAN
    VkRendererTexture texture;
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer*)renderer, surface, &texture,
                                                   VK_FILTER_LINEAR) != VK_SUCCESS) {
        return;
    }
    vk_renderer_draw_texture((VkRenderer*)renderer, &texture, NULL, dst);
    vk_renderer_queue_texture_destroy((VkRenderer*)renderer, &texture);
#else
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!textTexture) return;
    SDL_RenderCopy(renderer, textTexture, NULL, dst);
    SDL_DestroyTexture(textTexture);
#endif
}

const char* menu_space_mode_button_label(void) {
    return EditorModeRouter_SpaceButtonLabel();
}

void RenderText(SDL_Renderer *renderer, TTF_Font *font, int x, int y, const char *format, ...) {
    char buffer[64];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    SDL_Color textColor = has_shared_palette ? palette.text_primary : (SDL_Color){255, 255, 255, 255};
    SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, buffer, textColor);
    if (!textSurface) return;

    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    render_surface(renderer, textSurface, &textRect);
    SDL_FreeSurface(textSurface);
}

static void render_text_color(SDL_Renderer *renderer, TTF_Font *font, int x, int y, SDL_Color color, const char *text) {
    if (!text || !*text) return;
    SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, text, color);
    if (!textSurface) return;
    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    render_surface(renderer, textSurface, &textRect);
    SDL_FreeSurface(textSurface);
}

static void render_centered_text_color(SDL_Renderer *renderer,
                                       TTF_Font *font,
                                       const SDL_Rect *rect,
                                       SDL_Color color,
                                       const char *text) {
    SDL_Surface *textSurface;
    SDL_Rect textRect;
    if (!font || !rect || !text || !text[0]) return;
    textSurface = TTF_RenderUTF8_Blended(font, text, color);
    if (!textSurface) return;
    textRect = (SDL_Rect){
        rect->x + (rect->w - textSurface->w) / 2,
        rect->y + (rect->h - textSurface->h) / 2,
        textSurface->w,
        textSurface->h
    };
    render_surface(renderer, textSurface, &textRect);
    SDL_FreeSurface(textSurface);
}

static int color_luma(SDL_Color color) {
    return (299 * (int)color.r + 587 * (int)color.g + 114 * (int)color.b) / 1000;
}

static int color_contrast_gap(SDL_Color a, SDL_Color b) {
    int gap = color_luma(a) - color_luma(b);
    return gap < 0 ? -gap : gap;
}

static Uint8 mix_u8(Uint8 a, Uint8 b, int a_weight, int b_weight) {
    int total = a_weight + b_weight;
    if (total <= 0) return a;
    return (Uint8)(((int)a * a_weight + (int)b * b_weight) / total);
}

static SDL_Color mix_color(SDL_Color a, SDL_Color b, int a_weight, int b_weight) {
    return (SDL_Color){
        mix_u8(a.r, b.r, a_weight, b_weight),
        mix_u8(a.g, b.g, a_weight, b_weight),
        mix_u8(a.b, b.b, a_weight, b_weight),
        mix_u8(a.a, b.a, a_weight, b_weight)
    };
}

static SDL_Color ensure_highlight_fill_contrast(SDL_Color fill,
                                                SDL_Color preferred_text,
                                                SDL_Color darker_anchor) {
    if (color_contrast_gap(fill, preferred_text) >= 110) {
        return fill;
    }
    if (color_luma(preferred_text) >= 150) {
        return mix_color(fill, darker_anchor, 1, 2);
    }
    return mix_color(fill, (SDL_Color){240, 243, 247, fill.a}, 1, 2);
}

static SDL_Color choose_readable_text(SDL_Color background, SDL_Color preferred_text) {
    if (color_contrast_gap(background, preferred_text) >= 110) {
        return preferred_text;
    }
    if (color_luma(background) >= 150) {
        return (SDL_Color){24, 28, 34, preferred_text.a ? preferred_text.a : 255};
    }
    return (SDL_Color){245, 247, 250, preferred_text.a ? preferred_text.a : 255};
}

static void fit_text_to_width(TTF_Font *font,
                              const char *text,
                              int max_width,
                              char *out,
                              size_t out_size);

static void format_manifest_button_label(MenuRuntimeState* state, char *out, size_t outSize) {
    if (!state || !out || outSize == 0) return;
    const char *base = "Load Scene";
    if (!animSettings.fluidManifest[0]) {
        snprintf(out, outSize, "%s", base);
        return;
    }
    char label[128];
    menu_state_build_manifest_label(animSettings.fluidManifest, label, sizeof(label));
    snprintf(out, outSize, "%s: %s", base, label);
    if (strlen(out) >= outSize) {
        out[outSize - 1] = '\0';
    }
}

static void render_manifest_dropdown(SDL_Renderer *renderer,
                                     TTF_Font *font,
                                     MenuRuntimeState* state,
                                     const SDL_Rect* loadButtonRect) {
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    int panelX;
    int panelY;
    int panelW;
    int available;
    if (!state || !loadButtonRect) return;
    panelX = loadButtonRect->x;
    panelY = loadButtonRect->y + loadButtonRect->h + 6;
    panelW = loadButtonRect->w + MANIFEST_PANEL_EXTRA_WIDTH;
    available = BOTTOM_BUTTON_MARGIN_Y_PREVIEW - 10 - panelY;
    int panelH = MANIFEST_PANEL_MIN_HEIGHT;
    if (available > MANIFEST_PANEL_MIN_HEIGHT) panelH = available;
    if (panelH > MANIFEST_PANEL_MAX_HEIGHT) panelH = MANIFEST_PANEL_MAX_HEIGHT;
    int minH = SDL_MENU_MANIFEST_ITEM_HEIGHT + MANIFEST_ITEM_PADDING * 2 + 4;
    if (panelH < minH) panelH = minH;

    state->manifestPanelRect = (SDL_Rect){panelX, panelY, panelW, panelH};
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 28, 28, 30, 230);
    }
    SDL_RenderFillRect(renderer, &state->manifestPanelRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
    }
    SDL_RenderDrawRect(renderer, &state->manifestPanelRect);

    int listX = panelX + MANIFEST_ITEM_PADDING;
    int listY = panelY + MANIFEST_ITEM_PADDING;
    int listW = panelW - MANIFEST_ITEM_PADDING * 2 - MANIFEST_SCROLLBAR_WIDTH - 4;
    if (listW < 40) listW = 40;
    int listH = panelH - MANIFEST_ITEM_PADDING * 2;
    state->manifestListRect = (SDL_Rect){listX, listY, listW, listH};

    int contentH = (int)(state->manifestOptionCount * SDL_MENU_MANIFEST_ITEM_HEIGHT);
    state->manifestMaxScroll = (contentH > listH) ? (float)(contentH - listH) : 0.0f;
    state->manifestScrollbarVisible = state->manifestMaxScroll > 0.5f;
    menu_state_manifest_clamp_scroll(state);
    state->manifestTrackHeight = (float)listH;
    state->manifestThumbHeight = 0.0f;
    state->manifestScrollbarRect = (SDL_Rect){0, 0, 0, 0};

    if (state->manifestScrollbarVisible) {
        float thumb = ((float)listH * (float)listH) / (float)contentH;
        if (thumb < 16.0f) thumb = 16.0f;
        state->manifestThumbHeight = thumb;
        float trackRange = (float)listH - thumb;
        float thumbY = (trackRange > 0.0f && state->manifestMaxScroll > 0.0f)
                           ? (float)listY + (state->manifestScroll / state->manifestMaxScroll) * trackRange
                           : (float)listY;
        int scrollX = panelX + panelW - MANIFEST_SCROLLBAR_WIDTH - MANIFEST_ITEM_PADDING;
        SDL_Rect track = {scrollX, listY, MANIFEST_SCROLLBAR_WIDTH, listH};
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_border.r, palette.panel_border.g,
                                   palette.panel_border.b, palette.panel_border.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
        }
        SDL_RenderFillRect(renderer, &track);

        state->manifestScrollbarRect = (SDL_Rect){scrollX, (int)thumbY, MANIFEST_SCROLLBAR_WIDTH, (int)thumb};
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.accent_primary.r, palette.accent_primary.g,
                                   palette.accent_primary.b, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 120, 120, 140, 255);
        }
        SDL_RenderFillRect(renderer, &state->manifestScrollbarRect);
    }

    int firstIndex = (int)(state->manifestScroll / SDL_MENU_MANIFEST_ITEM_HEIGHT);
    int yOffset = -(int)state->manifestScroll % SDL_MENU_MANIFEST_ITEM_HEIGHT;
    SDL_Rect prev_clip = {0, 0, 0, 0};
    SDL_RenderGetClipRect(renderer, &prev_clip);
    SDL_RenderSetClipRect(renderer, &state->manifestListRect);
    for (int i = firstIndex; i < (int)state->manifestOptionCount; ++i) {
        int itemY = listY + yOffset + (i - firstIndex) * SDL_MENU_MANIFEST_ITEM_HEIGHT;
        if (itemY > listY + listH - SDL_MENU_MANIFEST_ITEM_HEIGHT) break;
        SDL_Rect itemRect = {listX, itemY, listW, SDL_MENU_MANIFEST_ITEM_HEIGHT};
        bool isSelected = animSettings.fluidManifest[0] &&
                          strcmp(animSettings.fluidManifest, state->manifestOptions[i].path) == 0;
        if (has_shared_palette) {
            SDL_Color fill = isSelected ? palette.accent_primary : palette.button_fill;
            if (isSelected) {
                fill = ensure_highlight_fill_contrast(fill, palette.text_primary, palette.panel_fill);
            }
            SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
        } else {
            SDL_SetRenderDrawColor(renderer,
                                   isSelected ? 70 : 50,
                                   isSelected ? 120 : 70,
                                   isSelected ? 90 : 70,
                                   255);
        }
        SDL_RenderFillRect(renderer, &itemRect);
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_border.r, palette.panel_border.g,
                                   palette.panel_border.b, palette.panel_border.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        }
        SDL_RenderDrawRect(renderer, &itemRect);
        SDL_Color textColor;
        if (has_shared_palette) {
            SDL_Color itemFill = isSelected
                                     ? ensure_highlight_fill_contrast(palette.accent_primary,
                                                                      palette.text_primary,
                                                                      palette.panel_fill)
                                     : palette.button_fill;
            textColor = choose_readable_text(itemFill, palette.text_primary);
        } else {
            textColor = isSelected ? (SDL_Color){220, 240, 220, 255} : (SDL_Color){230, 230, 230, 255};
        }
        char label_fit[256];
        fit_text_to_width(font, state->manifestOptions[i].name, itemRect.w - 12, label_fit, sizeof(label_fit));
        render_text_color(renderer, font, itemRect.x + 6, itemRect.y + 4, textColor, label_fit);
    }

    if (state->manifestOptionCount == 0) {
        SDL_Color c = has_shared_palette ? palette.text_muted : (SDL_Color){210, 210, 210, 255};
        render_text_color(renderer, font, listX, listY + 4, c, "No manifests found");
    }
    SDL_RenderSetClipRect(renderer, &prev_clip);
}

static int max_int(int a, int b) {
    return (a > b) ? a : b;
}

static int min_int(int a, int b) {
    return (a < b) ? a : b;
}

static SDL_Rect build_adaptive_button_rect(TTF_Font* font,
                                           int x,
                                           int y,
                                           int minWidth,
                                           int minHeight,
                                           const char* text,
                                           int maxWidth) {
    int width = minWidth;
    int height = minHeight;
    int textW = 0;
    int textH = 0;
    if (font && text && text[0] && TTF_SizeUTF8(font, text, &textW, &textH) == 0) {
        width = max_int(width, textW + 24);
        height = max_int(height, textH + 14);
    }
    if (maxWidth > 0) {
        width = min_int(width, maxWidth);
    }
    return (SDL_Rect){x, y, width, height};
}

static SDL_Rect build_adaptive_button_rect_right(TTF_Font* font,
                                                 int rightX,
                                                 int y,
                                                 int minWidth,
                                                 int minHeight,
                                                 const char* text,
                                                 int maxWidth) {
    SDL_Rect rect = build_adaptive_button_rect(font, 0, y, minWidth, minHeight, text, maxWidth);
    rect.x = rightX - rect.w;
    return rect;
}

static void render_button_rect(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect* rect, const char *text, bool active) {
    if (!rect) return;
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    SDL_Color fill = has_shared_palette
                         ? (active ? palette.button_active_fill : palette.button_fill)
                         : (active ? (SDL_Color){0, 255, 0, 255} : (SDL_Color){100, 100, 100, 255});
    SDL_Color textColor = has_shared_palette ? palette.button_text : (SDL_Color){255, 255, 255, 255};
    if (has_shared_palette && active) {
        fill = ensure_highlight_fill_contrast(fill, textColor, palette.panel_fill);
    }
    if (has_shared_palette) {
        textColor = choose_readable_text(fill, textColor);
    }

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, rect);

    render_centered_text_color(renderer, font, rect, textColor, text);
}

static void fit_text_to_width(TTF_Font *font,
                              const char *text,
                              int max_width,
                              char *out,
                              size_t out_size) {
    int w = 0;
    size_t len = 0;
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!text) return;
    snprintf(out, out_size, "%s", text);
    if (!font || max_width <= 0) return;
    if (TTF_SizeUTF8(font, out, &w, NULL) == 0 && w <= max_width) return;
    len = strlen(out);
    while (len > 0) {
        --len;
        out[len] = '\0';
        {
            char candidate[PATH_MAX];
            snprintf(candidate, sizeof(candidate), "%s...", out);
            if (TTF_SizeUTF8(font, candidate, &w, NULL) == 0 && w <= max_width) {
                snprintf(out, out_size, "%s", candidate);
                return;
            }
        }
    }
    snprintf(out, out_size, "...");
}

static void render_root_row(SDL_Renderer *renderer,
                            TTF_Font *font,
                            const SDL_Rect *value_rect,
                            const char *label,
                            const char *value,
                            bool editing) {
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    char line[PATH_MAX];
    if (!renderer || !font || !value_rect || !label) return;
    if (has_shared_palette) {
        SDL_Color fill = editing
                             ? ensure_highlight_fill_contrast(palette.button_active_fill,
                                                              palette.button_text,
                                                              palette.panel_fill)
                             : palette.button_fill;
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, value_rect);
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
        SDL_RenderDrawRect(renderer, value_rect);
    } else {
        SDL_SetRenderDrawColor(renderer, editing ? 65 : 40, editing ? 98 : 40, editing ? 124 : 44, 255);
        SDL_RenderFillRect(renderer, value_rect);
        SDL_SetRenderDrawColor(renderer, 92, 96, 108, 255);
        SDL_RenderDrawRect(renderer, value_rect);
    }
    snprintf(line, sizeof(line), "%s: %s", label, (value && value[0]) ? value : "(unset)");
    fit_text_to_width(font, line, value_rect->w - 12, line, sizeof(line));
    SDL_Rect prev_clip;
    SDL_RenderGetClipRect(renderer, &prev_clip);
    SDL_RenderSetClipRect(renderer, value_rect);
    if (has_shared_palette) {
        SDL_Color text_color = choose_readable_text(editing ? palette.button_active_fill : palette.button_fill,
                                                    palette.button_text);
        render_text_color(renderer, font, value_rect->x + 6, value_rect->y + 8, text_color, line);
    } else {
        render_text_color(renderer, font, value_rect->x + 6, value_rect->y + 8, (SDL_Color){235, 235, 235, 255}, line);
    }
    SDL_RenderSetClipRect(renderer, &prev_clip);
}

void RenderButton(SDL_Renderer *renderer,
                  TTF_Font *font,
                  int x,
                  int y,
                  int width,
                  int height,
                  const char *text,
                  bool active) {
    SDL_Rect rect = {x, y, width, height};
    render_button_rect(renderer, font, &rect, text, active);
}

void menu_render_build_button_layout(TTF_Font* font,
                                     MenuRuntimeState* state,
                                     MenuButtonLayout* out_layout) {
    MenuButtonLayout layout;
    char manifestLabel[160];
    const char* integratorLabel = "Integrator: Forward Light";
    int leftX = TOGGLE_BUTTON_MARGIN_X;
    int subX = SUBSETTING_BUTTON_MARGIN_X;
    int maxLeftWidth = SLIDER_MARGIN_X - leftX - 24;
    int leftColumnRight;
    int centerX;
    int centerMaxWidth;
    int rightEdge = MENU_WIDTH - MENU_MARGIN_X;
    int rightLimit;

    memset(&layout, 0, sizeof(layout));
    layout.showPathToggles = (animSettings.integratorMode == 1);
    layout.showLightHeight = false;
    if (maxLeftWidth < 120) maxLeftWidth = 120;

    layout.interactiveRect = build_adaptive_button_rect(font, leftX, TOGGLE_BUTTON_MARGIN_Y,
                                                        TOGGLE_BUTTON_WIDTH, TOGGLE_BUTTON_HEIGHT,
                                                        "Interactive Mode", maxLeftWidth);
    layout.deepRenderRect = build_adaptive_button_rect(font, leftX,
                                                       layout.interactiveRect.y + layout.interactiveRect.h + TOGGLE_BUTTON_SPACING,
                                                       TOGGLE_BUTTON_WIDTH, TOGGLE_BUTTON_HEIGHT,
                                                       "Deep Render", maxLeftWidth);

    layout.bounceRect = build_adaptive_button_rect(font, subX,
                                                   layout.deepRenderRect.y + layout.deepRenderRect.h + 15,
                                                   SUBSETTING_BUTTON_WIDTH, SUBSETTING_BUTTON_HEIGHT,
                                                   "Bounce Mode", maxLeftWidth);
    layout.autoMp4Rect = build_adaptive_button_rect(font, subX,
                                                    layout.bounceRect.y + layout.bounceRect.h + TOGGLE_BUTTON_SPACING,
                                                    SUBSETTING_BUTTON_WIDTH, SUBSETTING_BUTTON_HEIGHT,
                                                    "Auto MP4", maxLeftWidth);

    if (animSettings.integratorMode == 1) integratorLabel = "Integrator: Hybrid";
    else if (animSettings.integratorMode == 2) integratorLabel = "Integrator: Direct Light";
    layout.integratorRect = build_adaptive_button_rect(font, leftX,
                                                       layout.autoMp4Rect.y + layout.autoMp4Rect.h + 10,
                                                       INTEGRATOR_BUTTON_WIDTH, INTEGRATOR_BUTTON_HEIGHT,
                                                       integratorLabel, maxLeftWidth);

    layout.pathRouletteRect = build_adaptive_button_rect(font, leftX,
                                                         layout.integratorRect.y + layout.integratorRect.h + 10,
                                                         PATH_TOGGLE_WIDTH, PATH_TOGGLE_HEIGHT,
                                                         animSettings.pathRussianRoulette ? "Roulette: ON" : "Roulette: OFF",
                                                         maxLeftWidth);
    layout.pathBsdfRect = build_adaptive_button_rect(font, leftX,
                                                     layout.pathRouletteRect.y + layout.pathRouletteRect.h + PATH_TOGGLE_SPACING,
                                                     PATH_TOGGLE_WIDTH, PATH_TOGGLE_HEIGHT,
                                                     (animSettings.bsdfModel == 0) ? "BSDF: Lambert" : "BSDF: GGX",
                                                     maxLeftWidth);

    format_manifest_button_label(state, manifestLabel, sizeof(manifestLabel));
    layout.loadSceneRect = build_adaptive_button_rect(font, LOAD_SCENE_BUTTON_X,
                                                      (layout.showPathToggles ? layout.pathBsdfRect.y + layout.pathBsdfRect.h
                                                                              : layout.integratorRect.y + layout.integratorRect.h) + LOAD_SCENE_BUTTON_SPACING,
                                                      LOAD_SCENE_BUTTON_WIDTH, LOAD_SCENE_BUTTON_HEIGHT,
                                                      manifestLabel, maxLeftWidth);

    {
        int root_buttons_w = ROOT_CTRL_BUTTON_W * 3 + 8;
        int root_value_w = maxLeftWidth - root_buttons_w;
        int root_y = layout.loadSceneRect.y + layout.loadSceneRect.h + ROOT_ROW_SPACING;
        if (root_value_w < 140) {
            root_value_w = 140;
        }
        layout.inputRootValueRect = (SDL_Rect){leftX, root_y, root_value_w, ROOT_ROW_HEIGHT};
        layout.inputRootEditRect = (SDL_Rect){leftX + root_value_w + 4, root_y, ROOT_CTRL_BUTTON_W, ROOT_ROW_HEIGHT};
        layout.inputRootFolderRect = (SDL_Rect){layout.inputRootEditRect.x + ROOT_CTRL_BUTTON_W + 2,
                                                root_y, ROOT_CTRL_BUTTON_W, ROOT_ROW_HEIGHT};
        layout.inputRootApplyRect = (SDL_Rect){layout.inputRootFolderRect.x + ROOT_CTRL_BUTTON_W + 2,
                                               root_y, ROOT_CTRL_BUTTON_W, ROOT_ROW_HEIGHT};

        root_y += ROOT_ROW_HEIGHT + ROOT_ROW_SPACING;
        layout.outputRootValueRect = (SDL_Rect){leftX, root_y, root_value_w, ROOT_ROW_HEIGHT};
        layout.outputRootEditRect = (SDL_Rect){leftX + root_value_w + 4, root_y, ROOT_CTRL_BUTTON_W, ROOT_ROW_HEIGHT};
        layout.outputRootFolderRect = (SDL_Rect){layout.outputRootEditRect.x + ROOT_CTRL_BUTTON_W + 2,
                                                 root_y, ROOT_CTRL_BUTTON_W, ROOT_ROW_HEIGHT};
        layout.outputRootApplyRect = (SDL_Rect){layout.outputRootFolderRect.x + ROOT_CTRL_BUTTON_W + 2,
                                                root_y, ROOT_CTRL_BUTTON_W, ROOT_ROW_HEIGHT};
    }

    leftColumnRight = max_int(layout.interactiveRect.x + layout.interactiveRect.w,
                              layout.deepRenderRect.x + layout.deepRenderRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.bounceRect.x + layout.bounceRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.autoMp4Rect.x + layout.autoMp4Rect.w);
    leftColumnRight = max_int(leftColumnRight, layout.integratorRect.x + layout.integratorRect.w);
    if (layout.showPathToggles) {
        leftColumnRight = max_int(leftColumnRight, layout.pathRouletteRect.x + layout.pathRouletteRect.w);
        leftColumnRight = max_int(leftColumnRight, layout.pathBsdfRect.x + layout.pathBsdfRect.w);
    }
    leftColumnRight = max_int(leftColumnRight, layout.loadSceneRect.x + layout.loadSceneRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.inputRootApplyRect.x + layout.inputRootApplyRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.outputRootApplyRect.x + layout.outputRootApplyRect.w);

    centerX = leftColumnRight + 24;
    centerMaxWidth = SLIDER_MARGIN_X - centerX - 16;
    if (centerMaxWidth < 120) centerMaxWidth = 120;
    layout.falloffRect = build_adaptive_button_rect(font, centerX, TOGGLE_BUTTON_MARGIN_Y + 10,
                                                    FORWARD_FALLOFF_BUTTON_WIDTH, FORWARD_FALLOFF_BUTTON_HEIGHT,
                                                    "Quadratic (1/r^2)", centerMaxWidth);
    layout.tileRect = build_adaptive_button_rect(font, centerX,
                                                 layout.falloffRect.y + layout.falloffRect.h + FORWARD_FALLOFF_BUTTON_SPACING,
                                                 TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                                 animSettings.useTiledRenderer ? "Tile Renderer: ON" : "Tile Renderer: OFF",
                                                 centerMaxWidth);
    layout.tilePreviewRect = build_adaptive_button_rect(font, centerX,
                                                        layout.tileRect.y + layout.tileRect.h + FORWARD_FALLOFF_BUTTON_SPACING,
                                                        TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                                        animSettings.tilePreviewEnabled ? "Tile Preview: ON" : "Tile Preview: OFF",
                                                        centerMaxWidth);
    layout.lightHeightRect = build_adaptive_button_rect(font, centerX,
                                                        layout.tilePreviewRect.y + layout.tilePreviewRect.h + FORWARD_FALLOFF_BUTTON_SPACING,
                                                        TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                                        "Light Height", centerMaxWidth);

    layout.startRect = build_adaptive_button_rect_right(font, rightEdge, BOTTOM_BUTTON_MARGIN_Y_START,
                                                        BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                        "Start", 0);
    layout.sceneEditorRect = build_adaptive_button_rect_right(font, rightEdge,
                                                              layout.startRect.y - (BOTTOM_BUTTON_HEIGHT_START + 8),
                                                              BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                              "Scene Editor", 0);
    int clampedEditorMode = EditorModeRouter_ClampEditorMode(animSettings.editorMode,
                                                             AnimationUseFluidScene());
    if (clampedEditorMode != animSettings.editorMode) {
        animSettings.editorMode = clampedEditorMode;
    }
    const char* editorModeLabel = (clampedEditorMode == 0) ? "Editor: Path" :
                                  (clampedEditorMode == 1) ? "Editor: Scene" : "Editor: Camera";
    layout.sceneModeRect = build_adaptive_button_rect_right(font, rightEdge,
                                                            layout.sceneEditorRect.y - (BOTTOM_BUTTON_HEIGHT_START + 6),
                                                            BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                            editorModeLabel, 0);
    layout.spaceModeRect = build_adaptive_button_rect_right(font, rightEdge,
                                                            layout.sceneModeRect.y - (BOTTOM_BUTTON_HEIGHT_START + 6),
                                                            BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                            menu_space_mode_button_label(), 0);
    layout.exitRect = build_adaptive_button_rect(font, BOTTOM_BUTTON_MARGIN_X_EXIT, BOTTOM_BUTTON_MARGIN_Y_EXIT,
                                                 BOTTOM_BUTTON_WIDTH_EXIT, BOTTOM_BUTTON_HEIGHT_EXIT,
                                                 "Exit w/o Saving", 280);
    layout.previewRect = build_adaptive_button_rect(font, BOTTOM_BUTTON_MARGIN_X_PREVIEW, BOTTOM_BUTTON_MARGIN_Y_PREVIEW,
                                                    BOTTOM_BUTTON_WIDTH_PREVIEW, BOTTOM_BUTTON_HEIGHT_PREVIEW,
                                                    "Preview", 240);
    layout.restoreRect = build_adaptive_button_rect(font, layout.exitRect.x + layout.exitRect.w + 10,
                                                    BOTTOM_BUTTON_MARGIN_Y_RESTORE,
                                                    BOTTOM_BUTTON_WIDTH_RESTORE, BOTTOM_BUTTON_HEIGHT_RESTORE,
                                                    "Restore Defaults", 260);
    layout.saveRect = build_adaptive_button_rect(font, layout.restoreRect.x + layout.restoreRect.w + 10,
                                                 BOTTOM_BUTTON_MARGIN_Y_SAVE,
                                                 BOTTOM_BUTTON_WIDTH_SAVE, BOTTOM_BUTTON_HEIGHT_SAVE,
                                                 "Save", 180);
    rightLimit = layout.startRect.x - 14;
    if (layout.saveRect.x + layout.saveRect.w > rightLimit) {
        int overflow = (layout.saveRect.x + layout.saveRect.w) - rightLimit;
        layout.saveRect.x -= overflow;
    }
    if (layout.saveRect.x < layout.restoreRect.x + layout.restoreRect.w + BOTTOM_BUTTON_SPACING) {
        layout.saveRect.x = layout.restoreRect.x + layout.restoreRect.w + BOTTOM_BUTTON_SPACING;
        if (layout.saveRect.x + layout.saveRect.w > rightLimit) {
            layout.saveRect.w = max_int(70, rightLimit - layout.saveRect.x);
        }
    }

    if (out_layout) {
        *out_layout = layout;
    }
}

void menu_render_frame(SDL_Renderer* renderer, TTF_Font* font, MenuRuntimeState* state) {
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    MenuButtonLayout buttons;
    SliderLayout sliderLayout;
    if (!state) return;

    menu_render_build_button_layout(font, state, &buttons);
    menu_render_build_slider_layout(font, state, &buttons, &sliderLayout);

    if (has_shared_palette) {
        render_set_clear_color(renderer,
                               palette.background_fill.r, palette.background_fill.g,
                               palette.background_fill.b, palette.background_fill.a);
    } else {
        render_set_clear_color(renderer, 0, 0, 0, 255);
    }
    if (!render_begin_frame()) {
        return;
    }

    render_button_rect(renderer, font, &buttons.interactiveRect, "Interactive Mode", animSettings.interactiveMode);
    render_button_rect(renderer, font, &buttons.deepRenderRect, "Deep Render", animSettings.deepRenderMode);
    render_button_rect(renderer, font, &buttons.bounceRect, "Bounce Mode", animSettings.bounceMode);
    render_button_rect(renderer, font, &buttons.autoMp4Rect, "Auto MP4", animSettings.autoMP4);

    const char* integratorLabel = "Integrator: Forward Light";
    if (animSettings.integratorMode == 1) integratorLabel = "Integrator: Hybrid";
    else if (animSettings.integratorMode == 2) integratorLabel = "Integrator: Direct Light";
    render_button_rect(renderer, font, &buttons.integratorRect, integratorLabel, true);

    if (buttons.showPathToggles) {
        render_button_rect(renderer, font, &buttons.pathRouletteRect,
                           animSettings.pathRussianRoulette ? "Roulette: ON" : "Roulette: OFF",
                           animSettings.pathRussianRoulette);
        const char* bsdfLabel = (animSettings.bsdfModel == 0) ? "BSDF: Lambert" : "BSDF: GGX";
        render_button_rect(renderer, font, &buttons.pathBsdfRect, bsdfLabel, animSettings.bsdfModel != 0);
    }

    char manifestLabel[160];
    format_manifest_button_label(state, manifestLabel, sizeof(manifestLabel));
    render_button_rect(renderer, font, &buttons.loadSceneRect, manifestLabel, state->manifestLoadEnabled);
    if (state->manifestLoadEnabled) {
        render_manifest_dropdown(renderer, font, state, &buttons.loadSceneRect);
    } else {
        state->manifestPanelRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestListRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestScrollbarRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestScrollbarVisible = false;
        state->manifestThumbHeight = 0.0f;
        state->manifestTrackHeight = 0.0f;
    }
    render_root_row(renderer, font, &buttons.inputRootValueRect,
                    "Input Root",
                    state->editingInputRoot ? state->pathInputBuffer : animSettings.inputRoot,
                    state->editingInputRoot);
    render_button_rect(renderer, font, &buttons.inputRootEditRect, "Edit", state->editingInputRoot);
    render_button_rect(renderer, font, &buttons.inputRootFolderRect, "Folder", false);
    render_button_rect(renderer, font, &buttons.inputRootApplyRect, "Apply", false);
    render_root_row(renderer, font, &buttons.outputRootValueRect,
                    "Output Root",
                    state->editingOutputRoot ? state->pathInputBuffer : animSettings.outputRoot,
                    state->editingOutputRoot);
    render_button_rect(renderer, font, &buttons.outputRootEditRect, "Edit", state->editingOutputRoot);
    render_button_rect(renderer, font, &buttons.outputRootFolderRect, "Folder", false);
    render_button_rect(renderer, font, &buttons.outputRootApplyRect, "Apply", false);

    const char* falloffLabel = "Quadratic (1/r^2)";
    if (animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_LINEAR) {
        falloffLabel = "Linear (1/r)";
    } else if (animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_NONE) {
        falloffLabel = "Falloff: None";
    }
    render_button_rect(renderer, font, &buttons.falloffRect, falloffLabel,
                       animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_LINEAR);

    const char* tileButtonLabel = animSettings.useTiledRenderer ? "Tile Renderer: ON" : "Tile Renderer: OFF";
    render_button_rect(renderer, font, &buttons.tileRect, tileButtonLabel, animSettings.useTiledRenderer);

    const char* previewLabel = animSettings.tilePreviewEnabled ? "Tile Preview: ON" : "Tile Preview: OFF";
    render_button_rect(renderer, font, &buttons.tilePreviewRect, previewLabel, animSettings.tilePreviewEnabled);

    if (buttons.showLightHeight) {
        char heightLabel[64];
        snprintf(heightLabel, sizeof(heightLabel), "Light Height: %.1f", animSettings.lightHeight);
        render_button_rect(renderer, font, &buttons.lightHeightRect, heightLabel, true);
    }

    menu_render_draw_sliders(renderer, font, state, &sliderLayout);

    render_button_rect(renderer, font, &buttons.sceneEditorRect, "Scene Editor", false);
    int currentEditorMode = EditorModeRouter_ClampEditorMode(animSettings.editorMode,
                                                             AnimationUseFluidScene());
    if (currentEditorMode != animSettings.editorMode) {
        animSettings.editorMode = currentEditorMode;
    }
    const char* editorModeText = (currentEditorMode == 0) ? "Editor: Path" :
                                 (currentEditorMode == 1) ? "Editor: Scene" : "Editor: Camera";
    render_button_rect(renderer, font, &buttons.sceneModeRect, editorModeText, false);
    render_button_rect(renderer, font, &buttons.spaceModeRect,
                       menu_space_mode_button_label(),
                       animSettings.spaceMode == SPACE_MODE_3D);
    if (EditorModeRouter_IsControlled3D()) {
        SDL_Color scaffoldHintColor = {255, 220, 140, 240};
        int hintY = buttons.spaceModeRect.y - 18;
        if (hintY < MENU_MARGIN_Y) {
            hintY = buttons.spaceModeRect.y + buttons.spaceModeRect.h + 4;
        }
        render_text_color(renderer,
                          font,
                          buttons.spaceModeRect.x,
                          hintY,
                          scaffoldHintColor,
                          EditorModeRouter_RuntimeHintLabel());
    }

    render_button_rect(renderer, font, &buttons.saveRect, "Save", false);
    render_button_rect(renderer, font, &buttons.restoreRect, "Restore Defaults", false);
    render_button_rect(renderer, font, &buttons.previewRect, "Preview", animSettings.previewMode);
    render_button_rect(renderer, font, &buttons.exitRect, "Exit w/o Saving", false);

    if (has_shared_palette) {
        SDL_Color startFill = ensure_highlight_fill_contrast(palette.accent_primary,
                                                             palette.button_text,
                                                             palette.panel_fill);
        SDL_SetRenderDrawColor(renderer,
                               startFill.r, startFill.g,
                               startFill.b, startFill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 90, 220, 110, 255);
    }
    SDL_RenderFillRect(renderer, &buttons.startRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    }
    SDL_RenderDrawRect(renderer, &buttons.startRect);
    if (has_shared_palette) {
        SDL_Color startFill = ensure_highlight_fill_contrast(palette.accent_primary,
                                                             palette.button_text,
                                                             palette.panel_fill);
        SDL_Color startText = choose_readable_text(startFill, palette.button_text);
        render_centered_text_color(renderer, font, &buttons.startRect, startText, "Start");
    } else {
        RenderButtonText(renderer, buttons.startRect, "Start");
    }

    Uint32 now = SDL_GetTicks();
    if (state->statusExpireMs > now) {
        double remaining = (double)(state->statusExpireMs - now);
        double frac = remaining / 1500.0;
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;
        SDL_Color c = state->statusColor;
        int alpha = (int)lrint((double)c.a * frac);
        if (alpha < 5) {
            state->statusExpireMs = 0;
        } else {
            c.a = (Uint8)alpha;
            int textX = buttons.saveRect.x + buttons.saveRect.w + 15;
            int textY = buttons.saveRect.y + (buttons.saveRect.h / 2) - 10;
            render_text_color(renderer, font, textX, textY, c, state->statusLabel);
        }
    }

    render_end_frame();
}
