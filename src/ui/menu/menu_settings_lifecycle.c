#include "ui/menu_settings_lifecycle.h"
#include <stdio.h>

#include "config/config_manager.h"
#include "render/runtime_native_3d_prepare_cache.h"

static bool menu_settings_lifecycle_target_invalidates_during_set(
    const MenuRuntimeState* state,
    const int* target) {
    if (!state || !target) return false;
    return target == &state->envSliderValue ||
           target == &state->topFillStrengthSliderValue ||
           target == &state->environmentBackgroundBrightnessSliderValue ||
           target == &state->environmentBackgroundRedSliderValue ||
           target == &state->environmentBackgroundGreenSliderValue ||
           target == &state->environmentBackgroundBlueSliderValue;
}

void menu_settings_lifecycle_commit(MenuRuntimeState* state,
                                    const char* reason,
                                    bool render_state_changed) {
    if (!state) return;
    menu_state_apply_effective_render_recipe(state);
    if (render_state_changed) {
        RuntimeNative3DPreparedSceneMarkDirty(
            (reason && reason[0]) ? reason : "menu_top_level_setting");
    }
    if (!SaveAnimationConfigChecked()) {
        snprintf(state->statusLabel, sizeof(state->statusLabel), "Save failed; changes remain in memory. Retry Save.");
        state->statusColor = (SDL_Color){255, 110, 90, 255};
        state->statusExpireMs = SDL_GetTicks() + 8000;
    }
}

void menu_settings_lifecycle_commit_slider_release(MenuRuntimeState* state,
                                                   int* target) {
    if (!state || !target) return;
    menu_settings_lifecycle_commit(
        state,
        "menu_persistent_slider_release",
        !menu_settings_lifecycle_target_invalidates_during_set(state, target));
}
