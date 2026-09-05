#include "ui/menu_numeric_controls.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "config/config_manager.h"
#include "render/text_draw.h"
#include "ui/menu_settings_lifecycle.h"
#include "ui/shared_theme_font_adapter.h"
#include "ui/menu_panel_chrome.h"
#include "engine/Render/render_pipeline.h"

static bool contains(SDL_Rect rect, int x, int y) {
    SDL_Point point = {x, y};
    return SDL_PointInRect(&point, &rect);
}

MenuNumericSpec menu_numeric_spec(const MenuRuntimeState *state, int *target, int min, int max) {
    MenuNumericSpec spec = {min, max, max >= 1000 ? 5 : 1, 1, 1, 0};
    if (target == &sceneSettings.windowWidth || target == &sceneSettings.windowHeight) {
        spec.multiple = spec.step = 2;
    } else if (target == &animSettings.tileSize) {
        spec.multiple = spec.step = 4;
    } else if (target == &state->rouletteSliderValue || target == &state->rouletteThreshold3DSliderValue) {
        spec.divisor = 1000;
        spec.decimals = 3;
        spec.step = 1;
    } else if (target == &state->envSliderValue) {
        spec.divisor = 255;
        spec.decimals = 3;
        spec.step = 1;
    } else if (target == &state->cacheWeightSliderValue ||
               target == &state->lightIntensitySliderValue ||
               target == &state->lightDecaySoftnessSliderValue ||
               target == &state->topFillStrengthSliderValue ||
               target == &state->environmentBackgroundBrightnessSliderValue ||
               target == &state->environmentBackgroundRedSliderValue ||
               target == &state->environmentBackgroundGreenSliderValue ||
               target == &state->environmentBackgroundBlueSliderValue) {
        spec.divisor = 100;
        spec.decimals = 2;
        spec.step = 1;
    }
    return spec;
}

void menu_numeric_apply(MenuRuntimeState *state, int *target, int value) {
    const int width = sceneSettings.windowWidth;
    const int height = sceneSettings.windowHeight;
    *target = value;
    menu_state_apply_special_slider_rules(state, target);
    if ((target == &sceneSettings.windowWidth || target == &sceneSettings.windowHeight) &&
        (width != sceneSettings.windowWidth || height != sceneSettings.windowHeight)) {
        menu_state_reanchor_camera_after_resize(width, height);
        state->oldWindowWidth = sceneSettings.windowWidth;
        state->oldWindowHeight = sceneSettings.windowHeight;
    }
}

void menu_numeric_layout(MenuSlider *slider, int text_height, int right) {
    const int height = text_height + 4;
    slider->valueRect = (SDL_Rect){slider->valueX - 3, slider->valueY - 2,
                                  right - slider->valueX - 19, height};
    slider->increaseRect = (SDL_Rect){right - 18, slider->valueY - 2, 16, height / 2};
    slider->decreaseRect = slider->increaseRect;
    slider->decreaseRect.y += height / 2;
    slider->decreaseRect.h = height - height / 2;
}

bool menu_numeric_finish(MenuRuntimeState *state, bool apply) {
    MenuNumericEdit *edit = &state->numericEdit;
    if (!edit->active) return true;
    if (apply) {
        int value;
        if (!menu_numeric_parse(edit->spec, edit->text, &value)) {
            edit->invalid = true;
            return false;
        }
        menu_numeric_apply(state, edit->target, value);
        menu_settings_lifecycle_commit_slider_release(state, edit->target);
    }
    if (edit->ownsTextInput) SDL_StopTextInput();
    edit->active = false;
    return true;
}

static int prefix_width(SDL_Renderer *renderer, TTF_Font *font, const char *text, size_t count) {
    char prefix[64];
    int width = 0;
    snprintf(prefix, sizeof(prefix), "%.*s", (int)count, text);
    (void)ray_tracing_text_measure_utf8(renderer, font, prefix, &width, NULL);
    return width;
}

bool menu_numeric_click(SDL_Event *event, const SliderLayout *layout, MenuRuntimeState *state, TTF_Font *font) {
    const int x = event->button.x, y = event->button.y;
    if (event->button.button != SDL_BUTTON_LEFT || !contains(layout->panelRect, x, y)) return false;
    if (layout->panelRect.x == state->sliderPanelRect.x &&
        y < layout->panelRect.y + MENU_PANEL_CHROME_TITLE_BAND) return false;
    for (size_t i = 0; i < layout->count; ++i) {
        const MenuSlider *slider = &layout->items[i];
        MenuNumericEdit *edit = &state->numericEdit;
        const bool value_hit = contains(slider->valueRect, x, y);
        int direction = contains(slider->increaseRect, x, y) ? 1 :
                        contains(slider->decreaseRect, x, y) ? -1 : 0;
        if (!value_hit && !direction) continue;
        if (edit->active && (edit->target != slider->value || direction) &&
            !menu_numeric_finish(state, true)) return true;
        MenuNumericSpec spec = menu_numeric_spec(state, slider->value, slider->min, slider->max);
        if (direction) {
            menu_numeric_apply(state, slider->value,
                menu_numeric_normalize(spec, (double)*slider->value + direction * spec.step, false));
            menu_settings_lifecycle_commit_slider_release(state, slider->value);
        } else if (event->button.clicks >= 2) {
            if (!edit->active) {
                menu_numeric_begin(edit, slider->value, spec);
                edit->ownsTextInput = !SDL_IsTextInputActive();
            }
            edit->anchor = 0;
            edit->cursor = strlen(edit->text);
            edit->blinkEpoch = SDL_GetTicks();
            SDL_StartTextInput();
            SDL_SetTextInputRect(&slider->valueRect);
        } else if (edit->active) {
            size_t index = 0;
            for (; index < strlen(edit->text); ++index) {
                SDL_Renderer *renderer = getRenderContext() ? getRenderContext()->renderer : NULL;
                int left = prefix_width(renderer, font, edit->text, index);
                int right = prefix_width(renderer, font, edit->text, index + 1);
                if (x - slider->valueX < (left + right) / 2) break;
            }
            menu_numeric_move(edit, (int)index, (SDL_GetModState() & KMOD_SHIFT) != 0);
            edit->blinkEpoch = SDL_GetTicks();
        }
        return true;
    }
    return false;
}

bool menu_numeric_key(MenuRuntimeState *state, const SDL_Event *event) {
    MenuNumericEdit *edit = &state->numericEdit;
    if (!edit->active) return false;
    const SDL_Keycode key = event->key.keysym.sym;
    const bool command = (event->key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0;
    const bool shift = (event->key.keysym.mod & KMOD_SHIFT) != 0;
    const size_t start = edit->cursor < edit->anchor ? edit->cursor : edit->anchor;
    const size_t end = edit->cursor > edit->anchor ? edit->cursor : edit->anchor;
    edit->blinkEpoch = SDL_GetTicks();
    if (command && key == SDLK_a) {
        edit->anchor = 0;
        edit->cursor = strlen(edit->text);
    } else if (command && (key == SDLK_c || key == SDLK_x)) {
        if (start != end) {
            char selection[64];
            snprintf(selection, sizeof(selection), "%.*s", (int)(end - start), edit->text + start);
            SDL_SetClipboardText(selection);
            if (key == SDLK_x) menu_numeric_delete(edit, false);
        }
    } else if (command && key == SDLK_v) {
        char *clipboard = SDL_GetClipboardText();
        if (clipboard) { (void)menu_numeric_insert(edit, clipboard); SDL_free(clipboard); }
    } else if (key == SDLK_LEFT) {
        menu_numeric_move(edit, command ? 0 : !shift && start != end ? (int)start : (int)edit->cursor - 1, shift);
    } else if (key == SDLK_RIGHT) {
        menu_numeric_move(edit, command ? (int)strlen(edit->text) : !shift && start != end ? (int)end : (int)edit->cursor + 1, shift);
    } else if (key == SDLK_HOME) {
        menu_numeric_move(edit, 0, shift);
    } else if (key == SDLK_END) {
        menu_numeric_move(edit, (int)strlen(edit->text), shift);
    } else if (key == SDLK_BACKSPACE || key == SDLK_DELETE) {
        menu_numeric_delete(edit, key == SDLK_DELETE);
    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_TAB) {
        (void)menu_numeric_finish(state, true);
    } else if (key == SDLK_ESCAPE) {
        (void)menu_numeric_finish(state, false);
    }
    return true; /* Editing captures shortcuts even for unhandled keys. */
}

void menu_numeric_draw(SDL_Renderer *renderer, TTF_Font *font, MenuRuntimeState *state, const MenuSlider *slider) {
    MenuNumericEdit *edit = &state->numericEdit;
    const bool active = edit->active && edit->target == slider->value;
    char formatted[64];
    MenuNumericSpec spec = menu_numeric_spec(state, slider->value, slider->min, slider->max);
    RayTracingThemePalette palette = {0};
    bool themed = ray_tracing_shared_theme_resolve_palette(&palette);
    SDL_Color color = themed ? palette.text_primary : (SDL_Color){230,230,235,255};
    SDL_Color border = active ? (edit->invalid ? (SDL_Color){255,110,90,255} :
                                (SDL_Color){110,180,240,255}) :
                       themed ? palette.panel_border : (SDL_Color){65,68,75,255};
    menu_numeric_format(spec, *slider->value, formatted, sizeof(formatted));
    if (!active && slider->value == &state->renderScale3DSliderValue) {
        if (*slider->value == RUNTIME_3D_RENDER_SCALE_HIDPI) snprintf(formatted, sizeof(formatted), "HiDPI");
        else snprintf(formatted, sizeof(formatted), "%dx", *slider->value);
    }
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &slider->valueRect);
    SDL_Rect old_clip, clip;
    bool clipped = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &old_clip);
    clip = slider->valueRect;
    clip.x += 2; clip.w -= 4;
    if (clipped) SDL_IntersectRect(&clip, &old_clip, &clip);
    SDL_RenderSetClipRect(renderer, &clip);
    const char *text = active ? edit->text : formatted;
    int x = slider->valueX;
    if (active) {
        int caret_width = prefix_width(renderer, font, text, edit->cursor);
        if (caret_width > clip.w - 4) x -= caret_width - clip.w + 4;
        const size_t start = edit->cursor < edit->anchor ? edit->cursor : edit->anchor;
        const size_t end = edit->cursor > edit->anchor ? edit->cursor : edit->anchor;
        SDL_Rect selection = {x + prefix_width(renderer, font, text, start), slider->valueY,
            prefix_width(renderer, font, text, end) - prefix_width(renderer, font, text, start), slider->valueRect.h - 4};
        SDL_SetRenderDrawColor(renderer, 45,85,125,255);
        SDL_RenderFillRect(renderer, &selection);
        if (((SDL_GetTicks() - edit->blinkEpoch) / 500) % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_RenderDrawLine(renderer, x + caret_width, slider->valueY, x + caret_width, slider->valueY + slider->valueRect.h - 5);
        }
    }
    menu_render_draw_text_color(renderer, font, x, slider->valueY, color, text);
    SDL_RenderSetClipRect(renderer, clipped ? &old_clip : NULL);
    for (int direction = 0; direction < 2; ++direction) {
        SDL_Rect rect = direction ? slider->decreaseRect : slider->increaseRect;
        int mx, my; SDL_GetMouseState(&mx, &my);
        SDL_Color arrow = contains(rect, mx, my) ? color :
            themed ? palette.text_muted : (SDL_Color){155,160,170,255};
        SDL_SetRenderDrawColor(renderer, arrow.r, arrow.g, arrow.b, 255);
        int cx = rect.x + rect.w / 2, cy = rect.y + rect.h / 2;
        int tip = direction ? 2 : -2;
        SDL_RenderDrawLine(renderer, cx - 3, cy - tip, cx, cy + tip);
        SDL_RenderDrawLine(renderer, cx, cy + tip, cx + 3, cy - tip);
    }
}
