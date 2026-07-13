#include "ui/sdl_menu_state.h"

#include "ui/menu_batch_panel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "app/data_paths.h"
#include "camera/camera.h"
#include "config/config_manager.h"
#include "editor/editor_mode_router.h"
#include "engine/Render/render_pipeline.h"
#include "render/font_runtime.h"
#include "import/fluid_volume_import_3d.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_volume_3d_debug.h"
#include "ui/scene_source_catalog.h"
#include "ui/scene_source_ui_labels.h"
#include "ui/volume_source_catalog.h"
#include "ui/volume_source_ui_labels.h"

#define TOGGLE_BUTTON_TEXT_SIZE 14
static const int kMenuMinFontPointSize = 6;

static int clamp_tile_size_menu(int value) {
    if (value < 4) value = 4;
    if (value % 4 != 0) {
        value += 4 - (value % 4);
    }
    return value;
}

static double clamp_double(double value, double min_v, double max_v) {
    if (value < min_v) return min_v;
    if (value > max_v) return max_v;
    return value;
}

static int clamp_secondary_diffuse_samples_3d_menu(int value) {
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

static int clamp_bounce_depth_3d_menu(int value) {
    if (value < RUNTIME_3D_BOUNCE_DEPTH_MIN) {
        value = RUNTIME_3D_BOUNCE_DEPTH_MIN;
    }
    if (value > RUNTIME_3D_BOUNCE_DEPTH_MAX) {
        value = RUNTIME_3D_BOUNCE_DEPTH_MAX;
    }
    return value;
}

static int clamp_roulette_threshold_3d_menu(int value) {
    if (value < 0) {
        value = 0;
    }
    if (value > 100) {
        value = 100;
    }
    return value;
}

static int clamp_transmission_samples_3d_menu(int value) {
    if (value < RUNTIME_3D_TRANSMISSION_SAMPLES_MIN) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MIN;
    }
    if (value > RUNTIME_3D_TRANSMISSION_SAMPLES_MAX) {
        value = RUNTIME_3D_TRANSMISSION_SAMPLES_MAX;
    }
    return value;
}

static int clamp_temporal_frames_3d_menu(int value) {
    if (value < RUNTIME_3D_TEMPORAL_FRAMES_MIN) {
        value = RUNTIME_3D_TEMPORAL_FRAMES_MIN;
    }
    if (value > RUNTIME_3D_TEMPORAL_FRAMES_MAX) {
        value = RUNTIME_3D_TEMPORAL_FRAMES_MAX;
    }
    return value;
}

static int clamp_render_scale_3d_menu(int value) {
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

static int clamp_upscale_mode_3d_menu(int value) {
    if (value < RUNTIME_3D_UPSCALE_MODE_MIN) {
        value = RUNTIME_3D_UPSCALE_MODE_MIN;
    }
    if (value > RUNTIME_3D_UPSCALE_MODE_MAX) {
        value = RUNTIME_3D_UPSCALE_MODE_MAX;
    }
    return value;
}

static MenuSceneLibraryLane menu_library_lane_from_source(int source) {
    int clamped = animation_config_scene_source_clamp(source);
    if (clamped == SCENE_SOURCE_FLUID_MANIFEST) return MENU_SCENE_LIBRARY_FLUID_MANIFEST;
    if (clamped == SCENE_SOURCE_RUNTIME_SCENE) return MENU_SCENE_LIBRARY_RUNTIME_SCENE;
    return MENU_SCENE_LIBRARY_2D_CONFIG;
}

static void menu_state_sync_load_scene_dropdown_flags(MenuRuntimeState* state) {
    if (!state) return;
    state->manifestLoadEnabled = state->manifestDropdownOpen;
    if (!state->manifestDropdownOpen) {
        state->manifestScrollbarDragging = false;
    }
}

static void menu_state_sync_volume_dropdown_flags(MenuRuntimeState* state) {
    if (!state) return;
    if (!state->volumeDropdownOpen) {
        state->volumeScrollbarDragging = false;
    }
}

static void menu_state_sync_source_and_library(MenuRuntimeState* state) {
    if (!state) return;
    state->activeSceneSource = animation_config_scene_source_clamp(animSettings.sceneSource);
    state->activeSceneLibraryLane = menu_library_lane_from_source(state->activeSceneSource);
    menu_state_sync_load_scene_dropdown_flags(state);
    menu_state_sync_volume_dropdown_flags(state);
}

void menu_state_build_manifest_label(const char *path, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!path || !*path) return;
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    if (strcmp(filename, "manifest.json") == 0 ||
        strcmp(filename, "scene_bundle.json") == 0 ||
        strcmp(filename, "scene_runtime.json") == 0 ||
        strcmp(filename, "scene_authoring.json") == 0) {
        char parent_buf[PATH_MAX];
        size_t len = (size_t)(filename - path - 1);
        if (len >= sizeof(parent_buf)) len = sizeof(parent_buf) - 1;
        memcpy(parent_buf, path, len);
        parent_buf[len] = '\0';
        const char *dir_name = strrchr(parent_buf, '/');
        filename = dir_name ? dir_name + 1 : parent_buf;
    }

    const char *suffix = NULL;
    if (strstr(path, "physics_sim")) suffix = "phys";
    else if (strstr(path, "ray_tracing")) suffix = "ray";
    else if (strstr(path, "shared")) suffix = "shared";

    if (suffix) {
        snprintf(out, out_size, "%s (%s)", filename, suffix);
    } else {
        snprintf(out, out_size, "%s", filename);
    }
}

static void add_manifest_option_from_catalog_entry(MenuRuntimeState* state,
                                                   const SceneSourceCatalogEntry *entry) {
    if (!state || !entry || !entry->path[0]) return;
    if (state->manifestOptionCount >= SDL_MENU_MAX_MANIFEST_OPTIONS) return;

    ManifestOption *opt = &state->manifestOptions[state->manifestOptionCount++];
    strncpy(opt->path, entry->path, sizeof(opt->path) - 1);
    opt->path[sizeof(opt->path) - 1] = '\0';
    opt->source = animation_config_scene_source_clamp(entry->source);
    scene_source_ui_format_catalog_option_label(entry->path,
                                                opt->source,
                                                opt->name,
                                                sizeof(opt->name));
}

static void add_manifest_option_2d_config(MenuRuntimeState* state) {
    if (!state) return;
    if (state->manifestOptionCount >= SDL_MENU_MAX_MANIFEST_OPTIONS) return;

    ManifestOption *opt = &state->manifestOptions[state->manifestOptionCount++];
    snprintf(opt->name, sizeof(opt->name), "2D config");
    opt->path[0] = '\0';
    opt->source = SCENE_SOURCE_CONFIG_2D;
}

static int manifest_option_compare(const void *lhs, const void *rhs) {
    const ManifestOption *a = (const ManifestOption *)lhs;
    const ManifestOption *b = (const ManifestOption *)rhs;
    int a_source = animation_config_scene_source_clamp(a ? a->source : SCENE_SOURCE_CONFIG_2D);
    int b_source = animation_config_scene_source_clamp(b ? b->source : SCENE_SOURCE_CONFIG_2D);
    int cmp = 0;
    if (!a || !b) return 0;

    if (a_source != b_source) return a_source - b_source;
    cmp = strcmp(a->name, b->name);
    if (cmp != 0) return cmp;
    return strcmp(a->path, b->path);
}

static int volume_option_compare(const void *lhs, const void *rhs) {
    const VolumeSourceOption *a = (const VolumeSourceOption *)lhs;
    const VolumeSourceOption *b = (const VolumeSourceOption *)rhs;
    int a_kind = animation_config_volume_source_kind_clamp(a ? a->kind : VOLUME_SOURCE_NONE);
    int b_kind = animation_config_volume_source_kind_clamp(b ? b->kind : VOLUME_SOURCE_NONE);
    int cmp = 0;
    if (!a || !b) return 0;
    if (a_kind != b_kind) return a_kind - b_kind;
    cmp = strcmp(a->name, b->name);
    if (cmp != 0) return cmp;
    return strcmp(a->path, b->path);
}

static void add_volume_option_from_catalog_entry(MenuRuntimeState* state,
                                                 const VolumeSourceCatalogEntry *entry) {
    VolumeSourceOption *opt = NULL;
    if (!state || !entry || !entry->path[0]) return;
    if (state->volumeOptionCount >= SDL_MENU_MAX_MANIFEST_OPTIONS) return;

    opt = &state->volumeOptions[state->volumeOptionCount++];
    strncpy(opt->path, entry->path, sizeof(opt->path) - 1);
    opt->path[sizeof(opt->path) - 1] = '\0';
    opt->kind = animation_config_volume_source_kind_clamp(entry->kind);
    volume_source_ui_format_catalog_option_label(entry->path,
                                                 opt->kind,
                                                 opt->name,
                                                 sizeof(opt->name));
}

static void menu_state_clear_volume_summary(MenuRuntimeState* state) {
    if (!state) return;
    state->volumeSummaryValid = false;
    state->volumeSummaryLine1[0] = '\0';
    state->volumeSummaryLine2[0] = '\0';
    state->volumeSummaryPath[0] = '\0';
    state->volumeSummaryKind = VOLUME_SOURCE_NONE;
    state->volumeSummaryEnabled = false;
}

static void menu_state_refresh_volume_debug_summary(MenuRuntimeState* state) {
    RuntimeVolumeAttachment3D attachment = {0};
    RuntimeVolumeDebugSummary3D summary;
    RuntimeVolume3DSourceKind runtime_kind = RUNTIME_VOLUME_3D_SOURCE_NONE;
    char diagnostics[128] = {0};

    if (!state) return;
    if (!animSettings.volumeSourcePath[0] ||
        animation_config_volume_source_kind_clamp(animSettings.volumeSourceKind) == VOLUME_SOURCE_NONE) {
        menu_state_clear_volume_summary(state);
        snprintf(state->volumeSummaryLine1, sizeof(state->volumeSummaryLine1), "Volume: none");
        snprintf(state->volumeSummaryLine2, sizeof(state->volumeSummaryLine2), "Attach an external VF3D, pack, or bundle");
        return;
    }

    if (strcmp(state->volumeSummaryPath, animSettings.volumeSourcePath) == 0 &&
        state->volumeSummaryKind == animation_config_volume_source_kind_clamp(animSettings.volumeSourceKind) &&
        state->volumeSummaryEnabled == animSettings.volumeInteractionEnabled &&
        state->volumeSummaryValid) {
        return;
    }

    menu_state_clear_volume_summary(state);
    runtime_kind = (RuntimeVolume3DSourceKind)animation_config_volume_source_kind_clamp(animSettings.volumeSourceKind);
    switch (runtime_kind) {
        case RUNTIME_VOLUME_3D_SOURCE_MANIFEST:
        case RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D:
        case RUNTIME_VOLUME_3D_SOURCE_PACK:
            break;
        case RUNTIME_VOLUME_3D_SOURCE_NONE:
        default:
            snprintf(state->volumeSummaryLine1, sizeof(state->volumeSummaryLine1), "Volume: unsupported");
            snprintf(state->volumeSummaryLine2, sizeof(state->volumeSummaryLine2), "Configured kind is invalid");
            return;
    }

    if (!fluid_volume_import_3d_load_source(animSettings.volumeSourcePath,
                                            runtime_kind,
                                            &attachment,
                                            diagnostics,
                                            sizeof(diagnostics))) {
        snprintf(state->volumeSummaryLine1,
                 sizeof(state->volumeSummaryLine1),
                 "Volume unresolved");
        snprintf(state->volumeSummaryLine2,
                 sizeof(state->volumeSummaryLine2),
                 "%s",
                 diagnostics[0] ? diagnostics : "Attach failed");
        return;
    }

    RuntimeVolumeDebugSummary3D_Reset(&summary);
    if (!RuntimeVolumeDebugSummary3D_Build(&attachment, &summary)) {
        RuntimeVolumeAttachment3D_Reset(&attachment);
        snprintf(state->volumeSummaryLine1, sizeof(state->volumeSummaryLine1), "Volume summary failed");
        snprintf(state->volumeSummaryLine2, sizeof(state->volumeSummaryLine2), "Debug summary unavailable");
        return;
    }

    snprintf(state->volumeSummaryLine1,
             sizeof(state->volumeSummaryLine1),
             "%s | %ux%ux%u | %s",
             summary.sourceKind == RUNTIME_VOLUME_3D_SOURCE_MANIFEST ? "Manifest" :
             summary.sourceKind == RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D ? "VF3D" : "Pack",
             summary.gridW,
             summary.gridH,
             summary.gridD,
             animSettings.volumeInteractionEnabled ? "on" : "off");
    if (summary.hasDensityRange) {
        snprintf(state->volumeSummaryLine2,
                 sizeof(state->volumeSummaryLine2),
                 "Density %.4f..%.4f nz=%llu%s",
                 summary.densityMin,
                 summary.densityMax,
                 (unsigned long long)summary.densityNonZeroCellCount,
                 summary.sourceKind == RUNTIME_VOLUME_3D_SOURCE_MANIFEST ? " first-frame" : "");
    } else {
        snprintf(state->volumeSummaryLine2,
                 sizeof(state->volumeSummaryLine2),
                 "Density range unavailable%s",
                 summary.sourceKind == RUNTIME_VOLUME_3D_SOURCE_MANIFEST ? " first-frame" : "");
    }

    strncpy(state->volumeSummaryPath, animSettings.volumeSourcePath, sizeof(state->volumeSummaryPath) - 1);
    state->volumeSummaryPath[sizeof(state->volumeSummaryPath) - 1] = '\0';
    state->volumeSummaryKind = animation_config_volume_source_kind_clamp(animSettings.volumeSourceKind);
    state->volumeSummaryEnabled = animSettings.volumeInteractionEnabled;
    state->volumeSummaryValid = true;
    RuntimeVolumeAttachment3D_Reset(&attachment);
}

bool menu_state_manifest_option_visible(const MenuRuntimeState* state,
                                        const ManifestOption* option) {
    const int space_mode = animation_config_space_mode_clamp(animSettings.spaceMode);
    const int source = animation_config_scene_source_clamp(option ? option->source : SCENE_SOURCE_CONFIG_2D);
    static const bool kAllowFluidRowsIn3D = false;

    if (!state || !option) return false;
    if (space_mode == SPACE_MODE_2D) {
        return source == SCENE_SOURCE_CONFIG_2D || source == SCENE_SOURCE_FLUID_MANIFEST;
    }
    if (space_mode == SPACE_MODE_3D) {
        if (source == SCENE_SOURCE_RUNTIME_SCENE) return true;
        if (source == SCENE_SOURCE_FLUID_MANIFEST) return kAllowFluidRowsIn3D;
        return false;
    }
    return true;
}

void menu_state_refresh_manifest_options(MenuRuntimeState* state) {
    SceneSourceCatalogEntry catalog_entries[SDL_MENU_MAX_MANIFEST_OPTIONS];
    const char **roots = NULL;
    size_t root_count = 0;
    size_t catalog_entry_count = 0;
    if (!state) return;
    state->manifestOptionCount = 0;
    root_count = ray_tracing_manifest_default_roots(&roots);
    catalog_entry_count = scene_source_catalog_collect(catalog_entries,
                                                       SDL_MENU_MAX_MANIFEST_OPTIONS,
                                                       roots,
                                                       root_count,
                                                       animSettings.fluidManifest,
                                                       animSettings.runtimeScenePath);
    add_manifest_option_2d_config(state);
    for (size_t i = 0; i < catalog_entry_count; ++i) {
        add_manifest_option_from_catalog_entry(state, &catalog_entries[i]);
    }
    qsort(state->manifestOptions,
          state->manifestOptionCount,
          sizeof(state->manifestOptions[0]),
          manifest_option_compare);
    state->manifestScroll = 0.0f;
    state->manifestMaxScroll = 0.0f;
}

void menu_state_refresh_volume_options(MenuRuntimeState* state) {
    VolumeSourceCatalogEntry catalog_entries[SDL_MENU_MAX_MANIFEST_OPTIONS];
    const char **roots = NULL;
    size_t root_count = 0;
    size_t catalog_entry_count = 0;
    if (!state) return;
    state->volumeOptionCount = 0;
    root_count = ray_tracing_manifest_default_roots(&roots);
    catalog_entry_count = volume_source_catalog_collect(catalog_entries,
                                                        SDL_MENU_MAX_MANIFEST_OPTIONS,
                                                        roots,
                                                        root_count,
                                                        animSettings.volumeSourcePath);
    for (size_t i = 0; i < catalog_entry_count; ++i) {
        add_volume_option_from_catalog_entry(state, &catalog_entries[i]);
    }
    qsort(state->volumeOptions,
          state->volumeOptionCount,
          sizeof(state->volumeOptions[0]),
          volume_option_compare);
    state->volumeScroll = 0.0f;
    state->volumeMaxScroll = 0.0f;
}

void menu_state_manifest_clamp_scroll(MenuRuntimeState* state) {
    if (!state) return;
    if (state->manifestScroll < 0.0f) state->manifestScroll = 0.0f;
    if (state->manifestScroll > state->manifestMaxScroll) state->manifestScroll = state->manifestMaxScroll;
}

void menu_state_manifest_scroll_by(MenuRuntimeState* state, float delta) {
    if (!state) return;
    state->manifestScroll += delta;
    menu_state_manifest_clamp_scroll(state);
}

void menu_state_volume_clamp_scroll(MenuRuntimeState* state) {
    if (!state) return;
    if (state->volumeScroll < 0.0f) state->volumeScroll = 0.0f;
    if (state->volumeScroll > state->volumeMaxScroll) state->volumeScroll = state->volumeMaxScroll;
}

void menu_state_volume_scroll_by(MenuRuntimeState* state, float delta) {
    if (!state) return;
    state->volumeScroll += delta;
    menu_state_volume_clamp_scroll(state);
}

float menu_state_slider_clamp_scroll(float value, float max_scroll) {
    if (value < 0.0f) return 0.0f;
    if (value > max_scroll) return max_scroll;
    return value;
}

static void sync_roulette_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider && state->selectedSlider == &state->rouletteSliderValue) {
        return;
    }
    state->rouletteSliderValue = (int)lround(animSettings.rouletteThreshold * 1000.0);
    if (state->rouletteSliderValue < 1) state->rouletteSliderValue = 1;
}

static void sync_env_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider && state->selectedSlider == &state->envSliderValue) {
        return;
    }
    state->envSliderValue = (int)lround(animSettings.environmentBrightness);
    if (state->envSliderValue < 0) state->envSliderValue = 0;
    if (state->envSliderValue > 255) state->envSliderValue = 255;
}

static void sync_cache_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider && state->selectedSlider == &state->cacheWeightSliderValue) {
        return;
    }
    double weight = clamp_double(animSettings.cacheContributionWeight, 0.0, 1.0);
    state->cacheWeightSliderValue = (int)lround(weight * 100.0);
}

static void sync_light_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider && state->selectedSlider == &state->lightIntensitySliderValue) {
        return;
    }
    double intensity = clamp_double(animSettings.lightIntensity, 0.0, 20.0);
    state->lightIntensitySliderValue = (int)lround(intensity * 100.0);
}

static void sync_decay_softness_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider && state->selectedSlider == &state->lightDecaySoftnessSliderValue) {
        return;
    }
    double softness = clamp_double(animSettings.lightDecaySoftness, 0.1, 10.0);
    state->lightDecaySoftnessSliderValue = (int)lround(softness * 100.0);
}

static void sync_forward_decay_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider && state->selectedSlider == &state->forwardDecaySliderValue) {
        return;
    }
    double distance = clamp_double(animSettings.forwardDecay,
                                   SDL_MENU_FORWARD_FALLOFF_DISTANCE_MIN,
                                   SDL_MENU_FORWARD_FALLOFF_DISTANCE_MAX);
    state->forwardDecaySliderValue = (int)lround(distance);
}

static void sync_top_fill_strength_slider_from_settings(MenuRuntimeState* state) {
    double strength;
    if (!state) return;
    if (state->draggingSlider &&
        state->selectedSlider == &state->topFillStrengthSliderValue) {
        return;
    }
    strength = clamp_double(animSettings.topFillStrength, 0.0, 20.0);
    state->topFillStrengthSliderValue = (int)lround(strength * 100.0);
}

static void sync_environment_background_slider_from_settings(MenuRuntimeState* state) {
    double brightness;
    if (!state) return;
    if (state->draggingSlider &&
        state->selectedSlider == &state->environmentBackgroundBrightnessSliderValue) {
        return;
    }
    brightness = animSettings.environmentBackgroundBrightnessAuto
                     ? clamp_double(animSettings.environmentBrightness / 255.0, 0.0, 4.0)
                     : clamp_double(animSettings.environmentBackgroundBrightness, 0.0, 4.0);
    state->environmentBackgroundBrightnessSliderValue =
        (int)lround(brightness * 100.0);
}

static void sync_secondary_diffuse_samples_3d_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider &&
        state->selectedSlider == &state->secondaryDiffuseSamples3DSliderValue) {
        return;
    }
    state->secondaryDiffuseSamples3DSliderValue =
        clamp_secondary_diffuse_samples_3d_menu(animSettings.secondaryDiffuseSamples3D);
}

static void sync_bounce_depth_3d_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider &&
        state->selectedSlider == &state->bounceDepth3DSliderValue) {
        return;
    }
    state->bounceDepth3DSliderValue =
        clamp_bounce_depth_3d_menu(animSettings.bounceDepth3D);
}

static void sync_roulette_threshold_3d_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider &&
        state->selectedSlider == &state->rouletteThreshold3DSliderValue) {
        return;
    }
    state->rouletteThreshold3DSliderValue =
        clamp_roulette_threshold_3d_menu((int)lround(animSettings.rouletteThreshold3D * 1000.0));
}

static void sync_transmission_samples_3d_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider &&
        state->selectedSlider == &state->transmissionSamples3DSliderValue) {
        return;
    }
    state->transmissionSamples3DSliderValue =
        clamp_transmission_samples_3d_menu(animSettings.transmissionSamples3D);
}

static void sync_temporal_frames_3d_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider &&
        state->selectedSlider == &state->temporalFrames3DSliderValue) {
        return;
    }
    state->temporalFrames3DSliderValue =
        clamp_temporal_frames_3d_menu(animSettings.temporalFrames3D);
}

static void sync_render_scale_3d_slider_from_settings(MenuRuntimeState* state) {
    if (!state) return;
    if (state->draggingSlider &&
        state->selectedSlider == &state->renderScale3DSliderValue) {
        return;
    }
    state->renderScale3DSliderValue =
        clamp_render_scale_3d_menu(animSettings.renderScale3D);
}

void menu_state_sync_from_anim(MenuRuntimeState* state) {
    if (!state) return;
    RayTracingIntegratorCatalog_NormalizeAnimationConfig(&animSettings);
    sync_roulette_slider_from_settings(state);
    sync_env_slider_from_settings(state);
    sync_cache_slider_from_settings(state);
    sync_light_slider_from_settings(state);
    sync_decay_softness_slider_from_settings(state);
    sync_forward_decay_slider_from_settings(state);
    sync_top_fill_strength_slider_from_settings(state);
    sync_environment_background_slider_from_settings(state);
    sync_bounce_depth_3d_slider_from_settings(state);
    sync_roulette_threshold_3d_slider_from_settings(state);
    sync_secondary_diffuse_samples_3d_slider_from_settings(state);
    sync_transmission_samples_3d_slider_from_settings(state);
    sync_temporal_frames_3d_slider_from_settings(state);
    sync_render_scale_3d_slider_from_settings(state);
    animSettings.upscaleMode3D =
        clamp_upscale_mode_3d_menu(animSettings.upscaleMode3D);
    menu_state_sync_source_and_library(state);
    menu_state_refresh_volume_debug_summary(state);
}

void menu_state_reanchor_camera_after_resize(int previousWidth, int previousHeight) {
    if (previousWidth <= 0 || previousHeight <= 0) return;
    if (sceneSettings.windowWidth <= 0 || sceneSettings.windowHeight <= 0) return;
    CameraPoint topLeft = CameraScreenToWorld(&sceneSettings.camera,
                                              0.0,
                                              0.0,
                                              previousWidth,
                                              previousHeight);
    sceneSettings.camera.x = topLeft.x + (sceneSettings.windowWidth * 0.5) / sceneSettings.camera.zoom;
    sceneSettings.camera.y = topLeft.y + (sceneSettings.windowHeight * 0.5) / sceneSettings.camera.zoom;
    sceneSettings.cameraMargin = CameraClampMarginPixels(sceneSettings.cameraMargin,
                                                         sceneSettings.windowWidth,
                                                         sceneSettings.windowHeight);
}

void menu_state_set_load_scene_enabled(MenuRuntimeState* state, bool enabled) {
    if (!state) return;
    if (enabled) {
        state->manifestDropdownOpen = true;
        state->volumeDropdownOpen = false;
        menu_state_refresh_manifest_options(state);
        state->manifestScroll = 0.0f;
        state->manifestScrollbarDragging = false;
    } else {
        state->manifestDropdownOpen = false;
    }
    menu_state_sync_source_and_library(state);
}

void menu_state_set_volume_load_enabled(MenuRuntimeState* state, bool enabled) {
    if (!state) return;
    if (enabled) {
        state->volumeDropdownOpen = true;
        state->manifestDropdownOpen = false;
        menu_state_refresh_volume_options(state);
        state->volumeScroll = 0.0f;
        state->volumeScrollbarDragging = false;
    } else {
        state->volumeDropdownOpen = false;
    }
    menu_state_sync_source_and_library(state);
}

void menu_state_apply_special_slider_rules(MenuRuntimeState* state, int* target) {
    if (!state || !target) return;
    if (target == &animSettings.tileSize) {
        animSettings.tileSize = clamp_tile_size_menu(animSettings.tileSize);
    } else if (target == &state->rouletteSliderValue) {
        if (state->rouletteSliderValue < 1) state->rouletteSliderValue = 1;
        if (state->rouletteSliderValue > 2000) state->rouletteSliderValue = 2000;
        animSettings.rouletteThreshold = state->rouletteSliderValue / 1000.0;
    } else if (target == &animSettings.pathSamplesPerPixel) {
        if (animSettings.pathSamplesPerPixel < 1) animSettings.pathSamplesPerPixel = 1;
    } else if (target == &animSettings.pathMaxDepth) {
        if (animSettings.pathMaxDepth < 1) animSettings.pathMaxDepth = 1;
    } else if (target == &state->envSliderValue) {
        if (state->envSliderValue < 0) state->envSliderValue = 0;
        if (state->envSliderValue > 255) state->envSliderValue = 255;
        animSettings.environmentBrightness = (double)state->envSliderValue;
    } else if (target == &state->cacheWeightSliderValue) {
        if (state->cacheWeightSliderValue < 0) state->cacheWeightSliderValue = 0;
        if (state->cacheWeightSliderValue > 100) state->cacheWeightSliderValue = 100;
        animSettings.cacheContributionWeight = state->cacheWeightSliderValue / 100.0;
    } else if (target == &state->lightIntensitySliderValue) {
        if (state->lightIntensitySliderValue < 0) state->lightIntensitySliderValue = 0;
        if (state->lightIntensitySliderValue > 2000) state->lightIntensitySliderValue = 2000;
        animSettings.lightIntensity = state->lightIntensitySliderValue / 100.0;
    } else if (target == &state->lightDecaySoftnessSliderValue) {
        if (state->lightDecaySoftnessSliderValue < 10) state->lightDecaySoftnessSliderValue = 10;
        if (state->lightDecaySoftnessSliderValue > 1000) state->lightDecaySoftnessSliderValue = 1000;
        animSettings.lightDecaySoftness = state->lightDecaySoftnessSliderValue / 100.0;
    } else if (target == &state->forwardDecaySliderValue) {
        if (state->forwardDecaySliderValue < SDL_MENU_FORWARD_FALLOFF_DISTANCE_MIN) {
            state->forwardDecaySliderValue = SDL_MENU_FORWARD_FALLOFF_DISTANCE_MIN;
        }
        if (state->forwardDecaySliderValue > SDL_MENU_FORWARD_FALLOFF_DISTANCE_MAX) {
            state->forwardDecaySliderValue = SDL_MENU_FORWARD_FALLOFF_DISTANCE_MAX;
        }
        animSettings.forwardDecay = state->forwardDecaySliderValue;
    } else if (target == &state->topFillStrengthSliderValue) {
        if (state->topFillStrengthSliderValue < 0) state->topFillStrengthSliderValue = 0;
        if (state->topFillStrengthSliderValue > 2000) state->topFillStrengthSliderValue = 2000;
        animSettings.topFillStrength = state->topFillStrengthSliderValue / 100.0;
    } else if (target == &state->environmentBackgroundBrightnessSliderValue) {
        if (state->environmentBackgroundBrightnessSliderValue < 0) {
            state->environmentBackgroundBrightnessSliderValue = 0;
        }
        if (state->environmentBackgroundBrightnessSliderValue > 400) {
            state->environmentBackgroundBrightnessSliderValue = 400;
        }
        animSettings.environmentBackgroundBrightnessAuto = false;
        animSettings.environmentBackgroundBrightness =
            state->environmentBackgroundBrightnessSliderValue / 100.0;
    } else if (target == &state->bounceDepth3DSliderValue) {
        state->bounceDepth3DSliderValue =
            clamp_bounce_depth_3d_menu(state->bounceDepth3DSliderValue);
        animSettings.bounceDepth3D = state->bounceDepth3DSliderValue;
    } else if (target == &state->rouletteThreshold3DSliderValue) {
        state->rouletteThreshold3DSliderValue =
            clamp_roulette_threshold_3d_menu(state->rouletteThreshold3DSliderValue);
        animSettings.rouletteThreshold3D =
            state->rouletteThreshold3DSliderValue / 1000.0;
    } else if (target == &state->secondaryDiffuseSamples3DSliderValue) {
        state->secondaryDiffuseSamples3DSliderValue =
            clamp_secondary_diffuse_samples_3d_menu(state->secondaryDiffuseSamples3DSliderValue);
        animSettings.secondaryDiffuseSamples3D = state->secondaryDiffuseSamples3DSliderValue;
    } else if (target == &state->transmissionSamples3DSliderValue) {
        state->transmissionSamples3DSliderValue =
            clamp_transmission_samples_3d_menu(state->transmissionSamples3DSliderValue);
        animSettings.transmissionSamples3D = state->transmissionSamples3DSliderValue;
    } else if (target == &state->temporalFrames3DSliderValue) {
        state->temporalFrames3DSliderValue =
            clamp_temporal_frames_3d_menu(state->temporalFrames3DSliderValue);
        animSettings.temporalFrames3D = state->temporalFrames3DSliderValue;
    } else if (target == &state->renderScale3DSliderValue) {
        state->renderScale3DSliderValue =
            clamp_render_scale_3d_menu(state->renderScale3DSliderValue);
        animSettings.renderScale3D = state->renderScale3DSliderValue;
    } else if (target == &sceneSettings.windowWidth || target == &sceneSettings.windowHeight) {
        if (sceneSettings.windowWidth < 200) sceneSettings.windowWidth = 200;
        if (sceneSettings.windowHeight < 200) sceneSettings.windowHeight = 200;
        if (sceneSettings.windowWidth % 2) sceneSettings.windowWidth += 1;
        if (sceneSettings.windowHeight % 2) sceneSettings.windowHeight += 1;
    }
}

static bool open_menu_font_for_current_zoom(TTF_Font** out_font) {
    int point_size = ray_tracing_font_runtime_ui_regular_base_point_size(TOGGLE_BUTTON_TEXT_SIZE);
    TTF_Font* opened_font;
    SDL_Renderer* renderer = getRenderContext() ? getRenderContext()->renderer : NULL;
    if (!out_font) return false;

    point_size = animation_config_scale_text_point_size(&animSettings,
                                                        point_size,
                                                        kMenuMinFontPointSize);
    opened_font = ray_tracing_font_runtime_get_ui_regular(renderer,
                                                          point_size,
                                                          kMenuMinFontPointSize);
    if (!opened_font) return false;
    printf("Menu font loaded: size=%d cache=yes\n", point_size);
    *out_font = opened_font;
    return true;
}

bool menu_state_reload_font(TTF_Font** font) {
    TTF_Font* replacement = NULL;
    if (!font) return false;
    if (!open_menu_font_for_current_zoom(&replacement)) {
        return false;
    }
    *font = replacement;
    return true;
}

void menu_state_init(MenuRuntimeState* state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->activeView = MENU_VIEW_MAIN;
    state->statusColor = (SDL_Color){255, 255, 255, 255};
    state->rouletteSliderValue = 10;
    state->envSliderValue = 0;
    state->cacheWeightSliderValue = 100;
    state->lightIntensitySliderValue = (int)lround(RAY_TRACING_DEFAULT_LIGHT_INTENSITY * 100.0);
    state->lightDecaySoftnessSliderValue = 100;
    state->forwardDecaySliderValue = 2000;
    state->topFillStrengthSliderValue = 100;
    state->environmentBackgroundBrightnessSliderValue = 0;
    state->bounceDepth3DSliderValue = RUNTIME_3D_BOUNCE_DEPTH_DEFAULT;
    state->rouletteThreshold3DSliderValue =
        (int)lround(RUNTIME_3D_ROULETTE_THRESHOLD_DEFAULT * 1000.0);
    state->secondaryDiffuseSamples3DSliderValue = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    state->transmissionSamples3DSliderValue = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    state->temporalFrames3DSliderValue = RUNTIME_3D_TEMPORAL_FRAMES_DEFAULT;
    state->renderScale3DSliderValue = RUNTIME_3D_RENDER_SCALE_DEFAULT;
    state->manifestDropdownOpen = false;
    state->volumeDropdownOpen = false;
    state->rendererControlsTab = MENU_RENDERER_CONTROLS_LIGHTING;
    (void)menu_workspace_host_init(&state->menuWorkspaceHost);
    menu_state_sync_load_scene_dropdown_flags(state);
    menu_state_sync_from_anim(state);
    menu_state_sync_source_and_library(state);
    if (animSettings.inputRoot[0]) {
        (void)setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    }
    if (animSettings.meshAssetRoot[0]) {
        (void)setenv("RAY_TRACING_MESH_ASSET_ROOT", animSettings.meshAssetRoot, 1);
    } else {
        (void)unsetenv("RAY_TRACING_MESH_ASSET_ROOT");
    }
    if (animSettings.outputRoot[0]) {
        (void)setenv("RAY_TRACING_OUTPUT_ROOT", animSettings.outputRoot, 1);
    }
    if (animSettings.videoOutputRoot[0]) {
        (void)setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", animSettings.videoOutputRoot, 1);
    }
    menu_state_refresh_manifest_options(state);
    menu_state_refresh_volume_options(state);
    state->editingInputRoot = false;
    state->editingMeshAssetRoot = false;
    state->editingOutputRoot = false;
    state->editingFrameDir = false;
    state->editingVideoOutputRoot = false;
    state->editingStartFrame = false;
    state->pathInputBuffer[0] = '\0';
    menu_state_clear_volume_summary(state);
    menu_batch_panel_refresh(state);
    state->sliderScroll = 0.0f;
    state->sliderMaxScroll = 0.0f;
    state->sliderPanelRect = (SDL_Rect){0, 0, 0, 0};
    state->oldWindowWidth = sceneSettings.windowWidth;
    state->oldWindowHeight = sceneSettings.windowHeight;
}

void menu_state_reset_defaults(MenuRuntimeState* state) {
    animSettings.interactiveMode = true;
    animSettings.deepRenderMode = false;
    animSettings.bounceMode = false;
    animSettings.autoMP4 = false;
    animSettings.bounceLimit = SDL_MENU_DEFAULT_BOUNCE_LIMIT;
    animSettings.frameLimit = SDL_MENU_DEFAULT_FRAME_LIMIT;
    animSettings.framesForTravel = SDL_MENU_DEFAULT_FRAME_FOR_TRAVEL;
    animSettings.startFrameIndex = 0;
    animSettings.resumeFromExistingFrames = false;
    animSettings.fps = 30;
    strncpy(animSettings.inputRoot, ray_tracing_default_input_root(), sizeof(animSettings.inputRoot) - 1);
    animSettings.inputRoot[sizeof(animSettings.inputRoot) - 1] = '\0';
    animSettings.meshAssetRoot[0] = '\0';
    strncpy(animSettings.outputRoot, ray_tracing_default_output_root(), sizeof(animSettings.outputRoot) - 1);
    animSettings.outputRoot[sizeof(animSettings.outputRoot) - 1] = '\0';
    strncpy(animSettings.frameDir, ray_tracing_default_frame_dir(), sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot,
            ray_tracing_default_video_output_root(),
            sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    (void)setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    (void)unsetenv("RAY_TRACING_MESH_ASSET_ROOT");
    (void)setenv("RAY_TRACING_OUTPUT_ROOT", animSettings.outputRoot, 1);
    (void)setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", animSettings.videoOutputRoot, 1);
    if (state) {
        state->editingInputRoot = false;
        state->editingMeshAssetRoot = false;
        state->editingOutputRoot = false;
        state->editingFrameDir = false;
        state->editingVideoOutputRoot = false;
        state->editingStartFrame = false;
        state->pathInputBuffer[0] = '\0';
        menu_batch_panel_refresh(state);
    }
    animSettings.useTiledRenderer = false;
    animSettings.tilePreviewEnabled = false;
    animSettings.tileSize = 16;
    animSettings.rouletteThreshold = 0.01;
    animSettings.integratorMode = 0;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    animSettings.pathSamplesPerPixel = 4;
    animSettings.pathMaxDepth = 4;
    animSettings.pathDirectLighting = true;
    animSettings.pathRussianRoulette = true;
    animSettings.pathEnableMIS = true;
    animSettings.environmentBrightness = 0.0;
    animSettings.environmentPreset = ENVIRONMENT_PRESET_SKY;
    animSettings.environmentBackgroundLightingAuthored = true;
    animSettings.environmentBackgroundBrightnessAuto = true;
    animSettings.environmentBackgroundBrightness = 0.0;
    animSettings.environmentBackgroundColorR = 1.0;
    animSettings.environmentBackgroundColorG = 1.0;
    animSettings.environmentBackgroundColorB = 1.0;
    animSettings.pathSeed = 1;
    animSettings.cacheContributionWeight = 1.0;
    animSettings.bsdfModel = 1;
    animSettings.lightIntensity = RAY_TRACING_DEFAULT_LIGHT_INTENSITY;
    animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    animSettings.topFillStrength = 1.0;
    animSettings.disneyDenoiseEnabled = true;
    animSettings.spaceMode = SPACE_MODE_2D;
    animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
    animSettings.useFluidScene = false;
    animSettings.fluidManifest[0] = '\0';
    animSettings.runtimeScenePath[0] = '\0';
    AnimationClearVolumeSource();
    animSettings.textZoomStep = 0;
    animSettings.previewMode = false;
    animSettings.previewDuration = 5.0;
    if (state) {
        state->sliderScroll = 0.0f;
        state->rendererControlsTab = MENU_RENDERER_CONTROLS_LIGHTING;
        if (!state->menuWorkspaceHost.initialized) {
            (void)menu_workspace_host_init(&state->menuWorkspaceHost);
        } else {
            (void)menu_workspace_host_select(&state->menuWorkspaceHost,
                                             MENU_WORKSPACE_RENDER);
        }
    }
    double diag = hypot(sceneSettings.windowWidth, sceneSettings.windowHeight);
    animSettings.forwardDecay = (diag > 0.0) ? diag : 2000.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_QUADRATIC;
    animSettings.bounceDepth3D = RUNTIME_3D_BOUNCE_DEPTH_DEFAULT;
    animSettings.rouletteThreshold3D = RUNTIME_3D_ROULETTE_THRESHOLD_DEFAULT;
    animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    animSettings.transmissionSamples3D = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    animSettings.temporalFrames3D = RUNTIME_3D_TEMPORAL_FRAMES_DEFAULT;
    animSettings.renderScale3D = RUNTIME_3D_RENDER_SCALE_DEFAULT;
    animSettings.upscaleMode3D = RUNTIME_3D_UPSCALE_MODE_DEFAULT;
    menu_state_sync_from_anim(state);
    if (state) {
        menu_state_refresh_manifest_options(state);
        menu_state_refresh_volume_options(state);
    }
    if (state) {
        state->oldWindowWidth = sceneSettings.windowWidth;
        state->oldWindowHeight = sceneSettings.windowHeight;
    }
}
