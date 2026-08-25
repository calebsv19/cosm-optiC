#include "ui/menu_environment_settings.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config/config_manager.h"
#include "render/runtime_native_3d_prepare_cache.h"
#include "render/runtime_scene_3d.h"

static double menu_environment_clamp(double value, double minimum, double maximum) {
    if (!isfinite(value)) return minimum;
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static bool menu_environment_changed_double(double current, double requested) {
    return !isfinite(current) || fabs(current - requested) > 1e-12;
}

static bool menu_environment_commit_change(bool changed, const char* reason) {
    if (!changed) return false;
    RuntimeNative3DPreparedSceneMarkDirty(reason);
    return true;
}

static const char* menu_environment_mode_label(int mode) {
    switch ((EnvironmentLightMode)mode) {
        case ENVIRONMENT_LIGHT_MODE_TOP_FILL:
            return "Top Fill";
        case ENVIRONMENT_LIGHT_MODE_AMBIENT:
            return "Ambient";
        case ENVIRONMENT_LIGHT_MODE_OFF:
        default:
            return "Off";
    }
}

bool menu_environment_settings_set_light_mode(int mode) {
    const int clamped = animation_config_environment_light_mode_clamp(mode);
    const bool changed = animSettings.environmentLightMode != clamped;
    animSettings.environmentLightMode = (EnvironmentLightMode)clamped;
    return menu_environment_commit_change(changed, "menu_environment_light_mode");
}

bool menu_environment_settings_set_ambient_brightness(double byte_brightness) {
    const double clamped = menu_environment_clamp(byte_brightness, 0.0, 255.0);
    const bool changed =
        menu_environment_changed_double(animSettings.environmentBrightness, clamped);
    animSettings.environmentBrightness = clamped;
    return menu_environment_commit_change(changed, "menu_environment_ambient_brightness");
}

bool menu_environment_settings_set_top_fill_strength(double strength) {
    const double clamped = menu_environment_clamp(strength, 0.0, 20.0);
    const bool changed =
        menu_environment_changed_double(animSettings.topFillStrength, clamped);
    animSettings.topFillStrength = clamped;
    return menu_environment_commit_change(changed, "menu_environment_top_fill_strength");
}

bool menu_environment_settings_set_preset(int preset) {
    const int clamped = animation_config_environment_preset_clamp(preset);
    const bool changed = animSettings.environmentPreset != clamped ||
                         !animSettings.environmentBackgroundLightingAuthored;
    animSettings.environmentPreset = (EnvironmentPreset)clamped;
    animSettings.environmentBackgroundLightingAuthored = true;
    return menu_environment_commit_change(changed, "menu_environment_preset");
}

bool menu_environment_settings_set_background_auto(bool automatic,
                                                   double manual_brightness) {
    bool changed = !animSettings.environmentBackgroundLightingAuthored ||
                   animSettings.environmentBackgroundBrightnessAuto != automatic;
    (void)manual_brightness;
    animSettings.environmentBackgroundLightingAuthored = true;
    animSettings.environmentBackgroundBrightnessAuto = automatic;
    return menu_environment_commit_change(changed, "menu_environment_background_mode");
}

bool menu_environment_settings_set_background_brightness(double brightness) {
    const double clamped = menu_environment_clamp(brightness, 0.0, 4.0);
    const bool changed = !animSettings.environmentBackgroundLightingAuthored ||
                         animSettings.environmentBackgroundBrightnessAuto ||
                         menu_environment_changed_double(
                             animSettings.environmentBackgroundBrightness,
                             clamped);
    animSettings.environmentBackgroundLightingAuthored = true;
    animSettings.environmentBackgroundBrightnessAuto = false;
    animSettings.environmentBackgroundBrightness = clamped;
    return menu_environment_commit_change(changed, "menu_environment_background_brightness");
}

bool menu_environment_settings_set_background_color(double red,
                                                    double green,
                                                    double blue) {
    const double clamped_red = menu_environment_clamp(red, 0.0, 1.0);
    const double clamped_green = menu_environment_clamp(green, 0.0, 1.0);
    const double clamped_blue = menu_environment_clamp(blue, 0.0, 1.0);
    const bool changed = !animSettings.environmentBackgroundLightingAuthored ||
                         menu_environment_changed_double(
                             animSettings.environmentBackgroundColorR,
                             clamped_red) ||
                         menu_environment_changed_double(
                             animSettings.environmentBackgroundColorG,
                             clamped_green) ||
                         menu_environment_changed_double(
                             animSettings.environmentBackgroundColorB,
                             clamped_blue);
    animSettings.environmentBackgroundLightingAuthored = true;
    animSettings.environmentBackgroundColorR = clamped_red;
    animSettings.environmentBackgroundColorG = clamped_green;
    animSettings.environmentBackgroundColorB = clamped_blue;
    return menu_environment_commit_change(changed, "menu_environment_background_color");
}

void menu_environment_settings_mark_runtime_dirty(const char* reason) {
    RuntimeNative3DPreparedSceneMarkDirty(
        (reason && reason[0]) ? reason : "menu_environment_settings");
}

void menu_environment_settings_readback(MenuEnvironmentRuntimeReadback* out_readback) {
    RuntimeEnvironment3D environment = {0};
    RuntimeNative3DPreparedSceneCacheStats cache = {0};
    const char* preset_label = NULL;
    const char* background_source = NULL;
    const char* runtime_state = NULL;

    if (!out_readback) return;
    memset(out_readback, 0, sizeof(*out_readback));
    RuntimeEnvironment3D_ResolveFromAnimationConfig(&environment, &animSettings);
    RuntimeNative3DPreparedSceneCacheStatsSnapshot(&cache);

    out_readback->lightMode = environment.lightMode;
    out_readback->preset = environment.preset;
    out_readback->ambientStrength =
        RuntimeEnvironment3D_AmbientStrength(&environment);
    out_readback->topFillStrength =
        environment.lightMode == ENVIRONMENT_LIGHT_MODE_TOP_FILL
            ? environment.topFillIntensity
            : 0.0;
    out_readback->backgroundBrightness =
        RuntimeEnvironment3D_BackgroundBrightness(&environment);
    out_readback->backgroundColorR = environment.backgroundColor.x;
    out_readback->backgroundColorG = environment.backgroundColor.y;
    out_readback->backgroundColorB = environment.backgroundColor.z;
    if (environment.backgroundIntensityDerivedFromAmbient) {
        const double gray = menu_environment_clamp(
            out_readback->backgroundBrightness, 0.0, 1.0);
        out_readback->backgroundPreviewColorR = gray;
        out_readback->backgroundPreviewColorG = gray;
        out_readback->backgroundPreviewColorB = gray;
    } else {
        const double preview_strength = menu_environment_clamp(
            out_readback->backgroundBrightness, 0.0, 1.0);
        out_readback->backgroundPreviewColorR = menu_environment_clamp(
            0.5 * (environment.backgroundTopColor.x +
                   environment.backgroundBottomColor.x) * preview_strength,
            0.0,
            1.0);
        out_readback->backgroundPreviewColorG = menu_environment_clamp(
            0.5 * (environment.backgroundTopColor.y +
                   environment.backgroundBottomColor.y) * preview_strength,
            0.0,
            1.0);
        out_readback->backgroundPreviewColorB = menu_environment_clamp(
            0.5 * (environment.backgroundTopColor.z +
                   environment.backgroundBottomColor.z) * preview_strength,
            0.0,
            1.0);
    }
    out_readback->backgroundBrightnessDerivedFromAmbient =
        environment.backgroundIntensityDerivedFromAmbient;
    out_readback->runtimeGeneration = cache.generation;
    out_readback->cachedGeneration = cache.cachedGeneration;
    out_readback->runtimeApplied =
        cache.valid && cache.cachedGeneration == cache.generation;

    preset_label = RuntimeEnvironment3DPresetLabel(environment.preset);
    background_source = environment.backgroundIntensityDerivedFromAmbient
                            ? "auto"
                            : "manual";
    runtime_state = out_readback->runtimeApplied ? "applied" : "refresh pending";
    snprintf(out_readback->summary,
             sizeof(out_readback->summary),
             "%s | Ambient %.2f white | BG %s %.2f %s rgb %.2f/%.2f/%.2f | %s",
             menu_environment_mode_label(environment.lightMode),
             out_readback->ambientStrength,
             preset_label,
             out_readback->backgroundBrightness,
             background_source,
             out_readback->backgroundColorR,
             out_readback->backgroundColorG,
             out_readback->backgroundColorB,
             runtime_state);
    out_readback->summary[sizeof(out_readback->summary) - 1] = '\0';
}
