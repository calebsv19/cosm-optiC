#include "editor/scene_editor_chrome_shell.h"

#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>

#include "config/config_manager.h"
#include "editor/scene_editor_surface_render.h"
#include "editor/scene_editor_tool_state.h"
#include "engine/Render/render_pipeline.h"
#include "render/render_helper.h"
#include "render/text_draw.h"
#include "render/text_font_cache.h"

SDL_Rect applyButton = {1000, 700, 150, 50};
SDL_Rect previewButton;
SDL_Rect changeModeButton;
SDL_Rect saveButton;
SDL_Rect backToMenuButton;
SDL_Rect selectButton;
SDL_Rect addButton;
SDL_Rect deleteButton;
static SDL_Rect modeSelectButtons[3];
static char g_sceneActionFeedbackText[128];
static Uint64 g_sceneActionFeedbackUntilMs = 0u;

static void scene_editor_chrome_shell_default_palette(RayTracingThemePalette* out_palette) {
    if (!out_palette) return;
    out_palette->background_fill = (SDL_Color){46, 46, 52, 255};
    out_palette->panel_fill = (SDL_Color){58, 58, 68, 230};
    out_palette->panel_border = (SDL_Color){95, 95, 112, 255};
    out_palette->button_fill = (SDL_Color){180, 180, 180, 255};
    out_palette->button_active_fill = (SDL_Color){70, 140, 215, 255};
    out_palette->button_text = (SDL_Color){0, 0, 0, 255};
    out_palette->text_primary = (SDL_Color){220, 220, 230, 255};
    out_palette->text_muted = (SDL_Color){210, 210, 215, 255};
    out_palette->accent_primary = (SDL_Color){120, 200, 255, 255};
}

RayTracingThemePalette SceneEditorChromeShellResolvePalette(void) {
    RayTracingThemePalette palette = {0};
    if (!ray_tracing_shared_theme_resolve_palette(&palette)) {
        scene_editor_chrome_shell_default_palette(&palette);
    }
    return palette;
}

static Uint8 scene_editor_chrome_shell_color_offset(Uint8 value, int offset) {
    int out = (int)value + offset;
    if (out < 0) return 0;
    if (out > 255) return 255;
    return (Uint8)out;
}

static SDL_Rect scene_editor_chrome_shell_rect_from_core(CorePaneRect rect) {
    SDL_Rect out = {0};
    out.x = (int)lroundf(rect.x);
    out.y = (int)lroundf(rect.y);
    out.w = (int)lroundf(rect.width);
    out.h = (int)lroundf(rect.height);
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static void scene_editor_chrome_shell_render_splitter_overlay(SDL_Renderer* renderer,
                                                              const CorePaneRect* splitter_rect,
                                                              bool hovered,
                                                              bool active) {
    SDL_Rect draw_rect = {0};
    SDL_Color fill = {210, 215, 225, 128};

    if (!renderer || !splitter_rect) return;
    draw_rect = scene_editor_chrome_shell_rect_from_core(*splitter_rect);
    if (draw_rect.w <= 0 || draw_rect.h <= 0) return;

    if (active) {
        fill = (SDL_Color){230, 178, 92, 228};
    } else if (hovered) {
        fill = (SDL_Color){214, 220, 232, 170};
    }

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &draw_rect);
    SDL_SetRenderDrawColor(renderer, 24, 26, 33, 220);
    SDL_RenderDrawRect(renderer, &draw_rect);
}

void SceneEditorChromeShellSetActionFeedback(const char* text, Uint32 lifetime_ms) {
    if (!text || !text[0]) {
        g_sceneActionFeedbackText[0] = '\0';
        g_sceneActionFeedbackUntilMs = 0u;
        return;
    }
    snprintf(g_sceneActionFeedbackText, sizeof(g_sceneActionFeedbackText), "%s", text);
    g_sceneActionFeedbackUntilMs = SDL_GetTicks64() + (Uint64)lifetime_ms;
}

static SDL_Color scene_editor_chrome_shell_resolve_button_text_color(SDL_Color fill,
                                                                     RayTracingThemePalette palette) {
    return ray_tracing_theme_choose_button_text(fill, palette);
}

static int scene_editor_chrome_shell_resolve_tone_delta(SDL_Color base_fill,
                                                        int dark_theme_delta,
                                                        int light_theme_delta) {
    int luminance = (int)base_fill.r * 299 + (int)base_fill.g * 587 + (int)base_fill.b * 114;
    return (luminance >= 140000) ? light_theme_delta : dark_theme_delta;
}

static SDL_Color scene_editor_chrome_shell_resolve_button_fill(SDL_Color base_fill,
                                                               SDL_Color disabled_fill,
                                                               bool enabled,
                                                               bool hovered,
                                                               bool emphasized) {
    SDL_Color out = enabled ? base_fill : disabled_fill;
    int delta = 0;
    if (!enabled) return out;
    if (emphasized) {
        delta = scene_editor_chrome_shell_resolve_tone_delta(base_fill,
                                                             hovered ? 24 : 16,
                                                             hovered ? -24 : -16);
        out.r = scene_editor_chrome_shell_color_offset(out.r, delta);
        out.g = scene_editor_chrome_shell_color_offset(out.g, delta);
        out.b = scene_editor_chrome_shell_color_offset(out.b, delta);
        return out;
    }
    if (hovered) {
        delta = scene_editor_chrome_shell_resolve_tone_delta(base_fill, 10, -10);
        out.r = scene_editor_chrome_shell_color_offset(out.r, delta);
        out.g = scene_editor_chrome_shell_color_offset(out.g, delta);
        out.b = scene_editor_chrome_shell_color_offset(out.b, delta);
    }
    return out;
}

static int scene_editor_chrome_shell_measure_button_width(const char* label, int min_width) {
    TTF_Font* font = NULL;
    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx ? ctx->renderer : NULL;
    int text_w = 0;
    int text_h = 0;
    int point_size = animation_config_scale_text_point_size(&animSettings, 26, 12);

    if (!label || !label[0]) return min_width;
    font = ray_tracing_text_font_cache_get_ui_regular(renderer, point_size, 12);
    if (!font) return min_width;
    if (!ray_tracing_text_measure_utf8(renderer, font, label, &text_w, &text_h)) {
        return min_width;
    }
    if (text_w + 28 > min_width) {
        min_width = text_w + 28;
    }
    return min_width;
}

static int scene_editor_chrome_shell_measure_button_height(int min_height) {
    int point_size = animation_config_scale_text_point_size(&animSettings, 24, 12);
    int target = point_size + 14;
    if (target > min_height) return target;
    return min_height;
}

static bool scene_editor_chrome_shell_point_in_rect(int x, int y, const SDL_Rect* rect) {
    if (!rect) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static bool scene_editor_chrome_shell_button_hovered(const SDL_Rect* rect) {
    int mx = 0;
    int my = 0;
    SDL_GetMouseState(&mx, &my);
    return scene_editor_chrome_shell_point_in_rect(mx, my, rect);
}

int SceneEditorChromeShellResolveModeButtonAtPoint(int mx, int my) {
    for (int i = 0; i < 3; i++) {
        if (scene_editor_chrome_shell_point_in_rect(mx, my, &modeSelectButtons[i])) {
            return i;
        }
    }
    return -1;
}

bool SceneEditorChromeShellIsButtonHit(int mx, int my) {
    if (SceneEditorChromeShellResolveModeButtonAtPoint(mx, my) >= 0) {
        return true;
    }
    if (scene_editor_chrome_shell_point_in_rect(mx, my, &selectButton)) {
        return true;
    }
    if (scene_editor_chrome_shell_point_in_rect(mx, my, &addButton)) {
        return true;
    }
    if (scene_editor_chrome_shell_point_in_rect(mx, my, &deleteButton)) {
        return true;
    }
    if (scene_editor_chrome_shell_point_in_rect(mx, my, &previewButton)) {
        return true;
    }
    if (scene_editor_chrome_shell_point_in_rect(mx, my, &changeModeButton)) {
        return true;
    }
    if (scene_editor_chrome_shell_point_in_rect(mx, my, &applyButton)) {
        return true;
    }
    if (scene_editor_chrome_shell_point_in_rect(mx, my, &saveButton)) {
        return true;
    }
    if (scene_editor_chrome_shell_point_in_rect(mx, my, &backToMenuButton)) {
        return true;
    }
    return false;
}

void SceneEditorChromeShellLayoutFallback(int width, int height) {
    int compactButtonHeight = scene_editor_chrome_shell_measure_button_height(40);
    int footerButtonHeight = scene_editor_chrome_shell_measure_button_height(46);
    int buttonGap = 10;
    int footerMargin = 30;
    int contentWidth = scene_editor_chrome_shell_measure_button_width("Back to Menu", 320);
    int pairWidth = (contentWidth - buttonGap) / 2;

    selectButton = (SDL_Rect){20,
                              height - compactButtonHeight - footerMargin,
                              contentWidth,
                              compactButtonHeight};
    addButton = (SDL_Rect){20,
                           selectButton.y - compactButtonHeight - buttonGap,
                           pairWidth,
                           compactButtonHeight};
    deleteButton = (SDL_Rect){addButton.x + pairWidth + buttonGap,
                              addButton.y,
                              contentWidth - pairWidth - buttonGap,
                              compactButtonHeight};
    backToMenuButton = (SDL_Rect){width - contentWidth - footerMargin,
                                  height - footerButtonHeight - footerMargin,
                                  contentWidth,
                                  footerButtonHeight};
    applyButton = (SDL_Rect){backToMenuButton.x,
                             backToMenuButton.y - footerButtonHeight - buttonGap,
                             pairWidth,
                             footerButtonHeight};
    saveButton = (SDL_Rect){applyButton.x + pairWidth + buttonGap,
                            applyButton.y,
                            contentWidth - pairWidth - buttonGap,
                            footerButtonHeight};
    changeModeButton = (SDL_Rect){backToMenuButton.x,
                                  applyButton.y - footerButtonHeight - buttonGap,
                                  pairWidth,
                                  footerButtonHeight};
    previewButton = (SDL_Rect){changeModeButton.x + pairWidth + buttonGap,
                               changeModeButton.y,
                               contentWidth - pairWidth - buttonGap,
                               footerButtonHeight};

    for (int i = 0; i < 3; i++) {
        modeSelectButtons[i] = (SDL_Rect){0, 0, 0, 0};
    }
}

void SceneEditorChromeShellLayoutFromPane(const SceneEditorPaneLayout* layout) {
    int compactButtonWidth = 0;
    int actionButtonHeight = 0;
    int actionRowWidth = 0;
    int actionHalfWidth = 0;
    int buttonGap = 10;
    int modeGap = 4;
    SDL_Rect left = {0};
    SDL_Rect right = {0};
    SDL_Rect modeRect = {0};
    int modeButtonWidth = 0;
    int modeButtonRemain = 0;
    int modeButtonX = 0;

    if (!layout) {
        SceneEditorChromeShellLayoutFallback(sceneSettings.windowWidth, sceneSettings.windowHeight);
        return;
    }

    actionButtonHeight = scene_editor_chrome_shell_measure_button_height(44);
    left = layout->left_content_rect;
    right = layout->right_content_rect;
    modeRect = layout->mode_router_rect;
    actionRowWidth = right.w;
    if (actionRowWidth < 160) {
        actionRowWidth = 160;
    }
    actionHalfWidth = (actionRowWidth - buttonGap) / 2;

    compactButtonWidth = left.w;
    selectButton = (SDL_Rect){left.x,
                              left.y + left.h - actionButtonHeight,
                              compactButtonWidth,
                              actionButtonHeight};
    addButton = (SDL_Rect){left.x,
                           selectButton.y - actionButtonHeight - buttonGap,
                           actionHalfWidth,
                           actionButtonHeight};
    deleteButton = (SDL_Rect){addButton.x + actionHalfWidth + buttonGap,
                              addButton.y,
                              actionRowWidth - actionHalfWidth - buttonGap,
                              actionButtonHeight};
    if (deleteButton.x + deleteButton.w > left.x + left.w) {
        deleteButton.w = (left.x + left.w) - deleteButton.x;
    }
    if (deleteButton.w < 60) {
        deleteButton.w = 60;
    }
    if (addButton.w < 60) {
        addButton.w = 60;
    }
    if (addButton.x + addButton.w > left.x + left.w) {
        addButton.w = left.w;
    }
    if (deleteButton.x < left.x) {
        deleteButton.x = left.x;
    }
    if (deleteButton.y < left.y) {
        deleteButton.y = left.y;
    }
    if (addButton.y < left.y) {
        addButton.y = left.y;
    }
    if (selectButton.y < left.y) {
        selectButton.y = left.y;
    }
    if (selectButton.w < compactButtonWidth) {
        selectButton.w = compactButtonWidth;
    }
    if (selectButton.x != left.x) {
        selectButton.x = left.x;
    }
    if (addButton.x != left.x) {
        addButton.x = left.x;
    }
    if (selectButton.w > left.w) {
        selectButton.w = left.w;
    }
    if (addButton.x + addButton.w > left.x + left.w) {
        addButton.w = left.w;
    }
    if (deleteButton.x + deleteButton.w > left.x + left.w) {
        deleteButton.w = (left.x + left.w) - deleteButton.x;
    }
    backToMenuButton = (SDL_Rect){right.x,
                                  right.y + right.h - actionButtonHeight,
                                  actionRowWidth,
                                  actionButtonHeight};
    applyButton = (SDL_Rect){right.x,
                             backToMenuButton.y - actionButtonHeight - buttonGap,
                             actionHalfWidth,
                             actionButtonHeight};
    saveButton = (SDL_Rect){applyButton.x + actionHalfWidth + buttonGap,
                            applyButton.y,
                            actionRowWidth - actionHalfWidth - buttonGap,
                            actionButtonHeight};
    changeModeButton = (SDL_Rect){right.x,
                                  applyButton.y - actionButtonHeight - buttonGap,
                                  actionHalfWidth,
                                  actionButtonHeight};
    previewButton = (SDL_Rect){changeModeButton.x + actionHalfWidth + buttonGap,
                               changeModeButton.y,
                               actionRowWidth - actionHalfWidth - buttonGap,
                               actionButtonHeight};

    modeButtonWidth = (modeRect.w - modeGap * 2) / 3;
    if (modeButtonWidth < 80) modeButtonWidth = 80;
    modeButtonRemain = modeRect.w - (modeButtonWidth * 3 + modeGap * 2);
    modeButtonX = modeRect.x;
    for (int i = 0; i < 3; i++) {
        int button_w = modeButtonWidth;
        if (i == 2 && modeButtonRemain > 0) {
            button_w += modeButtonRemain;
        }
        modeSelectButtons[i] = (SDL_Rect){modeButtonX, modeRect.y, button_w, modeRect.h};
        modeButtonX += button_w + modeGap;
    }
}

static void scene_editor_chrome_shell_render_button(SDL_Renderer* renderer,
                                                    SDL_Rect rect,
                                                    const char* label,
                                                    bool enabled,
                                                    bool hovered,
                                                    bool emphasized,
                                                    SDL_Color base_fill,
                                                    SDL_Color disabled_fill,
                                                    SDL_Color border_color,
                                                    RayTracingThemePalette palette) {
    SDL_Color resolvedFill = {0};
    SDL_Color resolvedText = {0};

    if (!renderer) return;
    resolvedFill = scene_editor_chrome_shell_resolve_button_fill(base_fill,
                                                                 disabled_fill,
                                                                 enabled,
                                                                 hovered,
                                                                 emphasized);
    SDL_SetRenderDrawColor(renderer, resolvedFill.r, resolvedFill.g, resolvedFill.b, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, border_color.a);
    SDL_RenderDrawRect(renderer, &rect);
    resolvedText = scene_editor_chrome_shell_resolve_button_text_color(resolvedFill, palette);
    RenderButtonTextWithColor(renderer, rect, label, resolvedText);
}

void SceneEditorChromeShellRender(SDL_Renderer* renderer,
                                  const SceneEditorPaneLayout* layout,
                                  bool layout_valid,
                                  const SceneEditorControlSurfaceContract* contract,
                                  const CorePaneRect* splitter_rect,
                                  bool splitter_hovered,
                                  bool splitter_active) {
    RayTracingThemePalette palette = SceneEditorChromeShellResolvePalette();
    SDL_Color paneLabelColor = palette.text_primary;
    SDL_Color statusColor = palette.text_muted;
    SDL_Color paneFill = palette.panel_fill;
    SDL_Color paneBorder = palette.panel_border;
    SDL_Color modeBarFill = (SDL_Color){
        scene_editor_chrome_shell_color_offset(palette.panel_fill.r, -10),
        scene_editor_chrome_shell_color_offset(palette.panel_fill.g, -10),
        scene_editor_chrome_shell_color_offset(palette.panel_fill.b, -10),
        240
    };
    SDL_Color disabledFill = (SDL_Color){
        scene_editor_chrome_shell_color_offset(palette.panel_fill.r, -6),
        scene_editor_chrome_shell_color_offset(palette.panel_fill.g, -6),
        scene_editor_chrome_shell_color_offset(palette.panel_fill.b, -6),
        255
    };
    SDL_Color borderColor = (SDL_Color){
        scene_editor_chrome_shell_color_offset(palette.panel_border.r, -30),
        scene_editor_chrome_shell_color_offset(palette.panel_border.g, -30),
        scene_editor_chrome_shell_color_offset(palette.panel_border.b, -30),
        255
    };
    SDL_Rect titleRect = {0};
    SDL_Rect feedbackRect = {0};
    bool showFeedback = false;
    SceneEditorTool active_tool = SceneEditorToolStateGetActive();

    if (!renderer || !contract) return;

    if (layout_valid && layout) {
        SDL_SetRenderDrawColor(renderer, paneFill.r, paneFill.g, paneFill.b, paneFill.a);
        SDL_RenderFillRect(renderer, &layout->left_pane_rect);
        SDL_RenderFillRect(renderer, &layout->right_pane_rect);
        SDL_SetRenderDrawColor(renderer, modeBarFill.r, modeBarFill.g, modeBarFill.b, modeBarFill.a);
        SDL_RenderFillRect(renderer, &layout->mode_router_rect);

        titleRect = (SDL_Rect){layout->left_pane_rect.x + 10,
                               layout->left_pane_rect.y + 6,
                               layout->left_pane_rect.w - 20,
                               20};
        RenderLabelText(renderer, titleRect, contract->paneLeftTitle, paneLabelColor);

        titleRect = (SDL_Rect){layout->center_pane_rect.x + 10,
                               layout->center_pane_rect.y + 6,
                               layout->center_pane_rect.w - 20,
                               20};
        RenderLabelText(renderer, titleRect, contract->paneCenterTitle, paneLabelColor);

        titleRect = (SDL_Rect){layout->right_pane_rect.x + 10,
                               layout->right_pane_rect.y + 6,
                               layout->right_pane_rect.w - 20,
                               20};
        RenderLabelText(renderer, titleRect, contract->paneRightTitle, paneLabelColor);

        SDL_SetRenderDrawColor(renderer, paneBorder.r, paneBorder.g, paneBorder.b, paneBorder.a);
        SDL_RenderDrawRect(renderer, &layout->left_pane_rect);
        SDL_RenderDrawRect(renderer, &layout->center_pane_rect);
        SDL_RenderDrawRect(renderer, &layout->right_pane_rect);
        SDL_RenderDrawRect(renderer, &layout->mode_router_rect);
        SDL_SetRenderDrawColor(renderer, paneBorder.r, paneBorder.g, paneBorder.b, 220);
        SDL_RenderDrawRect(renderer, &layout->viewport_rect);
    }

    scene_editor_chrome_shell_render_splitter_overlay(renderer,
                                                      splitter_rect,
                                                      splitter_hovered,
                                                      splitter_active);

    for (int i = 0; i < 3; i++) {
        bool selectable = contract->modeSelectable[i];
        bool active = (i == contract->activeMode);
        scene_editor_chrome_shell_render_button(renderer,
                                                modeSelectButtons[i],
                                                SceneEditorControlSurfaceModeLabel(i),
                                                selectable,
                                                scene_editor_chrome_shell_button_hovered(&modeSelectButtons[i]),
                                                active,
                                                active ? ray_tracing_theme_resolve_button_active_fill(palette)
                                                       : palette.button_fill,
                                                disabledFill,
                                                borderColor,
                                                palette);
    }

    scene_editor_chrome_shell_render_button(renderer,
                                            selectButton,
                                            SceneEditorToolStateToolLabel(SCENE_EDITOR_TOOL_SELECT),
                                            true,
                                            scene_editor_chrome_shell_button_hovered(&selectButton),
                                            active_tool == SCENE_EDITOR_TOOL_SELECT,
                                            palette.button_fill,
                                            disabledFill,
                                            borderColor,
                                            palette);
    scene_editor_chrome_shell_render_button(renderer,
                                            addButton,
                                            SceneEditorToolStateToolLabel(SCENE_EDITOR_TOOL_ADD),
                                            true,
                                            scene_editor_chrome_shell_button_hovered(&addButton),
                                            active_tool == SCENE_EDITOR_TOOL_ADD,
                                            palette.button_fill,
                                            disabledFill,
                                            borderColor,
                                            palette);
    scene_editor_chrome_shell_render_button(renderer,
                                            deleteButton,
                                            SceneEditorToolStateToolLabel(SCENE_EDITOR_TOOL_DELETE),
                                            true,
                                            scene_editor_chrome_shell_button_hovered(&deleteButton),
                                            active_tool == SCENE_EDITOR_TOOL_DELETE,
                                            palette.button_fill,
                                            disabledFill,
                                            borderColor,
                                            palette);

    scene_editor_chrome_shell_render_button(renderer,
                                            applyButton,
                                            contract->applyLabel,
                                            contract->applyEnabled,
                                            scene_editor_chrome_shell_button_hovered(&applyButton),
                                            false,
                                            palette.button_fill,
                                            disabledFill,
                                            borderColor,
                                            palette);
    scene_editor_chrome_shell_render_button(renderer,
                                            previewButton,
                                            contract->previewLabel,
                                            contract->previewEnabled,
                                            scene_editor_chrome_shell_button_hovered(&previewButton),
                                            false,
                                            palette.button_fill,
                                            disabledFill,
                                            borderColor,
                                            palette);
    scene_editor_chrome_shell_render_button(renderer,
                                            changeModeButton,
                                            contract->cycleModeLabel,
                                            contract->cycleModeEnabled,
                                            scene_editor_chrome_shell_button_hovered(&changeModeButton),
                                            false,
                                            palette.button_fill,
                                            disabledFill,
                                            borderColor,
                                            palette);
    scene_editor_chrome_shell_render_button(renderer,
                                            saveButton,
                                            contract->saveLabel,
                                            contract->saveEnabled,
                                            scene_editor_chrome_shell_button_hovered(&saveButton),
                                            false,
                                            palette.button_fill,
                                            disabledFill,
                                            borderColor,
                                            palette);
    scene_editor_chrome_shell_render_button(renderer,
                                            backToMenuButton,
                                            contract->backToMenuLabel,
                                            contract->backToMenuEnabled,
                                            scene_editor_chrome_shell_button_hovered(&backToMenuButton),
                                            false,
                                            palette.button_fill,
                                            disabledFill,
                                            borderColor,
                                            palette);

    showFeedback = (g_sceneActionFeedbackText[0] &&
                    g_sceneActionFeedbackUntilMs > SDL_GetTicks64());
    if (showFeedback) {
        feedbackRect = (SDL_Rect){
            (layout_valid && layout) ? layout->right_content_rect.x : backToMenuButton.x,
            previewButton.y - 30,
            (layout_valid && layout) ? layout->right_content_rect.w : backToMenuButton.w,
            24
        };
        RenderLabelText(renderer, feedbackRect, g_sceneActionFeedbackText, palette.text_primary);
    }

    if (layout_valid && layout) {
        SceneEditorSurfaceRenderLeftPaneContent(renderer,
                                               layout,
                                               contract,
                                               paneLabelColor,
                                               statusColor);
        SceneEditorSurfaceRenderRightPaneStatus(renderer,
                                               layout,
                                               contract,
                                               (showFeedback ? feedbackRect.y : previewButton.y) - 10,
                                               paneLabelColor,
                                               statusColor);
    }
}
