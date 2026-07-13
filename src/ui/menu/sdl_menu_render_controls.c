#include "ui/sdl_menu_render.h"
#include "ui/menu/sdl_menu_render_internal.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "editor/editor_mode_router.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/text_draw.h"
#include "render/text_upload_policy.h"
#include "ui/menu_panel_chrome.h"
#include "ui/shared_theme_font_adapter.h"
#include "ui/volume_source_ui_labels.h"

#define MENU_WIDTH 1200
#define MENU_HEIGHT 900
#define MENU_MARGIN_X 30
#define MENU_MARGIN_Y 30
#define TOGGLE_BUTTON_WIDTH 200
#define TOGGLE_BUTTON_HEIGHT 50
#define COMPACT_TOGGLE_BUTTON_HEIGHT 34
#define TOGGLE_BUTTON_MARGIN_X MENU_MARGIN_X
#define TOGGLE_BUTTON_MARGIN_Y MENU_MARGIN_Y
#define TOGGLE_BUTTON_SPACING 10
#define SUBSETTING_BUTTON_WIDTH 175
#define SUBSETTING_BUTTON_HEIGHT 40
#define COMPACT_SUBSETTING_BUTTON_HEIGHT 30
#define SUBSETTING_BUTTON_MARGIN_X (MENU_MARGIN_X + 10)
#define BOTTOM_BUTTON_SPACING 10
#define BOTTOM_BUTTON_WIDTH_START 200
#define BOTTOM_BUTTON_HEIGHT_START 50
#define ROUTE_BUTTON_WIDTH 184
#define ROUTE_BUTTON_HEIGHT 38
#define ROUTE_BUTTON_GAP 6
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
#define RENDERER_CONTROL_TAB_HEIGHT 22
#define RENDERER_CONTROL_BUTTON_HEIGHT 32
#define RENDERER_CONTROL_ROW_GAP 8
#define RENDERER_CONTROL_COLUMN_GAP 10
#define RENDERER_CONTROL_SLIDER_HEIGHT 6
#define RENDERER_CONTROL_SLIDER_SPACING 5

static const char* menu_upscale_mode_button_label(void) {
    switch ((Runtime3DUpscaleMode)animSettings.upscaleMode3D) {
        case RUNTIME_3D_UPSCALE_MODE_NEAREST:
            return "Upscale: Nearest";
        case RUNTIME_3D_UPSCALE_MODE_BILINEAR:
            return "Upscale: Bilinear";
        case RUNTIME_3D_UPSCALE_MODE_OFF:
        default:
            return "Upscale: OFF";
    }
}

static const char* menu_environment_preset_label(void) {
    switch ((EnvironmentPreset)animSettings.environmentPreset) {
        case ENVIRONMENT_PRESET_NEUTRAL:
            return "Preset: Neutral";
        case ENVIRONMENT_PRESET_WARM_SKY:
            return "Preset: Warm";
        case ENVIRONMENT_PRESET_SKY:
        default:
            return "Preset: Sky";
    }
}

static SDL_Renderer* menu_context_renderer(void) {
    RenderContext* ctx = getRenderContext();
    if (!ctx) return NULL;
    return ctx->renderer;
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
    textRect = (SDL_Rect){
        rect->x + (rect->w - text_w) / 2,
        rect->y + (rect->h - text_h) / 2,
        text_w,
        text_h
    };
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

static int max_int(int a, int b) {
    return (a > b) ? a : b;
}

static int min_int(int a, int b) {
    return (a < b) ? a : b;
}

static int menu_renderer_controls_text_height(TTF_Font* font) {
    int textHeight = 18;
    if (font) {
        int measured_h = 0;
        if (ray_tracing_text_line_height(menu_context_renderer(), font, &measured_h)) {
            textHeight = measured_h;
        }
    }
    if (textHeight < 12) textHeight = 12;
    return textHeight;
}

static void menu_renderer_controls_add_slider(SliderLayout* layout,
                                              int* target,
                                              int min_value,
                                              int max_value,
                                              const char* label,
                                              int slider_x,
                                              int slider_width,
                                              int text_height) {
    int label_y;
    int track_y;
    if (!layout || !target || !label || layout->count >= SDL_MENU_MAX_SLIDERS) return;
    label_y = layout->nextY;
    track_y = label_y + text_height + 2;
    layout->items[layout->count++] = (MenuSlider){
        target,
        min_value,
        max_value,
        (SDL_Rect){slider_x, track_y, slider_width, layout->trackHeight},
        (SDL_Rect){slider_x, track_y - 6, slider_width, layout->trackHeight + 12},
        slider_x,
        label_y,
        slider_x + slider_width + 10,
        track_y - ((text_height - layout->trackHeight) / 2),
        label
    };
    layout->nextY = track_y + layout->trackHeight + RENDERER_CONTROL_SLIDER_SPACING;
}

static void menu_renderer_controls_build_slider_layout(TTF_Font* font,
                                                       MenuRuntimeState* state,
                                                       const MenuScreenLayout* screen_layout,
                                                       int start_y,
                                                       SliderLayout* out_layout) {
    SliderLayout layout = {0};
    int text_height;
    int content_left;
    int content_right;
    int value_reserve = 88;
    int slider_x;
    int slider_width;
    int panel_bottom;
    if (!state || !screen_layout || !out_layout) return;

    text_height = menu_renderer_controls_text_height(font);
    content_left = screen_layout->centerControlsRect.x + 12;
    content_right = screen_layout->centerControlsRect.x +
                    screen_layout->centerControlsRect.w - 12;
    slider_x = content_left;
    slider_width = content_right - content_left - value_reserve;
    if (slider_width < 120) slider_width = content_right - content_left;
    if (slider_width < 80) slider_width = 80;

    layout.trackHeight = max_int(RENDERER_CONTROL_SLIDER_HEIGHT, text_height / 3);
    layout.knobWidth = max_int(8, (text_height * 3) / 8);
    layout.knobHeight = layout.trackHeight + 4;
    layout.nextY = start_y;
    panel_bottom = screen_layout->centerControlsRect.y + screen_layout->centerControlsRect.h - 10;
    layout.panelRect = (SDL_Rect){
        content_left,
        start_y - 4,
        content_right - content_left,
        panel_bottom - (start_y - 4)
    };
    if (layout.panelRect.h < 0) layout.panelRect.h = 0;

    menu_state_sync_from_anim(state);
    if (state->rendererControlsTab == MENU_RENDERER_CONTROLS_CAUSTICS) {
        menu_renderer_controls_add_slider(&layout,
                                          &state->causticSettings.sampleBudget,
                                          0,
                                          100000,
                                          "Photon / Path Budget",
                                          slider_x,
                                          slider_width,
                                          text_height);
        menu_renderer_controls_add_slider(&layout,
                                          &state->causticSettings.maxPathDepth,
                                          0,
                                          16,
                                          "Caustic Path Depth",
                                          slider_x,
                                          slider_width,
                                          text_height);
    } else if (state->rendererControlsTab == MENU_RENDERER_CONTROLS_PERFORMANCE) {
        menu_renderer_controls_add_slider(&layout,
                                          &animSettings.tileSize,
                                          4,
                                          256,
                                          "Tile Size",
                                          slider_x,
                                          slider_width,
                                          text_height);
    } else {
        menu_renderer_controls_add_slider(&layout,
                                          &state->envSliderValue,
                                          0,
                                          255,
                                          "Ambient Brightness",
                                          slider_x,
                                          slider_width,
                                          text_height);
        menu_renderer_controls_add_slider(&layout,
                                          &state->topFillStrengthSliderValue,
                                          0,
                                          2000,
                                          "Top Fill Strength",
                                          slider_x,
                                          slider_width,
                                          text_height);
        menu_renderer_controls_add_slider(&layout,
                                          &state->environmentBackgroundBrightnessSliderValue,
                                          0,
                                          400,
                                          "BG Brightness",
                                          slider_x,
                                          slider_width,
                                          text_height);
        menu_renderer_controls_add_slider(&layout,
                                          &state->lightIntensitySliderValue,
                                          0,
                                          2000,
                                          "Light Intensity",
                                          slider_x,
                                          slider_width,
                                          text_height);
        menu_renderer_controls_add_slider(&layout,
                                          &state->lightDecaySoftnessSliderValue,
                                          10,
                                          1000,
                                          "Falloff Softness",
                                          slider_x,
                                          slider_width,
                                          text_height);
        menu_renderer_controls_add_slider(&layout,
                                          &state->forwardDecaySliderValue,
                                          SDL_MENU_FORWARD_FALLOFF_DISTANCE_MIN,
                                          SDL_MENU_FORWARD_FALLOFF_DISTANCE_MAX,
                                          "Falloff Distance",
                                          slider_x,
                                          slider_width,
                                          text_height);
    }

    layout.contentBottomY = layout.nextY;
    layout.maxScroll = 0.0f;
    layout.scroll = 0.0f;
    *out_layout = layout;
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
    if (font && text && text[0] &&
        ray_tracing_text_measure_utf8(menu_context_renderer(), font, text, &textW, &textH)) {
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
    if (!rect || rect->w <= 0 || rect->h <= 0) return;
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
    if (ray_tracing_text_measure_utf8(menu_context_renderer(), font, out, &w, NULL)) {
        if (w <= max_width) return;
    }
    len = strlen(out);
    while (len > 0) {
        --len;
        out[len] = '\0';
        {
            char candidate[PATH_MAX];
            snprintf(candidate, sizeof(candidate), "%s...", out);
            if (ray_tracing_text_measure_utf8(menu_context_renderer(), font, candidate, &w, NULL)) {
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
    char volumeLabel[160];
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
    const bool compact_scene_mode =
        animation_config_space_mode_clamp(animSettings.spaceMode) == SPACE_MODE_3D;

    memset(&layout, 0, sizeof(layout));
    layout.showPathToggles = integrator_menu.showPathToggles;
    layout.showLightHeight = false;
    if (screen_layout) {
        leftX = screen_layout->leftPanelRect.x + 18;
        subX = leftX + 10;
        maxLeftWidth = screen_layout->leftPanelRect.w - 36;
        rightEdge = screen_layout->routeStackRect.x + screen_layout->routeStackRect.w - 10;
        leftTopY = screen_layout->leftPanelRect.y + MENU_PANEL_CHROME_TITLE_BAND + 12;
        routeTopY = screen_layout->routeStackRect.y +
                    MENU_PANEL_CHROME_TITLE_BAND + 8;
        footerButtonY = screen_layout->bottomActionRowRect.y +
                        (screen_layout->bottomActionRowRect.h - BOTTOM_BUTTON_HEIGHT_EXIT) / 2;
        footerRightLimit = screen_layout->bottomActionRowRect.x + screen_layout->bottomActionRowRect.w - 14;
    }
    if (maxLeftWidth < 120) maxLeftWidth = 120;

    if (compact_scene_mode) {
        int mode_row_w = (maxLeftWidth - 8) / 2;
        if (mode_row_w < 120) mode_row_w = 120;
        layout.interactiveRect = (SDL_Rect){0, 0, 0, 0};
        layout.deepRenderRect = build_adaptive_button_rect(font,
                                                           leftX,
                                                           leftTopY,
                                                           mode_row_w,
                                                           COMPACT_TOGGLE_BUTTON_HEIGHT,
                                                           "3D Render",
                                                           mode_row_w);
        layout.asyncDeepRenderRect = build_adaptive_button_rect(
            font,
            leftX + maxLeftWidth - mode_row_w,
            layout.deepRenderRect.y,
            mode_row_w,
            COMPACT_TOGGLE_BUTTON_HEIGHT,
            "Async Render",
            mode_row_w);
        layout.bounceRect = build_adaptive_button_rect(font,
                                                       leftX,
                                                       layout.deepRenderRect.y +
                                                           layout.deepRenderRect.h + 8,
                                                       mode_row_w,
                                                       COMPACT_SUBSETTING_BUTTON_HEIGHT,
                                                       "Bounce",
                                                       mode_row_w);
        layout.autoMp4Rect = build_adaptive_button_rect(font,
                                                        leftX + maxLeftWidth - mode_row_w,
                                                        layout.bounceRect.y,
                                                        mode_row_w,
                                                        COMPACT_SUBSETTING_BUTTON_HEIGHT,
                                                        "Auto MP4",
                                                        mode_row_w);
        layout.integratorRect = build_adaptive_button_rect(font,
                                                           leftX,
                                                           layout.bounceRect.y + layout.bounceRect.h + 8,
                                                           INTEGRATOR_BUTTON_WIDTH,
                                                           COMPACT_TOGGLE_BUTTON_HEIGHT,
                                                           integratorLabel,
                                                           maxLeftWidth);
    } else {
        layout.interactiveRect = build_adaptive_button_rect(font, leftX, leftTopY,
                                                            TOGGLE_BUTTON_WIDTH, TOGGLE_BUTTON_HEIGHT,
                                                            "Interactive Mode", maxLeftWidth);
        layout.deepRenderRect = build_adaptive_button_rect(font, leftX,
                                                           layout.interactiveRect.y + layout.interactiveRect.h + TOGGLE_BUTTON_SPACING,
                                                           TOGGLE_BUTTON_WIDTH, TOGGLE_BUTTON_HEIGHT,
                                                           "Deep Render", maxLeftWidth);

        layout.asyncDeepRenderRect = build_adaptive_button_rect(
            font,
            subX,
            layout.deepRenderRect.y + layout.deepRenderRect.h + 15,
            SUBSETTING_BUTTON_WIDTH,
            SUBSETTING_BUTTON_HEIGHT,
            "Async Render",
            maxLeftWidth);

        layout.bounceRect = build_adaptive_button_rect(font, subX,
                                                       layout.asyncDeepRenderRect.y +
                                                           layout.asyncDeepRenderRect.h +
                                                           TOGGLE_BUTTON_SPACING,
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
    }

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
        if ((state && state->manifestDropdownOpen) ||
            animation_config_space_mode_clamp(animSettings.spaceMode) == SPACE_MODE_3D) {
            root_y += SDL_MENU_RENDER_MANIFEST_PANEL_MAX_HEIGHT + ROOT_ROW_SPACING;
        }
        if (root_value_w < 140) {
            root_value_w = 140;
        }
        layout.inputRootValueRect = (SDL_Rect){leftX, root_y, root_value_w, ROOT_ROW_HEIGHT};
        layout.inputRootEditRect = (SDL_Rect){leftX + root_value_w + 4, root_y, ROOT_CTRL_BUTTON_W, ROOT_ROW_HEIGHT};
        layout.inputRootFolderRect = (SDL_Rect){layout.inputRootEditRect.x + ROOT_CTRL_BUTTON_W + 2,
                                                root_y, ROOT_CTRL_BUTTON_W, ROOT_ROW_HEIGHT};
        layout.inputRootApplyRect = (SDL_Rect){layout.inputRootFolderRect.x + ROOT_CTRL_BUTTON_W + 2,
                                               root_y, ROOT_CTRL_BUTTON_W, ROOT_ROW_HEIGHT};
        layout.meshAssetRootValueRect = (SDL_Rect){leftX,
                                                   root_y + ROOT_ROW_HEIGHT + ROOT_ROW_SPACING,
                                                   root_value_w,
                                                   ROOT_ROW_HEIGHT};
        layout.meshAssetRootEditRect = (SDL_Rect){leftX + root_value_w + 4,
                                                  layout.meshAssetRootValueRect.y,
                                                  ROOT_CTRL_BUTTON_W,
                                                  ROOT_ROW_HEIGHT};
        layout.meshAssetRootFolderRect = (SDL_Rect){layout.meshAssetRootEditRect.x + ROOT_CTRL_BUTTON_W + 2,
                                                    layout.meshAssetRootValueRect.y,
                                                    ROOT_CTRL_BUTTON_W,
                                                    ROOT_ROW_HEIGHT};
        layout.meshAssetRootApplyRect = (SDL_Rect){layout.meshAssetRootFolderRect.x + ROOT_CTRL_BUTTON_W + 2,
                                                   layout.meshAssetRootValueRect.y,
                                                   ROOT_CTRL_BUTTON_W,
                                                   ROOT_ROW_HEIGHT};
        layout.attachVolumeRect = (SDL_Rect){0, 0, 0, 0};
        layout.volumeToggleRect = (SDL_Rect){0, 0, 0, 0};
        layout.volumeClearRect = (SDL_Rect){0, 0, 0, 0};
        if (animSettings.spaceMode == SPACE_MODE_3D) {
            int volume_buttons_y = layout.meshAssetRootValueRect.y + ROOT_ROW_HEIGHT + ROOT_ROW_SPACING + 4;
            int half_width = (maxLeftWidth - 6) / 2;
            if (half_width < 120) half_width = 120;
            volume_source_ui_format_active_button_label(volumeLabel, sizeof(volumeLabel));
            layout.attachVolumeRect = build_adaptive_button_rect(font,
                                                                 leftX,
                                                                 volume_buttons_y,
                                                                 LOAD_SCENE_BUTTON_WIDTH,
                                                                 LOAD_SCENE_BUTTON_HEIGHT,
                                                                 volumeLabel,
                                                                 maxLeftWidth);
            layout.volumeToggleRect = build_adaptive_button_rect(font,
                                                                 leftX,
                                                                 layout.attachVolumeRect.y + layout.attachVolumeRect.h + 6,
                                                                 half_width,
                                                                 ROOT_ROW_HEIGHT,
                                                                 animSettings.volumeInteractionEnabled
                                                                     ? "Atmosphere: ON"
                                                                     : "Atmosphere: OFF",
                                                                 half_width);
            layout.volumeClearRect = build_adaptive_button_rect(font,
                                                                leftX + maxLeftWidth - half_width,
                                                                layout.volumeToggleRect.y,
                                                                half_width,
                                                                ROOT_ROW_HEIGHT,
                                                                "Clear Volume",
                                                                half_width);
        }
    }
    leftColumnRight = max_int(layout.interactiveRect.x + layout.interactiveRect.w,
                              layout.deepRenderRect.x + layout.deepRenderRect.w);
    leftColumnRight = max_int(leftColumnRight,
                              layout.asyncDeepRenderRect.x +
                                  layout.asyncDeepRenderRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.bounceRect.x + layout.bounceRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.autoMp4Rect.x + layout.autoMp4Rect.w);
    leftColumnRight = max_int(leftColumnRight, layout.integratorRect.x + layout.integratorRect.w);
    if (layout.showPathToggles) {
        leftColumnRight = max_int(leftColumnRight, layout.pathRouletteRect.x + layout.pathRouletteRect.w);
        leftColumnRight = max_int(leftColumnRight, layout.pathBsdfRect.x + layout.pathBsdfRect.w);
    }
    leftColumnRight = max_int(leftColumnRight, layout.loadSceneRect.x + layout.loadSceneRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.inputRootApplyRect.x + layout.inputRootApplyRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.meshAssetRootApplyRect.x + layout.meshAssetRootApplyRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.attachVolumeRect.x + layout.attachVolumeRect.w);
    leftColumnRight = max_int(leftColumnRight, layout.volumeClearRect.x + layout.volumeClearRect.w);
    if (screen_layout) {
        int content_left = screen_layout->centerControlsRect.x + 12;
        int content_right = screen_layout->centerControlsRect.x +
                            screen_layout->centerControlsRect.w - 12;
        int content_width = content_right - content_left;
        int tab_y = screen_layout->centerControlsRect.y +
                    MENU_PANEL_CHROME_TITLE_BAND + 6;
        int tab_width = (content_width - RENDERER_CONTROL_COLUMN_GAP * 2) / 3;
        int first_row_y;
        int second_row_y;
        int slider_start_y;
        int column_width =
            (content_width - RENDERER_CONTROL_COLUMN_GAP) / 2;
        if (column_width < 120) column_width = 120;

        if (tab_width < 72) tab_width = 72;
        layout.rendererLightingTabRect = (SDL_Rect){
            content_left,
            tab_y,
            tab_width,
            RENDERER_CONTROL_TAB_HEIGHT
        };
        layout.rendererPerformanceTabRect = (SDL_Rect){
            layout.rendererLightingTabRect.x + tab_width +
                RENDERER_CONTROL_COLUMN_GAP,
            tab_y,
            tab_width,
            RENDERER_CONTROL_TAB_HEIGHT
        };
        layout.rendererCausticsTabRect = (SDL_Rect){
            layout.rendererPerformanceTabRect.x + tab_width +
                RENDERER_CONTROL_COLUMN_GAP,
            tab_y,
            tab_width,
            RENDERER_CONTROL_TAB_HEIGHT
        };
        first_row_y = tab_y + RENDERER_CONTROL_TAB_HEIGHT + 10;
        second_row_y = first_row_y + RENDERER_CONTROL_BUTTON_HEIGHT +
                       RENDERER_CONTROL_ROW_GAP;
        slider_start_y = second_row_y + RENDERER_CONTROL_BUTTON_HEIGHT + 12;
        centerLeftX = content_left;
        centerRightX = centerLeftX + column_width + RENDERER_CONTROL_COLUMN_GAP;
        centerColumnMaxWidth = column_width;

        if (state &&
            state->rendererControlsTab == MENU_RENDERER_CONTROLS_CAUSTICS) {
            layout.causticModeRect = build_adaptive_button_rect(
                font, content_left, first_row_y, content_width,
                RENDERER_CONTROL_BUTTON_HEIGHT, "Caustics", content_width);
            layout.causticEngineRect = build_adaptive_button_rect(
                font, content_left, second_row_y, content_width,
                RENDERER_CONTROL_BUTTON_HEIGHT, "Deposition", content_width);
            layout.causticSurfaceRect = build_adaptive_button_rect(
                font, centerLeftX,
                second_row_y + RENDERER_CONTROL_BUTTON_HEIGHT + RENDERER_CONTROL_ROW_GAP,
                column_width,
                RENDERER_CONTROL_BUTTON_HEIGHT, "Surface Cache", centerColumnMaxWidth);
            layout.causticVolumeRect = build_adaptive_button_rect(
                font, centerRightX,
                second_row_y + RENDERER_CONTROL_BUTTON_HEIGHT + RENDERER_CONTROL_ROW_GAP,
                column_width,
                RENDERER_CONTROL_BUTTON_HEIGHT, "Volume Cache", centerColumnMaxWidth);
            layout.causticDebugSummaryRect = build_adaptive_button_rect(
                font, centerLeftX,
                second_row_y + (RENDERER_CONTROL_BUTTON_HEIGHT + RENDERER_CONTROL_ROW_GAP) * 2,
                column_width, RENDERER_CONTROL_BUTTON_HEIGHT,
                "Debug Summary", centerColumnMaxWidth);
            layout.causticDebugExportRect = build_adaptive_button_rect(
                font, centerRightX,
                second_row_y + (RENDERER_CONTROL_BUTTON_HEIGHT + RENDERER_CONTROL_ROW_GAP) * 2,
                column_width, RENDERER_CONTROL_BUTTON_HEIGHT,
                "Debug Export", centerColumnMaxWidth);
            slider_start_y = second_row_y +
                             (RENDERER_CONTROL_BUTTON_HEIGHT + RENDERER_CONTROL_ROW_GAP) * 3 +
                             12;
        } else if (state &&
                   state->rendererControlsTab == MENU_RENDERER_CONTROLS_PERFORMANCE) {
            layout.tileRect = build_adaptive_button_rect(font,
                                                         centerLeftX,
                                                         first_row_y,
                                                         column_width,
                                                         RENDERER_CONTROL_BUTTON_HEIGHT,
                                                         animSettings.useTiledRenderer
                                                             ? "Tile Renderer: ON"
                                                             : "Tile Renderer: OFF",
                                                         centerColumnMaxWidth);
            layout.tilePreviewRect =
                build_adaptive_button_rect(font,
                                           centerRightX,
                                           first_row_y,
                                           column_width,
                                           RENDERER_CONTROL_BUTTON_HEIGHT,
                                           animSettings.tilePreviewEnabled
                                               ? "Tile Preview: ON"
                                               : "Tile Preview: OFF",
                                           centerColumnMaxWidth);
            layout.denoiseRect =
                build_adaptive_button_rect(font,
                                           centerLeftX,
                                           second_row_y,
                                           column_width,
                                           RENDERER_CONTROL_BUTTON_HEIGHT,
                                           animSettings.disneyDenoiseEnabled
                                               ? "Disney Denoise: ON"
                                               : "Disney Denoise: OFF",
                                           centerColumnMaxWidth);
            layout.upscaleModeRect =
                build_adaptive_button_rect(font,
                                           centerRightX,
                                           second_row_y,
                                           column_width,
                                           RENDERER_CONTROL_BUTTON_HEIGHT,
                                           menu_upscale_mode_button_label(),
                                           centerColumnMaxWidth);
        } else {
            layout.topFillRect =
                build_adaptive_button_rect(font,
                                           centerLeftX,
                                           first_row_y,
                                           column_width,
                                           RENDERER_CONTROL_BUTTON_HEIGHT,
                                           "Env: Ambient",
                                           centerColumnMaxWidth);
            layout.environmentPresetRect =
                build_adaptive_button_rect(font,
                                           centerRightX,
                                           first_row_y,
                                           column_width,
                                           RENDERER_CONTROL_BUTTON_HEIGHT,
                                           menu_environment_preset_label(),
                                           centerColumnMaxWidth);
            layout.falloffRect =
                build_adaptive_button_rect(font,
                                           centerLeftX,
                                           second_row_y,
                                           column_width,
                                           RENDERER_CONTROL_BUTTON_HEIGHT,
                                           "Falloff: Quadratic",
                                           centerColumnMaxWidth);
            layout.environmentBackgroundModeRect =
                build_adaptive_button_rect(font,
                                           centerRightX,
                                           second_row_y,
                                           column_width,
                                           RENDERER_CONTROL_BUTTON_HEIGHT,
                                           animSettings.environmentBackgroundBrightnessAuto
                                               ? "BG: Auto"
                                               : "BG: Manual",
                                           centerColumnMaxWidth);
        }
        menu_renderer_controls_build_slider_layout(font,
                                                   state,
                                                   screen_layout,
                                                   slider_start_y,
                                                   &layout.rendererControlSliders);
    } else {
        centerX = leftColumnRight + 24;
        centerMaxWidth = SLIDER_MARGIN_X - centerX - 16;
        if (centerMaxWidth < 120) centerMaxWidth = 120;
        layout.falloffRect = build_adaptive_button_rect(font,
                                                        centerX,
                                                        TOGGLE_BUTTON_MARGIN_Y + 10,
                                                        FORWARD_FALLOFF_BUTTON_WIDTH,
                                                        FORWARD_FALLOFF_BUTTON_HEIGHT,
                                                        "Quadratic (1/r^2)",
                                                        centerMaxWidth);
    }

    layout.startRect = build_adaptive_button_rect_right(font, rightEdge,
                                                        screen_layout ? (routeTopY + (ROUTE_BUTTON_HEIGHT + ROUTE_BUTTON_GAP) * 4) : BOTTOM_BUTTON_MARGIN_Y_START,
                                                        screen_layout ? ROUTE_BUTTON_WIDTH : BOTTOM_BUTTON_WIDTH_START,
                                                        screen_layout ? ROUTE_BUTTON_HEIGHT : BOTTOM_BUTTON_HEIGHT_START,
                                                        "Start", 0);
    layout.previewRect = build_adaptive_button_rect_right(font, rightEdge,
                                                          screen_layout ? (routeTopY + (ROUTE_BUTTON_HEIGHT + ROUTE_BUTTON_GAP) * 3) : BOTTOM_BUTTON_MARGIN_Y_PREVIEW,
                                                          screen_layout ? ROUTE_BUTTON_WIDTH : BOTTOM_BUTTON_WIDTH_START,
                                                          screen_layout ? ROUTE_BUTTON_HEIGHT : BOTTOM_BUTTON_HEIGHT_START,
                                                          "Preview", 0);
    layout.sceneEditorRect = build_adaptive_button_rect_right(font, rightEdge,
                                                              screen_layout ? (routeTopY + (ROUTE_BUTTON_HEIGHT + ROUTE_BUTTON_GAP) * 2) : (layout.startRect.y - (BOTTOM_BUTTON_HEIGHT_START + 8)),
                                                              screen_layout ? ROUTE_BUTTON_WIDTH : BOTTOM_BUTTON_WIDTH_START,
                                                              screen_layout ? ROUTE_BUTTON_HEIGHT : BOTTOM_BUTTON_HEIGHT_START,
                                                              "Scene Editor", 0);
    int clampedEditorMode = EditorModeRouter_ClampEditorMode(animSettings.editorMode,
                                                             AnimationUseFluidScene());
    if (clampedEditorMode != animSettings.editorMode) {
        animSettings.editorMode = clampedEditorMode;
    }
    const char* editorModeLabel = (clampedEditorMode == EDITOR_MODE_PATH) ? "Editor: Path" :
                                  (clampedEditorMode == EDITOR_MODE_OBJECT) ? "Editor: Scene" :
                                  (clampedEditorMode == EDITOR_MODE_CAMERA) ? "Editor: Camera" :
                                  "Editor: Material";
    layout.sceneModeRect = build_adaptive_button_rect_right(font, rightEdge,
                                                            screen_layout ? (routeTopY + ROUTE_BUTTON_HEIGHT + ROUTE_BUTTON_GAP) : (layout.sceneEditorRect.y - (BOTTOM_BUTTON_HEIGHT_START + 6)),
                                                            screen_layout ? ROUTE_BUTTON_WIDTH : BOTTOM_BUTTON_WIDTH_START,
                                                            screen_layout ? ROUTE_BUTTON_HEIGHT : BOTTOM_BUTTON_HEIGHT_START,
                                                            editorModeLabel, 0);
    layout.spaceModeRect = build_adaptive_button_rect_right(font, rightEdge,
                                                            screen_layout ? routeTopY : (layout.sceneModeRect.y - (BOTTOM_BUTTON_HEIGHT_START + 6)),
                                                            screen_layout ? ROUTE_BUTTON_WIDTH : BOTTOM_BUTTON_WIDTH_START,
                                                            screen_layout ? ROUTE_BUTTON_HEIGHT : BOTTOM_BUTTON_HEIGHT_START,
                                                            menu_space_mode_button_label(), 0);
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

    if (state &&
        state->menuWorkspaceHost.active_module != MENU_WORKSPACE_RENDER) {
        layout.rendererLightingTabRect = (SDL_Rect){0, 0, 0, 0};
        layout.rendererPerformanceTabRect = (SDL_Rect){0, 0, 0, 0};
        layout.rendererCausticsTabRect = (SDL_Rect){0, 0, 0, 0};
        layout.falloffRect = (SDL_Rect){0, 0, 0, 0};
        layout.tileRect = (SDL_Rect){0, 0, 0, 0};
        layout.tilePreviewRect = (SDL_Rect){0, 0, 0, 0};
        layout.denoiseRect = (SDL_Rect){0, 0, 0, 0};
        layout.topFillRect = (SDL_Rect){0, 0, 0, 0};
        layout.environmentPresetRect = (SDL_Rect){0, 0, 0, 0};
        layout.environmentBackgroundModeRect = (SDL_Rect){0, 0, 0, 0};
        layout.upscaleModeRect = (SDL_Rect){0, 0, 0, 0};
        layout.lightHeightRect = (SDL_Rect){0, 0, 0, 0};
        layout.causticModeRect = (SDL_Rect){0, 0, 0, 0};
        layout.causticEngineRect = (SDL_Rect){0, 0, 0, 0};
        layout.causticSurfaceRect = (SDL_Rect){0, 0, 0, 0};
        layout.causticVolumeRect = (SDL_Rect){0, 0, 0, 0};
        layout.causticDebugSummaryRect = (SDL_Rect){0, 0, 0, 0};
        layout.causticDebugExportRect = (SDL_Rect){0, 0, 0, 0};
        layout.rendererControlSliders.count = 0u;
    }

    if (out_layout) {
        *out_layout = layout;
    }
}
