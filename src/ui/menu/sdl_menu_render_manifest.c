#include "ui/menu/sdl_menu_render_internal.h"

#include <stdio.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "ui/shared_theme_font_adapter.h"

static const char* menu_active_scene_source_path(void) {
    int source = animation_config_scene_source_clamp(animSettings.sceneSource);
    if (source == SCENE_SOURCE_RUNTIME_SCENE) {
        return animSettings.runtimeScenePath;
    }
    if (source == SCENE_SOURCE_FLUID_MANIFEST) {
        return animSettings.fluidManifest;
    }
    return "";
}

static bool manifest_option_is_selected(const ManifestOption *option) {
    int selected_source = animation_config_scene_source_clamp(animSettings.sceneSource);
    const char *selected_path = menu_active_scene_source_path();
    int option_source = animation_config_scene_source_clamp(option ? option->source : SCENE_SOURCE_CONFIG_2D);
    if (!option) return false;
    if (selected_source != option_source) return false;
    if (option_source == SCENE_SOURCE_CONFIG_2D) return true;
    return selected_path[0] && strcmp(selected_path, option->path) == 0;
}

void menu_render_format_manifest_button_label(MenuRuntimeState* state,
                                              char *out,
                                              size_t out_size) {
    if (!state || !out || out_size == 0) return;
    const char *base = "Load Scene";
    const char *path = menu_active_scene_source_path();
    int source = animation_config_scene_source_clamp(animSettings.sceneSource);
    if (source == SCENE_SOURCE_CONFIG_2D) {
        snprintf(out, out_size, "%s [2D config]", base);
        return;
    }
    if (!path[0]) {
        snprintf(out, out_size, "%s", base);
        return;
    }
    char label[128];
    menu_state_build_manifest_label(path, label, sizeof(label));
    if (source == SCENE_SOURCE_RUNTIME_SCENE) {
        snprintf(out, out_size, "%s [Runtime]: %s", base, label);
    } else {
        snprintf(out, out_size, "%s [Fluid]: %s", base, label);
    }
    if (strlen(out) >= out_size) {
        out[out_size - 1] = '\0';
    }
}

void menu_render_draw_manifest_dropdown(SDL_Renderer *renderer,
                                        TTF_Font *font,
                                        MenuRuntimeState* state,
                                        const MenuButtonLayout* buttons,
                                        const MenuScreenLayout* screen_layout) {
    RayTracingThemePalette palette = {0};
    const bool has_shared_palette = ray_tracing_shared_theme_resolve_palette(&palette);
    SDL_Rect panel_rect = {0, 0, 0, 0};
    if (!state || !buttons) return;

    if (screen_layout && screen_layout->manifestReserveRect.w > 0 &&
        screen_layout->manifestReserveRect.h > 0) {
        panel_rect = screen_layout->manifestReserveRect;
    } else {
        int available = 0;
        panel_rect.x = buttons->loadSceneRect.x;
        panel_rect.y = buttons->inputRootValueRect.y + buttons->inputRootValueRect.h + 6;
        panel_rect.w = (buttons->inputRootApplyRect.x + buttons->inputRootApplyRect.w) - panel_rect.x;
        available = (screen_layout ? (screen_layout->leftPanelRect.y + screen_layout->leftPanelRect.h - 18)
                                   : SDL_MENU_RENDER_MANIFEST_FALLBACK_BOTTOM_Y) - panel_rect.y;
        panel_rect.h = SDL_MENU_RENDER_MANIFEST_PANEL_MIN_HEIGHT;
        if (available > SDL_MENU_RENDER_MANIFEST_PANEL_MIN_HEIGHT) panel_rect.h = available;
        if (panel_rect.h > SDL_MENU_RENDER_MANIFEST_PANEL_MAX_HEIGHT) {
            panel_rect.h = SDL_MENU_RENDER_MANIFEST_PANEL_MAX_HEIGHT;
        }
    }

    {
        const int min_h = SDL_MENU_MANIFEST_ITEM_HEIGHT +
                          SDL_MENU_RENDER_MANIFEST_ITEM_PADDING * 2 + 4;
        if (panel_rect.h < min_h) panel_rect.h = min_h;
    }
    if (panel_rect.w <= 0 || panel_rect.h <= 0) {
        state->manifestPanelRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestListRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestScrollbarRect = (SDL_Rect){0, 0, 0, 0};
        state->manifestScrollbarVisible = false;
        state->manifestThumbHeight = 0.0f;
        state->manifestTrackHeight = 0.0f;
        return;
    }

    state->manifestPanelRect = panel_rect;
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

    int list_x = panel_rect.x + SDL_MENU_RENDER_MANIFEST_ITEM_PADDING;
    int list_y = panel_rect.y + SDL_MENU_RENDER_MANIFEST_ITEM_PADDING;
    int list_w = panel_rect.w - SDL_MENU_RENDER_MANIFEST_ITEM_PADDING * 2 -
                 SDL_MENU_RENDER_MANIFEST_SCROLLBAR_WIDTH - 4;
    if (list_w < 40) list_w = 40;
    int list_h = panel_rect.h - SDL_MENU_RENDER_MANIFEST_ITEM_PADDING * 2;
    state->manifestListRect = (SDL_Rect){list_x, list_y, list_w, list_h};

    int visible_indices[SDL_MENU_MAX_MANIFEST_OPTIONS];
    int visible_count = 0;
    for (int i = 0; i < (int)state->manifestOptionCount; ++i) {
        if (menu_state_manifest_option_visible(state, &state->manifestOptions[i])) {
            visible_indices[visible_count++] = i;
        }
    }

    int content_h = visible_count * SDL_MENU_MANIFEST_ITEM_HEIGHT;
    state->manifestMaxScroll = (content_h > list_h) ? (float)(content_h - list_h) : 0.0f;
    state->manifestScrollbarVisible = state->manifestMaxScroll > 0.5f;
    menu_state_manifest_clamp_scroll(state);
    state->manifestTrackHeight = (float)list_h;
    state->manifestThumbHeight = 0.0f;
    state->manifestScrollbarRect = (SDL_Rect){0, 0, 0, 0};

    if (state->manifestScrollbarVisible) {
        float thumb = ((float)list_h * (float)list_h) / (float)content_h;
        if (thumb < 16.0f) thumb = 16.0f;
        state->manifestThumbHeight = thumb;
        float track_range = (float)list_h - thumb;
        float thumb_y = (track_range > 0.0f && state->manifestMaxScroll > 0.0f)
                            ? (float)list_y + (state->manifestScroll / state->manifestMaxScroll) * track_range
                            : (float)list_y;
        int scroll_x = panel_rect.x + panel_rect.w -
                       SDL_MENU_RENDER_MANIFEST_SCROLLBAR_WIDTH -
                       SDL_MENU_RENDER_MANIFEST_ITEM_PADDING;
        SDL_Rect track = {scroll_x, list_y, SDL_MENU_RENDER_MANIFEST_SCROLLBAR_WIDTH, list_h};
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_border.r, palette.panel_border.g,
                                   palette.panel_border.b, palette.panel_border.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
        }
        SDL_RenderFillRect(renderer, &track);

        state->manifestScrollbarRect =
            (SDL_Rect){scroll_x, (int)thumb_y, SDL_MENU_RENDER_MANIFEST_SCROLLBAR_WIDTH, (int)thumb};
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.accent_primary.r, palette.accent_primary.g,
                                   palette.accent_primary.b, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 120, 120, 140, 255);
        }
        SDL_RenderFillRect(renderer, &state->manifestScrollbarRect);
    }

    int first_index = (int)(state->manifestScroll / SDL_MENU_MANIFEST_ITEM_HEIGHT);
    int y_offset = -(int)state->manifestScroll % SDL_MENU_MANIFEST_ITEM_HEIGHT;
    SDL_Rect prev_clip = {0, 0, 0, 0};
    SDL_bool clip_was_enabled = SDL_RenderIsClipEnabled(renderer);
    if (clip_was_enabled) {
        SDL_RenderGetClipRect(renderer, &prev_clip);
    }
    SDL_RenderSetClipRect(renderer, &state->manifestListRect);
    for (int row = first_index; row < visible_count; ++row) {
        int i = visible_indices[row];
        int item_y = list_y + y_offset + (row - first_index) * SDL_MENU_MANIFEST_ITEM_HEIGHT;
        if (item_y > list_y + list_h - SDL_MENU_MANIFEST_ITEM_HEIGHT) break;
        SDL_Rect item_rect = {list_x, item_y, list_w, SDL_MENU_MANIFEST_ITEM_HEIGHT};
        bool is_selected = manifest_option_is_selected(&state->manifestOptions[i]);
        if (has_shared_palette) {
            SDL_Color fill = is_selected ? palette.accent_primary : palette.button_fill;
            if (is_selected) {
                fill = menu_render_ensure_highlight_fill_contrast(fill,
                                                                  palette.text_primary,
                                                                  palette.panel_fill);
            }
            SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
        } else {
            SDL_SetRenderDrawColor(renderer,
                                   is_selected ? 70 : 50,
                                   is_selected ? 120 : 70,
                                   is_selected ? 90 : 70,
                                   255);
        }
        SDL_RenderFillRect(renderer, &item_rect);
        if (has_shared_palette) {
            SDL_SetRenderDrawColor(renderer,
                                   palette.panel_border.r, palette.panel_border.g,
                                   palette.panel_border.b, palette.panel_border.a);
        } else {
            SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        }
        SDL_RenderDrawRect(renderer, &item_rect);
        SDL_Color text_color;
        if (has_shared_palette) {
            SDL_Color item_fill = is_selected
                                      ? menu_render_ensure_highlight_fill_contrast(palette.accent_primary,
                                                                                   palette.text_primary,
                                                                                   palette.panel_fill)
                                      : palette.button_fill;
            text_color = menu_render_choose_readable_text(item_fill, palette.text_primary);
        } else {
            text_color = is_selected ? (SDL_Color){220, 240, 220, 255}
                                     : (SDL_Color){230, 230, 230, 255};
        }
        char label_fit[256];
        menu_render_fit_text_to_width(font,
                                      state->manifestOptions[i].name,
                                      item_rect.w - 12,
                                      label_fit,
                                      sizeof(label_fit));
        menu_render_draw_text_color(renderer,
                                    font,
                                    item_rect.x + 6,
                                    item_rect.y + 4,
                                    text_color,
                                    label_fit);
    }

    if (visible_count == 0) {
        SDL_Color color = has_shared_palette ? palette.text_muted
                                             : (SDL_Color){210, 210, 210, 255};
        menu_render_draw_text_color(renderer,
                                    font,
                                    list_x,
                                    list_y + 4,
                                    color,
                                    "No scene sources found");
    }
    if (clip_was_enabled) {
        SDL_RenderSetClipRect(renderer, &prev_clip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }
}
