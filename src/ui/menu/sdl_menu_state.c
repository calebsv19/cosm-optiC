#include "ui/sdl_menu_state.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/animation.h"
#include "app/data_paths.h"
#include "camera/camera.h"
#include "config/config_manager.h"
#include "editor/editor_mode_router.h"
#include "ui/shared_theme_font_adapter.h"

#define TOGGLE_BUTTON_TEXT_SIZE 24

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

static bool file_exists_regular(const char *path) {
    if (!path || !*path) return false;
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
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

static void add_manifest_option(MenuRuntimeState* state, const char *path) {
    if (!state || !path || !*path) return;
    if (state->manifestOptionCount >= SDL_MENU_MAX_MANIFEST_OPTIONS) return;

    char resolved[PATH_MAX];
    const char *use_path = path;
    if (realpath(path, resolved)) {
        use_path = resolved;
    }
    if (!file_exists_regular(use_path)) return;

    for (size_t i = 0; i < state->manifestOptionCount; ++i) {
        if (strcmp(state->manifestOptions[i].path, use_path) == 0) {
            return;
        }
    }

    ManifestOption *opt = &state->manifestOptions[state->manifestOptionCount++];
    strncpy(opt->path, use_path, sizeof(opt->path) - 1);
    opt->path[sizeof(opt->path) - 1] = '\0';
    menu_state_build_manifest_label(use_path, opt->name, sizeof(opt->name));
}

static void scan_manifest_root(MenuRuntimeState* state, const char *root) {
    if (!state || !root || !*root) return;

    char path_buf[PATH_MAX];
    snprintf(path_buf, sizeof(path_buf), "%s/manifest.json", root);
    add_manifest_option(state, path_buf);
    snprintf(path_buf, sizeof(path_buf), "%s/scene_bundle.json", root);
    add_manifest_option(state, path_buf);

    DIR *dir = opendir(root);
    if (!dir) return;

    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        snprintf(path_buf, sizeof(path_buf), "%s/%s", root, ent->d_name);
        struct stat st;
        if (stat(path_buf, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            char manifest_path[PATH_MAX];
            snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", path_buf);
            add_manifest_option(state, manifest_path);
            char bundle_path[PATH_MAX];
            snprintf(bundle_path, sizeof(bundle_path), "%s/scene_bundle.json", path_buf);
            add_manifest_option(state, bundle_path);
        } else if (S_ISREG(st.st_mode) && strcmp(ent->d_name, "manifest.json") == 0) {
            add_manifest_option(state, path_buf);
        } else if (S_ISREG(st.st_mode) && strcmp(ent->d_name, "scene_bundle.json") == 0) {
            add_manifest_option(state, path_buf);
        }
    }
    closedir(dir);
}

void menu_state_refresh_manifest_options(MenuRuntimeState* state) {
    const char **roots = NULL;
    size_t root_count = 0;
    if (!state) return;
    state->manifestOptionCount = 0;
    root_count = ray_tracing_manifest_default_roots(&roots);
    for (size_t i = 0; i < root_count; ++i) {
        scan_manifest_root(state, roots[i]);
    }
    if (animSettings.fluidManifest[0]) {
        add_manifest_option(state, animSettings.fluidManifest);
    }
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
    sync_roulette_slider_from_settings(state);
    sync_env_slider_from_settings(state);
    sync_cache_slider_from_settings(state);
    sync_light_slider_from_settings(state);
    sync_decay_softness_slider_from_settings(state);
    sync_forward_decay_slider_from_settings(state);
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
    state->manifestLoadEnabled = enabled;
    animSettings.useFluidScene = enabled;
    state->manifestDropdownOpen = enabled;
    if (enabled) {
        menu_state_refresh_manifest_options(state);
        state->manifestScroll = 0.0f;
        state->manifestScrollbarDragging = false;
        if (animSettings.fluidManifest[0]) {
            AnimationApplyFluidScene(animSettings.fluidManifest);
        }
    } else {
        state->manifestDropdownOpen = false;
        AnimationClearFluidGrid();
        LoadSceneConfig();
    }
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
    char shared_font_path[256];
    const char* font_path = "/System/Library/Fonts/Supplemental/Arial.ttf";
    int point_size = TOGGLE_BUTTON_TEXT_SIZE;
    TTF_Font* opened_font;
    bool used_shared_path = false;
    if (!out_font) return false;

    if (ray_tracing_shared_font_resolve_ui_regular(
            shared_font_path, sizeof(shared_font_path), &point_size)) {
        font_path = shared_font_path;
        used_shared_path = true;
    }
    point_size = animation_config_scale_text_point_size(&animSettings, point_size, 8);
    opened_font = TTF_OpenFont(font_path, point_size);
    if (!opened_font && font_path != NULL && strcmp(font_path, "/System/Library/Fonts/Supplemental/Arial.ttf") != 0) {
        opened_font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", point_size);
        if (opened_font) {
            font_path = "/System/Library/Fonts/Supplemental/Arial.ttf";
            used_shared_path = false;
        }
    }
    if (!opened_font) return false;
    printf("Menu font loaded: path=%s size=%d shared=%s\n",
           font_path,
           point_size,
           used_shared_path ? "yes" : "no");
    *out_font = opened_font;
    return true;
}

bool menu_state_reload_font(TTF_Font** font) {
    TTF_Font* replacement = NULL;
    if (!font) return false;
    if (!open_menu_font_for_current_zoom(&replacement)) {
        return false;
    }
    if (*font) {
        TTF_CloseFont(*font);
    }
    *font = replacement;
    return true;
}

void menu_state_init(MenuRuntimeState* state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->statusColor = (SDL_Color){255, 255, 255, 255};
    state->rouletteSliderValue = 10;
    state->envSliderValue = 0;
    state->cacheWeightSliderValue = 100;
    state->lightIntensitySliderValue = 500;
    state->lightDecaySoftnessSliderValue = 100;
    state->forwardDecaySliderValue = 2000;
    menu_state_sync_from_anim(state);
    state->manifestLoadEnabled = animSettings.useFluidScene;
    state->manifestDropdownOpen = state->manifestLoadEnabled;
    menu_state_refresh_manifest_options(state);
    if (animSettings.inputRoot[0]) {
        (void)setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    }
    if (animSettings.outputRoot[0]) {
        (void)setenv("RAY_TRACING_OUTPUT_ROOT", animSettings.outputRoot, 1);
    }
    state->editingInputRoot = false;
    state->editingOutputRoot = false;
    state->pathInputBuffer[0] = '\0';
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
    (void)setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    (void)setenv("RAY_TRACING_OUTPUT_ROOT", animSettings.outputRoot, 1);
    animSettings.useTiledRenderer = false;
    animSettings.tilePreviewEnabled = false;
    animSettings.tileSize = 16;
    animSettings.rouletteThreshold = 0.01;
    animSettings.integratorMode = 0;
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
