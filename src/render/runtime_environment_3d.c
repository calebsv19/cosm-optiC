#include "render/runtime_scene_3d.h"

#include <math.h>
#include <string.h>

static double runtime_environment_3d_clamp(double value, double min_value, double max_value) {
    if (!isfinite(value)) return min_value;
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static Vec3 runtime_environment_3d_color_clamped(double r, double g, double b) {
    return vec3(runtime_environment_3d_clamp(r, 0.0, 1.0),
                runtime_environment_3d_clamp(g, 0.0, 1.0),
                runtime_environment_3d_clamp(b, 0.0, 1.0));
}

static Vec3 runtime_environment_3d_color_mul(Vec3 a, Vec3 b) {
    return vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

static EnvironmentLightMode runtime_environment_3d_light_mode_clamp(int mode) {
    if (mode < ENVIRONMENT_LIGHT_MODE_OFF) return ENVIRONMENT_LIGHT_MODE_OFF;
    if (mode > ENVIRONMENT_LIGHT_MODE_AMBIENT) return ENVIRONMENT_LIGHT_MODE_OFF;
    return (EnvironmentLightMode)mode;
}

static EnvironmentPreset runtime_environment_3d_preset_clamp(int preset) {
    if (preset < ENVIRONMENT_PRESET_NEUTRAL) return ENVIRONMENT_PRESET_SKY;
    if (preset > ENVIRONMENT_PRESET_WARM_SKY) return ENVIRONMENT_PRESET_SKY;
    return (EnvironmentPreset)preset;
}

const char* RuntimeEnvironment3DPresetLabel(EnvironmentPreset preset) {
    switch (runtime_environment_3d_preset_clamp(preset)) {
        case ENVIRONMENT_PRESET_NEUTRAL:
            return "neutral";
        case ENVIRONMENT_PRESET_WARM_SKY:
            return "warm_sky";
        case ENVIRONMENT_PRESET_SKY:
        default:
            return "sky";
    }
}

EnvironmentPreset RuntimeEnvironment3DPresetFromLabel(const char* label) {
    if (!label || !label[0]) return ENVIRONMENT_PRESET_SKY;
    if (strcmp(label, "neutral") == 0 || strcmp(label, "white") == 0) {
        return ENVIRONMENT_PRESET_NEUTRAL;
    }
    if (strcmp(label, "warm_sky") == 0 || strcmp(label, "warm-sky") == 0 ||
        strcmp(label, "warm") == 0) {
        return ENVIRONMENT_PRESET_WARM_SKY;
    }
    if (strcmp(label, "sky") == 0 || strcmp(label, "sky_tinted") == 0 ||
        strcmp(label, "sky-tinted") == 0) {
        return ENVIRONMENT_PRESET_SKY;
    }
    return ENVIRONMENT_PRESET_SKY;
}

void RuntimeEnvironment3D_ApplyPreset(RuntimeEnvironment3D* environment) {
    Vec3 top = vec3(1.0, 1.0, 1.0);
    Vec3 bottom = vec3(1.0, 1.0, 1.0);
    Vec3 tint = vec3(1.0, 1.0, 1.0);
    if (!environment) return;

    environment->preset = runtime_environment_3d_preset_clamp(environment->preset);
    tint = runtime_environment_3d_color_clamped(environment->backgroundColor.x,
                                               environment->backgroundColor.y,
                                               environment->backgroundColor.z);
    environment->backgroundColor = tint;

    switch (environment->preset) {
        case ENVIRONMENT_PRESET_NEUTRAL:
            top = vec3(1.0, 1.0, 1.0);
            bottom = vec3(1.0, 1.0, 1.0);
            break;
        case ENVIRONMENT_PRESET_WARM_SKY:
            top = vec3(0.90, 0.78, 0.62);
            bottom = vec3(1.00, 0.90, 0.74);
            break;
        case ENVIRONMENT_PRESET_SKY:
        default:
            top = vec3(0.66, 0.70, 0.76);
            bottom = vec3(0.84, 0.85, 0.87);
            break;
    }

    environment->backgroundTopColor = runtime_environment_3d_color_mul(top, tint);
    environment->backgroundBottomColor = runtime_environment_3d_color_mul(bottom, tint);
}

void RuntimeEnvironment3D_Init(RuntimeEnvironment3D* environment) {
    if (!environment) return;
    memset(environment, 0, sizeof(*environment));
    environment->lightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    environment->preset = ENVIRONMENT_PRESET_SKY;
    environment->ambientIntensity = 0.0;
    environment->backgroundIntensity = 0.0;
    environment->backgroundIntensityDerivedFromAmbient = true;
    environment->topFillIntensity = 1.0;
    environment->ambientColor = vec3(1.0, 1.0, 1.0);
    environment->backgroundColor = vec3(1.0, 1.0, 1.0);
    environment->topDownBias = 0.18;
    RuntimeEnvironment3D_ApplyPreset(environment);
}

void RuntimeEnvironment3D_ResolveFromAnimationConfig(RuntimeEnvironment3D* environment,
                                                     const AnimationConfig* config) {
    double ambient_strength = 0.0;
    bool background_authored = false;
    if (!environment || !config) return;

    RuntimeEnvironment3D_Init(environment);
    background_authored = config->environmentBackgroundLightingAuthored;
    environment->lightMode =
        runtime_environment_3d_light_mode_clamp(config->environmentLightMode);
    ambient_strength =
        runtime_environment_3d_clamp(config->environmentBrightness / 255.0, 0.0, 1.0);
    environment->ambientIntensity = ambient_strength;
    environment->backgroundIntensityDerivedFromAmbient =
        !background_authored || config->environmentBackgroundBrightnessAuto;
    environment->backgroundIntensity =
        environment->backgroundIntensityDerivedFromAmbient
            ? ambient_strength
            : runtime_environment_3d_clamp(config->environmentBackgroundBrightness, 0.0, 4.0);
    environment->topFillIntensity =
        runtime_environment_3d_clamp(config->topFillStrength, 0.0, 20.0);
    environment->ambientColor = vec3(1.0, 1.0, 1.0);
    environment->backgroundColor =
        background_authored
            ? runtime_environment_3d_color_clamped(config->environmentBackgroundColorR,
                                                   config->environmentBackgroundColorG,
                                                   config->environmentBackgroundColorB)
            : vec3(1.0, 1.0, 1.0);
    environment->preset =
        background_authored
            ? runtime_environment_3d_preset_clamp(config->environmentPreset)
            : ENVIRONMENT_PRESET_SKY;
    environment->topDownBias = 0.18;
    RuntimeEnvironment3D_ApplyPreset(environment);
}

double RuntimeEnvironment3D_AmbientStrength(const RuntimeEnvironment3D* environment) {
    if (!environment || environment->lightMode != ENVIRONMENT_LIGHT_MODE_AMBIENT) return 0.0;
    return runtime_environment_3d_clamp(environment->ambientIntensity, 0.0, 1.0);
}

double RuntimeEnvironment3D_BackgroundBrightness(const RuntimeEnvironment3D* environment) {
    if (!environment || environment->lightMode != ENVIRONMENT_LIGHT_MODE_AMBIENT) return 0.0;
    return runtime_environment_3d_clamp(environment->backgroundIntensity, 0.0, 4.0);
}
