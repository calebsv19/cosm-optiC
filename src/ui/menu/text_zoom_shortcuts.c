#include "ui/text_zoom_shortcuts.h"

#include "config/config_manager.h"
#include "engine/Render/render_font.h"

static int text_zoom_step_delta_for_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_EQUALS:
        case SDLK_PLUS:
        case SDLK_KP_PLUS:
            return 1;
        case SDLK_MINUS:
        case SDLK_UNDERSCORE:
        case SDLK_KP_MINUS:
            return -1;
        default:
            return 0;
    }
}

bool ray_tracing_text_zoom_apply_shortcut(SDL_Keycode key,
                                          SDL_Keymod mod,
                                          bool* out_changed,
                                          int* out_step,
                                          int* out_percent) {
    bool ctrl_or_cmd = (mod & (KMOD_CTRL | KMOD_GUI)) != 0;
    int previous_step;
    int delta;
    int next_step;
    bool changed;

    if (out_changed) *out_changed = false;
    if (out_step) *out_step = animSettings.textZoomStep;
    if (out_percent) *out_percent = animation_config_text_zoom_percent_from_step(animSettings.textZoomStep);

    if (!ctrl_or_cmd) {
        return false;
    }

    previous_step = animation_config_text_zoom_step_clamp(animSettings.textZoomStep);
    if (key == SDLK_0 || key == SDLK_KP_0) {
        next_step = 0;
    } else {
        delta = text_zoom_step_delta_for_key(key);
        if (delta == 0) {
            return false;
        }
        next_step = animation_config_text_zoom_step_clamp(previous_step + delta);
    }

    changed = next_step != previous_step;
    animSettings.textZoomStep = next_step;
    if (changed) {
        SaveAnimationConfig();
    }

    (void)refreshActiveFontFromAnimationConfig();

    if (out_changed) *out_changed = changed;
    if (out_step) *out_step = animSettings.textZoomStep;
    if (out_percent) *out_percent = animation_config_text_zoom_percent_from_step(animSettings.textZoomStep);
    return true;
}
