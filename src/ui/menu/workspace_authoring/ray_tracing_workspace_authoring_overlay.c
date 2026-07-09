#include "ui/menu/workspace_authoring/ray_tracing_workspace_authoring_overlay.h"

#include <stdio.h>
#include <string.h>

#include "config/config_manager.h"
#include "core_font.h"
#include "core_theme.h"
#include "ui/sdl_menu_render.h"
#include "ui/shared_theme_font_adapter.h"

enum {
    RAY_TRACING_AUTHORING_PANE_ROW_CAP = 3
};

typedef struct RayTracingWorkspaceAuthoringPaneDef {
    const char* pane_label;
    const char* module_key;
    const char* module_label;
} RayTracingWorkspaceAuthoringPaneDef;

static const RayTracingWorkspaceAuthoringPaneDef k_ray_tracing_authoring_panes[] = {
    { "P1 Scene Tools", "ray.scene_tools", "Scene Tools" },
    { "P2 Viewport", "ray.viewport", "Viewport" },
    { "P3 Inspector", "ray.inspector", "Inspector" }
};

static SDL_Rect ray_tracing_authoring_sdl_rect(CorePaneRect rect) {
    SDL_Rect out;
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static SDL_Rect ray_tracing_authoring_sdl_kit_rect(KitRenderRect rect) {
    SDL_Rect out;
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static SDL_Color ray_tracing_authoring_alpha(SDL_Color color, Uint8 alpha) {
    color.a = alpha;
    return color;
}

static SDL_Color ray_tracing_authoring_color(SDL_Color shared_color,
                                             SDL_Color fallback,
                                             int has_shared_palette) {
    return has_shared_palette ? shared_color : fallback;
}

uint32_t ray_tracing_workspace_authoring_overlay_build_pane_rows(
    int width,
    int height,
    const SceneEditorPaneLayout* scene_layout,
    RayTracingWorkspaceAuthoringPaneRow* out_rows,
    uint32_t cap) {
    SceneEditorPaneLayout fallback_layout;
    SceneEditorPaneHost fallback_host;
    const SceneEditorPaneLayout* layout = scene_layout;
    uint32_t count = 0u;

    if (!out_rows || cap == 0u || width <= 0 || height <= 0) return 0u;
    if (!layout) {
        if (!scene_editor_pane_host_init(&fallback_host, width, height)) {
            return 0u;
        }
        layout = scene_editor_pane_host_layout(&fallback_host);
        if (!layout) return 0u;
        fallback_layout = *layout;
        layout = &fallback_layout;
    }

    if (count < cap) {
        out_rows[count].pane_rect = layout->left_pane_rect;
        out_rows[count].pane_label = k_ray_tracing_authoring_panes[count].pane_label;
        out_rows[count].module_key = k_ray_tracing_authoring_panes[count].module_key;
        out_rows[count].module_label = k_ray_tracing_authoring_panes[count].module_label;
        ++count;
    }
    if (count < cap) {
        out_rows[count].pane_rect = layout->center_pane_rect;
        out_rows[count].pane_label = k_ray_tracing_authoring_panes[count].pane_label;
        out_rows[count].module_key = k_ray_tracing_authoring_panes[count].module_key;
        out_rows[count].module_label = k_ray_tracing_authoring_panes[count].module_label;
        ++count;
    }
    if (count < cap) {
        out_rows[count].pane_rect = layout->right_pane_rect;
        out_rows[count].pane_label = k_ray_tracing_authoring_panes[count].pane_label;
        out_rows[count].module_key = k_ray_tracing_authoring_panes[count].module_key;
        out_rows[count].module_label = k_ray_tracing_authoring_panes[count].module_label;
        ++count;
    }
    return count;
}

static void ray_tracing_authoring_draw_button(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const KitWorkspaceAuthoringOverlayButton* button,
    const RayTracingThemePalette* palette,
    int has_shared_palette) {
    SDL_Rect rect;
    SDL_Color fill;
    SDL_Color border;
    SDL_Color text;
    if (!renderer || !font || !button || !button->visible) return;

    rect = ray_tracing_authoring_sdl_rect(button->rect);
    fill = ray_tracing_authoring_color(palette->button_fill,
                                       (SDL_Color){28, 31, 38, 238},
                                       has_shared_palette);
    border = ray_tracing_authoring_color(palette->accent_primary,
                                         (SDL_Color){150, 174, 220, 245},
                                         has_shared_palette);
    text = ray_tracing_authoring_color(palette->button_text,
                                       (SDL_Color){236, 240, 248, 255},
                                       has_shared_palette);
    fill = ray_tracing_authoring_alpha(fill, button->enabled ? 238u : 140u);
    border = ray_tracing_authoring_alpha(border, button->enabled ? 245u : 150u);
    text = ray_tracing_authoring_alpha(text, button->enabled ? 255u : 150u);

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);
    menu_render_draw_text_color(renderer,
                                font,
                                rect.x + 7,
                                rect.y + 3,
                                text,
                                button->label ? button->label : "");
}

static void ray_tracing_authoring_draw_controls(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const RayTracingWorkspaceAuthoringHostState* host,
    int width,
    const RayTracingThemePalette* palette,
    int has_shared_palette) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count = 0u;
    uint32_t i = 0u;
    if (!renderer || !font || !host || width <= 0) return;
    count = kit_workspace_authoring_ui_build_overlay_buttons(
        width,
        ray_tracing_workspace_authoring_host_active(host),
        ray_tracing_workspace_authoring_host_pane_overlay_active(host),
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    for (i = 0u; i < count; ++i) {
        ray_tracing_authoring_draw_button(renderer,
                                          font,
                                          &buttons[i],
                                          palette,
                                          has_shared_palette);
    }
}

static void ray_tracing_authoring_draw_pane_rows(
    SDL_Renderer* renderer,
    TTF_Font* font,
    int width,
    int height,
    const SceneEditorPaneLayout* scene_layout,
    const RayTracingThemePalette* palette,
    int has_shared_palette) {
    RayTracingWorkspaceAuthoringPaneRow rows[RAY_TRACING_AUTHORING_PANE_ROW_CAP];
    uint32_t count = 0u;
    uint32_t i = 0u;
    SDL_Color border = ray_tracing_authoring_color(palette->accent_primary,
                                                   (SDL_Color){150, 174, 220, 230},
                                                   has_shared_palette);
    SDL_Color label_fill = ray_tracing_authoring_color(palette->panel_fill,
                                                       (SDL_Color){18, 21, 26, 238},
                                                       has_shared_palette);
    SDL_Color text = ray_tracing_authoring_color(palette->text_primary,
                                                 (SDL_Color){238, 241, 248, 255},
                                                 has_shared_palette);
    SDL_Color muted = ray_tracing_authoring_color(palette->text_muted,
                                                  (SDL_Color){175, 184, 204, 255},
                                                  has_shared_palette);

    if (!renderer || !font || width <= 0 || height <= 0) return;
    border = ray_tracing_authoring_alpha(border, 220u);
    label_fill = ray_tracing_authoring_alpha(label_fill, 236u);

    count = ray_tracing_workspace_authoring_overlay_build_pane_rows(
        width,
        height,
        scene_layout,
        rows,
        (uint32_t)(sizeof(rows) / sizeof(rows[0])));
    for (i = 0u; i < count; ++i) {
        SDL_Rect pane_rect = rows[i].pane_rect;
        SDL_Rect label_rect;
        char detail[192];
        if (pane_rect.w <= 0 || pane_rect.h <= 0) continue;

        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(renderer, &pane_rect);
        label_rect = (SDL_Rect){ pane_rect.x + 8, pane_rect.y + 8, 230, 24 };
        if (label_rect.x + label_rect.w > pane_rect.x + pane_rect.w - 8) {
            label_rect.w = (pane_rect.x + pane_rect.w - 8) - label_rect.x;
        }
        if (label_rect.w < 96) label_rect.w = pane_rect.w - 16;
        if (label_rect.w > 0) {
            SDL_SetRenderDrawColor(renderer, label_fill.r, label_fill.g, label_fill.b, label_fill.a);
            SDL_RenderFillRect(renderer, &label_rect);
            SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
            SDL_RenderDrawRect(renderer, &label_rect);
            menu_render_draw_text_color(renderer,
                                        font,
                                        label_rect.x + 6,
                                        label_rect.y + 4,
                                        text,
                                        rows[i].pane_label ? rows[i].pane_label : "Pane");
        }

        if (pane_rect.w > 260 && pane_rect.h > 82) {
            snprintf(detail,
                     sizeof(detail),
                     "%s / %s",
                     rows[i].module_key ? rows[i].module_key : "unbound",
                     rows[i].module_label ? rows[i].module_label : "Unbound");
            menu_render_draw_text_color(renderer,
                                        font,
                                        pane_rect.x + 14,
                                        pane_rect.y + 42,
                                        muted,
                                        detail);
        }
    }
}

static void ray_tracing_authoring_draw_section(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const KitRenderRect* rect,
    const char* title,
    const char* subtitle,
    const RayTracingThemePalette* palette,
    int has_shared_palette) {
    SDL_Rect sdl_rect;
    SDL_Color fill;
    SDL_Color border;
    SDL_Color text;
    SDL_Color muted;
    if (!renderer || !font || !rect) return;
    sdl_rect = ray_tracing_authoring_sdl_kit_rect(*rect);
    if (sdl_rect.w <= 0 || sdl_rect.h <= 0) return;

    fill = ray_tracing_authoring_color(palette->panel_fill,
                                       (SDL_Color){20, 23, 29, 242},
                                       has_shared_palette);
    border = ray_tracing_authoring_color(palette->panel_border,
                                         (SDL_Color){94, 116, 154, 220},
                                         has_shared_palette);
    text = ray_tracing_authoring_color(palette->text_primary,
                                       (SDL_Color){238, 242, 250, 255},
                                       has_shared_palette);
    muted = ray_tracing_authoring_color(palette->text_muted,
                                        (SDL_Color){174, 184, 204, 255},
                                        has_shared_palette);
    fill = ray_tracing_authoring_alpha(fill, 244u);
    border = ray_tracing_authoring_alpha(border, 218u);

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &sdl_rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &sdl_rect);
    menu_render_draw_text_color(renderer,
                                font,
                                sdl_rect.x + 14,
                                sdl_rect.y + 12,
                                text,
                                title ? title : "");
    if (subtitle && subtitle[0]) {
        menu_render_draw_text_color(renderer,
                                    font,
                                    sdl_rect.x + 14,
                                    sdl_rect.y + 38,
                                    muted,
                                    subtitle);
    }
}

static void ray_tracing_authoring_draw_font_theme_button(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const KitRenderRect* rect,
    const char* label,
    int selected,
    int enabled,
    const RayTracingThemePalette* palette,
    int has_shared_palette) {
    SDL_Rect sdl_rect;
    SDL_Color fill;
    SDL_Color border;
    SDL_Color text;
    if (!renderer || !font || !rect) return;
    sdl_rect = ray_tracing_authoring_sdl_kit_rect(*rect);
    if (sdl_rect.w <= 0 || sdl_rect.h <= 0) return;

    fill = selected
               ? ray_tracing_authoring_color(
                     ray_tracing_theme_resolve_button_active_fill(*palette),
                     (SDL_Color){122, 157, 214, 245},
                     has_shared_palette)
               : ray_tracing_authoring_color(palette->button_fill,
                                             (SDL_Color){34, 38, 48, 238},
                                             has_shared_palette);
    border = ray_tracing_authoring_color(selected ? palette->text_primary
                                                  : palette->accent_primary,
                                         selected ? (SDL_Color){238, 242, 250, 255}
                                                  : (SDL_Color){132, 162, 214, 235},
                                         has_shared_palette);
    text = ray_tracing_theme_choose_button_text(fill, *palette);
    fill = ray_tracing_authoring_alpha(fill, enabled ? 238u : 126u);
    border = ray_tracing_authoring_alpha(border, enabled ? 238u : 128u);
    text = ray_tracing_authoring_alpha(text, enabled ? 255u : 138u);

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &sdl_rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &sdl_rect);
    menu_render_draw_text_color(renderer,
                                font,
                                sdl_rect.x + 8,
                                sdl_rect.y + 5,
                                text,
                                label ? label : "");
}

static void ray_tracing_authoring_draw_font_theme_overlay(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const RayTracingWorkspaceAuthoringHostState* host,
    int width,
    int height,
    const RayTracingThemePalette* palette,
    int has_shared_palette) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    SDL_Color background;
    char font_preset[64] = "ide";
    char theme_preset[64] = "midnight_contrast";
    char text_size[96];
    char status[192];
    uint32_t i;
    if (!renderer || !font || !host || width <= 0 || height <= 0) return;
    if (!kit_workspace_authoring_ui_font_theme_build_layout(NULL, width, height, &layout)) {
        return;
    }

    (void)ray_tracing_shared_font_current_preset(font_preset, sizeof(font_preset));
    (void)ray_tracing_shared_theme_current_preset(theme_preset, sizeof(theme_preset));
    snprintf(text_size,
             sizeof(text_size),
             "Text Size step:%d (%d%%)",
             animSettings.textZoomStep,
             animation_config_text_zoom_percent_from_step(animSettings.textZoomStep));
    snprintf(status,
             sizeof(status),
             "%s",
             host->font_theme_status_active
                 ? host->font_theme_status
                 : "Click a preset to preview live; Apply accepts, Cancel restores baseline.");

    background = ray_tracing_authoring_color(palette->background_fill,
                                             (SDL_Color){8, 10, 14, 255},
                                             has_shared_palette);
    SDL_SetRenderDrawColor(renderer, background.r, background.g, background.b, 255);
    SDL_RenderFillRect(renderer, NULL);

    ray_tracing_authoring_draw_section(renderer,
                                       font,
                                       &layout.font_preset_section,
                                       "Font Preset",
                                       font_preset,
                                       palette,
                                       has_shared_palette);
    for (i = 0u; i < layout.font_preset_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT + i);
        CoreFontPresetId preset_id;
        int selected = 0;
        if (kit_workspace_authoring_ui_font_theme_button_font_preset_id(button_id, &preset_id)) {
            const char* name = core_font_preset_name(preset_id);
            selected = name && strcmp(name, font_preset) == 0;
        }
        ray_tracing_authoring_draw_font_theme_button(
            renderer,
            font,
            &layout.font_preset_buttons[i],
            kit_workspace_authoring_ui_font_theme_button_label(button_id),
            selected,
            (int)kit_workspace_authoring_ui_font_theme_button_enabled(button_id),
            palette,
            has_shared_palette);
    }

    ray_tracing_authoring_draw_section(renderer,
                                       font,
                                       &layout.text_size_section,
                                       "Text Size",
                                       text_size,
                                       palette,
                                       has_shared_palette);
    ray_tracing_authoring_draw_font_theme_button(
        renderer,
        font,
        &layout.text_size_dec_button,
        kit_workspace_authoring_ui_font_theme_button_label(
            KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC),
        0,
        1,
        palette,
        has_shared_palette);
    ray_tracing_authoring_draw_font_theme_button(
        renderer,
        font,
        &layout.text_size_inc_button,
        kit_workspace_authoring_ui_font_theme_button_label(
            KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC),
        0,
        1,
        palette,
        has_shared_palette);
    ray_tracing_authoring_draw_font_theme_button(
        renderer,
        font,
        &layout.text_size_value_chip,
        text_size,
        1,
        1,
        palette,
        has_shared_palette);
    ray_tracing_authoring_draw_font_theme_button(
        renderer,
        font,
        &layout.text_size_reset_button,
        kit_workspace_authoring_ui_font_theme_button_label(
            KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET),
        0,
        1,
        palette,
        has_shared_palette);

    ray_tracing_authoring_draw_section(renderer,
                                       font,
                                       &layout.theme_preset_section,
                                       "Theme Preset",
                                       theme_preset,
                                       palette,
                                       has_shared_palette);
    for (i = 0u; i < layout.theme_preset_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_DAW_DEFAULT + i);
        CoreThemePresetId preset_id;
        int selected = 0;
        if (kit_workspace_authoring_ui_font_theme_button_theme_preset_id(button_id, &preset_id)) {
            const char* name = core_theme_preset_name(preset_id);
            selected = name && strcmp(name, theme_preset) == 0;
        }
        ray_tracing_authoring_draw_font_theme_button(
            renderer,
            font,
            &layout.theme_preset_buttons[i],
            kit_workspace_authoring_ui_font_theme_button_label(button_id),
            selected,
            (int)kit_workspace_authoring_ui_font_theme_button_enabled(button_id),
            palette,
            has_shared_palette);
    }

    ray_tracing_authoring_draw_section(renderer,
                                       font,
                                       &layout.custom_theme_section,
                                       "Custom Theme Slots",
                                       status,
                                       palette,
                                       has_shared_palette);
    for (i = 0u; i < layout.custom_theme_button_count; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_CREATE_STUB + i);
        ray_tracing_authoring_draw_font_theme_button(
            renderer,
            font,
            &layout.custom_theme_buttons[i],
            kit_workspace_authoring_ui_font_theme_button_label(button_id),
            0,
            (int)kit_workspace_authoring_ui_font_theme_button_enabled(button_id),
            palette,
            has_shared_palette);
    }
}

void ray_tracing_workspace_authoring_overlay_draw(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const RayTracingWorkspaceAuthoringHostState* host,
    int width,
    int height,
    const SceneEditorPaneLayout* scene_layout) {
    RayTracingThemePalette palette = {0};
    int has_shared_palette = 0;
    if (!renderer || !font || !host ||
        !ray_tracing_workspace_authoring_host_active(host) ||
        width <= 0 || height <= 0) {
        return;
    }

    has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette) ? 1 : 0;
    if (ray_tracing_workspace_authoring_host_pane_overlay_active(host)) {
        ray_tracing_authoring_draw_pane_rows(renderer,
                                             font,
                                             width,
                                             height,
                                             scene_layout,
                                             &palette,
                                             has_shared_palette);
    } else if (ray_tracing_workspace_authoring_host_font_theme_overlay_active(host)) {
        ray_tracing_authoring_draw_font_theme_overlay(renderer,
                                                      font,
                                                      host,
                                                      width,
                                                      height,
                                                      &palette,
                                                      has_shared_palette);
    }
    ray_tracing_authoring_draw_controls(renderer,
                                        font,
                                        host,
                                        width,
                                        &palette,
                                        has_shared_palette);
}
