#include "ui/menu/sdl_menu_render_internal.h"

#include <stdio.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "ui/shared_theme_font_adapter.h"
#include "ui/volume_source_ui_labels.h"

static bool volume_option_is_selected(const VolumeSourceOption *option) {
    int selected_kind = animation_config_volume_source_kind_clamp(animSettings.volumeSourceKind);
    int option_kind = animation_config_volume_source_kind_clamp(option ? option->kind : VOLUME_SOURCE_NONE);
    if (!option) return false;
    if (selected_kind != option_kind) return false;
    return animSettings.volumeSourcePath[0] &&
           strcmp(animSettings.volumeSourcePath, option->path) == 0;
}

void menu_render_format_volume_button_label(MenuRuntimeState* state,
                                            char *out,
                                            size_t out_size) {
    (void)state;
    volume_source_ui_format_active_button_label(out, out_size);
}

void menu_render_draw_volume_dropdown(SDL_Renderer *renderer,
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
        panel_rect.x = buttons->attachVolumeRect.x;
        panel_rect.y = buttons->volumeClearRect.y + buttons->volumeClearRect.h + 6;
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
        state->volumePanelRect = (SDL_Rect){0, 0, 0, 0};
        state->volumeListRect = (SDL_Rect){0, 0, 0, 0};
        state->volumeScrollbarRect = (SDL_Rect){0, 0, 0, 0};
        state->volumeScrollbarVisible = false;
        state->volumeThumbHeight = 0.0f;
        state->volumeTrackHeight = 0.0f;
        return;
    }

    state->volumePanelRect = panel_rect;
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_fill.r, palette.panel_fill.g,
                               palette.panel_fill.b, palette.panel_fill.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 28, 28, 30, 230);
    }
    SDL_RenderFillRect(renderer, &state->volumePanelRect);
    if (has_shared_palette) {
        SDL_SetRenderDrawColor(renderer,
                               palette.panel_border.r, palette.panel_border.g,
                               palette.panel_border.b, palette.panel_border.a);
    } else {
        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
    }
    SDL_RenderDrawRect(renderer, &state->volumePanelRect);

    int list_x = panel_rect.x + SDL_MENU_RENDER_MANIFEST_ITEM_PADDING;
    int list_y = panel_rect.y + SDL_MENU_RENDER_MANIFEST_ITEM_PADDING;
    int list_w = panel_rect.w - SDL_MENU_RENDER_MANIFEST_ITEM_PADDING * 2 -
                 SDL_MENU_RENDER_MANIFEST_SCROLLBAR_WIDTH - 4;
    if (list_w < 40) list_w = 40;
    int list_h = panel_rect.h - SDL_MENU_RENDER_MANIFEST_ITEM_PADDING * 2;
    state->volumeListRect = (SDL_Rect){list_x, list_y, list_w, list_h};

    {
        int content_h = (int)state->volumeOptionCount * SDL_MENU_MANIFEST_ITEM_HEIGHT;
        state->volumeMaxScroll = (content_h > list_h) ? (float)(content_h - list_h) : 0.0f;
        state->volumeScrollbarVisible = state->volumeMaxScroll > 0.5f;
        menu_state_volume_clamp_scroll(state);
        state->volumeTrackHeight = (float)list_h;
        state->volumeThumbHeight = 0.0f;
        state->volumeScrollbarRect = (SDL_Rect){0, 0, 0, 0};

        if (state->volumeScrollbarVisible) {
            float thumb = ((float)list_h * (float)list_h) / (float)content_h;
            if (thumb < 16.0f) thumb = 16.0f;
            state->volumeThumbHeight = thumb;
            float track_range = (float)list_h - thumb;
            float thumb_y = (track_range > 0.0f && state->volumeMaxScroll > 0.0f)
                                ? (float)list_y + (state->volumeScroll / state->volumeMaxScroll) * track_range
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

            state->volumeScrollbarRect =
                (SDL_Rect){scroll_x, (int)thumb_y, SDL_MENU_RENDER_MANIFEST_SCROLLBAR_WIDTH, (int)thumb};
            if (has_shared_palette) {
                SDL_SetRenderDrawColor(renderer,
                                       palette.accent_primary.r, palette.accent_primary.g,
                                       palette.accent_primary.b, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 120, 120, 140, 255);
            }
            SDL_RenderFillRect(renderer, &state->volumeScrollbarRect);
        }
    }

    {
        int first_index = (int)(state->volumeScroll / SDL_MENU_MANIFEST_ITEM_HEIGHT);
        int y_offset = -(int)state->volumeScroll % SDL_MENU_MANIFEST_ITEM_HEIGHT;
        SDL_Rect prev_clip = {0, 0, 0, 0};
        SDL_bool clip_was_enabled = SDL_RenderIsClipEnabled(renderer);
        if (clip_was_enabled) {
            SDL_RenderGetClipRect(renderer, &prev_clip);
        }
        SDL_RenderSetClipRect(renderer, &state->volumeListRect);
        for (int i = first_index; i < (int)state->volumeOptionCount; ++i) {
            int item_y = list_y + y_offset + (i - first_index) * SDL_MENU_MANIFEST_ITEM_HEIGHT;
            SDL_Rect item_rect = {list_x, item_y, list_w, SDL_MENU_MANIFEST_ITEM_HEIGHT};
            bool is_selected = volume_option_is_selected(&state->volumeOptions[i]);
            if (item_y > list_y + list_h - SDL_MENU_MANIFEST_ITEM_HEIGHT) break;

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
                                          state->volumeOptions[i].name,
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

        if (state->volumeOptionCount == 0) {
            SDL_Color color = has_shared_palette ? palette.text_muted
                                                 : (SDL_Color){210, 210, 210, 255};
            menu_render_draw_text_color(renderer,
                                        font,
                                        list_x,
                                        list_y + 4,
                                        color,
                                        "No volume sources found");
        }
        if (clip_was_enabled) {
            SDL_RenderSetClipRect(renderer, &prev_clip);
        } else {
            SDL_RenderSetClipRect(renderer, NULL);
        }
    }
}
