#include "app/animation_input_helpers.h"

#include "render/fluid/fluid_state.h"
#include "ui/text_zoom_shortcuts.h"

#include <stdio.h>

void animation_handle_fluid_overlay_key(SDL_Keycode key) {
    if (key == SDLK_f) {
        g_fluidOverlayEnabled = !g_fluidOverlayEnabled;
        printf("[fluid] overlay %s\n", g_fluidOverlayEnabled ? "enabled" : "disabled");
    } else if (key == SDLK_LEFTBRACKET) {
        if (g_fluidFrameIndex > 0) g_fluidFrameIndex--;
        printf("[fluid] frame %d\n", g_fluidFrameIndex);
    } else if (key == SDLK_RIGHTBRACKET) {
        g_fluidFrameIndex++;
        printf("[fluid] frame %d\n", g_fluidFrameIndex);
    } else if (key == SDLK_v) {
        if (g_fluidOverlayMode == FLUID_OVERLAY_MODE_DENSITY) {
            g_fluidOverlayMode = FLUID_OVERLAY_MODE_DENSITY_VELOCITY;
        } else if (g_fluidOverlayMode == FLUID_OVERLAY_MODE_DENSITY_VELOCITY) {
            g_fluidOverlayMode = FLUID_OVERLAY_MODE_VELOCITY_HEATMAP;
        } else {
            g_fluidOverlayMode = FLUID_OVERLAY_MODE_DENSITY;
        }
        printf("[fluid] overlay mode %s\n",
               g_fluidOverlayMode == FLUID_OVERLAY_MODE_DENSITY
                   ? "density"
                   : (g_fluidOverlayMode == FLUID_OVERLAY_MODE_DENSITY_VELOCITY
                          ? "density+velocity"
                          : "velocity-heatmap+velocity"));
    }
}

bool animation_handle_text_zoom_shortcut(const SDL_KeyboardEvent* key_event) {
    bool changed = false;
    int zoom_step = 0;
    int zoom_percent = 100;
    if (!key_event) return false;
    if (!ray_tracing_text_zoom_apply_shortcut(key_event->keysym.sym,
                                              key_event->keysym.mod,
                                              &changed,
                                              &zoom_step,
                                              &zoom_percent)) {
        return false;
    }
    printf("[font] text zoom %d%% step=%d%s\n",
           zoom_percent,
           zoom_step,
           changed ? "" : " (clamped)");
    return true;
}
