#include "ui/scene_source_ui_labels.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "config/config_manager.h"
#include "import/runtime_scene_volume_defaults.h"
#include "ui/sdl_menu_state.h"

typedef enum RuntimeSceneVolumeUiState {
    RUNTIME_SCENE_VOLUME_UI_RUNTIME_ONLY = 0,
    RUNTIME_SCENE_VOLUME_UI_AUTO_AVAILABLE,
    RUNTIME_SCENE_VOLUME_UI_AUTO_DISABLED,
    RUNTIME_SCENE_VOLUME_UI_CUSTOM_ENABLED,
    RUNTIME_SCENE_VOLUME_UI_CUSTOM_DISABLED
} RuntimeSceneVolumeUiState;

static bool scene_source_ui_runtime_scene_is_selected(const char* runtime_scene_path) {
    return runtime_scene_path && runtime_scene_path[0] &&
           animation_config_scene_source_clamp(animSettings.sceneSource) ==
               SCENE_SOURCE_RUNTIME_SCENE &&
           strcmp(animSettings.runtimeScenePath, runtime_scene_path) == 0;
}

static bool scene_source_ui_volume_matches(int expected_kind, const char* expected_path) {
    const int current_kind = animation_config_volume_source_kind_clamp(animSettings.volumeSourceKind);
    if (current_kind != animation_config_volume_source_kind_clamp(expected_kind)) {
        return false;
    }
    if (!expected_path || !expected_path[0] || animSettings.volumeSourcePath[0] == '\0') {
        return false;
    }
    return strcmp(animSettings.volumeSourcePath, expected_path) == 0;
}

static RuntimeSceneVolumeUiState scene_source_ui_resolve_runtime_volume_state(
    const char* runtime_scene_path) {
    int default_kind = VOLUME_SOURCE_NONE;
    char default_path[PATH_MAX] = {0};
    const bool has_default =
        runtime_scene_volume_defaults_resolve(runtime_scene_path,
                                              &default_kind,
                                              default_path,
                                              sizeof(default_path));
    const int current_kind = animation_config_volume_source_kind_clamp(animSettings.volumeSourceKind);
    const bool current_has_source =
        current_kind != VOLUME_SOURCE_NONE && animSettings.volumeSourcePath[0] != '\0';
    const bool selected = scene_source_ui_runtime_scene_is_selected(runtime_scene_path);

    if (!selected) {
        return has_default ? RUNTIME_SCENE_VOLUME_UI_AUTO_AVAILABLE
                           : RUNTIME_SCENE_VOLUME_UI_RUNTIME_ONLY;
    }

    if (!current_has_source) {
        return RUNTIME_SCENE_VOLUME_UI_RUNTIME_ONLY;
    }

    if (has_default && scene_source_ui_volume_matches(default_kind, default_path)) {
        return animSettings.volumeInteractionEnabled
                   ? RUNTIME_SCENE_VOLUME_UI_AUTO_AVAILABLE
                   : RUNTIME_SCENE_VOLUME_UI_AUTO_DISABLED;
    }

    return animSettings.volumeInteractionEnabled
               ? RUNTIME_SCENE_VOLUME_UI_CUSTOM_ENABLED
               : RUNTIME_SCENE_VOLUME_UI_CUSTOM_DISABLED;
}

static const char* scene_source_ui_runtime_badge(const char* runtime_scene_path) {
    switch (scene_source_ui_resolve_runtime_volume_state(runtime_scene_path)) {
        case RUNTIME_SCENE_VOLUME_UI_AUTO_AVAILABLE:
            return " + atmosphere";
        case RUNTIME_SCENE_VOLUME_UI_AUTO_DISABLED:
            return " + atmosphere off";
        case RUNTIME_SCENE_VOLUME_UI_CUSTOM_ENABLED:
            return " + custom volume";
        case RUNTIME_SCENE_VOLUME_UI_CUSTOM_DISABLED:
            return " + custom volume off";
        case RUNTIME_SCENE_VOLUME_UI_RUNTIME_ONLY:
        default:
            return "";
    }
}

static const char* scene_source_ui_runtime_button_badge(const char* runtime_scene_path) {
    switch (scene_source_ui_resolve_runtime_volume_state(runtime_scene_path)) {
        case RUNTIME_SCENE_VOLUME_UI_AUTO_AVAILABLE:
            return "Runtime + Atmosphere";
        case RUNTIME_SCENE_VOLUME_UI_AUTO_DISABLED:
            return "Runtime + Atmosphere Off";
        case RUNTIME_SCENE_VOLUME_UI_CUSTOM_ENABLED:
            return "Runtime + Custom Volume";
        case RUNTIME_SCENE_VOLUME_UI_CUSTOM_DISABLED:
            return "Runtime + Custom Volume Off";
        case RUNTIME_SCENE_VOLUME_UI_RUNTIME_ONLY:
        default:
            return "Runtime";
    }
}

void scene_source_ui_format_catalog_option_label(const char* path,
                                                 int source,
                                                 char* out,
                                                 size_t out_size) {
    char label[128] = {0};
    const int clamped_source = animation_config_scene_source_clamp(source);

    if (!out || out_size == 0u) return;
    out[0] = '\0';

    if (clamped_source == SCENE_SOURCE_CONFIG_2D) {
        snprintf(out, out_size, "2D config");
        return;
    }

    menu_state_build_manifest_label(path, label, sizeof(label));
    if (clamped_source == SCENE_SOURCE_RUNTIME_SCENE) {
        snprintf(out,
                 out_size,
                 "runtime: %s%s",
                 label,
                 scene_source_ui_runtime_badge(path));
        return;
    }

    snprintf(out, out_size, "fluid: %s", label);
}

void scene_source_ui_format_active_button_label(char* out, size_t out_size) {
    char label[128] = {0};
    const char* base = "Load Scene";
    const int source = animation_config_scene_source_clamp(animSettings.sceneSource);

    if (!out || out_size == 0u) return;
    out[0] = '\0';

    if (source == SCENE_SOURCE_CONFIG_2D) {
        snprintf(out, out_size, "%s [2D config]", base);
        return;
    }

    if (source == SCENE_SOURCE_RUNTIME_SCENE) {
        if (!animSettings.runtimeScenePath[0]) {
            snprintf(out, out_size, "%s [Runtime]", base);
            return;
        }
        menu_state_build_manifest_label(animSettings.runtimeScenePath, label, sizeof(label));
        snprintf(out,
                 out_size,
                 "%s [%s]: %s",
                 base,
                 scene_source_ui_runtime_button_badge(animSettings.runtimeScenePath),
                 label);
        return;
    }

    if (!animSettings.fluidManifest[0]) {
        snprintf(out, out_size, "%s [Fluid]", base);
        return;
    }
    menu_state_build_manifest_label(animSettings.fluidManifest, label, sizeof(label));
    snprintf(out, out_size, "%s [Fluid]: %s", base, label);
}

void scene_source_ui_format_scene_select_status(int source,
                                                const char* path,
                                                char* out,
                                                size_t out_size) {
    const int clamped_source = animation_config_scene_source_clamp(source);

    if (!out || out_size == 0u) return;
    out[0] = '\0';

    if (clamped_source == SCENE_SOURCE_CONFIG_2D) {
        snprintf(out, out_size, "2D config active");
        return;
    }

    if (clamped_source == SCENE_SOURCE_FLUID_MANIFEST) {
        snprintf(out, out_size, "Fluid scene set");
        return;
    }

    switch (scene_source_ui_resolve_runtime_volume_state(path)) {
        case RUNTIME_SCENE_VOLUME_UI_AUTO_AVAILABLE:
            snprintf(out, out_size, "Runtime scene + atmosphere");
            break;
        case RUNTIME_SCENE_VOLUME_UI_AUTO_DISABLED:
            snprintf(out, out_size, "Runtime scene; atmosphere off");
            break;
        case RUNTIME_SCENE_VOLUME_UI_CUSTOM_ENABLED:
            snprintf(out, out_size, "Runtime scene + custom volume");
            break;
        case RUNTIME_SCENE_VOLUME_UI_CUSTOM_DISABLED:
            snprintf(out, out_size, "Runtime scene; custom volume off");
            break;
        case RUNTIME_SCENE_VOLUME_UI_RUNTIME_ONLY:
        default:
            snprintf(out, out_size, "Runtime scene set");
            break;
    }
}
