#include "config/config_manager.h"
#include "config/config_file_io.h"
#include "config/core/config_runtime_paths.h"
#include "config_animation_runtime3d.h"
#include "app/data_paths.h"
#include "render/ray_tracing_integrator_catalog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <math.h>
#include <limits.h>

#define ANIMATION_CONFIG_DEFAULT_FILE "config/animation_config.json"
#define ANIMATION_CONFIG_RUNTIME_FILE "data/runtime/animation_config.json"
#define ANIMATION_CONFIG_LEGACY_FILE "Configs/animation_config.json"
#define TEXT_ZOOM_STEP_MIN (-4)
#define TEXT_ZOOM_STEP_MAX (5)
#define TEXT_ZOOM_STEP_PERCENT_PER_STEP (10)

static double ClampDoubleValue(double value, double minValue, double maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static FILE* OpenAnimationConfigRead(const char** loaded_path) {
    char stable_input_root[PATH_MAX];
    char stable_output_root[PATH_MAX];
    char stable_runtime_file[PATH_MAX];
    char stable_default_file[PATH_MAX];
    const char* candidates[5] = {0};
    size_t candidate_count = 0;

    candidates[candidate_count++] = ANIMATION_CONFIG_RUNTIME_FILE;
    if (ray_tracing_find_stable_output_root(stable_output_root, sizeof(stable_output_root)) &&
        ray_tracing_compose_path(stable_output_root,
                                 "animation_config.json",
                                 stable_runtime_file,
                                 sizeof(stable_runtime_file))) {
        candidates[candidate_count++] = stable_runtime_file;
    }
    candidates[candidate_count++] = ANIMATION_CONFIG_DEFAULT_FILE;
    if (ray_tracing_find_stable_input_root(stable_input_root, sizeof(stable_input_root)) &&
        ray_tracing_compose_path(stable_input_root,
                                 "animation_config.json",
                                 stable_default_file,
                                 sizeof(stable_default_file))) {
        candidates[candidate_count++] = stable_default_file;
    }

    candidates[candidate_count++] = ANIMATION_CONFIG_LEGACY_FILE;
    return config_io_open_read_first(candidates, candidate_count, loaded_path);
}

static double animation_config_legacy_environment_radiance_to_byte_floor(double radiance) {
    double mapped = 0.0;
    if (!(radiance > 0.0)) return 0.0;
    mapped = radiance / (1.0 + radiance);
    mapped = pow(ClampDoubleValue(mapped, 0.0, 1.0), 0.55);
    return ClampDoubleValue(lround(mapped * 255.0), 0.0, 255.0);
}

int animation_config_text_zoom_step_clamp(int step) {
    if (step < TEXT_ZOOM_STEP_MIN) return TEXT_ZOOM_STEP_MIN;
    if (step > TEXT_ZOOM_STEP_MAX) return TEXT_ZOOM_STEP_MAX;
    return step;
}

int animation_config_text_zoom_percent_from_step(int step) {
    int clamped = animation_config_text_zoom_step_clamp(step);
    return 100 + (clamped * TEXT_ZOOM_STEP_PERCENT_PER_STEP);
}

int animation_config_environment_light_mode_clamp(int mode) {
    if (mode < ENVIRONMENT_LIGHT_MODE_OFF) return ENVIRONMENT_LIGHT_MODE_OFF;
    if (mode > ENVIRONMENT_LIGHT_MODE_AMBIENT) return ENVIRONMENT_LIGHT_MODE_OFF;
    return mode;
}

int animation_config_environment_preset_clamp(int preset) {
    if (preset < ENVIRONMENT_PRESET_NEUTRAL) return ENVIRONMENT_PRESET_SKY;
    if (preset > ENVIRONMENT_PRESET_WARM_SKY) return ENVIRONMENT_PRESET_SKY;
    return preset;
}

int animation_config_space_mode_clamp(int mode) {
    if (mode < SPACE_MODE_2D) return SPACE_MODE_2D;
    if (mode > SPACE_MODE_3D) return SPACE_MODE_2D;
    return mode;
}

int animation_config_scene_source_clamp(int source) {
    if (source < SCENE_SOURCE_CONFIG_2D) return SCENE_SOURCE_CONFIG_2D;
    if (source > SCENE_SOURCE_RUNTIME_SCENE) return SCENE_SOURCE_CONFIG_2D;
    return source;
}

int animation_config_volume_source_kind_clamp(int kind) {
    if (kind < VOLUME_SOURCE_NONE) return VOLUME_SOURCE_NONE;
    if (kind > VOLUME_SOURCE_PACK) return VOLUME_SOURCE_NONE;
    return kind;
}

bool animation_config_scene_source_is_fluid(int source) {
    return animation_config_scene_source_clamp(source) == SCENE_SOURCE_FLUID_MANIFEST;
}

static void animation_config_copy_path(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0u) return;
    if (!src) src = "";
    strncpy(dst, src, dst_size - 1u);
    dst[dst_size - 1u] = '\0';
}

void animation_config_scene_source_state_capture(const AnimationConfig* cfg,
                                                 AnimationConfigSceneSourceState* out_state) {
    if (!cfg || !out_state) return;
    out_state->source = animation_config_scene_source_clamp(cfg->sceneSource);
    out_state->useFluidScene = cfg->useFluidScene;
    animation_config_copy_path(out_state->fluidManifest,
                               sizeof(out_state->fluidManifest),
                               cfg->fluidManifest);
    animation_config_copy_path(out_state->runtimeScenePath,
                               sizeof(out_state->runtimeScenePath),
                               cfg->runtimeScenePath);
    out_state->volumeInteractionEnabled = cfg->volumeInteractionEnabled;
    out_state->volumeSourceKind =
        animation_config_volume_source_kind_clamp(cfg->volumeSourceKind);
    animation_config_copy_path(out_state->volumeSourcePath,
                               sizeof(out_state->volumeSourcePath),
                               cfg->volumeSourcePath);
}

void animation_config_scene_source_state_restore(AnimationConfig* cfg,
                                                 const AnimationConfigSceneSourceState* state) {
    if (!cfg || !state) return;
    cfg->sceneSource = (SceneSource)animation_config_scene_source_clamp(state->source);
    cfg->useFluidScene = state->useFluidScene;
    animation_config_copy_path(cfg->fluidManifest,
                               sizeof(cfg->fluidManifest),
                               state->fluidManifest);
    animation_config_copy_path(cfg->runtimeScenePath,
                               sizeof(cfg->runtimeScenePath),
                               state->runtimeScenePath);
    cfg->volumeInteractionEnabled = state->volumeInteractionEnabled;
    cfg->volumeSourceKind =
        animation_config_volume_source_kind_clamp(state->volumeSourceKind);
    animation_config_copy_path(cfg->volumeSourcePath,
                               sizeof(cfg->volumeSourcePath),
                               state->volumeSourcePath);
}

bool animation_config_set_scene_source_selection(AnimationConfig* cfg,
                                                int source,
                                                const char* path) {
    if (!cfg) return false;
    source = animation_config_scene_source_clamp(source);
    if (source == SCENE_SOURCE_CONFIG_2D) {
        cfg->sceneSource = SCENE_SOURCE_CONFIG_2D;
        cfg->useFluidScene = false;
        cfg->fluidManifest[0] = '\0';
        cfg->runtimeScenePath[0] = '\0';
        return true;
    }
    if (!path || !path[0]) {
        return false;
    }
    if (source == SCENE_SOURCE_FLUID_MANIFEST) {
        cfg->sceneSource = SCENE_SOURCE_FLUID_MANIFEST;
        cfg->useFluidScene = true;
        animation_config_copy_path(cfg->fluidManifest, sizeof(cfg->fluidManifest), path);
        cfg->runtimeScenePath[0] = '\0';
        return true;
    }
    if (source == SCENE_SOURCE_RUNTIME_SCENE) {
        cfg->sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
        cfg->useFluidScene = false;
        animation_config_copy_path(cfg->runtimeScenePath,
                                   sizeof(cfg->runtimeScenePath),
                                   path);
        cfg->fluidManifest[0] = '\0';
        return true;
    }
    return false;
}

bool animation_config_set_volume_source_selection(AnimationConfig* cfg,
                                                 int kind,
                                                 const char* path) {
    if (!cfg) return false;
    kind = animation_config_volume_source_kind_clamp(kind);
    if (kind == VOLUME_SOURCE_NONE || !path || !path[0]) {
        return false;
    }
    cfg->volumeSourceKind = kind;
    animation_config_copy_path(cfg->volumeSourcePath, sizeof(cfg->volumeSourcePath), path);
    cfg->volumeInteractionEnabled = true;
    return true;
}

void animation_config_clear_volume_source_selection(AnimationConfig* cfg) {
    if (!cfg) return;
    cfg->volumeInteractionEnabled = false;
    cfg->volumeSourceKind = VOLUME_SOURCE_NONE;
    cfg->volumeSourcePath[0] = '\0';
}

static void animation_config_sync_scene_source_legacy_fields(AnimationConfig* cfg) {
    if (!cfg) return;
    cfg->sceneSource = animation_config_scene_source_clamp(cfg->sceneSource);
    cfg->useFluidScene = animation_config_scene_source_is_fluid(cfg->sceneSource);
}

static void animation_config_sync_volume_source_fields(AnimationConfig* cfg) {
    if (!cfg) return;
    cfg->volumeSourceKind = animation_config_volume_source_kind_clamp(cfg->volumeSourceKind);
    if (cfg->volumeSourceKind == VOLUME_SOURCE_NONE) {
        cfg->volumeInteractionEnabled = false;
    }
    if (cfg->volumeSourceKind != VOLUME_SOURCE_NONE &&
        cfg->volumeSourcePath[0] == '\0') {
        cfg->volumeInteractionEnabled = false;
    }
}

int animation_config_scale_text_point_size(const AnimationConfig* cfg,
                                           int base_point_size,
                                           int min_point_size) {
    int percent = 100;
    double scaled = 0.0;
    int out_size = 0;
    if (base_point_size <= 0) base_point_size = 1;
    if (min_point_size <= 0) min_point_size = 1;
    if (cfg) {
        percent = animation_config_text_zoom_percent_from_step(cfg->textZoomStep);
    }
    scaled = ((double)base_point_size * (double)percent) / 100.0;
    out_size = (int)lround(scaled);
    if (out_size < min_point_size) out_size = min_point_size;
    return out_size;
}

static double DefaultForwardFalloffDistance(void) {
    double w = (sceneSettings.windowWidth > 0) ? sceneSettings.windowWidth : 1200.0;
    double h = (sceneSettings.windowHeight > 0) ? sceneSettings.windowHeight : 800.0;
    return hypot(w, h);
}

void SaveAnimationConfig(void) {
    if (!config_io_ensure_parent_directory_for_file(ANIMATION_CONFIG_RUNTIME_FILE)) {
        fprintf(stderr, "Error: Failed to prepare runtime config lane for %s\n", ANIMATION_CONFIG_RUNTIME_FILE);
        return;
    }
    FILE* file = fopen(ANIMATION_CONFIG_RUNTIME_FILE, "w");
    if (!file) {
        perror("Failed to open animation config file for writing");
        return;
    }

    struct json_object* config = json_object_new_object();
    animation_config_sync_scene_source_legacy_fields(&animSettings);
    animation_config_sync_volume_source_fields(&animSettings);
    animSettings.runtimeWindowWidth =
        animation_config_runtime_window_dimension_clamp(sceneSettings.windowWidth, 1200);
    animSettings.runtimeWindowHeight =
        animation_config_runtime_window_dimension_clamp(sceneSettings.windowHeight, 800);

    json_object_object_add(config, "interactiveMode", json_object_new_boolean(animSettings.interactiveMode));
    json_object_object_add(config, "deepRenderMode", json_object_new_boolean(animSettings.deepRenderMode));
    json_object_object_add(config,
                           "asyncDeepRender",
                           json_object_new_boolean(animSettings.asyncDeepRender));
    json_object_object_add(config, "bounceMode", json_object_new_boolean(animSettings.bounceMode));
    json_object_object_add(config, "autoMP4", json_object_new_boolean(animSettings.autoMP4));
    json_object_object_add(config, "bounceLimit", json_object_new_int(animSettings.bounceLimit));
    json_object_object_add(config, "frameLimit", json_object_new_int(animSettings.frameLimit));
    json_object_object_add(config, "framesForTravel", json_object_new_int(animSettings.framesForTravel));
    json_object_object_add(config, "startFrameIndex", json_object_new_int(animSettings.startFrameIndex));
    json_object_object_add(config,
                           "resumeFromExistingFrames",
                           json_object_new_boolean(animSettings.resumeFromExistingFrames));
    json_object_object_add(config, "fps", json_object_new_int(animSettings.fps));
    json_object_object_add(config, "frameDuration", json_object_new_double(animSettings.frameDuration));
    config_runtime_paths_normalize_data_roots();
    json_object_object_add(config, "inputRoot", json_object_new_string(animSettings.inputRoot));
    json_object_object_add(config, "meshAssetRoot", json_object_new_string(animSettings.meshAssetRoot));
    json_object_object_add(config, "outputRoot", json_object_new_string(animSettings.outputRoot));
    config_runtime_paths_normalize_frame_dir();
    json_object_object_add(config, "frameDir", json_object_new_string(animSettings.frameDir));
    config_runtime_paths_normalize_video_output_root();
    json_object_object_add(config, "videoOutputRoot", json_object_new_string(animSettings.videoOutputRoot));
    json_object_object_add(config, "maxLoopCount", json_object_new_int(animSettings.maxLoopCount));
    json_object_object_add(config, "loopMode", json_object_new_string(animSettings.loopMode));
    json_object_object_add(config, "lightMode", json_object_new_int(animSettings.lightMode));
    json_object_object_add(config, "blurMode", json_object_new_int(animSettings.blurMode));
    json_object_object_add(config, "lightDiffusionEnabled", json_object_new_boolean(animSettings.lightDiffusionEnabled));
    json_object_object_add(config, "lightDiffusionRadius", json_object_new_int(animSettings.lightDiffusionRadius));
    json_object_object_add(config, "lightDiffusionStrength", json_object_new_double(animSettings.lightDiffusionStrength));
    json_object_object_add(config, "editorMode", json_object_new_int(animSettings.editorMode));
    json_object_object_add(config, "spaceMode",
                           json_object_new_int(animation_config_space_mode_clamp(animSettings.spaceMode)));
    json_object_object_add(config, "textZoomStep",
                           json_object_new_int(animation_config_text_zoom_step_clamp(animSettings.textZoomStep)));
    json_object_object_add(config, "useTiledRenderer", json_object_new_boolean(animSettings.useTiledRenderer));
    json_object_object_add(config, "tilePreviewEnabled", json_object_new_boolean(animSettings.tilePreviewEnabled));
    json_object_object_add(config, "tileSize", json_object_new_int(animSettings.tileSize));
    json_object_object_add(config, "rouletteThreshold", json_object_new_double(animSettings.rouletteThreshold));
    RayTracingIntegratorCatalog_NormalizeAnimationConfig(&animSettings);
    json_object_object_add(config, "integratorMode", json_object_new_int(animSettings.integratorMode));
    json_object_object_add(config, "integratorMode3D", json_object_new_int(animSettings.integratorMode3D));
    json_object_object_add(config, "previewDuration", json_object_new_double(animSettings.previewDuration));
    json_object_object_add(config, "pathSamplesPerPixel", json_object_new_int(animSettings.pathSamplesPerPixel));
    json_object_object_add(config, "pathMaxDepth", json_object_new_int(animSettings.pathMaxDepth));
    json_object_object_add(config, "pathDirectLighting", json_object_new_boolean(animSettings.pathDirectLighting));
    json_object_object_add(config, "pathRussianRoulette", json_object_new_boolean(animSettings.pathRussianRoulette));
    json_object_object_add(config, "pathEnableMIS", json_object_new_boolean(animSettings.pathEnableMIS));
    json_object_object_add(config, "environmentBrightness", json_object_new_double(animSettings.environmentBrightness));
    json_object_object_add(config, "environmentBrightnessUsesByteFloor", json_object_new_boolean(true));
    json_object_object_add(config,
                           "environmentBackgroundLightingAuthored",
                           json_object_new_boolean(
                               animSettings.environmentBackgroundLightingAuthored));
    json_object_object_add(config,
                           "environmentPreset",
                           json_object_new_int(animation_config_environment_preset_clamp(
                               animSettings.environmentPreset)));
    json_object_object_add(config,
                           "environmentBackgroundBrightnessAuto",
                           json_object_new_boolean(animSettings.environmentBackgroundBrightnessAuto));
    json_object_object_add(config,
                           "environmentBackgroundBrightness",
                           json_object_new_double(animSettings.environmentBackgroundBrightness));
    json_object_object_add(config,
                           "environmentBackgroundColorR",
                           json_object_new_double(animSettings.environmentBackgroundColorR));
    json_object_object_add(config,
                           "environmentBackgroundColorG",
                           json_object_new_double(animSettings.environmentBackgroundColorG));
    json_object_object_add(config,
                           "environmentBackgroundColorB",
                           json_object_new_double(animSettings.environmentBackgroundColorB));
    json_object_object_add(config, "pathSeed", json_object_new_int(animSettings.pathSeed));
    json_object_object_add(config, "cacheContributionWeight", json_object_new_double(animSettings.cacheContributionWeight));
    json_object_object_add(config, "bsdfModel", json_object_new_int(animSettings.bsdfModel));
    json_object_object_add(config, "lightIntensity", json_object_new_double(animSettings.lightIntensity));
    json_object_object_add(config, "forwardDecay", json_object_new_double(animSettings.forwardDecay));
    json_object_object_add(config, "forwardFalloffMode", json_object_new_int(animSettings.forwardFalloffMode));
    json_object_object_add(config, "renderQuality", json_object_new_int(animSettings.renderQuality));
    json_object_object_add(config, "cacheVarianceCutoff", json_object_new_double(animSettings.cacheVarianceCutoff));
    json_object_object_add(config, "cacheHaloRadius", json_object_new_double(animSettings.cacheHaloRadius));
    json_object_object_add(config, "lightDecaySoftness", json_object_new_double(animSettings.lightDecaySoftness));
    json_object_object_add(config, "lightRadius", json_object_new_double(animSettings.lightRadius));
    json_object_object_add(config, "lightHeight", json_object_new_double(animSettings.lightHeight));
    json_object_object_add(config,
                           "environmentLightMode",
                           json_object_new_int(animation_config_environment_light_mode_clamp(
                               animSettings.environmentLightMode)));
    json_object_object_add(config, "topFillStrength",
                           json_object_new_double(animSettings.topFillStrength));
    json_object_object_add(config, "disneyDenoiseEnabled",
                           json_object_new_boolean(animSettings.disneyDenoiseEnabled));
    json_object_object_add(config,
                           "bounceDepth3D",
                           json_object_new_int(animSettings.bounceDepth3D));
    json_object_object_add(config,
                           "specularDepth3D",
                           json_object_new_int(animSettings.specularDepth3D));
    json_object_object_add(config,
                           "transmissionDepth3D",
                           json_object_new_int(animSettings.transmissionDepth3D));
    json_object_object_add(config,
                           "rouletteThreshold3D",
                           json_object_new_double(animSettings.rouletteThreshold3D));
    json_object_object_add(config,
                           "secondaryDiffuseSamples3D",
                           json_object_new_int(animSettings.secondaryDiffuseSamples3D));
    json_object_object_add(config,
                           "transmissionSamples3D",
                           json_object_new_int(animSettings.transmissionSamples3D));
    json_object_object_add(config,
                           "temporalFrames3D",
                           json_object_new_int(animSettings.temporalFrames3D));
    json_object_object_add(config,
                           "renderScale3D",
                           json_object_new_int(animSettings.renderScale3D));
    json_object_object_add(config,
                           "upscaleMode3D",
                           json_object_new_int(animSettings.upscaleMode3D));
    json_object_object_add(config, "causticMode3D",
                           json_object_new_int(animSettings.causticMode3D));
    json_object_object_add(config, "causticTransportEngine3D",
                           json_object_new_int(animSettings.causticTransportEngine3D));
    json_object_object_add(config, "causticSurfaceCacheEnabled3D",
                           json_object_new_boolean(animSettings.causticSurfaceCacheEnabled3D));
    json_object_object_add(config, "causticVolumeCacheEnabled3D",
                           json_object_new_boolean(animSettings.causticVolumeCacheEnabled3D));
    json_object_object_add(config, "causticSampleBudget3D",
                           json_object_new_int(animSettings.causticSampleBudget3D));
    json_object_object_add(config, "causticMaxPathDepth3D",
                           json_object_new_int(animSettings.causticMaxPathDepth3D));
    json_object_object_add(config, "causticDebugSummaryEnabled3D",
                           json_object_new_boolean(animSettings.causticDebugSummaryEnabled3D));
    json_object_object_add(config, "causticDebugExportEnabled3D",
                           json_object_new_boolean(animSettings.causticDebugExportEnabled3D));
    json_object_object_add(config, "menuWorkspaceModule",
                           json_object_new_int(animSettings.menuWorkspaceModule));
    json_object_object_add(config, "menuPaneSceneWidth",
                           json_object_new_int(animSettings.menuPaneSceneWidth));
    json_object_object_add(config, "menuPaneHealthWidth",
                           json_object_new_int(animSettings.menuPaneHealthWidth));
    json_object_object_add(config,
                           "runtimeWindowWidth",
                           json_object_new_int(animSettings.runtimeWindowWidth));
    json_object_object_add(config,
                           "runtimeWindowHeight",
                           json_object_new_int(animSettings.runtimeWindowHeight));
    json_object_object_add(config, "sceneSource",
                           json_object_new_int(animation_config_scene_source_clamp(animSettings.sceneSource)));
    json_object_object_add(config, "useFluidScene", json_object_new_boolean(animSettings.useFluidScene));
    json_object_object_add(config, "fluidManifest", json_object_new_string(animSettings.fluidManifest));
    json_object_object_add(config, "runtimeScenePath", json_object_new_string(animSettings.runtimeScenePath));
    json_object_object_add(config,
                           "volumeInteractionEnabled",
                           json_object_new_boolean(animSettings.volumeInteractionEnabled));
    json_object_object_add(config,
                           "volumeSourceKind",
                           json_object_new_int(animation_config_volume_source_kind_clamp(
                               animSettings.volumeSourceKind)));
    json_object_object_add(config,
                           "volumeSourcePath",
                           json_object_new_string(animSettings.volumeSourcePath));
    json_object_object_add(config,
                           "volumeAffectsLighting",
                           json_object_new_boolean(animSettings.volumeAffectsLighting));
    json_object_object_add(config,
                           "volumeDebugOverlayEnabled",
                           json_object_new_boolean(animSettings.volumeDebugOverlayEnabled));
    json_object_object_add(config, "lightHeight", json_object_new_double(animSettings.lightHeight));
    fprintf(file, "%s", json_object_to_json_string_ext(config, JSON_C_TO_STRING_PRETTY));
    fclose(file);
    json_object_put(config);

    printf("✅ Animation config saved successfully.\n");
}

void LoadAnimationConfig(void) {
    const char* loaded_path = NULL;
    animSettings.startFrameIndex = 0;
    animSettings.resumeFromExistingFrames = false;
    FILE* file = OpenAnimationConfigRead(&loaded_path);
    if (!file) {
        printf("INFO: Animation config file not found (tried %s, %s, %s); using defaults.\n",
               ANIMATION_CONFIG_RUNTIME_FILE,
               ANIMATION_CONFIG_DEFAULT_FILE,
               ANIMATION_CONFIG_LEGACY_FILE);
        return;
    }
    if (loaded_path) {
        printf("INFO: Loaded animation config from: %s\n", loaded_path);
    }

    struct json_object* config = config_io_parse_json_file(file, "animation config", true);
    if (!config) {
        return;
    }

    struct json_object* temp;
    struct json_object* authored_environment_temp = NULL;
    struct json_object* authored_environment_key_temp = NULL;
    bool has_scene_source = false;
    bool has_environment_background_lighting_authored =
        json_object_object_get_ex(config,
                                  "environmentBackgroundLightingAuthored",
                                  &authored_environment_temp) &&
        json_object_is_type(authored_environment_temp, json_type_boolean);
    bool has_environment_background_authored_field =
        json_object_object_get_ex(config, "environmentPreset", &authored_environment_key_temp) ||
        json_object_object_get_ex(config,
                                  "environmentBackgroundBrightnessAuto",
                                  &authored_environment_key_temp) ||
        json_object_object_get_ex(config,
                                  "environmentBackgroundBrightness",
                                  &authored_environment_key_temp) ||
        json_object_object_get_ex(config,
                                  "environmentBackgroundColorR",
                                  &authored_environment_key_temp) ||
        json_object_object_get_ex(config,
                                  "environmentBackgroundColorG",
                                  &authored_environment_key_temp) ||
        json_object_object_get_ex(config,
                                  "environmentBackgroundColorB",
                                  &authored_environment_key_temp);

    if (json_object_object_get_ex(config, "interactiveMode", &temp))
        animSettings.interactiveMode = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "deepRenderMode", &temp))
        animSettings.deepRenderMode = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "asyncDeepRender", &temp))
        animSettings.asyncDeepRender = json_object_get_boolean(temp);
    else
        animSettings.asyncDeepRender = false;
    if (json_object_object_get_ex(config, "bounceMode", &temp))
        animSettings.bounceMode = json_object_get_boolean(temp);
    if (animSettings.deepRenderMode) {
        animSettings.interactiveMode = false;
    }
    if (json_object_object_get_ex(config, "autoMP4", &temp))
        animSettings.autoMP4 = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "bounceLimit", &temp))
        animSettings.bounceLimit = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "frameLimit", &temp))
        animSettings.frameLimit = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "framesForTravel", &temp))
        animSettings.framesForTravel = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "startFrameIndex", &temp)) {
        animSettings.startFrameIndex = json_object_get_int(temp);
    } else {
        animSettings.startFrameIndex = 0;
    }
    if (json_object_object_get_ex(config, "resumeFromExistingFrames", &temp) &&
        json_object_is_type(temp, json_type_boolean)) {
        animSettings.resumeFromExistingFrames = json_object_get_boolean(temp);
    } else {
        animSettings.resumeFromExistingFrames = false;
    }
    if (json_object_object_get_ex(config, "fps", &temp)) {
        animSettings.fps = json_object_get_int(temp);
        animSettings.frameDuration = (animSettings.fps > 0) ? 1.0 / animSettings.fps : 1.0 / 30.0;
    }
    if (json_object_object_get_ex(config, "inputRoot", &temp) && json_object_is_type(temp, json_type_string)) {
        const char* path = json_object_get_string(temp);
        if (path) {
            strncpy(animSettings.inputRoot, path, sizeof(animSettings.inputRoot) - 1);
            animSettings.inputRoot[sizeof(animSettings.inputRoot) - 1] = '\0';
        }
    }
    if (json_object_object_get_ex(config, "meshAssetRoot", &temp) && json_object_is_type(temp, json_type_string)) {
        const char* path = json_object_get_string(temp);
        if (path) {
            strncpy(animSettings.meshAssetRoot, path, sizeof(animSettings.meshAssetRoot) - 1);
            animSettings.meshAssetRoot[sizeof(animSettings.meshAssetRoot) - 1] = '\0';
        }
    }
    if (json_object_object_get_ex(config, "outputRoot", &temp) && json_object_is_type(temp, json_type_string)) {
        const char* path = json_object_get_string(temp);
        if (path) {
            strncpy(animSettings.outputRoot, path, sizeof(animSettings.outputRoot) - 1);
            animSettings.outputRoot[sizeof(animSettings.outputRoot) - 1] = '\0';
        }
    }
    config_runtime_paths_normalize_data_roots();
    (void)setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    if (animSettings.meshAssetRoot[0]) {
        (void)setenv("RAY_TRACING_MESH_ASSET_ROOT", animSettings.meshAssetRoot, 1);
    } else {
        (void)unsetenv("RAY_TRACING_MESH_ASSET_ROOT");
    }
    (void)setenv("RAY_TRACING_OUTPUT_ROOT", animSettings.outputRoot, 1);
    if (json_object_object_get_ex(config, "loopMode", &temp)) {
        strncpy(animSettings.loopMode, json_object_get_string(temp), sizeof(animSettings.loopMode) - 1);
        animSettings.loopMode[sizeof(animSettings.loopMode) - 1] = '\0';
    }
    if (json_object_object_get_ex(config, "maxLoopCount", &temp))
        animSettings.maxLoopCount = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "frameDir", &temp)) {
        const char* dir = json_object_get_string(temp);
        if (dir) {
            strncpy(animSettings.frameDir, dir, sizeof(animSettings.frameDir) - 1);
            animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
        }
    }
    config_runtime_paths_normalize_frame_dir();
    if (json_object_object_get_ex(config, "videoOutputRoot", &temp) &&
        json_object_is_type(temp, json_type_string)) {
        const char* root = json_object_get_string(temp);
        if (root) {
            strncpy(animSettings.videoOutputRoot, root, sizeof(animSettings.videoOutputRoot) - 1);
            animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
        }
    } else {
        char migrated_video_root[sizeof(animSettings.videoOutputRoot)];
        if (ray_tracing_compose_path(animSettings.outputRoot,
                                     "videos",
                                     migrated_video_root,
                                     sizeof(migrated_video_root))) {
            strncpy(animSettings.videoOutputRoot,
                    migrated_video_root,
                    sizeof(animSettings.videoOutputRoot) - 1);
            animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
        }
    }
    config_runtime_paths_normalize_video_output_root();
    (void)setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", animSettings.videoOutputRoot, 1);
    if (json_object_object_get_ex(config, "lightMode", &temp))
        animSettings.lightMode = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "blurMode", &temp))
        animSettings.blurMode = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "lightDiffusionEnabled", &temp))
        animSettings.lightDiffusionEnabled = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "lightDiffusionRadius", &temp))
        animSettings.lightDiffusionRadius = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "lightDiffusionStrength", &temp))
        animSettings.lightDiffusionStrength = json_object_get_double(temp);
    if (json_object_object_get_ex(config, "editorMode", &temp))
        animSettings.editorMode = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "spaceMode", &temp)) {
        animSettings.spaceMode = animation_config_space_mode_clamp(json_object_get_int(temp));
    } else if (json_object_object_get_ex(config, "space_mode", &temp)) {
        animSettings.spaceMode = animation_config_space_mode_clamp(json_object_get_int(temp));
    } else {
        animSettings.spaceMode = SPACE_MODE_2D;
    }
    if (json_object_object_get_ex(config, "textZoomStep", &temp)) {
        animSettings.textZoomStep = json_object_get_int(temp);
    } else if (json_object_object_get_ex(config, "text_zoom_step", &temp)) {
        animSettings.textZoomStep = json_object_get_int(temp);
    } else {
        animSettings.textZoomStep = 0;
    }
    if (json_object_object_get_ex(config, "useTiledRenderer", &temp))
        animSettings.useTiledRenderer = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "tilePreviewEnabled", &temp))
        animSettings.tilePreviewEnabled = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "tileSize", &temp))
        animSettings.tileSize = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "rouletteThreshold", &temp))
        animSettings.rouletteThreshold = json_object_get_double(temp);
    if (json_object_object_get_ex(config, "integratorMode", &temp))
        animSettings.integratorMode = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "integratorMode3D", &temp)) {
        animSettings.integratorMode3D = json_object_get_int(temp);
    } else if (json_object_object_get_ex(config, "integrator_mode_3d", &temp)) {
        animSettings.integratorMode3D = json_object_get_int(temp);
    } else {
        animSettings.integratorMode3D = RayTracingIntegratorCatalog_Default3D();
    }
    RayTracingIntegratorCatalog_NormalizeAnimationConfig(&animSettings);
    if (json_object_object_get_ex(config, "previewDuration", &temp)) {
        animSettings.previewDuration = json_object_get_double(temp);
        if (animSettings.previewDuration <= 0.1) animSettings.previewDuration = 5.0;
    } else {
        animSettings.previewDuration = 5.0;
    }
    if (json_object_object_get_ex(config, "pathSamplesPerPixel", &temp))
        animSettings.pathSamplesPerPixel = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "pathMaxDepth", &temp))
        animSettings.pathMaxDepth = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "pathDirectLighting", &temp))
        animSettings.pathDirectLighting = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "pathRussianRoulette", &temp))
        animSettings.pathRussianRoulette = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "pathEnableMIS", &temp))
        animSettings.pathEnableMIS = json_object_get_boolean(temp);
    if (json_object_object_get_ex(config, "environmentBrightness", &temp)) {
        struct json_object* env_mode = NULL;
        const bool uses_byte_floor =
            json_object_object_get_ex(config, "environmentBrightnessUsesByteFloor", &env_mode) &&
            json_object_get_boolean(env_mode);
        const double raw_environment = json_object_get_double(temp);
        if (uses_byte_floor) {
            animSettings.environmentBrightness = ClampDoubleValue(raw_environment, 0.0, 255.0);
        } else {
            animSettings.environmentBrightness =
                animation_config_legacy_environment_radiance_to_byte_floor(raw_environment);
        }
    }
    if (json_object_object_get_ex(config, "environmentPreset", &temp)) {
        animSettings.environmentPreset =
            animation_config_environment_preset_clamp(json_object_get_int(temp));
    } else {
        animSettings.environmentPreset = ENVIRONMENT_PRESET_SKY;
    }
    if (json_object_object_get_ex(config, "environmentBackgroundBrightnessAuto", &temp) &&
        json_object_is_type(temp, json_type_boolean)) {
        animSettings.environmentBackgroundBrightnessAuto = json_object_get_boolean(temp) != 0;
    } else {
        animSettings.environmentBackgroundBrightnessAuto = true;
    }
    if (json_object_object_get_ex(config, "environmentBackgroundBrightness", &temp)) {
        animSettings.environmentBackgroundBrightness = json_object_get_double(temp);
    } else {
        animSettings.environmentBackgroundBrightness = 0.0;
    }
    if (json_object_object_get_ex(config, "environmentBackgroundColorR", &temp)) {
        animSettings.environmentBackgroundColorR = json_object_get_double(temp);
    } else {
        animSettings.environmentBackgroundColorR = 1.0;
    }
    if (json_object_object_get_ex(config, "environmentBackgroundColorG", &temp)) {
        animSettings.environmentBackgroundColorG = json_object_get_double(temp);
    } else {
        animSettings.environmentBackgroundColorG = 1.0;
    }
    if (json_object_object_get_ex(config, "environmentBackgroundColorB", &temp)) {
        animSettings.environmentBackgroundColorB = json_object_get_double(temp);
    } else {
        animSettings.environmentBackgroundColorB = 1.0;
    }
    animSettings.environmentBackgroundLightingAuthored =
        has_environment_background_lighting_authored
            ? (json_object_get_boolean(authored_environment_temp) != 0)
            : has_environment_background_authored_field;
    if (json_object_object_get_ex(config, "pathSeed", &temp))
        animSettings.pathSeed = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "cacheContributionWeight", &temp))
        animSettings.cacheContributionWeight = json_object_get_double(temp);
    if (json_object_object_get_ex(config, "bsdfModel", &temp))
        animSettings.bsdfModel = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "lightIntensity", &temp)) {
        animSettings.lightIntensity = json_object_get_double(temp);
    } else {
        animSettings.lightIntensity = RAY_TRACING_DEFAULT_LIGHT_INTENSITY;
    }
    if (json_object_object_get_ex(config, "forwardDecay", &temp))
        animSettings.forwardDecay = json_object_get_double(temp);
    if (json_object_object_get_ex(config, "forwardFalloffMode", &temp))
        animSettings.forwardFalloffMode = json_object_get_int(temp);
    if (json_object_object_get_ex(config, "renderQuality", &temp))
        animSettings.renderQuality = (RenderQuality)json_object_get_int(temp);
    if (json_object_object_get_ex(config, "cacheVarianceCutoff", &temp)) {
        animSettings.cacheVarianceCutoff = json_object_get_double(temp);
    } else {
        animSettings.cacheVarianceCutoff = 0.35;
    }
    if (json_object_object_get_ex(config, "cacheHaloRadius", &temp)) {
        animSettings.cacheHaloRadius = json_object_get_double(temp);
    } else {
        animSettings.cacheHaloRadius = 3.5;
    }
    if (json_object_object_get_ex(config, "lightDecaySoftness", &temp)) {
        animSettings.lightDecaySoftness = json_object_get_double(temp);
    } else {
        animSettings.lightDecaySoftness = 1.0;
    }
    if (json_object_object_get_ex(config, "lightRadius", &temp)) {
        animSettings.lightRadius = json_object_get_double(temp);
    } else {
        animSettings.lightRadius = 0.0;
    }
    if (json_object_object_get_ex(config, "lightHeight", &temp)) {
        animSettings.lightHeight = json_object_get_double(temp);
    } else {
        animSettings.lightHeight = 8.0;
    }
    if (json_object_object_get_ex(config, "environmentLightMode", &temp)) {
        animSettings.environmentLightMode =
            animation_config_environment_light_mode_clamp(json_object_get_int(temp));
    } else if (json_object_object_get_ex(config, "topFillLightEnabled", &temp) &&
               json_object_is_type(temp, json_type_boolean) &&
               json_object_get_boolean(temp)) {
        animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_TOP_FILL;
    } else {
        animSettings.environmentLightMode = ENVIRONMENT_LIGHT_MODE_OFF;
    }
    if (json_object_object_get_ex(config, "topFillStrength", &temp)) {
        animSettings.topFillStrength = json_object_get_double(temp);
    } else {
        animSettings.topFillStrength = 1.0;
    }
    if (json_object_object_get_ex(config, "disneyDenoiseEnabled", &temp) &&
        json_object_is_type(temp, json_type_boolean)) {
        animSettings.disneyDenoiseEnabled = json_object_get_boolean(temp);
    } else {
        animSettings.disneyDenoiseEnabled = true;
    }
    if (json_object_object_get_ex(config, "bounceDepth3D", &temp)) {
        animSettings.bounceDepth3D = json_object_get_int(temp);
    } else {
        animSettings.bounceDepth3D = RUNTIME_3D_BOUNCE_DEPTH_DEFAULT;
    }
    if (json_object_object_get_ex(config, "specularDepth3D", &temp)) {
        animSettings.specularDepth3D = json_object_get_int(temp);
    } else {
        animSettings.specularDepth3D = RUNTIME_3D_SPECULAR_DEPTH_DEFAULT;
    }
    if (json_object_object_get_ex(config, "transmissionDepth3D", &temp)) {
        animSettings.transmissionDepth3D = json_object_get_int(temp);
    } else {
        animSettings.transmissionDepth3D = RUNTIME_3D_TRANSMISSION_DEPTH_DEFAULT;
    }
    if (json_object_object_get_ex(config, "rouletteThreshold3D", &temp)) {
        animSettings.rouletteThreshold3D = json_object_get_double(temp);
    } else {
        animSettings.rouletteThreshold3D = RUNTIME_3D_ROULETTE_THRESHOLD_DEFAULT;
    }
    if (json_object_object_get_ex(config, "secondaryDiffuseSamples3D", &temp)) {
        animSettings.secondaryDiffuseSamples3D = json_object_get_int(temp);
    } else {
        animSettings.secondaryDiffuseSamples3D = RUNTIME_3D_SECONDARY_SAMPLES_DEFAULT;
    }
    if (json_object_object_get_ex(config, "transmissionSamples3D", &temp)) {
        animSettings.transmissionSamples3D = json_object_get_int(temp);
    } else {
        animSettings.transmissionSamples3D = RUNTIME_3D_TRANSMISSION_SAMPLES_DEFAULT;
    }
    if (json_object_object_get_ex(config, "temporalFrames3D", &temp)) {
        animSettings.temporalFrames3D = json_object_get_int(temp);
    } else {
        animSettings.temporalFrames3D = RUNTIME_3D_TEMPORAL_FRAMES_DEFAULT;
    }
    if (json_object_object_get_ex(config, "renderScale3D", &temp)) {
        animSettings.renderScale3D = json_object_get_int(temp);
    } else {
        animSettings.renderScale3D = RUNTIME_3D_RENDER_SCALE_DEFAULT;
    }
    if (json_object_object_get_ex(config, "upscaleMode3D", &temp)) {
        animSettings.upscaleMode3D = json_object_get_int(temp);
    } else {
        animSettings.upscaleMode3D = RUNTIME_3D_UPSCALE_MODE_DEFAULT;
    }
    if (json_object_object_get_ex(config, "causticMode3D", &temp)) {
        animSettings.causticMode3D = json_object_get_int(temp);
    } else {
        animSettings.causticMode3D = RUNTIME_3D_CAUSTIC_MODE_DEFAULT;
    }
    if (json_object_object_get_ex(config, "causticTransportEngine3D", &temp)) {
        animSettings.causticTransportEngine3D = json_object_get_int(temp);
    } else {
        animSettings.causticTransportEngine3D = RUNTIME_3D_CAUSTIC_ENGINE_DEFAULT;
    }
    animSettings.causticSurfaceCacheEnabled3D =
        json_object_object_get_ex(config, "causticSurfaceCacheEnabled3D", &temp) &&
        json_object_get_boolean(temp);
    animSettings.causticVolumeCacheEnabled3D =
        json_object_object_get_ex(config, "causticVolumeCacheEnabled3D", &temp) &&
        json_object_get_boolean(temp);
    animSettings.causticSampleBudget3D =
        json_object_object_get_ex(config, "causticSampleBudget3D", &temp)
            ? json_object_get_int(temp)
            : 0;
    animSettings.causticMaxPathDepth3D =
        json_object_object_get_ex(config, "causticMaxPathDepth3D", &temp)
            ? json_object_get_int(temp)
            : 0;
    animSettings.causticDebugSummaryEnabled3D =
        json_object_object_get_ex(config, "causticDebugSummaryEnabled3D", &temp) &&
        json_object_get_boolean(temp);
    animSettings.causticDebugExportEnabled3D =
        json_object_object_get_ex(config, "causticDebugExportEnabled3D", &temp) &&
        json_object_get_boolean(temp);
    animSettings.menuWorkspaceModule =
        json_object_object_get_ex(config, "menuWorkspaceModule", &temp)
            ? json_object_get_int(temp)
            : MENU_WORKSPACE_MODULE_DEFAULT;
    animSettings.menuPaneSceneWidth =
        json_object_object_get_ex(config, "menuPaneSceneWidth", &temp)
            ? json_object_get_int(temp)
            : MENU_PANE_SCENE_WIDTH_DEFAULT;
    animSettings.menuPaneHealthWidth =
        json_object_object_get_ex(config, "menuPaneHealthWidth", &temp)
            ? json_object_get_int(temp)
            : MENU_PANE_HEALTH_WIDTH_DEFAULT;
    if (json_object_object_get_ex(config, "runtimeWindowWidth", &temp)) {
        animSettings.runtimeWindowWidth = json_object_get_int(temp);
    } else {
        animSettings.runtimeWindowWidth = 0;
    }
    if (json_object_object_get_ex(config, "runtimeWindowHeight", &temp)) {
        animSettings.runtimeWindowHeight = json_object_get_int(temp);
    } else {
        animSettings.runtimeWindowHeight = 0;
    }
    if (json_object_object_get_ex(config, "sceneSource", &temp)) {
        animSettings.sceneSource = animation_config_scene_source_clamp(json_object_get_int(temp));
        has_scene_source = true;
    } else {
        animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
    }
    if (json_object_object_get_ex(config, "useFluidScene", &temp) && json_object_is_type(temp, json_type_boolean)) {
        animSettings.useFluidScene = json_object_get_boolean(temp);
    } else {
        animSettings.useFluidScene = false;
    }
    if (json_object_object_get_ex(config, "fluidManifest", &temp) && json_object_is_type(temp, json_type_string)) {
        const char* fm = json_object_get_string(temp);
        if (fm) {
            strncpy(animSettings.fluidManifest, fm, sizeof(animSettings.fluidManifest) - 1);
            animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
        }
    }
    if (json_object_object_get_ex(config, "runtimeScenePath", &temp) &&
        json_object_is_type(temp, json_type_string)) {
        const char* path = json_object_get_string(temp);
        if (path) {
            strncpy(animSettings.runtimeScenePath, path, sizeof(animSettings.runtimeScenePath) - 1);
            animSettings.runtimeScenePath[sizeof(animSettings.runtimeScenePath) - 1] = '\0';
        }
    } else {
        animSettings.runtimeScenePath[0] = '\0';
    }
    if (json_object_object_get_ex(config, "volumeInteractionEnabled", &temp) &&
        json_object_is_type(temp, json_type_boolean)) {
        animSettings.volumeInteractionEnabled = json_object_get_boolean(temp);
    } else {
        animSettings.volumeInteractionEnabled = false;
    }
    if (json_object_object_get_ex(config, "volumeSourceKind", &temp)) {
        animSettings.volumeSourceKind =
            animation_config_volume_source_kind_clamp(json_object_get_int(temp));
    } else {
        animSettings.volumeSourceKind = VOLUME_SOURCE_NONE;
    }
    if (json_object_object_get_ex(config, "volumeSourcePath", &temp) &&
        json_object_is_type(temp, json_type_string)) {
        const char* path = json_object_get_string(temp);
        if (path) {
            strncpy(animSettings.volumeSourcePath,
                    path,
                    sizeof(animSettings.volumeSourcePath) - 1);
            animSettings.volumeSourcePath[sizeof(animSettings.volumeSourcePath) - 1] = '\0';
        }
    } else {
        animSettings.volumeSourcePath[0] = '\0';
    }
    if (json_object_object_get_ex(config, "volumeAffectsLighting", &temp) &&
        json_object_is_type(temp, json_type_boolean)) {
        animSettings.volumeAffectsLighting = json_object_get_boolean(temp);
    } else {
        animSettings.volumeAffectsLighting = true;
    }
    if (json_object_object_get_ex(config, "volumeDebugOverlayEnabled", &temp) &&
        json_object_is_type(temp, json_type_boolean)) {
        animSettings.volumeDebugOverlayEnabled = json_object_get_boolean(temp);
    } else {
        animSettings.volumeDebugOverlayEnabled = false;
    }
    if (!has_scene_source) {
        animSettings.sceneSource = animSettings.useFluidScene
                                       ? SCENE_SOURCE_FLUID_MANIFEST
                                       : SCENE_SOURCE_CONFIG_2D;
    }

    bool root_corrected = false;
    animSettings.cacheContributionWeight = ClampDoubleValue(animSettings.cacheContributionWeight, 0.0, 1.0);
    if (animSettings.startFrameIndex < 0) {
        animSettings.startFrameIndex = 0;
    }
    animSettings.bsdfModel = (animSettings.bsdfModel != 0) ? 1 : 0;
    animSettings.lightIntensity = ClampDoubleValue(animSettings.lightIntensity, 0.0, 20.0);
    if (animSettings.forwardDecay <= 1.0) {
        double legacyDrop = 1.0 - ClampDoubleValue(animSettings.forwardDecay, 0.0, 0.999999);
        double scaleFactor = 1.0 / fmax(legacyDrop, 1e-6);
        animSettings.forwardDecay = DefaultForwardFalloffDistance() * scaleFactor;
    }
    if (animSettings.forwardDecay <= 0.0) {
        animSettings.forwardDecay = DefaultForwardFalloffDistance();
    }
    animSettings.forwardDecay = ClampDoubleValue(animSettings.forwardDecay, 50.0, 100000.0);

    if (animSettings.forwardFalloffMode < FORWARD_FALLOFF_MODE_QUADRATIC || animSettings.forwardFalloffMode > FORWARD_FALLOFF_MODE_NONE) {
        animSettings.forwardFalloffMode = FORWARD_FALLOFF_MODE_QUADRATIC;
    }
    if (animSettings.renderQuality < RENDER_QUALITY_LOW || animSettings.renderQuality > RENDER_QUALITY_HIGH) {
        animSettings.renderQuality = RENDER_QUALITY_MEDIUM;
    }
    if (animSettings.cacheVarianceCutoff <= 0.0) {
        animSettings.cacheVarianceCutoff = 0.35;
    }
    if (animSettings.cacheHaloRadius <= 0.0) {
        animSettings.cacheHaloRadius = 3.5;
    }
    if (animSettings.lightDecaySoftness <= 0.0) {
        animSettings.lightDecaySoftness = 1.0;
    }
    if (animSettings.lightDecaySoftness > 10.0) {
        animSettings.lightDecaySoftness = 10.0;
    }
    if (!isfinite(animSettings.lightRadius) || animSettings.lightRadius < 0.0) {
        animSettings.lightRadius = 0.0;
    }
    if (animSettings.lightRadius > 25.0) {
        animSettings.lightRadius = 25.0;
    }
    if (!isfinite(animSettings.lightHeight) || animSettings.lightHeight < 0.0) {
        animSettings.lightHeight = 8.0;
    }
    animSettings.environmentLightMode =
        animation_config_environment_light_mode_clamp(animSettings.environmentLightMode);
    animSettings.environmentPreset =
        animation_config_environment_preset_clamp(animSettings.environmentPreset);
    if (!isfinite(animSettings.environmentBackgroundBrightness) ||
        animSettings.environmentBackgroundBrightness < 0.0) {
        animSettings.environmentBackgroundBrightness = 0.0;
    }
    if (animSettings.environmentBackgroundBrightness > 4.0) {
        animSettings.environmentBackgroundBrightness = 4.0;
    }
    if (!isfinite(animSettings.environmentBackgroundColorR)) {
        animSettings.environmentBackgroundColorR = 1.0;
    }
    if (!isfinite(animSettings.environmentBackgroundColorG)) {
        animSettings.environmentBackgroundColorG = 1.0;
    }
    if (!isfinite(animSettings.environmentBackgroundColorB)) {
        animSettings.environmentBackgroundColorB = 1.0;
    }
    animSettings.environmentBackgroundColorR =
        ClampDoubleValue(animSettings.environmentBackgroundColorR, 0.0, 1.0);
    animSettings.environmentBackgroundColorG =
        ClampDoubleValue(animSettings.environmentBackgroundColorG, 0.0, 1.0);
    animSettings.environmentBackgroundColorB =
        ClampDoubleValue(animSettings.environmentBackgroundColorB, 0.0, 1.0);
    if (!isfinite(animSettings.topFillStrength) || animSettings.topFillStrength < 0.0) {
        animSettings.topFillStrength = 1.0;
    }
    if (animSettings.topFillStrength > 20.0) {
        animSettings.topFillStrength = 20.0;
    }
    animation_config_normalize_runtime3d_fields(&animSettings);
    animSettings.runtimeWindowWidth =
        animation_config_runtime_window_dimension_clamp(animSettings.runtimeWindowWidth,
                                                        sceneSettings.windowWidth);
    animSettings.runtimeWindowHeight =
        animation_config_runtime_window_dimension_clamp(animSettings.runtimeWindowHeight,
                                                        sceneSettings.windowHeight);
    root_corrected |= config_runtime_paths_validate_root(animSettings.inputRoot,
                                                         sizeof(animSettings.inputRoot),
                                                         ray_tracing_default_input_root(),
                                                         "input",
                                                         false,
                                                         false);
    if (animSettings.meshAssetRoot[0] && !config_io_directory_exists(animSettings.meshAssetRoot)) {
        fprintf(stderr,
                "[startup] mesh asset root '%s' missing; clearing mesh asset root.\n",
                animSettings.meshAssetRoot);
        animSettings.meshAssetRoot[0] = '\0';
        root_corrected = true;
    }
    root_corrected |= config_runtime_paths_validate_root(animSettings.outputRoot,
                                                         sizeof(animSettings.outputRoot),
                                                         ray_tracing_default_output_root(),
                                                         "output",
                                                         true,
                                                         true);
    root_corrected |= config_runtime_paths_validate_root(animSettings.videoOutputRoot,
                                                         sizeof(animSettings.videoOutputRoot),
                                                         ray_tracing_default_video_output_root(),
                                                         "video output",
                                                         true,
                                                         true);
    (void)setenv("RAY_TRACING_INPUT_ROOT", animSettings.inputRoot, 1);
    if (animSettings.meshAssetRoot[0]) {
        (void)setenv("RAY_TRACING_MESH_ASSET_ROOT", animSettings.meshAssetRoot, 1);
    } else {
        (void)unsetenv("RAY_TRACING_MESH_ASSET_ROOT");
    }
    (void)setenv("RAY_TRACING_OUTPUT_ROOT", animSettings.outputRoot, 1);
    (void)setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", animSettings.videoOutputRoot, 1);
    if (root_corrected) {
        SaveAnimationConfig();
        fprintf(stderr,
                "[startup] Data root fallback correction persisted to runtime animation config.\n");
    }
    animSettings.spaceMode = animation_config_space_mode_clamp(animSettings.spaceMode);
    animation_config_sync_scene_source_legacy_fields(&animSettings);
    animation_config_sync_volume_source_fields(&animSettings);
    animSettings.textZoomStep = animation_config_text_zoom_step_clamp(animSettings.textZoomStep);
    RayTracingIntegratorCatalog_NormalizeAnimationConfig(&animSettings);

    printf(" Loaded animation config successfully.\n");
    json_object_put(config);
}
