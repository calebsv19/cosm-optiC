#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "app/animation_output.h"
#include "app/data_paths.h"
#include "app/render_export_batch.h"
#include "config/config_manager.h"
#include "material/material_manager.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "render/runtime_native_3d_resolution.h"
#include "test_support.h"

static int test_scene_object_z_roundtrip(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeSceneConfigPath, &backup_size);

    memset(&sceneSettings, 0, sizeof(sceneSettings));
    sceneSettings.windowWidth = 320;
    sceneSettings.windowHeight = 240;
    sceneSettings.camera.x = 10.0;
    sceneSettings.camera.y = 20.0;
    sceneSettings.camera.zoom = 1.0;
    sceneSettings.camera.rotation = 0.0;
    sceneSettings.cameraMargin = 24.0;
    sceneSettings.rays = 128;

    sceneSettings.objectCount = 2;

    InitObject(&sceneSettings.sceneObjects[0], OBJECT_CIRCLE, 12.0, 34.0, 5.0, 0.0, NULL, 0);
    sceneSettings.sceneObjects[0].z = 7.25;

    double tri[3][2] = {
        {-10.0, -10.0},
        {10.0, -10.0},
        {0.0, 10.0}
    };
    InitObject(&sceneSettings.sceneObjects[1], OBJECT_POLYGON, -40.0, 5.0, 0.0, 0.0, tri, 3);
    sceneSettings.sceneObjects[1].z = -2.5;

    sceneSettings.bezierPath.numPoints = 2;
    sceneSettings.bezierPath.mode = BEZIER_CUBIC;
    sceneSettings.bezierPath.points[0].x = 0.0;
    sceneSettings.bezierPath.points[0].y = 0.0;
    sceneSettings.bezierPath.points[1].x = 10.0;
    sceneSettings.bezierPath.points[1].y = 10.0;
    sceneSettings.cameraPath.numPoints = 1;
    sceneSettings.cameraPath.mode = BEZIER_CUBIC;
    sceneSettings.cameraPath.points[0].x = 10.0;
    sceneSettings.cameraPath.points[0].y = 20.0;

    SaveSceneConfig();
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    LoadSceneConfig();

    assert_true("scene_z_roundtrip_object_count", sceneSettings.objectCount >= 2);
    if (sceneSettings.objectCount >= 2) {
        assert_close("scene_z_roundtrip_obj0", sceneSettings.sceneObjects[0].z, 7.25, 1e-6);
        assert_close("scene_z_roundtrip_obj1", sceneSettings.sceneObjects[1].z, -2.5, 1e-6);
    }

    restore_runtime_scene_config(backup, backup_size);
    return 0;
}

static int test_scene_object_z_missing_fallback(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeSceneConfigPath, &backup_size);

    // Ensure runtime lane exists, then inject a minimal scene file without object.z.
    SaveSceneConfig();

    const char* json_no_z =
        "{\n"
        "  \"window\": {\"width\": 200, \"height\": 120},\n"
        "  \"camera\": {\"x\": 0.0, \"y\": 0.0, \"zoom\": 1.0, \"rotation\": 0.0, \"margin\": 10.0},\n"
        "  \"rays\": 64,\n"
        "  \"objects\": [\n"
        "    {\"type\": \"circle\", \"x\": 1.0, \"y\": 2.0, \"radius\": 3.0, \"scale\": 1.0, \"rotation\": 0.0}\n"
        "  ],\n"
        "  \"path\": {\"mode\": \"BEZIER_CUBIC\", \"points\": [{\"x\": 0.0, \"y\": 0.0}, {\"x\": 2.0, \"y\": 2.0}]},\n"
        "  \"cameraPath\": {\"mode\": \"BEZIER_CUBIC\", \"points\": [{\"x\": 0.0, \"y\": 0.0}]}\n"
        "}\n";

    bool wrote = write_text_file(kRuntimeSceneConfigPath, json_no_z);
    assert_true("scene_z_missing_write_runtime", wrote);
    if (wrote) {
        memset(&sceneSettings, 0, sizeof(sceneSettings));
        LoadSceneConfig();
        assert_true("scene_z_missing_object_count", sceneSettings.objectCount >= 1);
        if (sceneSettings.objectCount >= 1) {
            assert_close("scene_z_missing_default_zero", sceneSettings.sceneObjects[0].z, 0.0, 1e-9);
        }
    }

    restore_runtime_scene_config(backup, backup_size);
    return 0;
}

static int test_animation_scene_source_legacy_migration(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_legacy_fluid =
        "{\n"
        "  \"inputRoot\": \"config\",\n"
        "  \"outputRoot\": \"data/runtime\",\n"
        "  \"spaceMode\": 0,\n"
        "  \"useFluidScene\": true,\n"
        "  \"fluidManifest\": \"import/legacy_manifest.json\"\n"
        "}\n";
    const char* json_legacy_missing_manifest =
        "{\n"
        "  \"inputRoot\": \"config\",\n"
        "  \"outputRoot\": \"data/runtime\",\n"
        "  \"spaceMode\": 0,\n"
        "  \"useFluidScene\": true,\n"
        "  \"fluidManifest\": \"\"\n"
        "}\n";

    bool wrote = write_text_file(kRuntimeAnimationConfigPath, json_legacy_fluid);
    assert_true("scene_source_legacy_write_fluid", wrote);
    if (wrote) {
        LoadAnimationConfig();
        assert_true("scene_source_legacy_migrates_to_fluid",
                    animSettings.sceneSource == SCENE_SOURCE_FLUID_MANIFEST);
        assert_true("scene_source_legacy_use_fluid_true", animSettings.useFluidScene);
    }

    wrote = write_text_file(kRuntimeAnimationConfigPath, json_legacy_missing_manifest);
    assert_true("scene_source_legacy_write_missing_manifest", wrote);
    if (wrote) {
        LoadAnimationConfig();
        assert_true("scene_source_legacy_missing_manifest_fallback_2d",
                    animSettings.sceneSource == SCENE_SOURCE_CONFIG_2D);
        assert_true("scene_source_legacy_missing_manifest_use_fluid_false",
                    !animSettings.useFluidScene);
    }

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_scene_source_roundtrip_runtime_lane(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.useFluidScene = true;
    strncpy(animSettings.fluidManifest, "import/should_not_drive_runtime_lane.json",
            sizeof(animSettings.fluidManifest) - 1);
    animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
    strncpy(animSettings.runtimeScenePath, "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json",
            sizeof(animSettings.runtimeScenePath) - 1);
    animSettings.runtimeScenePath[sizeof(animSettings.runtimeScenePath) - 1] = '\0';

    SaveAnimationConfig();
    animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
    animSettings.useFluidScene = false;
    animSettings.fluidManifest[0] = '\0';
    animSettings.runtimeScenePath[0] = '\0';
    LoadAnimationConfig();

    assert_true("scene_source_roundtrip_runtime_lane",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_roundtrip_runtime_not_fluid", !animSettings.useFluidScene);
    assert_true("scene_source_roundtrip_runtime_path",
                strcmp(animSettings.runtimeScenePath,
                       "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json") == 0);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_integrator_split_roundtrip_and_default_3d(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_3d_integrator =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode\": 1\n"
        "}\n";

    assert_true("integrator_split_write_missing_3d",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_3d_integrator));
    LoadAnimationConfig();
    assert_true("integrator_split_missing_3d_defaulted",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT);
    assert_true("integrator_split_2d_preserved_on_load",
                animSettings.integratorMode == RAY_TRACING_2D_INTEGRATOR_HYBRID);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    SaveAnimationConfig();
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    LoadAnimationConfig();

    assert_true("integrator_split_roundtrip_2d_persisted",
                animSettings.integratorMode == RAY_TRACING_2D_INTEGRATOR_HYBRID);
    assert_true("integrator_split_roundtrip_3d_persisted",
                animSettings.integratorMode3D == RAY_TRACING_3D_INTEGRATOR_DISNEY);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_temporal_frames_roundtrip_and_clamp(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_temporal =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1\n"
        "}\n";
    const char* json_invalid_temporal =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1,\n"
        "  \"temporalFrames3D\": 0\n"
        "}\n";

    assert_true("native_3d_temporal_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_temporal));
    LoadAnimationConfig();
    assert_true("native_3d_temporal_missing_defaults",
                animSettings.temporalFrames3D == RUNTIME_3D_TEMPORAL_FRAMES_DEFAULT);

    assert_true("native_3d_temporal_write_invalid",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_temporal));
    LoadAnimationConfig();
    assert_true("native_3d_temporal_invalid_clamped_min",
                animSettings.temporalFrames3D == RUNTIME_3D_TEMPORAL_FRAMES_MIN);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.temporalFrames3D = 18;
    SaveAnimationConfig();
    animSettings.temporalFrames3D = RUNTIME_3D_TEMPORAL_FRAMES_DEFAULT;
    LoadAnimationConfig();
    assert_true("native_3d_temporal_roundtrip_persisted",
                animSettings.temporalFrames3D == 18);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_bounce_depth_and_roulette_roundtrip_and_clamp(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_native_3d_bounce =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1\n"
        "}\n";
    const char* json_invalid_native_3d_bounce =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1,\n"
        "  \"bounceDepth3D\": 0,\n"
        "  \"rouletteThreshold3D\": 0.5\n"
        "}\n";

    assert_true("native_3d_bounce_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_native_3d_bounce));
    LoadAnimationConfig();
    assert_true("native_3d_bounce_missing_depth_defaults",
                animSettings.bounceDepth3D == RUNTIME_3D_BOUNCE_DEPTH_DEFAULT);
    assert_true("native_3d_bounce_missing_roulette_defaults",
                fabs(animSettings.rouletteThreshold3D -
                     RUNTIME_3D_ROULETTE_THRESHOLD_DEFAULT) <= 1e-9);

    assert_true("native_3d_bounce_write_invalid",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_native_3d_bounce));
    LoadAnimationConfig();
    assert_true("native_3d_bounce_invalid_depth_clamped_min",
                animSettings.bounceDepth3D == RUNTIME_3D_BOUNCE_DEPTH_MIN);
    assert_true("native_3d_bounce_invalid_roulette_clamped_max",
                fabs(animSettings.rouletteThreshold3D -
                     RUNTIME_3D_ROULETTE_THRESHOLD_MAX) <= 1e-9);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.bounceDepth3D = 5;
    animSettings.rouletteThreshold3D = 0.025;
    SaveAnimationConfig();
    animSettings.bounceDepth3D = RUNTIME_3D_BOUNCE_DEPTH_DEFAULT;
    animSettings.rouletteThreshold3D = RUNTIME_3D_ROULETTE_THRESHOLD_DEFAULT;
    LoadAnimationConfig();
    assert_true("native_3d_bounce_roundtrip_depth_persisted",
                animSettings.bounceDepth3D == 5);
    assert_true("native_3d_bounce_roundtrip_roulette_persisted",
                fabs(animSettings.rouletteThreshold3D - 0.025) <= 1e-9);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_top_fill_roundtrip_and_default(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_top_fill =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1\n"
        "}\n";

    assert_true("native_3d_top_fill_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_top_fill));
    LoadAnimationConfig();
    assert_true("native_3d_top_fill_missing_defaults_off",
                !animSettings.topFillLightEnabled);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.topFillLightEnabled = true;
    SaveAnimationConfig();
    animSettings.topFillLightEnabled = false;
    LoadAnimationConfig();
    assert_true("native_3d_top_fill_roundtrip_persisted",
                animSettings.topFillLightEnabled);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_disney_denoise_roundtrip_and_default(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_denoise =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 4\n"
        "}\n";

    assert_true("native_3d_denoise_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_denoise));
    LoadAnimationConfig();
    assert_true("native_3d_denoise_missing_defaults_on",
                animSettings.disneyDenoiseEnabled);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.disneyDenoiseEnabled = false;
    SaveAnimationConfig();
    animSettings.disneyDenoiseEnabled = true;
    LoadAnimationConfig();
    assert_true("native_3d_denoise_roundtrip_persisted",
                !animSettings.disneyDenoiseEnabled);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_environment_brightness_byte_floor_roundtrip_and_legacy_migration(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_legacy_environment =
        "{\n"
        "  \"environmentBrightness\": 0.35\n"
        "}\n";

    assert_true("environment_floor_legacy_write",
                write_text_file(kRuntimeAnimationConfigPath, json_legacy_environment));
    LoadAnimationConfig();
    assert_true("environment_floor_legacy_migrated_to_byte_domain",
                animSettings.environmentBrightness >= 0.0 &&
                animSettings.environmentBrightness <= 255.0 &&
                fabs(animSettings.environmentBrightness - 121.0) <= 1.0);

    animSettings.environmentBrightness = 128.0;
    SaveAnimationConfig();
    animSettings.environmentBrightness = 0.0;
    LoadAnimationConfig();
    assert_true("environment_floor_roundtrip_persisted",
                fabs(animSettings.environmentBrightness - 128.0) <= 1e-6);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_runtime_window_override_roundtrip_and_apply(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;

    sceneSettings.windowWidth = 640;
    sceneSettings.windowHeight = 904;
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    SaveAnimationConfig();

    animSettings.runtimeWindowWidth = 0;
    animSettings.runtimeWindowHeight = 0;
    sceneSettings.windowWidth = 1200;
    sceneSettings.windowHeight = 800;
    LoadAnimationConfig();

    assert_true("runtime_window_override_roundtrip_width",
                animSettings.runtimeWindowWidth == 640);
    assert_true("runtime_window_override_roundtrip_height",
                animSettings.runtimeWindowHeight == 904);

    ApplyAnimationWindowSizeOverride();
    assert_true("runtime_window_override_apply_width",
                sceneSettings.windowWidth == 640);
    assert_true("runtime_window_override_apply_height",
                sceneSettings.windowHeight == 904);

    sceneSettings = saved_scene;
    animSettings = saved_anim;
    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_native_3d_render_scale_roundtrip_and_clamp(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_scale =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1\n"
        "}\n";
    const char* json_invalid_scale =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"integratorMode3D\": 1,\n"
        "  \"renderScale3D\": 0\n"
        "}\n";

    assert_true("native_3d_render_scale_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_scale));
    LoadAnimationConfig();
    assert_true("native_3d_render_scale_missing_defaults",
                animSettings.renderScale3D == RUNTIME_3D_RENDER_SCALE_DEFAULT);

    assert_true("native_3d_render_scale_write_invalid",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_scale));
    LoadAnimationConfig();
    assert_true("native_3d_render_scale_invalid_clamped_min",
                animSettings.renderScale3D == RUNTIME_3D_RENDER_SCALE_MIN);

    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.renderScale3D = 4;
    SaveAnimationConfig();
    animSettings.renderScale3D = RUNTIME_3D_RENDER_SCALE_DEFAULT;
    LoadAnimationConfig();
    assert_true("native_3d_render_scale_roundtrip_persisted",
                animSettings.renderScale3D == 4);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_runtime_native_3d_resolution_scale_contract(void) {
    uint8_t src[4] = {10, 20, 30, 40};
    uint8_t dst[16] = {0};
    int width = 0;
    int height = 0;
    int rect_x = 0;
    int rect_y = 0;
    int rect_w = 0;
    int rect_h = 0;

    assert_true("runtime_native_3d_scale_resolve_ok",
                RuntimeNative3DResolveScaledDimensions(1200, 900, 2, &width, &height));
    assert_true("runtime_native_3d_scale_resolve_width", width == 600);
    assert_true("runtime_native_3d_scale_resolve_height", height == 450);
    assert_true("runtime_native_3d_scale_clamp_max",
                RuntimeNative3DClampRenderScale(99) == RUNTIME_3D_RENDER_SCALE_MAX);

    RuntimeNative3DUpscaleNearest(src, 2, 2, dst, 4, 4);
    assert_true("runtime_native_3d_scale_upscale_top_left", dst[0] == 10);
    assert_true("runtime_native_3d_scale_upscale_top_right", dst[3] == 20);
    assert_true("runtime_native_3d_scale_upscale_bottom_left", dst[12] == 30);
    assert_true("runtime_native_3d_scale_upscale_bottom_right", dst[15] == 40);
    assert_true("runtime_native_3d_scale_rect_map_ok",
                RuntimeNative3DResolveUpscaledRect(32,
                                                   16,
                                                   32,
                                                   32,
                                                   300,
                                                   225,
                                                   1200,
                                                   900,
                                                   &rect_x,
                                                   &rect_y,
                                                   &rect_w,
                                                   &rect_h));
    assert_true("runtime_native_3d_scale_rect_map_x", rect_x == 128);
    assert_true("runtime_native_3d_scale_rect_map_y", rect_y == 64);
    assert_true("runtime_native_3d_scale_rect_map_w", rect_w == 128);
    assert_true("runtime_native_3d_scale_rect_map_h", rect_h == 128);
    return 0;
}

static int test_animation_scene_source_select_runtime_failure_rolls_back(void) {
    static const char *kFixtureRuntimePath = "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json";
    static const char *kMissingRuntimePath = "/tmp/ray_tracing_missing_scene_source_contract.json";
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);

    remove(kMissingRuntimePath);
    MaterialManagerInit();
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.useFluidScene = false;
    animSettings.fluidManifest[0] = '\0';
    strncpy(animSettings.runtimeScenePath, kFixtureRuntimePath, sizeof(animSettings.runtimeScenePath) - 1);
    animSettings.runtimeScenePath[sizeof(animSettings.runtimeScenePath) - 1] = '\0';

    assert_true("scene_source_select_runtime_baseline_source_runtime",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_select_runtime_baseline_runtime_path",
                strcmp(animSettings.runtimeScenePath, kFixtureRuntimePath) == 0);

    assert_true("scene_source_select_runtime_missing_rejected",
                !AnimationSelectSceneSource(SCENE_SOURCE_RUNTIME_SCENE,
                                            kMissingRuntimePath,
                                            true));
    assert_true("scene_source_select_runtime_rollback_source_runtime",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_select_runtime_rollback_runtime_path",
                strcmp(animSettings.runtimeScenePath, kFixtureRuntimePath) == 0);
    assert_true("scene_source_select_runtime_rollback_not_fluid",
                !animSettings.useFluidScene);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_scene_source_select_runtime_persists_on_save(void) {
    static const char *kFixtureRuntimePath = "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json";
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);

    MaterialManagerInit();
    assert_true("scene_source_select_runtime_apply_ok",
                AnimationSelectSceneSource(SCENE_SOURCE_RUNTIME_SCENE,
                                           kFixtureRuntimePath,
                                           false));
    assert_true("scene_source_select_runtime_apply_source_runtime",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_select_runtime_apply_runtime_path",
                strcmp(animSettings.runtimeScenePath, kFixtureRuntimePath) == 0);
    assert_true("scene_source_select_runtime_apply_not_fluid",
                !animSettings.useFluidScene);

    SaveAnimationConfig();
    animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
    animSettings.useFluidScene = false;
    animSettings.fluidManifest[0] = '\0';
    animSettings.runtimeScenePath[0] = '\0';
    LoadAnimationConfig();

    assert_true("scene_source_select_runtime_persist_source_runtime",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_select_runtime_persist_runtime_path",
                strcmp(animSettings.runtimeScenePath, kFixtureRuntimePath) == 0);
    assert_true("scene_source_select_runtime_persist_not_fluid",
                !animSettings.useFluidScene);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_apply_active_scene_source_invalid_fluid_falls_back_2d(void) {
    static const char *kMissingFluidPath = "/tmp/ray_tracing_missing_fluid_manifest.json";
    static const char *kRuntimeFixturePath = "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json";
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);

    remove(kMissingFluidPath);
    animSettings.sceneSource = SCENE_SOURCE_FLUID_MANIFEST;
    animSettings.useFluidScene = true;
    strncpy(animSettings.fluidManifest, kMissingFluidPath, sizeof(animSettings.fluidManifest) - 1);
    animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
    strncpy(animSettings.runtimeScenePath, kRuntimeFixturePath, sizeof(animSettings.runtimeScenePath) - 1);
    animSettings.runtimeScenePath[sizeof(animSettings.runtimeScenePath) - 1] = '\0';

    assert_true("scene_source_invalid_fluid_apply_rejected",
                !AnimationApplyActiveSceneSource());
    assert_true("scene_source_invalid_fluid_fallback_source_2d",
                animSettings.sceneSource == SCENE_SOURCE_CONFIG_2D);
    assert_true("scene_source_invalid_fluid_fallback_not_fluid",
                !animSettings.useFluidScene);
    assert_true("scene_source_invalid_fluid_fallback_fluid_path_cleared",
                animSettings.fluidManifest[0] == '\0');
    assert_true("scene_source_invalid_fluid_fallback_runtime_path_cleared",
                animSettings.runtimeScenePath[0] == '\0');

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_restore_active_scene_source_persists_fallback_correction(void) {
    static const char *kMissingRuntimePath = "/tmp/ray_tracing_missing_restore_runtime_scene.json";
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char *json_invalid_runtime_source =
        "{\n"
        "  \"inputRoot\": \"config\",\n"
        "  \"outputRoot\": \"data/runtime\",\n"
        "  \"spaceMode\": 1,\n"
        "  \"sceneSource\": 2,\n"
        "  \"useFluidScene\": false,\n"
        "  \"fluidManifest\": \"\",\n"
        "  \"runtimeScenePath\": \"/tmp/ray_tracing_missing_restore_runtime_scene.json\"\n"
        "}\n";

    remove(kMissingRuntimePath);
    assert_true("scene_source_restore_write_invalid_runtime",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_runtime_source));
    LoadAnimationConfig();

    assert_true("scene_source_restore_invalid_runtime_rejected",
                !AnimationRestoreActiveSceneSource(true));
    assert_true("scene_source_restore_invalid_runtime_fallback_source_2d",
                animSettings.sceneSource == SCENE_SOURCE_CONFIG_2D);
    assert_true("scene_source_restore_invalid_runtime_fallback_runtime_path_cleared",
                animSettings.runtimeScenePath[0] == '\0');

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.runtimeScenePath[0] = 'x';
    animSettings.runtimeScenePath[1] = '\0';
    LoadAnimationConfig();
    assert_true("scene_source_restore_persisted_source_2d",
                animSettings.sceneSource == SCENE_SOURCE_CONFIG_2D);
    assert_true("scene_source_restore_persisted_runtime_path_cleared",
                animSettings.runtimeScenePath[0] == '\0');

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_video_output_root_migrates_from_output_root(void) {
    char tmp_template[] = "/tmp/ray_tracing_video_root_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    size_t backup_size = 0;
    char *backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    char json[1024];
    char expected_video_root[PATH_MAX];

    assert_true("video_output_root_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) {
        restore_runtime_animation_config(backup, backup_size);
        return 0;
    }

    snprintf(json,
             sizeof(json),
             "{\n"
             "  \"inputRoot\": \"config\",\n"
             "  \"outputRoot\": \"%s\",\n"
             "  \"frameDir\": \"%s/frames/default\",\n"
             "  \"fps\": 24,\n"
             "  \"spaceMode\": 0\n"
             "}\n",
             tmp_root,
             tmp_root);
    assert_true("video_output_root_write_runtime",
                write_text_file(kRuntimeAnimationConfigPath, json));
    LoadAnimationConfig();

    assert_true("video_output_root_migrated_compose",
                ray_tracing_compose_path(tmp_root,
                                         "videos",
                                         expected_video_root,
                                         sizeof(expected_video_root)));
    assert_true("video_output_root_migrated_matches",
                strcmp(animSettings.videoOutputRoot, expected_video_root) == 0);
    assert_true("video_output_root_migrated_exists",
                path_exists(animSettings.videoOutputRoot));

    rmdir(expected_video_root);
    rmdir(tmp_root);
    setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", ray_tracing_default_video_output_root(), 1);
    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_data_paths_resolve_video_output_path_uses_configured_root(void) {
    char output_path[PATH_MAX];
    const char *video_env = getenv("RAY_TRACING_VIDEO_OUTPUT_ROOT");
    char video_env_backup[PATH_MAX] = {0};
    bool had_video_env = false;
    if (video_env && video_env[0]) {
        strncpy(video_env_backup, video_env, sizeof(video_env_backup) - 1);
        video_env_backup[sizeof(video_env_backup) - 1] = '\0';
        had_video_env = true;
    }

    assert_true("video_output_path_resolve_configured",
                ray_tracing_resolve_video_output_path("/tmp/ray_tracing_video_root",
                                                      output_path,
                                                      sizeof(output_path)));
    assert_true("video_output_path_resolve_configured_value",
                strcmp(output_path,
                       "/tmp/ray_tracing_video_root/output.mp4") == 0);

    setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", ray_tracing_default_video_output_root(), 1);
    assert_true("video_output_path_resolve_default",
                ray_tracing_resolve_video_output_path("",
                                                      output_path,
                                                      sizeof(output_path)));
    assert_true("video_output_path_resolve_default_value",
                strcmp(output_path,
                       ray_tracing_default_video_output_path()) == 0);
    if (had_video_env) {
        setenv("RAY_TRACING_VIDEO_OUTPUT_ROOT", video_env_backup, 1);
    } else {
        unsetenv("RAY_TRACING_VIDEO_OUTPUT_ROOT");
    }
    return 0;
}

static int test_render_export_batch_counts_and_clears_frames(void) {
    char tmp_template[] = "/tmp/ray_tracing_export_frames_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char frame0[PATH_MAX];
    char frame1[PATH_MAX];
    char note[PATH_MAX];
    char expected_video_output[PATH_MAX];
    char original_frame_dir[sizeof(animSettings.frameDir)];
    char original_video_root[sizeof(animSettings.videoOutputRoot)];
    RayTracingRenderExportStatus status = {0};

    assert_true("export_batch_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(frame0, sizeof(frame0), "%s/frame_0000.bmp", tmp_root);
    snprintf(frame1, sizeof(frame1), "%s/frame_0001.bmp", tmp_root);
    snprintf(note, sizeof(note), "%s/keep.txt", tmp_root);
    assert_true("export_batch_write_frame0", write_text_file(frame0, "a"));
    assert_true("export_batch_write_frame1", write_text_file(frame1, "b"));
    assert_true("export_batch_write_note", write_text_file(note, "keep"));

    strncpy(original_frame_dir, animSettings.frameDir, sizeof(original_frame_dir) - 1);
    original_frame_dir[sizeof(original_frame_dir) - 1] = '\0';
    strncpy(original_video_root, animSettings.videoOutputRoot, sizeof(original_video_root) - 1);
    original_video_root[sizeof(original_video_root) - 1] = '\0';

    strncpy(animSettings.frameDir, tmp_root, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, tmp_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';
    assert_true("export_batch_expected_video_path",
                ray_tracing_resolve_video_output_path(animSettings.videoOutputRoot,
                                                      expected_video_output,
                                                      sizeof(expected_video_output)));

    assert_true("export_batch_count_ok", ray_tracing_render_export_count_active_frames(&status));
    assert_true("export_batch_count_two", status.frame_count == 2u);
    assert_true("export_batch_video_path",
                strcmp(status.video_output_path, expected_video_output) == 0);

    assert_true("export_batch_clear_ok", ray_tracing_render_export_clear_active_frames(&status));
    assert_true("export_batch_clear_removed_two", status.files_cleared == 2u);
    assert_true("export_batch_frame0_removed", !path_exists(frame0));
    assert_true("export_batch_frame1_removed", !path_exists(frame1));
    assert_true("export_batch_note_retained", path_exists(note));

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    unlink(note);
    rmdir(tmp_root);
    return 0;
}

static int test_render_export_batch_reports_highest_and_next_frame(void) {
    char tmp_template[] = "/tmp/ray_tracing_export_frames_next_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char frame0[PATH_MAX];
    char frame5[PATH_MAX];
    char frame12[PATH_MAX];
    char original_frame_dir[sizeof(animSettings.frameDir)];
    char original_video_root[sizeof(animSettings.videoOutputRoot)];
    RayTracingRenderExportStatus status = {0};

    assert_true("export_batch_next_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    snprintf(frame0, sizeof(frame0), "%s/frame_0000.bmp", tmp_root);
    snprintf(frame5, sizeof(frame5), "%s/frame_0005.bmp", tmp_root);
    snprintf(frame12, sizeof(frame12), "%s/frame_0012.bmp", tmp_root);
    assert_true("export_batch_next_write_frame0", write_text_file(frame0, "a"));
    assert_true("export_batch_next_write_frame5", write_text_file(frame5, "b"));
    assert_true("export_batch_next_write_frame12", write_text_file(frame12, "c"));

    strncpy(original_frame_dir, animSettings.frameDir, sizeof(original_frame_dir) - 1);
    original_frame_dir[sizeof(original_frame_dir) - 1] = '\0';
    strncpy(original_video_root, animSettings.videoOutputRoot, sizeof(original_video_root) - 1);
    original_video_root[sizeof(original_video_root) - 1] = '\0';

    strncpy(animSettings.frameDir, tmp_root, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, tmp_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    assert_true("export_batch_next_describe_ok",
                ray_tracing_render_export_describe_active(&status));
    assert_true("export_batch_next_count_three", status.frame_count == 3u);
    assert_true("export_batch_next_highest_twelve", status.highest_frame_index == 12);
    assert_true("export_batch_next_next_thirteen", status.next_frame_index == 13);

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(original_video_root) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    unlink(frame0);
    unlink(frame5);
    unlink(frame12);
    rmdir(tmp_root);
    return 0;
}

static int test_render_export_batch_make_video_rejects_empty_frame_dir(void) {
    char tmp_template[] = "/tmp/ray_tracing_export_video_empty_XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    char original_frame_dir[sizeof(animSettings.frameDir)];
    char original_video_root[sizeof(animSettings.videoOutputRoot)];
    RayTracingRenderExportStatus status = {0};

    assert_true("export_video_empty_tmpdir_created", tmp_root != NULL);
    if (!tmp_root) return 0;

    strncpy(original_frame_dir, animSettings.frameDir, sizeof(original_frame_dir) - 1);
    original_frame_dir[sizeof(original_frame_dir) - 1] = '\0';
    strncpy(original_video_root, animSettings.videoOutputRoot, sizeof(original_video_root) - 1);
    original_video_root[sizeof(original_video_root) - 1] = '\0';

    strncpy(animSettings.frameDir, tmp_root, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, tmp_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    assert_true("export_video_empty_rejected",
                !ray_tracing_render_export_make_video(&status));
    assert_true("export_video_empty_code",
                status.code == RAY_TRACING_RENDER_EXPORT_NO_FRAMES);

    strncpy(animSettings.frameDir, original_frame_dir, sizeof(animSettings.frameDir) - 1);
    animSettings.frameDir[sizeof(animSettings.frameDir) - 1] = '\0';
    strncpy(animSettings.videoOutputRoot, original_video_root, sizeof(animSettings.videoOutputRoot) - 1);
    animSettings.videoOutputRoot[sizeof(animSettings.videoOutputRoot) - 1] = '\0';

    rmdir(tmp_root);
    return 0;
}

static int test_animation_deep_render_frame_resume_roundtrip_and_clamp(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_start_resume =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"deepRenderMode\": true\n"
        "}\n";
    const char* json_invalid_start_resume =
        "{\n"
        "  \"spaceMode\": 1,\n"
        "  \"deepRenderMode\": true,\n"
        "  \"startFrameIndex\": -7,\n"
        "  \"resumeFromExistingFrames\": true\n"
        "}\n";

    assert_true("deep_render_start_resume_write_missing",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_start_resume));
    LoadAnimationConfig();
    assert_true("deep_render_start_missing_defaults_zero",
                animSettings.startFrameIndex == 0);
    assert_true("deep_render_resume_missing_defaults_off",
                !animSettings.resumeFromExistingFrames);

    assert_true("deep_render_start_resume_write_invalid",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_start_resume));
    LoadAnimationConfig();
    assert_true("deep_render_start_invalid_clamped_zero",
                animSettings.startFrameIndex == 0);
    assert_true("deep_render_resume_invalid_preserved_true",
                animSettings.resumeFromExistingFrames);

    animSettings.startFrameIndex = 117;
    animSettings.resumeFromExistingFrames = true;
    SaveAnimationConfig();
    animSettings.startFrameIndex = 0;
    animSettings.resumeFromExistingFrames = false;
    LoadAnimationConfig();
    assert_true("deep_render_start_roundtrip_persisted",
                animSettings.startFrameIndex == 117);
    assert_true("deep_render_resume_roundtrip_persisted",
                animSettings.resumeFromExistingFrames);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}


int run_test_config_animation_tests(void) {
    int before = test_support_failures();

    test_scene_object_z_roundtrip();
    test_scene_object_z_missing_fallback();
    test_animation_scene_source_legacy_migration();
    test_animation_scene_source_roundtrip_runtime_lane();
    test_animation_integrator_split_roundtrip_and_default_3d();
    test_animation_native_3d_temporal_frames_roundtrip_and_clamp();
    test_animation_native_3d_bounce_depth_and_roulette_roundtrip_and_clamp();
    test_animation_native_3d_top_fill_roundtrip_and_default();
    test_animation_native_3d_disney_denoise_roundtrip_and_default();
    test_animation_environment_brightness_byte_floor_roundtrip_and_legacy_migration();
    test_animation_runtime_window_override_roundtrip_and_apply();
    test_animation_native_3d_render_scale_roundtrip_and_clamp();
    test_runtime_native_3d_resolution_scale_contract();
    test_animation_scene_source_select_runtime_failure_rolls_back();
    test_animation_scene_source_select_runtime_persists_on_save();
    test_animation_apply_active_scene_source_invalid_fluid_falls_back_2d();
    test_animation_restore_active_scene_source_persists_fallback_correction();
    test_animation_video_output_root_migrates_from_output_root();
    test_data_paths_resolve_video_output_path_uses_configured_root();
    test_animation_deep_render_frame_resume_roundtrip_and_clamp();
    test_render_export_batch_counts_and_clears_frames();
    test_render_export_batch_reports_highest_and_next_frame();
    test_render_export_batch_make_video_rejects_empty_frame_dir();

    return test_support_failures() - before;
}
