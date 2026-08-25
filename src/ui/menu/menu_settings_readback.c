#include "ui/menu_settings_readback.h"

#include <stdio.h>
#include <string.h>

#include "config/config_manager.h"
#include "render/runtime_native_3d_prepare_cache.h"

const char* menu_settings_applied_state_label(MenuSettingsAppliedState state) {
    switch (state) {
        case MENU_SETTINGS_APPLIED_STATE_APPLIED:
            return "applied";
        case MENU_SETTINGS_APPLIED_STATE_RENDER_REFRESH_PENDING:
            return "render refresh pending";
        case MENU_SETTINGS_APPLIED_STATE_EDIT_PENDING:
        default:
            return "edit pending";
    }
}

void menu_settings_runtime_readback(MenuSettingsRuntimeReadback* out_readback) {
    RuntimeNative3DPreparedSceneCacheStats cache = {0};
    if (!out_readback) return;
    memset(out_readback, 0, sizeof(*out_readback));

    out_readback->configuredWidth = animSettings.runtimeWindowWidth;
    out_readback->configuredHeight = animSettings.runtimeWindowHeight;
    out_readback->configuredRayCount = animSettings.runtimeRayCount;
    out_readback->runtimeWidth = sceneSettings.windowWidth;
    out_readback->runtimeHeight = sceneSettings.windowHeight;
    out_readback->runtimeRayCount = sceneSettings.rays;
    out_readback->configuredValuesMatchRuntime =
        out_readback->configuredWidth == out_readback->runtimeWidth &&
        out_readback->configuredHeight == out_readback->runtimeHeight &&
        out_readback->configuredRayCount == out_readback->runtimeRayCount;

    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&cache);
    out_readback->runtimeGeneration = cache.generation;
    out_readback->cachedGeneration = cache.cachedGeneration;
    out_readback->preparedSceneCurrent =
        cache.valid && cache.cachedGeneration == cache.generation;
    if (!out_readback->configuredValuesMatchRuntime) {
        out_readback->appliedState = MENU_SETTINGS_APPLIED_STATE_EDIT_PENDING;
    } else if (!out_readback->preparedSceneCurrent) {
        out_readback->appliedState =
            MENU_SETTINGS_APPLIED_STATE_RENDER_REFRESH_PENDING;
    } else {
        out_readback->appliedState = MENU_SETTINGS_APPLIED_STATE_APPLIED;
    }

    snprintf(out_readback->summary,
             sizeof(out_readback->summary),
             "Config %dx%d %d rays | Runtime %dx%d %d | %s",
             out_readback->configuredWidth,
             out_readback->configuredHeight,
             out_readback->configuredRayCount,
             out_readback->runtimeWidth,
             out_readback->runtimeHeight,
             out_readback->runtimeRayCount,
             menu_settings_applied_state_label(out_readback->appliedState));
    out_readback->summary[sizeof(out_readback->summary) - 1] = '\0';
}
