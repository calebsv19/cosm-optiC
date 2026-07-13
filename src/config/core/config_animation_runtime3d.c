#include "config_animation_runtime3d.h"

static int ClampSecondaryDiffuseSamples3D(int value) {
    if (value < RUNTIME_3D_SECONDARY_SAMPLES_MIN) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MIN;
    }
    if (value > RUNTIME_3D_SECONDARY_SAMPLES_MAX) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MAX;
    }
    value = ((value + (RUNTIME_3D_SECONDARY_SAMPLES_STEP / 2)) /
             RUNTIME_3D_SECONDARY_SAMPLES_STEP) *
            RUNTIME_3D_SECONDARY_SAMPLES_STEP;
    if (value < RUNTIME_3D_SECONDARY_SAMPLES_MIN) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MIN;
    }
    if (value > RUNTIME_3D_SECONDARY_SAMPLES_MAX) {
        value = RUNTIME_3D_SECONDARY_SAMPLES_MAX;
    }
    return value;
}

static int ClampBounceDepth3D(int value) {
    if (value < RUNTIME_3D_BOUNCE_DEPTH_MIN) {
        value = RUNTIME_3D_BOUNCE_DEPTH_MIN;
    }
    if (value > RUNTIME_3D_BOUNCE_DEPTH_MAX) {
        value = RUNTIME_3D_BOUNCE_DEPTH_MAX;
    }
    return value;
}

static int ClampSpecularDepth3D(int value) {
    if (value < RUNTIME_3D_SPECULAR_DEPTH_MIN) {
        value = RUNTIME_3D_SPECULAR_DEPTH_MIN;
    }
    if (value > RUNTIME_3D_SPECULAR_DEPTH_MAX) {
        value = RUNTIME_3D_SPECULAR_DEPTH_MAX;
    }
    return value;
}

static int ClampTransmissionDepth3D(int value) {
    if (value < RUNTIME_3D_TRANSMISSION_DEPTH_MIN) {
        value = RUNTIME_3D_TRANSMISSION_DEPTH_MIN;
    }
    if (value > RUNTIME_3D_TRANSMISSION_DEPTH_MAX) {
        value = RUNTIME_3D_TRANSMISSION_DEPTH_MAX;
    }
    return value;
}

static double ClampRouletteThreshold3D(double value) {
    if (value < RUNTIME_3D_ROULETTE_THRESHOLD_MIN) {
        return RUNTIME_3D_ROULETTE_THRESHOLD_MIN;
    }
    if (value > RUNTIME_3D_ROULETTE_THRESHOLD_MAX) {
        return RUNTIME_3D_ROULETTE_THRESHOLD_MAX;
    }
    return value;
}

static int ClampTransmissionSamples3D(int value) {
    if (value < RUNTIME_3D_TRANSMISSION_SAMPLES_MIN) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MIN;
    }
    if (value > RUNTIME_3D_TRANSMISSION_SAMPLES_MAX) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MAX;
    }
    return value;
}

static int ClampTemporalFrames3D(int value) {
    if (value < RUNTIME_3D_TEMPORAL_FRAMES_MIN) {
        value = RUNTIME_3D_TEMPORAL_FRAMES_MIN;
    }
    if (value > RUNTIME_3D_TEMPORAL_FRAMES_MAX) {
        value = RUNTIME_3D_TEMPORAL_FRAMES_MAX;
    }
    return value;
}

static int ClampRenderScale3D(int value) {
    if (value == RUNTIME_3D_RENDER_SCALE_HIDPI) {
        return RUNTIME_3D_RENDER_SCALE_HIDPI;
    }
    if (value < 1) {
        value = RUNTIME_3D_RENDER_SCALE_DEFAULT;
    }
    if (value > RUNTIME_3D_RENDER_SCALE_MAX) {
        value = RUNTIME_3D_RENDER_SCALE_MAX;
    }
    return value;
}

static int ClampUpscaleMode3D(int value) {
    if (value < RUNTIME_3D_UPSCALE_MODE_MIN) {
        value = RUNTIME_3D_UPSCALE_MODE_MIN;
    }
    if (value > RUNTIME_3D_UPSCALE_MODE_MAX) {
        value = RUNTIME_3D_UPSCALE_MODE_MAX;
    }
    return value;
}

static int ClampInt(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

int animation_config_runtime_window_dimension_clamp(int value, int fallback) {
    if (value <= 0) {
        value = fallback;
    }
    if (value < 200) {
        value = 200;
    }
    if (value % 2 != 0) {
        value += 1;
    }
    return value;
}

void animation_config_normalize_runtime3d_fields(AnimationConfig* cfg) {
    if (!cfg) return;
    cfg->secondaryDiffuseSamples3D =
        ClampSecondaryDiffuseSamples3D(cfg->secondaryDiffuseSamples3D);
    cfg->bounceDepth3D =
        ClampBounceDepth3D(cfg->bounceDepth3D);
    cfg->specularDepth3D =
        ClampSpecularDepth3D(cfg->specularDepth3D);
    cfg->transmissionDepth3D =
        ClampTransmissionDepth3D(cfg->transmissionDepth3D);
    cfg->rouletteThreshold3D =
        ClampRouletteThreshold3D(cfg->rouletteThreshold3D);
    cfg->transmissionSamples3D =
        ClampTransmissionSamples3D(cfg->transmissionSamples3D);
    cfg->temporalFrames3D =
        ClampTemporalFrames3D(cfg->temporalFrames3D);
    cfg->renderScale3D =
        ClampRenderScale3D(cfg->renderScale3D);
    cfg->upscaleMode3D =
        ClampUpscaleMode3D(cfg->upscaleMode3D);
    cfg->causticMode3D = ClampInt(cfg->causticMode3D,
                                 RUNTIME_3D_CAUSTIC_MODE_MIN,
                                 RUNTIME_3D_CAUSTIC_MODE_MAX);
    cfg->causticTransportEngine3D = ClampInt(
        cfg->causticTransportEngine3D,
        RUNTIME_3D_CAUSTIC_ENGINE_MIN,
        RUNTIME_3D_CAUSTIC_ENGINE_MAX);
    cfg->causticSampleBudget3D = ClampInt(
        cfg->causticSampleBudget3D,
        0,
        RUNTIME_3D_CAUSTIC_SAMPLE_BUDGET_MAX);
    cfg->causticMaxPathDepth3D = ClampInt(
        cfg->causticMaxPathDepth3D,
        0,
        RUNTIME_3D_CAUSTIC_PATH_DEPTH_MAX);
    if (cfg->causticMode3D == RUNTIME_3D_CAUSTIC_MODE_DEFAULT) {
        cfg->causticSurfaceCacheEnabled3D = false;
        cfg->causticVolumeCacheEnabled3D = false;
    }
    cfg->menuWorkspaceModule = ClampInt(cfg->menuWorkspaceModule,
                                        MENU_WORKSPACE_MODULE_DEFAULT,
                                        MENU_WORKSPACE_MODULE_MAX);
    cfg->menuPaneSceneWidth = ClampInt(cfg->menuPaneSceneWidth, 350, 470);
    cfg->menuPaneHealthWidth = ClampInt(cfg->menuPaneHealthWidth, 250, 480);
}

void ApplyAnimationWindowSizeOverride(void) {
    if (animation_config_scene_source_is_fluid(animSettings.sceneSource)) {
        return;
    }
    if (animSettings.runtimeWindowWidth <= 0 || animSettings.runtimeWindowHeight <= 0) {
        return;
    }
    sceneSettings.windowWidth =
        animation_config_runtime_window_dimension_clamp(animSettings.runtimeWindowWidth,
                                                        sceneSettings.windowWidth);
    sceneSettings.windowHeight =
        animation_config_runtime_window_dimension_clamp(animSettings.runtimeWindowHeight,
                                                        sceneSettings.windowHeight);
}
