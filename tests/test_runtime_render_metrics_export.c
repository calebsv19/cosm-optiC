#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app/animation.h"
#include "app/animation_output.h"
#include "core_scene_compile.h"
#include "import/runtime_scene_bridge.h"
#include "render/ray_tracing_integrator_catalog.h"
#include "test_runtime_render_metrics_export.h"
#include "test_support.h"

#include "cJSON.h"

static int test_animation_output_render_metrics_route_truth_contract(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    int saved_frame_counter = frameCounter;
    int saved_loop_count = loopCount;
    double saved_current_time = currentTime;
    const char* export_env = getenv("RAY_TRACING_EXPORT_RENDER_METRICS_DATASET");
    const char* path_env = getenv("RAY_TRACING_RENDER_METRICS_DATASET_PATH");
    char* export_env_backup = export_env ? strdup(export_env) : NULL;
    char* path_env_backup = path_env ? strdup(path_env) : NULL;
    char tmp_template[] = "/tmp/ray_tracing_metrics_route_truthXXXXXX";
    int fd = mkstemp(tmp_template);
    char json_path[1024];
    char* json_text = NULL;
    const char *runtime_json_native =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_metrics_native_route\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"block\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-3.0,\"z\":0.5},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":2.0,\"height\":2.0,\"depth\":1.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":-1.5,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *runtime_json_compat =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_metrics_compat_route\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_box\","
            "\"object_type\":\"box\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_plane\","
            "\"object_type\":\"plane\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":20.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    cJSON* root = NULL;
    cJSON* metadata = NULL;
    cJSON* items = NULL;
    cJSON* table = NULL;
    cJSON* row0 = NULL;

    if (fd < 0) {
        assert_true("animation_output_metrics_route_truth_mkstemp", false);
        free(export_env_backup);
        free(path_env_backup);
        return 0;
    }
    close(fd);

    if (snprintf(json_path, sizeof(json_path), "%s.json", tmp_template) >= (int)sizeof(json_path)) {
        assert_true("animation_output_metrics_route_truth_path_overflow", false);
        unlink(tmp_template);
        free(export_env_backup);
        free(path_env_backup);
        return 0;
    }
    if (rename(tmp_template, json_path) != 0) {
        assert_true("animation_output_metrics_route_truth_path_rename", false);
        unlink(tmp_template);
        free(export_env_backup);
        free(path_env_backup);
        return 0;
    }

    setenv("RAY_TRACING_EXPORT_RENDER_METRICS_DATASET", "1", 1);
    setenv("RAY_TRACING_RENDER_METRICS_DATASET_PATH", json_path, 1);
    frameCounter = 7;
    loopCount = 3;
    currentTime = 1.5;

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_2D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_HYBRID;
    animSettings.fps = 30;
    animSettings.frameDuration = 1.0 / 30.0;
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_2d_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_2d_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_2d_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_2d_legacy_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode") &&
                    cJSON_GetObjectItem(row0, "integrator_mode")->valueint == 1);
        assert_true("animation_output_metrics_route_truth_2d_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 0);
        assert_true("animation_output_metrics_route_truth_2d_uses_3d_false",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    !cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_2d_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator: Hybrid") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIRECT_LIGHT;
    assert_true("animation_output_metrics_route_truth_native_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_native, &summary));
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_native_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_native_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_native_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_native_legacy_field_preserved",
                    cJSON_GetObjectItem(row0, "integrator_mode") &&
                    cJSON_GetObjectItem(row0, "integrator_mode")->valueint == 0);
        assert_true("animation_output_metrics_route_truth_native_3d_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode_3d") &&
                    cJSON_GetObjectItem(row0, "integrator_mode_3d")->valueint == 0);
        assert_true("animation_output_metrics_route_truth_native_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 2);
        assert_true("animation_output_metrics_route_truth_native_uses_3d_true",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_native_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator: 3D Direct Light") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DIFFUSE_BOUNCE;
    assert_true("animation_output_metrics_route_truth_native_diffuse_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_native, &summary));
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_native_diffuse_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_native_diffuse_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_native_diffuse_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_native_diffuse_3d_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode_3d") &&
                    cJSON_GetObjectItem(row0, "integrator_mode_3d")->valueint == 1);
        assert_true("animation_output_metrics_route_truth_native_diffuse_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 2);
        assert_true("animation_output_metrics_route_truth_native_diffuse_uses_3d_true",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_native_diffuse_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator: 3D Diffuse Bounce") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_MATERIAL;
    assert_true("animation_output_metrics_route_truth_native_material_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_native, &summary));
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_native_material_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_native_material_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_native_material_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_native_material_3d_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode_3d") &&
                    cJSON_GetObjectItem(row0, "integrator_mode_3d")->valueint == 2);
        assert_true("animation_output_metrics_route_truth_native_material_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 2);
        assert_true("animation_output_metrics_route_truth_native_material_uses_3d_true",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_native_material_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator: 3D Material") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_EMISSION_TRANSPARENCY;
    assert_true("animation_output_metrics_route_truth_native_emission_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_native, &summary));
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_native_emission_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_native_emission_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_native_emission_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_native_emission_3d_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode_3d") &&
                    cJSON_GetObjectItem(row0, "integrator_mode_3d")->valueint == 3);
        assert_true("animation_output_metrics_route_truth_native_emission_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 2);
        assert_true("animation_output_metrics_route_truth_native_emission_uses_3d_true",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_native_emission_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator: 3D Emission / Transparency") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    memset(&animSettings, 0, sizeof(animSettings));
    memset(&sceneSettings, 0, sizeof(sceneSettings));
    animSettings.spaceMode = SPACE_MODE_3D;
    animSettings.integratorMode = RAY_TRACING_2D_INTEGRATOR_FORWARD_LIGHT;
    animSettings.integratorMode3D = RAY_TRACING_3D_INTEGRATOR_DISNEY;
    assert_true("animation_output_metrics_route_truth_compat_apply_ok",
                runtime_scene_bridge_apply_json(runtime_json_compat, &summary));
    AnimationExportRenderMetricsDatasetIfEnabled();
    json_text = read_text_file_alloc(json_path, NULL);
    assert_true("animation_output_metrics_route_truth_compat_json", json_text != NULL);
    if (json_text) {
        root = cJSON_Parse(json_text);
        metadata = root ? cJSON_GetObjectItem(root, "metadata") : NULL;
        items = root ? cJSON_GetObjectItem(root, "items") : NULL;
        table = find_named_dataset_entry(items, "render_metrics_table_v2");
        row0 = table ? cJSON_GetObjectItem(table, "row0") : NULL;
        assert_true("animation_output_metrics_route_truth_compat_parse", root != NULL);
        assert_true("animation_output_metrics_route_truth_compat_row0", row0 != NULL);
        assert_true("animation_output_metrics_route_truth_compat_3d_mode",
                    cJSON_GetObjectItem(row0, "integrator_mode_3d") &&
                    cJSON_GetObjectItem(row0, "integrator_mode_3d")->valueint == 0);
        assert_true("animation_output_metrics_route_truth_compat_route_family",
                    cJSON_GetObjectItem(row0, "route_family") &&
                    cJSON_GetObjectItem(row0, "route_family")->valueint == 1);
        assert_true("animation_output_metrics_route_truth_compat_uses_3d_true",
                    cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog") &&
                    cJSON_IsTrue(cJSON_GetObjectItem(row0, "integrator_uses_3d_catalog")));
        assert_true("animation_output_metrics_route_truth_compat_status_label",
                    metadata &&
                    cJSON_GetObjectItem(metadata, "integrator_status_label") &&
                    strcmp(cJSON_GetObjectItem(metadata, "integrator_status_label")->valuestring,
                           "integrator fallback: compat 3D Direct Light") == 0);
        cJSON_Delete(root);
        root = NULL;
        free(json_text);
        json_text = NULL;
    }

    unlink(json_path);
    restore_env_or_unset("RAY_TRACING_EXPORT_RENDER_METRICS_DATASET", export_env_backup);
    restore_env_or_unset("RAY_TRACING_RENDER_METRICS_DATASET_PATH", path_env_backup);
    free(export_env_backup);
    free(path_env_backup);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    frameCounter = saved_frame_counter;
    loopCount = saved_loop_count;
    currentTime = saved_current_time;
    return 0;
}

int run_test_runtime_render_metrics_export_tests(void) {
    test_animation_output_render_metrics_route_truth_contract();
    return 0;
}
