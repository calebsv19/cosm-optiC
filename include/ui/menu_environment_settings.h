#ifndef UI_MENU_ENVIRONMENT_SETTINGS_H
#define UI_MENU_ENVIRONMENT_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct MenuEnvironmentRuntimeReadback {
    int lightMode;
    int preset;
    double ambientStrength;
    double topFillStrength;
    double backgroundBrightness;
    double backgroundColorR;
    double backgroundColorG;
    double backgroundColorB;
    double backgroundPreviewColorR;
    double backgroundPreviewColorG;
    double backgroundPreviewColorB;
    bool backgroundBrightnessDerivedFromAmbient;
    bool runtimeApplied;
    uint64_t runtimeGeneration;
    uint64_t cachedGeneration;
    char summary[192];
} MenuEnvironmentRuntimeReadback;

bool menu_environment_settings_set_light_mode(int mode);
bool menu_environment_settings_set_ambient_brightness(double byte_brightness);
bool menu_environment_settings_set_top_fill_strength(double strength);
bool menu_environment_settings_set_preset(int preset);
bool menu_environment_settings_set_background_auto(bool automatic,
                                                   double manual_brightness);
bool menu_environment_settings_set_background_brightness(double brightness);
bool menu_environment_settings_set_background_color(double red,
                                                    double green,
                                                    double blue);
void menu_environment_settings_mark_runtime_dirty(const char* reason);
void menu_environment_settings_readback(MenuEnvironmentRuntimeReadback* out_readback);

#endif
