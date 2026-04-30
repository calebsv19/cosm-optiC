#include "ui/sdl_menu_render.h"
#include "ui/menu/sdl_menu_render_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "editor/editor_mode_router.h"
#include "engine/Render/render_font.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/render_helper.h"
#include "render/ray_tracing_mode_backend.h"
#include "render/text_draw.h"
#include "render/text_upload_policy.h"
#include "engine/Render/render_pipeline.h"
#include "ui/menu_batch_panel.h"
#include "ui/menu_panel_chrome.h"
#include "ui/shared_theme_font_adapter.h"

#define MENU_WIDTH 1200
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

static SDL_Renderer* menu_context_renderer(void) {
    RenderContext* ctx = getRenderContext();
    if (!ctx) return NULL;
    return ctx->renderer;
}

static int menu_logical_pixels(int raster_pixels) {
    return ray_tracing_text_logical_pixels(menu_context_renderer(), raster_pixels);
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
    (void)ray_tracing_text_draw_utf8_at(renderer, font, buffer, x, y, textColor);
}

void menu_render_draw_text_color(SDL_Renderer *renderer, TTF_Font *font, int x, int y, SDL_Color color, const char *text) {
    if (!text || !*text) return;
    (void)ray_tracing_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static void render_centered_text_color(SDL_Renderer *renderer,
                                       TTF_Font *font,
                                       const SDL_Rect *rect,
                                       SDL_Color color,
                                       const char *text) {
    SDL_Rect textRect;
    int text_w = 0;
    int text_h = 0;
    if (!font || !rect || !text || !text[0]) return;
    if (!ray_tracing_text_measure_utf8(renderer, font, text, &text_w, &text_h)) return;
    {
        textRect = (SDL_Rect){
            rect->x + (rect->w - text_w) / 2,
            rect->y + (rect->h - text_h) / 2,
            text_w,
            text_h
        };
    }
    (void)ray_tracing_text_draw_utf8(renderer, font, text, color, &textRect);
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

SDL_Color menu_render_ensure_highlight_fill_contrast(SDL_Color fill,
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

SDL_Color menu_render_choose_readable_text(SDL_Color background, SDL_Color preferred_text) {
    if (color_contrast_gap(background, preferred_text) >= 110) {
        return preferred_text;
    }
    if (color_luma(background) >= 150) {
        return (SDL_Color){24, 28, 34, preferred_text.a ? preferred_text.a : 255};
    }
    return (SDL_Color){245, 247, 250, preferred_text.a ? preferred_text.a : 255};
}

void menu_render_fit_text_to_width(TTF_Font *font,
                                   const char *text,
                                   int max_width,
                                   char *out,
                                   size_t out_size);

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
        textW = menu_logical_pixels(textW);
        textH = menu_logical_pixels(textH);
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

void menu_render_draw_button_rect(SDL_Renderer *renderer, TTF_Font *font, const SDL_Rect* rect, const char *text, bool active) {
    if (!rect) return;
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    SDL_Color fill = has_shared_palette
                         ? (active ? palette.button_active_fill : palette.button_fill)
                         : (active ? (SDL_Color){0, 255, 0, 255} : (SDL_Color){100, 100, 100, 255});
    SDL_Color textColor = has_shared_palette ? palette.button_text : (SDL_Color){255, 255, 255, 255};
    if (has_shared_palette && active) {
        fill = menu_render_ensure_highlight_fill_contrast(fill, textColor, palette.panel_fill);
    }
    if (has_shared_palette) {
        textColor = menu_render_choose_readable_text(fill, textColor);
    }

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, rect);

    render_centered_text_color(renderer, font, rect, textColor, text);
}

void menu_render_fit_text_to_width(TTF_Font *font,
                                   const char *text,
                                   int max_width,
                                   char *out,
                                   size_t out_size) {
    char source[PATH_MAX];
    int w = 0;
    size_t len = 0;
    if (!out || out_size == 0) return;
    if (!text) {
        out[0] = '\0';
        return;
    }
    snprintf(source, sizeof(source), "%s", text);
    source[sizeof(source) - 1] = '\0';
    snprintf(out, out_size, "%s", source);
    if (!font || max_width <= 0) return;
    if (TTF_SizeUTF8(font, out, &w, NULL) == 0) {
        w = menu_logical_pixels(w);
        if (w <= max_width) return;
    }
    len = strlen(out);
    while (len > 0) {
        --len;
        out[len] = '\0';
        {
            char candidate[PATH_MAX];
            snprintf(candidate, sizeof(candidate), "%s...", out);
            if (TTF_SizeUTF8(font, candidate, &w, NULL) == 0) {
                w = menu_logical_pixels(w);
                if (w <= max_width) {
                    snprintf(out, out_size, "%s", candidate);
                    return;
                }
            }
        }
    }
    snprintf(out, out_size, "...");
}

void menu_render_draw_root_row(SDL_Renderer *renderer,
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
                             ? menu_render_ensure_highlight_fill_contrast(palette.button_active_fill,
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
    menu_render_fit_text_to_width(font, line, value_rect->w - 12, line, sizeof(line));
    SDL_Rect prev_clip = {0, 0, 0, 0};
    SDL_bool clip_was_enabled = SDL_RenderIsClipEnabled(renderer);
    if (clip_was_enabled) {
        SDL_RenderGetClipRect(renderer, &prev_clip);
    }
    SDL_RenderSetClipRect(renderer, value_rect);
    if (has_shared_palette) {
        SDL_Color text_color =
            menu_render_choose_readable_text(editing ? palette.button_active_fill : palette.button_fill,
                                             palette.button_text);
        menu_render_draw_text_color(renderer, font, value_rect->x + 6, value_rect->y + 8, text_color, line);
    } else {
        menu_render_draw_text_color(renderer, font, value_rect->x + 6, value_rect->y + 8, (SDL_Color){235, 235, 235, 255}, line);
    }
    if (clip_was_enabled) {
        SDL_RenderSetClipRect(renderer, &prev_clip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }
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
    menu_render_draw_button_rect(renderer, font, &rect, text, active);
}

void menu_render_build_button_layout(TTF_Font* font,
                                     MenuRuntimeState* state,
                                     const MenuScreenLayout* screen_layout,
                                     MenuButtonLayout* out_layout) {
    MenuButtonLayout layout;
    char manifestLabel[160];
    const RayTracingIntegratorMenuState integrator_menu =
        RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    const char* integratorLabel = integrator_menu.buttonLabel;
    int leftX = TOGGLE_BUTTON_MARGIN_X;
    int subX = SUBSETTING_BUTTON_MARGIN_X;
    int maxLeftWidth = 320;
    int leftColumnRight;
    int centerX;
    int centerMaxWidth;
    int centerLeftX;
    int centerRightX;
    int centerColumnMaxWidth;
    int rightEdge = MENU_WIDTH - MENU_MARGIN_X;
    int footerRightLimit = MENU_WIDTH - MENU_MARGIN_X;
    int leftTopY = TOGGLE_BUTTON_MARGIN_Y;
    int routeTopY = 0;
    int footerButtonY = BOTTOM_BUTTON_MARGIN_Y_EXIT;

    memset(&layout, 0, sizeof(layout));
    layout.showPathToggles = integrator_menu.showPathToggles;
    layout.showLightHeight = false;
    if (screen_layout) {
        leftX = screen_layout->leftPanelRect.x + 18;
        subX = leftX + 10;
        maxLeftWidth = screen_layout->leftPanelRect.w - 36;
        rightEdge = screen_layout->routeStackRect.x + screen_layout->routeStackRect.w - 10;
        leftTopY = screen_layout->leftPanelRect.y + MENU_PANEL_CHROME_TITLE_BAND + 12;
        routeTopY = screen_layout->routeStackRect.y + 10;
        footerButtonY = screen_layout->bottomActionRowRect.y +
                        (screen_layout->bottomActionRowRect.h - BOTTOM_BUTTON_HEIGHT_EXIT) / 2;
        footerRightLimit = screen_layout->bottomActionRowRect.x + screen_layout->bottomActionRowRect.w - 14;
    }
    if (maxLeftWidth < 120) maxLeftWidth = 120;

    layout.interactiveRect = build_adaptive_button_rect(font, leftX, leftTopY,
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

    menu_render_format_manifest_button_label(state, manifestLabel, sizeof(manifestLabel));
    layout.loadSceneRect = build_adaptive_button_rect(font, leftX,
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
    centerX = leftColumnRight + 24;
    centerMaxWidth =
        (screen_layout ? (screen_layout->centerControlsRect.x + screen_layout->centerControlsRect.w)
                       : SLIDER_MARGIN_X) -
        centerX - 16;
    if (centerMaxWidth < 120) centerMaxWidth = 120;
    if (screen_layout && centerX < screen_layout->centerControlsRect.x + 12) {
        centerX = screen_layout->centerControlsRect.x + 12;
    }
    centerLeftX = centerX;
    centerRightX = centerX + centerMaxWidth / 2;
    centerColumnMaxWidth = centerMaxWidth;
    if (screen_layout) {
        int content_left = screen_layout->centerControlsRect.x + 12;
        int content_right = screen_layout->centerControlsRect.x +
                            screen_layout->centerControlsRect.w - 12;
        int content_width = content_right - content_left;
        int column_gap = 14;
        int column_width = (content_width - column_gap) / 2;
        if (column_width < 120) {
            column_width = 120;
        }
        centerLeftX = content_left;
        centerRightX = centerLeftX + column_width + column_gap;
        centerColumnMaxWidth = column_width;
    } else {
        int column_gap = 14;
        centerColumnMaxWidth = (centerMaxWidth - column_gap) / 2;
        if (centerColumnMaxWidth < 120) {
            centerColumnMaxWidth = 120;
        }
        centerRightX = centerLeftX + centerColumnMaxWidth + column_gap;
    }
    layout.falloffRect = build_adaptive_button_rect(font, centerLeftX,
                                                    screen_layout ? (screen_layout->centerControlsRect.y + MENU_PANEL_CHROME_TITLE_BAND + 12) : (TOGGLE_BUTTON_MARGIN_Y + 10),
                                                    FORWARD_FALLOFF_BUTTON_WIDTH, FORWARD_FALLOFF_BUTTON_HEIGHT,
                                                    "Quadratic (1/r^2)", centerColumnMaxWidth);
    layout.tileRect = build_adaptive_button_rect(font, centerLeftX,
                                                 layout.falloffRect.y + layout.falloffRect.h + FORWARD_FALLOFF_BUTTON_SPACING,
                                                 TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                                 animSettings.useTiledRenderer ? "Tile Renderer: ON" : "Tile Renderer: OFF",
                                                 centerColumnMaxWidth);
    layout.tilePreviewRect = build_adaptive_button_rect(font, centerLeftX,
                                                        layout.tileRect.y + layout.tileRect.h + FORWARD_FALLOFF_BUTTON_SPACING,
                                                        TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                                        animSettings.tilePreviewEnabled ? "Tile Preview: ON" : "Tile Preview: OFF",
                                                        centerColumnMaxWidth);
    layout.denoiseRect = build_adaptive_button_rect(font, centerRightX,
                                                    layout.falloffRect.y,
                                                    TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                                    animSettings.disneyDenoiseEnabled ? "Disney Denoise: ON"
                                                                                     : "Disney Denoise: OFF",
                                                    centerColumnMaxWidth);
    layout.topFillRect = build_adaptive_button_rect(font, centerRightX,
                                                    layout.denoiseRect.y + layout.denoiseRect.h + FORWARD_FALLOFF_BUTTON_SPACING,
                                                    TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                                    animSettings.topFillLightEnabled ? "Top Fill: ON" : "Top Fill: OFF",
                                                    centerColumnMaxWidth);
    layout.lightHeightRect = build_adaptive_button_rect(font, centerRightX,
                                                        layout.topFillRect.y + layout.topFillRect.h + FORWARD_FALLOFF_BUTTON_SPACING,
                                                        TILE_BUTTON_WIDTH, TILE_BUTTON_HEIGHT,
                                                        "Light Height", centerColumnMaxWidth);

    layout.startRect = build_adaptive_button_rect_right(font, rightEdge,
                                                        screen_layout ? (routeTopY + (BOTTOM_BUTTON_HEIGHT_START + 8) * 3) : BOTTOM_BUTTON_MARGIN_Y_START,
                                                        BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                        "Start", 0);
    layout.previewRect = build_adaptive_button_rect_right(font, rightEdge,
                                                          screen_layout ? (routeTopY + (BOTTOM_BUTTON_HEIGHT_START + 8) * 4) : BOTTOM_BUTTON_MARGIN_Y_PREVIEW,
                                                          BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                          "Preview", 0);
    layout.sceneEditorRect = build_adaptive_button_rect_right(font, rightEdge,
                                                              screen_layout ? (routeTopY + (BOTTOM_BUTTON_HEIGHT_START + 8) * 2) : (layout.startRect.y - (BOTTOM_BUTTON_HEIGHT_START + 8)),
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
                                                            screen_layout ? (routeTopY + (BOTTOM_BUTTON_HEIGHT_START + 6)) : (layout.sceneEditorRect.y - (BOTTOM_BUTTON_HEIGHT_START + 6)),
                                                            BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                            editorModeLabel, 0);
    layout.spaceModeRect = build_adaptive_button_rect_right(font, rightEdge,
                                                            screen_layout ? routeTopY : (layout.sceneModeRect.y - (BOTTOM_BUTTON_HEIGHT_START + 6)),
                                                            BOTTOM_BUTTON_WIDTH_START, BOTTOM_BUTTON_HEIGHT_START,
                                                            menu_space_mode_button_label(), 0);
    if (screen_layout) {
        const int stackGap = 12;
        const int stackWidth = screen_layout->routeStackRect.w - 20;
        int leftControlWidth = 190;
        int rightStackWidth = 0;
        int leftControlX = screen_layout->routeStackRect.x + 10;
        int rightStackX = 0;

        if (leftControlWidth > stackWidth - stackGap - 120) {
            leftControlWidth = max_int(120, (stackWidth - stackGap) / 2);
        }
        rightStackWidth = stackWidth - leftControlWidth - stackGap;
        if (rightStackWidth < 120) {
            rightStackWidth = 120;
            leftControlWidth = stackWidth - stackGap - rightStackWidth;
        }
        rightStackX = leftControlX + leftControlWidth + stackGap;

        layout.resumeFramesRect = build_adaptive_button_rect(font,
                                                             leftControlX,
                                                             layout.sceneEditorRect.y,
                                                             leftControlWidth,
                                                             BOTTOM_BUTTON_HEIGHT_START,
                                                             "Resume Existing: OFF",
                                                             leftControlWidth);
        layout.startFrameRect = build_adaptive_button_rect(font,
                                                           leftControlX,
                                                           layout.startRect.y,
                                                           leftControlWidth,
                                                           BOTTOM_BUTTON_HEIGHT_START,
                                                           "Start Frame: 0",
                                                           leftControlWidth);
        layout.nextFrameRect = build_adaptive_button_rect(font,
                                                          leftControlX,
                                                          layout.previewRect.y,
                                                          leftControlWidth,
                                                          BOTTOM_BUTTON_HEIGHT_START,
                                                          "Next Existing: 0",
                                                          leftControlWidth);

        layout.spaceModeRect.x = rightStackX;
        layout.spaceModeRect.w = rightStackWidth;
        layout.sceneModeRect.x = rightStackX;
        layout.sceneModeRect.w = rightStackWidth;
        layout.sceneEditorRect.x = rightStackX;
        layout.sceneEditorRect.w = rightStackWidth;
        layout.startRect.x = rightStackX;
        layout.startRect.w = rightStackWidth;
        layout.previewRect.x = rightStackX;
        layout.previewRect.w = rightStackWidth;
    }
    layout.exitRect = build_adaptive_button_rect(font,
                                                 screen_layout ? (screen_layout->bottomActionRowRect.x + 14) : BOTTOM_BUTTON_MARGIN_X_EXIT,
                                                 footerButtonY,
                                                 BOTTOM_BUTTON_WIDTH_EXIT, BOTTOM_BUTTON_HEIGHT_EXIT,
                                                 "Exit w/o Saving", 280);
    layout.restoreRect = build_adaptive_button_rect(font, layout.exitRect.x + layout.exitRect.w + 10,
                                                    footerButtonY,
                                                    BOTTOM_BUTTON_WIDTH_RESTORE, BOTTOM_BUTTON_HEIGHT_RESTORE,
                                                    "Restore Defaults", 260);
    layout.saveRect = build_adaptive_button_rect(font, layout.restoreRect.x + layout.restoreRect.w + 10,
                                                 footerButtonY,
                                                 BOTTOM_BUTTON_WIDTH_SAVE, BOTTOM_BUTTON_HEIGHT_SAVE,
                                                 "Save", 180);
    if (layout.saveRect.x + layout.saveRect.w > footerRightLimit) {
        int overflow = (layout.saveRect.x + layout.saveRect.w) - footerRightLimit;
        layout.saveRect.x -= overflow;
    }
    if (layout.saveRect.x < layout.restoreRect.x + layout.restoreRect.w + BOTTOM_BUTTON_SPACING) {
        layout.saveRect.x = layout.restoreRect.x + layout.restoreRect.w + BOTTOM_BUTTON_SPACING;
        if (layout.saveRect.x + layout.saveRect.w > footerRightLimit) {
            layout.saveRect.w = max_int(70, footerRightLimit - layout.saveRect.x);
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
    MenuScreenLayout screenLayout;
    SliderLayout sliderLayout;
    MenuBatchPanelLayout batchPanel;
    RayTracingRuntimeRoute route;
    RayTracingSceneDigestStatus digestStatus;
    if (!state) return;

    menu_layout_build_base(font, state, &screenLayout);
    menu_render_build_button_layout(font, state, &screenLayout, &buttons);
    menu_layout_finalize_with_buttons(&screenLayout, &buttons, state);
    menu_render_build_slider_layout(font, state, &screenLayout, &sliderLayout);
    menu_batch_panel_build_layout(font, state, &screenLayout, &batchPanel);
    route = RayTracingModeBackend_ResolveRoute();
    digestStatus = RayTracingModeBackend_BuildSceneDigestStatus(&route);

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

    menu_panel_chrome_draw(renderer, font, &screenLayout.leftPanelRect, "Scene + Mode", true);
    menu_panel_chrome_draw(renderer, font, &screenLayout.centerControlsRect, "Renderer Controls", false);
    menu_panel_chrome_draw(renderer, font, &screenLayout.routeStackRect, NULL, false);
    menu_panel_chrome_draw(renderer, font, &screenLayout.bottomActionRowRect, NULL, false);

    menu_render_draw_button_rect(renderer, font, &buttons.interactiveRect, "Interactive Mode", animSettings.interactiveMode);
    menu_render_draw_button_rect(renderer, font, &buttons.deepRenderRect, "Deep Render", animSettings.deepRenderMode);
    menu_render_draw_button_rect(renderer, font, &buttons.bounceRect, "Bounce Mode", animSettings.bounceMode);
    menu_render_draw_button_rect(renderer, font, &buttons.autoMp4Rect, "Auto MP4", animSettings.autoMP4);

    const RayTracingIntegratorMenuState integrator_menu =
        RayTracingIntegratorCatalog_BuildMenuState(&animSettings);
    menu_render_draw_button_rect(renderer, font, &buttons.integratorRect, integrator_menu.buttonLabel, true);

    if (buttons.showPathToggles) {
        menu_render_draw_button_rect(renderer, font, &buttons.pathRouletteRect,
                                     animSettings.pathRussianRoulette ? "Roulette: ON" : "Roulette: OFF",
                                     animSettings.pathRussianRoulette);
        const char* bsdfLabel = (animSettings.bsdfModel == 0) ? "BSDF: Lambert" : "BSDF: GGX";
        menu_render_draw_button_rect(renderer, font, &buttons.pathBsdfRect, bsdfLabel, animSettings.bsdfModel != 0);
    }

    char manifestLabel[160];
    menu_render_format_manifest_button_label(state, manifestLabel, sizeof(manifestLabel));
    menu_render_draw_button_rect(renderer, font, &buttons.loadSceneRect, manifestLabel, state->manifestDropdownOpen);
    if (state->manifestDropdownOpen) {
        menu_render_draw_manifest_dropdown(renderer, font, state, &buttons, &screenLayout);
    } else {
        state->manifestPanelRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestListRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestScrollbarRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestScrollbarVisible = false;
        state->manifestThumbHeight = 0.0f;
        state->manifestTrackHeight = 0.0f;
    }
    menu_render_draw_root_row(renderer, font, &buttons.inputRootValueRect,
                              "Input Root",
                              state->editingInputRoot ? state->pathInputBuffer : animSettings.inputRoot,
                              state->editingInputRoot);
    menu_render_draw_button_rect(renderer, font, &buttons.inputRootEditRect, "Edit", state->editingInputRoot);
    menu_render_draw_button_rect(renderer, font, &buttons.inputRootFolderRect, "Folder", false);
    menu_render_draw_button_rect(renderer, font, &buttons.inputRootApplyRect, "Apply", false);
    const char* falloffLabel = "Quadratic (1/r^2)";
    if (animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_LINEAR) {
        falloffLabel = "Linear (1/r)";
    } else if (animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_NONE) {
        falloffLabel = "Falloff: None";
    }
    menu_render_draw_button_rect(renderer, font, &buttons.falloffRect, falloffLabel,
                                 animSettings.forwardFalloffMode == FORWARD_FALLOFF_MODE_LINEAR);

    const char* tileButtonLabel = animSettings.useTiledRenderer ? "Tile Renderer: ON" : "Tile Renderer: OFF";
    menu_render_draw_button_rect(renderer, font, &buttons.tileRect, tileButtonLabel, animSettings.useTiledRenderer);

    const char* previewLabel = animSettings.tilePreviewEnabled ? "Tile Preview: ON" : "Tile Preview: OFF";
    menu_render_draw_button_rect(renderer, font, &buttons.tilePreviewRect, previewLabel, animSettings.tilePreviewEnabled);

    const char* denoiseLabel = animSettings.disneyDenoiseEnabled ? "Disney Denoise: ON"
                                                                 : "Disney Denoise: OFF";
    menu_render_draw_button_rect(renderer, font, &buttons.denoiseRect, denoiseLabel, animSettings.disneyDenoiseEnabled);

    const char* topFillLabel = animSettings.topFillLightEnabled ? "Top Fill: ON" : "Top Fill: OFF";
    menu_render_draw_button_rect(renderer, font, &buttons.topFillRect, topFillLabel, animSettings.topFillLightEnabled);

    if (buttons.showLightHeight) {
        char heightLabel[64];
        snprintf(heightLabel, sizeof(heightLabel), "Light Height: %.1f", animSettings.lightHeight);
        menu_render_draw_button_rect(renderer, font, &buttons.lightHeightRect, heightLabel, true);
    }

    if (buttons.resumeFramesRect.w > 0 && buttons.resumeFramesRect.h > 0) {
        char resumeLabel[64];
        char startFrameLabel[64];
        char nextFrameLabel[64];
        snprintf(resumeLabel,
                 sizeof(resumeLabel),
                 "Resume Existing: %s",
                 animSettings.resumeFromExistingFrames ? "ON" : "OFF");
        if (state->editingStartFrame && state->inputBuffer[0] != '\0') {
            snprintf(startFrameLabel,
                     sizeof(startFrameLabel),
                     "Start Frame: %s",
                     state->inputBuffer);
        } else {
            snprintf(startFrameLabel,
                     sizeof(startFrameLabel),
                     "Start Frame: %d",
                     animSettings.startFrameIndex);
        }
        snprintf(nextFrameLabel,
                 sizeof(nextFrameLabel),
                 "Next Existing: %d",
                 state->exportBatchStatus.next_frame_index);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.resumeFramesRect,
                                     resumeLabel,
                                     animSettings.resumeFromExistingFrames);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.startFrameRect,
                                     startFrameLabel,
                                     state->editingStartFrame || !animSettings.resumeFromExistingFrames);
        menu_render_draw_button_rect(renderer,
                                     font,
                                     &buttons.nextFrameRect,
                                     nextFrameLabel,
                                     false);
    }

    menu_render_draw_sliders(renderer, font, state, &sliderLayout);
    menu_batch_panel_render(renderer, font, state, &batchPanel);

    menu_render_draw_button_rect(renderer, font, &buttons.sceneEditorRect, "Scene Editor", false);
    int currentEditorMode = EditorModeRouter_ClampEditorMode(animSettings.editorMode,
                                                             AnimationUseFluidScene());
    if (currentEditorMode != animSettings.editorMode) {
        animSettings.editorMode = currentEditorMode;
    }
    const char* editorModeText = (currentEditorMode == 0) ? "Editor: Path" :
                                 (currentEditorMode == 1) ? "Editor: Scene" : "Editor: Camera";
    menu_render_draw_button_rect(renderer, font, &buttons.sceneModeRect, editorModeText, false);
    menu_render_draw_button_rect(renderer, font, &buttons.spaceModeRect,
                                 menu_space_mode_button_label(),
                                 animSettings.spaceMode == SPACE_MODE_3D);
    if (EditorModeRouter_IsControlled3D()) {
        SDL_Color scaffoldHintColor = {255, 220, 140, 240};
        SDL_Color digestHintColor = has_shared_palette
                                        ? (SDL_Color){palette.text_muted.r,
                                                      palette.text_muted.g,
                                                      palette.text_muted.b,
                                                      230}
                                        : (SDL_Color){210, 210, 210, 230};
        int hintX = buttons.spaceModeRect.x;
        int hintMaxWidth = MENU_WIDTH - hintX - MENU_MARGIN_X;
        int hintLine1Y = 0;
        int hintLine2Y = 0;
        char hintLine1[192];
        char hintLine2[256];
        char hintFit[256];

        if (buttons.spaceModeRect.y - 36 >= MENU_MARGIN_Y) {
            hintLine1Y = buttons.spaceModeRect.y - 34;
            hintLine2Y = buttons.spaceModeRect.y - 18;
        } else {
            hintLine1Y = buttons.spaceModeRect.y + buttons.spaceModeRect.h + 4;
            hintLine2Y = hintLine1Y + 16;
        }

        snprintf(hintLine1,
                 sizeof(hintLine1),
                 "%s | %s",
                 EditorModeRouter_RuntimeHintLabel(),
                 RayTracingModeBackend_IntegratorStatusLabel(&route));
        menu_render_fit_text_to_width(font, hintLine1, hintMaxWidth, hintFit, sizeof(hintFit));
        menu_render_draw_text_color(renderer,
                                    font,
                                    hintX,
                                    hintLine1Y,
                                    scaffoldHintColor,
                                    hintFit);

        if (!digestStatus.valid) {
            snprintf(hintLine2, sizeof(hintLine2), "3D digest pending runtime payload");
        } else if (digestStatus.hasSceneBounds) {
            snprintf(hintLine2,
                     sizeof(hintLine2),
                     "3D digest prim=%d plane=%d prism=%d bx=%.1f..%.1f by=%.1f..%.1f",
                     digestStatus.digestPrimitiveCount,
                     digestStatus.planePrimitiveCount,
                     digestStatus.rectPrismPrimitiveCount,
                     digestStatus.boundsMinX,
                     digestStatus.boundsMaxX,
                     digestStatus.boundsMinY,
                     digestStatus.boundsMaxY);
        } else {
            snprintf(hintLine2,
                     sizeof(hintLine2),
                     "3D digest prim=%d plane=%d prism=%d",
                     digestStatus.digestPrimitiveCount,
                     digestStatus.planePrimitiveCount,
                     digestStatus.rectPrismPrimitiveCount);
        }
        menu_render_fit_text_to_width(font, hintLine2, hintMaxWidth, hintFit, sizeof(hintFit));
        menu_render_draw_text_color(renderer,
                                    font,
                                    hintX,
                                    hintLine2Y,
                                    digestHintColor,
                                    hintFit);
    }

    menu_render_draw_button_rect(renderer, font, &buttons.saveRect, "Save", false);
    menu_render_draw_button_rect(renderer, font, &buttons.restoreRect, "Restore Defaults", false);
    menu_render_draw_button_rect(renderer, font, &buttons.previewRect, "Preview", animSettings.previewMode);
    menu_render_draw_button_rect(renderer, font, &buttons.exitRect, "Exit w/o Saving", false);

    if (has_shared_palette) {
        SDL_Color startFill = menu_render_ensure_highlight_fill_contrast(palette.accent_primary,
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
        SDL_Color startFill = menu_render_ensure_highlight_fill_contrast(palette.accent_primary,
                                                                         palette.button_text,
                                                                         palette.panel_fill);
        SDL_Color startText = menu_render_choose_readable_text(startFill, palette.button_text);
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
            menu_render_draw_text_color(renderer, font, textX, textY, c, state->statusLabel);
        }
    }

    render_end_frame();
}
