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
#include "render/ray_tracing_integrator_catalog.h"
#include "render/text_font_cache.h"
#include "ui/scene_source_catalog.h"

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

static void menu_state_sync_source_and_library(MenuRuntimeState* state) {
    if (!state) return;
    state->activeSceneSource = animation_config_scene_source_clamp(animSettings.sceneSource);
    state->activeSceneLibraryLane = menu_library_lane_from_source(state->activeSceneSource);
    menu_state_sync_load_scene_dropdown_flags(state);
}

void menu_state_build_manifest_label(const char *path, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!path || !*path) return;
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    if (strcmp(filename, "manifest.json") == 0 || strcmp(filename, "scene_bundle.json") == 0) {
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
    menu_state_build_manifest_label(entry->path, opt->name, sizeof(opt->name));
    if (opt->source == SCENE_SOURCE_RUNTIME_SCENE) {
        char decorated[sizeof(opt->name)];
        snprintf(decorated, sizeof(decorated), "runtime: %s", opt->name);
        strncpy(opt->name, decorated, sizeof(opt->name) - 1);
        opt->name[sizeof(opt->name) - 1] = '\0';
    } else if (opt->source == SCENE_SOURCE_FLUID_MANIFEST) {
        char decorated[sizeof(opt->name)];
        snprintf(decorated, sizeof(decorated), "fluid: %s", opt->name);
        strncpy(opt->name, decorated, sizeof(opt->name) - 1);
        opt->name[sizeof(opt->name) - 1] = '\0';
    }
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
    state->envSliderValue = (int)lround(animSettings.environmentBrightness * 100.0);
    if (state->envSliderValue < 0) state->envSliderValue = 0;
    if (state->envSliderValue > 200) state->envSliderValue = 200;
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

void menu_state_sync_from_anim(MenuRuntimeState* state) {
    if (!state) return;
    RayTracingIntegratorCatalog_NormalizeAnimationConfig(&animSettings);
    sync_roulette_slider_from_settings(state);
    sync_env_slider_from_settings(state);
    sync_cache_slider_from_settings(state);
    sync_light_slider_from_settings(state);
    sync_decay_softness_slider_from_settings(state);
    sync_forward_decay_slider_from_settings(state);
    menu_state_sync_source_and_library(state);
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
        menu_state_refresh_manifest_options(state);
        state->manifestScroll = 0.0f;
        state->manifestScrollbarDragging = false;
    } else {
        state->manifestDropdownOpen = false;
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
        if (state->envSliderValue > 200) state->envSliderValue = 200;
        animSettings.environmentBrightness = state->envSliderValue / 100.0;
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
    } else if (target == &sceneSettings.windowWidth || target == &sceneSettings.windowHeight) {
        if (sceneSettings.windowWidth < 200) sceneSettings.windowWidth = 200;
        if (sceneSettings.windowHeight < 200) sceneSettings.windowHeight = 200;
        if (sceneSettings.windowWidth % 2) sceneSettings.windowWidth += 1;
        if (sceneSettings.windowHeight % 2) sceneSettings.windowHeight += 1;
    }
}

static bool open_menu_font_for_current_zoom(TTF_Font** out_font) {
    int point_size = ray_tracing_text_font_cache_ui_regular_base_point_size(TOGGLE_BUTTON_TEXT_SIZE);
    TTF_Font* opened_font;
    SDL_Renderer* renderer = getRenderContext() ? getRenderContext()->renderer : NULL;
    if (!out_font) return false;

    point_size = animation_config_scale_text_point_size(&animSettings,
                                                        point_size,
                                                        kMenuMinFontPointSize);
    opened_font = ray_tracing_text_font_cache_get_ui_regular(renderer,
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
    state->lightIntensitySliderValue = 500;
    state->lightDecaySoftnessSliderValue = 100;
    state->forwardDecaySliderValue = 2000;
    state->manifestDropdownOpen = false;
    menu_state_sync_load_scene_dropdown_flags(state);
    menu_state_sync_from_anim(state);
    menu_state_sync_source_and_library(state);
    menu_state_refresh_manifest_options(state);
    if (animSettings.inputRoot[0]) {
        (void)setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    }
    if (animSettings.outputRoot[0]) {
        (void)setenv("RAY_TRACING_OUTPUT_ROOT", animSettings.outputRoot, 1);
    }
    if (animSettings.videoOutputRoot[0]) {
        (void)setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", animSettings.videoOutputRoot, 1);
    }
    state->editingInputRoot = false;
    state->editingOutputRoot = false;
    state->editingFrameDir = false;
    state->editingVideoOutputRoot = false;
    state->pathInputBuffer[0] = '\0';
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
    animSettings.fps = 30;
    strncpy(animSettings.inputRoot, ray_tracing_default_input_root(), sizeof(animSettings.inputRoot) - 1);
    animSettings.inputRoot[sizeof(animSettings.inputRoot) - 1] = '\0';
    strncpy(animSettings.outputRoot, ray_tracing_default_output_root(), sizeof(animSettings.outputRoot) - 1);
    animSettings.outputRoot[sizeof(animSettings.outputRoot) - 1] = '\0';
    strncpy(animSettings.frameDir, ray_tracing_default_frame_dir(), sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot,
            ray_tracing_default_video_output_root(),
            sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    (void)setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    (void)setenv("RAY_TRACING_OUTPUT_ROOT", animSettings.outputRoot, 1);
    (void)setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", animSettings.videoOutputRoot, 1);
    if (state) {
        state->editingInputRoot = false;
        state->editingOutputRoot = false;
        state->editingFrameDir = false;
        state->editingVideoOutputRoot = false;
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
    animSettings.pathSeed = 1;
    animSettings.cacheContributionWeight = 1.0;
    animSettings.bsdfModel = 1;
    animSettings.lightIntensity = 5.0;
    animSettings.spaceMode = SPACE_MODE_2D;
    animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
    animSettings.useFluidScene = false;
    animSettings.fluidManifest[0] = '\0';
    animSettings.runtimeScenePath[0] = '\0';
    animSettings.textZoomStep = 0;
    animSettings.previewMode = false;
    animSettings.previewDuration = 5.0;
    if (state) {
        state->sliderScroll = 0.0f;
    }
    double diag = hypot(sceneSettings.windowWidth, sceneSettings.windowHeight);
    animSettings.forwardDecay = (diag > 0.0) ? diag : 2000.0;
    animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_QUADRATIC;
    menu_state_sync_from_anim(state);
    if (state) {
        state->oldWindowWidth = sceneSettings.windowWidth;
        state->oldWindowHeight = sceneSettings.windowHeight;
    }
}
