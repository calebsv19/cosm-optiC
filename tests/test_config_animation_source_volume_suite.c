#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "config/mesh_import_policy.h"
#include "material/material_manager.h"
#include "test_config_animation_internal.h"
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
        assert_true("scene_source_legacy_missing_manifest_preserves_fluid",
                    animSettings.sceneSource == SCENE_SOURCE_FLUID_MANIFEST);
        assert_true("scene_source_legacy_missing_manifest_use_fluid_true",
                    animSettings.useFluidScene);
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

static int test_animation_volume_source_roundtrip_and_defaults(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* json_missing_volume =
        "{\n"
        "  \"sceneSource\": 2,\n"
        "  \"runtimeScenePath\": \"third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json\"\n"
        "}\n";
    const char* json_invalid_volume =
        "{\n"
        "  \"volumeInteractionEnabled\": true,\n"
        "  \"volumeSourceKind\": 99,\n"
        "  \"volumeSourcePath\": \"/tmp/invalid.vf3d\",\n"
        "  \"volumeAffectsLighting\": false,\n"
        "  \"volumeDebugOverlayEnabled\": true\n"
        "}\n";

    animSettings.volumeInteractionEnabled = true;
    animSettings.volumeSourceKind = VOLUME_SOURCE_PACK;
    strncpy(animSettings.volumeSourcePath, "/tmp/stale_volume.pack",
            sizeof(animSettings.volumeSourcePath) - 1);
    animSettings.volumeSourcePath[sizeof(animSettings.volumeSourcePath) - 1] = '\0';
    animSettings.volumeAffectsLighting = false;
    animSettings.volumeDebugOverlayEnabled = true;

    assert_true("volume_source_missing_write_runtime",
                write_text_file(kRuntimeAnimationConfigPath, json_missing_volume));
    LoadAnimationConfig();
    assert_true("volume_source_missing_defaults_disabled",
                !animSettings.volumeInteractionEnabled);
    assert_true("volume_source_missing_defaults_kind_none",
                animSettings.volumeSourceKind == VOLUME_SOURCE_NONE);
    assert_true("volume_source_missing_defaults_path_empty",
                animSettings.volumeSourcePath[0] == '\0');
    assert_true("volume_source_missing_defaults_affects_lighting",
                animSettings.volumeAffectsLighting);
    assert_true("volume_source_missing_defaults_debug_off",
                !animSettings.volumeDebugOverlayEnabled);

    animSettings.volumeInteractionEnabled = true;
    animSettings.volumeSourceKind = VOLUME_SOURCE_PACK;
    strncpy(animSettings.volumeSourcePath, "/tmp/volume_fixture.pack",
            sizeof(animSettings.volumeSourcePath) - 1);
    animSettings.volumeSourcePath[sizeof(animSettings.volumeSourcePath) - 1] = '\0';
    animSettings.volumeAffectsLighting = false;
    animSettings.volumeDebugOverlayEnabled = true;
    SaveAnimationConfig();

    animSettings.volumeInteractionEnabled = false;
    animSettings.volumeSourceKind = VOLUME_SOURCE_NONE;
    animSettings.volumeSourcePath[0] = '\0';
    animSettings.volumeAffectsLighting = true;
    animSettings.volumeDebugOverlayEnabled = false;
    LoadAnimationConfig();

    assert_true("volume_source_roundtrip_enabled",
                animSettings.volumeInteractionEnabled);
    assert_true("volume_source_roundtrip_kind_pack",
                animSettings.volumeSourceKind == VOLUME_SOURCE_PACK);
    assert_true("volume_source_roundtrip_path",
                strcmp(animSettings.volumeSourcePath, "/tmp/volume_fixture.pack") == 0);
    assert_true("volume_source_roundtrip_affects_lighting_false",
                !animSettings.volumeAffectsLighting);
    assert_true("volume_source_roundtrip_debug_true",
                animSettings.volumeDebugOverlayEnabled);

    assert_true("volume_source_invalid_write_runtime",
                write_text_file(kRuntimeAnimationConfigPath, json_invalid_volume));
    LoadAnimationConfig();
    assert_true("volume_source_invalid_clamped_none",
                animSettings.volumeSourceKind == VOLUME_SOURCE_NONE);
    assert_true("volume_source_invalid_disabled",
                !animSettings.volumeInteractionEnabled);

    restore_runtime_animation_config(backup, backup_size);
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

static int test_animation_scene_source_select_runtime_updates_auto_paired_volume_and_preserves_disable(void) {
    char dir_a_template[] = "/tmp/ray_tracing_scene_pair_a_XXXXXX";
    char dir_b_template[] = "/tmp/ray_tracing_scene_pair_b_XXXXXX";
    char* dir_a = mkdtemp(dir_a_template);
    char* dir_b = mkdtemp(dir_b_template);
    char runtime_path_a[PATH_MAX] = {0};
    char runtime_path_b[PATH_MAX] = {0};
    char bundle_path_a[PATH_MAX] = {0};
    char bundle_path_b[PATH_MAX] = {0};
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_pair\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{\"object_id\":\"obj\",\"object_type\":\"circle\","
        "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
        "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char* bundle_json =
        "{\n"
        "  \"bundle_type\": \"physics_scene_bundle_v1\",\n"
        "  \"bundle_version\": 1,\n"
        "  \"profile\": \"physics\",\n"
        "  \"fluid_source\": {\n"
        "    \"kind\": \"pack\",\n"
        "    \"path\": \"frame_000017.pack\"\n"
        "  }\n"
        "}\n";

    assert_true("scene_source_pair_disable_dir_a", dir_a != NULL);
    assert_true("scene_source_pair_disable_dir_b", dir_b != NULL);
    if (!dir_a || !dir_b) {
        if (dir_a) rmdir(dir_a);
        if (dir_b) rmdir(dir_b);
        return 0;
    }

    assert_true("scene_source_pair_disable_runtime_a",
                snprintf(runtime_path_a, sizeof(runtime_path_a), "%s/scene_runtime.json", dir_a) <
                    (int)sizeof(runtime_path_a));
    assert_true("scene_source_pair_disable_runtime_b",
                snprintf(runtime_path_b, sizeof(runtime_path_b), "%s/scene_runtime.json", dir_b) <
                    (int)sizeof(runtime_path_b));
    assert_true("scene_source_pair_disable_bundle_a",
                snprintf(bundle_path_a, sizeof(bundle_path_a), "%s/scene_bundle.json", dir_a) <
                    (int)sizeof(bundle_path_a));
    assert_true("scene_source_pair_disable_bundle_b",
                snprintf(bundle_path_b, sizeof(bundle_path_b), "%s/scene_bundle.json", dir_b) <
                    (int)sizeof(bundle_path_b));
    assert_true("scene_source_pair_disable_write_runtime_a",
                write_text_file(runtime_path_a, runtime_json));
    assert_true("scene_source_pair_disable_write_runtime_b",
                write_text_file(runtime_path_b, runtime_json));
    assert_true("scene_source_pair_disable_write_bundle_a",
                write_text_file(bundle_path_a, bundle_json));
    assert_true("scene_source_pair_disable_write_bundle_b",
                write_text_file(bundle_path_b, bundle_json));

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.useFluidScene = false;
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s",
             runtime_path_a);
    animSettings.volumeInteractionEnabled = false;
    animSettings.volumeSourceKind = VOLUME_SOURCE_MANIFEST;
    snprintf(animSettings.volumeSourcePath,
             sizeof(animSettings.volumeSourcePath),
             "%s",
             bundle_path_a);

    assert_true("scene_source_pair_disable_select_runtime_b",
                AnimationSelectSceneSource(SCENE_SOURCE_RUNTIME_SCENE,
                                           runtime_path_b,
                                           false));
    assert_true("scene_source_pair_disable_runtime_path_updated",
                strcmp(animSettings.runtimeScenePath, runtime_path_b) == 0);
    assert_true("scene_source_pair_disable_volume_kind_manifest",
                animSettings.volumeSourceKind == VOLUME_SOURCE_MANIFEST);
    assert_true("scene_source_pair_disable_volume_path_updated",
                strcmp(animSettings.volumeSourcePath, bundle_path_b) == 0);
    assert_true("scene_source_pair_disable_disabled_preserved",
                !animSettings.volumeInteractionEnabled);

    unlink(bundle_path_a);
    unlink(bundle_path_b);
    unlink(runtime_path_a);
    unlink(runtime_path_b);
    rmdir(dir_a);
    rmdir(dir_b);
    return 0;
}

static int test_animation_volume_source_select_without_apply_updates_lane_and_clear_resets(void) {
    AnimationConfig saved_anim = animSettings;

    memset(&animSettings, 0, sizeof(animSettings));
    assert_true("volume_source_select_no_apply_ok",
                AnimationSelectVolumeSource(VOLUME_SOURCE_PACK,
                                            "/tmp/example_volume.pack",
                                            false));
    assert_true("volume_source_select_no_apply_enabled",
                animSettings.volumeInteractionEnabled);
    assert_true("volume_source_select_no_apply_kind_pack",
                animSettings.volumeSourceKind == VOLUME_SOURCE_PACK);
    assert_true("volume_source_select_no_apply_path",
                strcmp(animSettings.volumeSourcePath, "/tmp/example_volume.pack") == 0);

    AnimationClearVolumeSource();
    assert_true("volume_source_clear_disabled",
                !animSettings.volumeInteractionEnabled);
    assert_true("volume_source_clear_kind_none",
                animSettings.volumeSourceKind == VOLUME_SOURCE_NONE);
    assert_true("volume_source_clear_path_empty",
                animSettings.volumeSourcePath[0] == '\0');

    animSettings = saved_anim;
    return 0;
}

static int test_animation_volume_source_select_missing_apply_rolls_back(void) {
    AnimationConfig saved_anim = animSettings;

    memset(&animSettings, 0, sizeof(animSettings));
    animSettings.volumeInteractionEnabled = false;
    animSettings.volumeSourceKind = VOLUME_SOURCE_MANIFEST;
    snprintf(animSettings.volumeSourcePath,
             sizeof(animSettings.volumeSourcePath),
             "%s",
             "/tmp/baseline_volume.scene_bundle.json");

    assert_true("volume_source_select_missing_rejected",
                !AnimationSelectVolumeSource(VOLUME_SOURCE_MANIFEST,
                                             "/tmp/missing_volume.scene_bundle.json",
                                             true));
    assert_true("volume_source_select_missing_rolls_back_enabled",
                !animSettings.volumeInteractionEnabled);
    assert_true("volume_source_select_missing_rolls_back_kind",
                animSettings.volumeSourceKind == VOLUME_SOURCE_MANIFEST);
    assert_true("volume_source_select_missing_rolls_back_path",
                strcmp(animSettings.volumeSourcePath, "/tmp/baseline_volume.scene_bundle.json") == 0);

    animSettings = saved_anim;
    return 0;
}

static int test_animation_apply_active_scene_source_invalid_fluid_preserves_selection(void) {
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
    assert_true("scene_source_invalid_fluid_preserves_source",
                animSettings.sceneSource == SCENE_SOURCE_FLUID_MANIFEST);
    assert_true("scene_source_invalid_fluid_fallback_not_fluid",
                !animSettings.useFluidScene);
    assert_true("scene_source_invalid_fluid_path_preserved",
                strcmp(animSettings.fluidManifest, kMissingFluidPath) == 0);
    assert_true("scene_source_invalid_fluid_runtime_path_preserved",
                strcmp(animSettings.runtimeScenePath, kRuntimeFixturePath) == 0);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_restore_active_scene_source_preserves_runtime_selection_on_failure(void) {
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
    assert_true("scene_source_restore_invalid_runtime_preserves_source",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_restore_invalid_runtime_preserves_path",
                strcmp(animSettings.runtimeScenePath, kMissingRuntimePath) == 0);

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.runtimeScenePath[0] = 'x';
    animSettings.runtimeScenePath[1] = '\0';
    LoadAnimationConfig();
    assert_true("scene_source_restore_persisted_source_runtime",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_restore_persisted_runtime_path_preserved",
                strcmp(animSettings.runtimeScenePath, kMissingRuntimePath) == 0);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_apply_runtime_scene_defers_missing_mesh_assets(void) {
    static const char *kRuntimeScenePath =
        "/private/tmp/ray_tracing_animation_missing_mesh_scene_runtime.json";
    static const char *kMissingMeshPath =
        "/private/tmp/ray_tracing_animation_missing_mesh_asset.runtime.json";
    const char* runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_animation_missing_mesh\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"obj_missing_mesh\","
          "\"object_type\":\"mesh_asset_instance\","
          "\"transform\":{"
            "\"position\":{\"x\":1.0,\"y\":2.0,\"z\":3.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "},"
          "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_missing_for_animation\"},"
          "\"extensions\":{\"line_drawing\":{"
            "\"runtime_mesh_path\":\"/private/tmp/ray_tracing_animation_missing_mesh_asset.runtime.json\""
          "}}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";

    remove(kMissingMeshPath);
    assert_true("scene_source_runtime_missing_mesh_write",
                write_text_file(kRuntimeScenePath, runtime_json));
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.useFluidScene = false;
    strncpy(animSettings.runtimeScenePath, kRuntimeScenePath, sizeof(animSettings.runtimeScenePath) - 1);
    animSettings.runtimeScenePath[sizeof(animSettings.runtimeScenePath) - 1] = '\0';
    sceneSettings.objectCount = 0;

    assert_true("scene_source_runtime_missing_mesh_apply_deferred",
                AnimationApplyActiveSceneSource());
    assert_true("scene_source_runtime_missing_mesh_preserves_source",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_runtime_missing_mesh_preserves_path",
                strcmp(animSettings.runtimeScenePath, kRuntimeScenePath) == 0);
    assert_true("scene_source_runtime_missing_mesh_space_3d",
                animSettings.spaceMode == SPACE_MODE_3D);
    assert_true("scene_source_runtime_missing_mesh_scene_resident",
                sceneSettings.objectCount == 1);

    remove(kRuntimeScenePath);
    return 0;
}

static int test_animation_save_preserves_runtime_source_even_without_path(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);

    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.useFluidScene = false;
    animSettings.fluidManifest[0] = '\0';
    animSettings.runtimeScenePath[0] = '\0';

    SaveAnimationConfig();
    animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
    LoadAnimationConfig();

    assert_true("scene_source_save_empty_runtime_path_preserves_runtime_source",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("scene_source_save_empty_runtime_path_preserves_empty_path",
                animSettings.runtimeScenePath[0] == '\0');

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

static int test_animation_mesh_asset_root_roundtrip(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);
    const char* mesh_root = "/private/tmp/ray_tracing_mesh_asset_root_roundtrip";

    mkdir(mesh_root, 0777);
    snprintf(animSettings.meshAssetRoot, sizeof(animSettings.meshAssetRoot), "%s", mesh_root);
    SaveAnimationConfig();
    animSettings.meshAssetRoot[0] = '\0';
    LoadAnimationConfig();

    assert_true("mesh_asset_root_roundtrip_path",
                strcmp(animSettings.meshAssetRoot, mesh_root) == 0);
    assert_true("mesh_asset_root_roundtrip_env",
                getenv("RAY_TRACING_MESH_ASSET_ROOT") &&
                strcmp(getenv("RAY_TRACING_MESH_ASSET_ROOT"), mesh_root) == 0);

    restore_runtime_animation_config(backup, backup_size);
    rmdir(mesh_root);
    return 0;
}

static int test_animation_mesh_import_policy_roundtrip_and_defaults(void) {
    size_t backup_size = 0;
    char* backup = read_text_file_alloc(kRuntimeAnimationConfigPath, &backup_size);

    animSettings.meshImportNormalMode = RAY_TRACING_MESH_IMPORT_NORMAL_MODE_SMOOTH;
    animSettings.meshImportCreaseAngleDegrees = 90.0;
    SaveAnimationConfig();
    animSettings.meshImportNormalMode = RAY_TRACING_MESH_IMPORT_NORMAL_MODE_NONE;
    animSettings.meshImportCreaseAngleDegrees = 1.0;
    LoadAnimationConfig();

    assert_true("mesh_import_policy_roundtrip_mode",
                animSettings.meshImportNormalMode == RAY_TRACING_MESH_IMPORT_NORMAL_MODE_SMOOTH);
    assert_true("mesh_import_policy_roundtrip_crease",
                animSettings.meshImportCreaseAngleDegrees == 90.0);

    animSettings.meshImportNormalMode = -1;
    animSettings.meshImportCreaseAngleDegrees = 999.0;
    ray_tracing_mesh_import_policy_normalize(&animSettings);
    assert_true("mesh_import_policy_invalid_mode_defaults_to_crease_aware",
                animSettings.meshImportNormalMode ==
                    RAY_TRACING_MESH_IMPORT_NORMAL_MODE_CREASE_AWARE);
    assert_true("mesh_import_policy_invalid_crease_defaults_to_60",
                animSettings.meshImportCreaseAngleDegrees ==
                    RAY_TRACING_MESH_IMPORT_CREASE_ANGLE_DEFAULT);

    restore_runtime_animation_config(backup, backup_size);
    return 0;
}

int run_test_config_animation_source_volume_suite(void) {
    int before = test_support_failures();

    test_scene_object_z_roundtrip();
    test_scene_object_z_missing_fallback();
    test_animation_scene_source_legacy_migration();
    test_animation_scene_source_roundtrip_runtime_lane();
    test_animation_volume_source_roundtrip_and_defaults();
    test_animation_scene_source_select_runtime_failure_rolls_back();
    test_animation_scene_source_select_runtime_persists_on_save();
    test_animation_scene_source_select_runtime_updates_auto_paired_volume_and_preserves_disable();
    test_animation_volume_source_select_without_apply_updates_lane_and_clear_resets();
    test_animation_volume_source_select_missing_apply_rolls_back();
    test_animation_apply_active_scene_source_invalid_fluid_preserves_selection();
    test_animation_restore_active_scene_source_preserves_runtime_selection_on_failure();
    test_animation_apply_runtime_scene_defers_missing_mesh_assets();
    test_animation_save_preserves_runtime_source_even_without_path();
    test_animation_mesh_asset_root_roundtrip();
    test_animation_mesh_import_policy_roundtrip_and_defaults();
    return test_support_failures() - before;
}
