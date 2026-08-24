#ifndef UI_MENU_SETTINGS_READBACK_H
#define UI_MENU_SETTINGS_READBACK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum MenuSettingsAppliedState {
    MENU_SETTINGS_APPLIED_STATE_EDIT_PENDING = 0,
    MENU_SETTINGS_APPLIED_STATE_RENDER_REFRESH_PENDING = 1,
    MENU_SETTINGS_APPLIED_STATE_APPLIED = 2
} MenuSettingsAppliedState;

typedef struct MenuSettingsRuntimeReadback {
    int configuredWidth;
    int configuredHeight;
    int configuredRayCount;
    int runtimeWidth;
    int runtimeHeight;
    int runtimeRayCount;
    bool configuredValuesMatchRuntime;
    bool preparedSceneCurrent;
    uint64_t runtimeGeneration;
    uint64_t cachedGeneration;
    MenuSettingsAppliedState appliedState;
    char summary[192];
} MenuSettingsRuntimeReadback;

void menu_settings_runtime_readback(MenuSettingsRuntimeReadback* out_readback);
const char* menu_settings_applied_state_label(MenuSettingsAppliedState state);

#endif
