#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "config/config_manager.h"
#include "core_scene_compile.h"
#include "import/runtime_scene_bridge.h"
#include "test_runtime_scene_bridge_writeback.h"
#include "test_support.h"

static int test_runtime_scene_bridge_writeback_overlay_preserves_non_ray_state(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_1\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[{\"object_id\":\"obj_base\"}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"physics_sim\":{\"gravity\":9.81},"
          "\"custom_tool\":{\"foo\":1}"
        "},"
        "\"compile_meta\":{\"compiler\":\"core_scene_compile\"}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":10},"
        "\"space_mode_default\":\"3d\","
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"exposure\":1.25,"
            "\"integrator\":\"hybrid\""
          "}"
        "}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_overlay_ok", ok);
    if (!ok || !merged) {
        free(merged);
        return 0;
    }

    assert_true("runtime_scene_writeback_preserve_physics",
                strstr(merged, "\"physics_sim\"") != NULL);
    assert_true("runtime_scene_writeback_preserve_custom",
                strstr(merged, "\"custom_tool\"") != NULL);
    assert_true("runtime_scene_writeback_add_ray",
                strstr(merged, "\"ray_tracing\"") != NULL);
    assert_true("runtime_scene_writeback_update_space_mode",
                strstr(merged, "\"space_mode_default\":\"3d\"") != NULL);
    assert_true("runtime_scene_writeback_preserve_compile_meta",
                strstr(merged, "\"compile_meta\"") != NULL);
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_foreign_extension_namespace(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_2\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":11},"
        "\"extensions\":{"
          "\"physics_sim\":{\"gravity\":9.81}"
        "}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_foreign_namespace", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_foreign_diag",
                    strstr(diagnostics, "namespace not allowed") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_forbidden_top_level_overlay_key(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_3\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":12},"
        "\"objects\":[{\"object_id\":\"hack\"}]"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_forbidden_top_key", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_forbidden_top_key_diag",
                    strstr(diagnostics, "overlay key not allowed") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_invalid_space_mode_value(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_4\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":13},"
        "\"space_mode_default\":\"4d\""
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_invalid_space_mode", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_invalid_space_mode_diag",
                    strstr(diagnostics, "space_mode_default must be '2d' or '3d'") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_non_object_ray_extension_payload(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_5\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":14},"
        "\"extensions\":{"
          "\"ray_tracing\":[1,2,3]"
        "}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_non_object_ray_payload", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_non_object_ray_payload_diag",
                    strstr(diagnostics, "payload must be object") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_missing_overlay_meta(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_6\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"extensions\":{\"ray_tracing\":{\"samples\":8}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_missing_meta", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_missing_meta_diag",
                    strstr(diagnostics, "overlay_meta object is required") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_stale_logical_clock(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_7\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"overlay_merge\":{"
            "\"producer_clocks\":{\"ray_tracing\":20}"
          "}"
        "}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":20},"
        "\"extensions\":{\"ray_tracing\":{\"samples\":16}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_stale_clock", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_stale_clock_diag",
                    strstr(diagnostics, "logical_clock is stale") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_wrong_overlay_producer(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_8\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_sim\",\"logical_clock\":1},"
        "\"extensions\":{\"ray_tracing\":{\"samples\":8}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_wrong_producer", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_wrong_producer_diag",
                    strstr(diagnostics, "producer not allowed") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_negative_logical_clock(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_9\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":-1},"
        "\"extensions\":{\"ray_tracing\":{\"samples\":8}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_negative_clock", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_negative_clock_diag",
                    strstr(diagnostics, "logical_clock must be >= 0") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_runtime_core_unit_system_overlay(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_10\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":15},"
        "\"unit_system\":\"centimeters\""
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_unit_system_top_key", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_unit_system_top_key_diag",
                    strstr(diagnostics, "overlay key not allowed") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_rejects_runtime_core_world_scale_overlay(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_11\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":16},"
        "\"world_scale\":2.0"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_reject_world_scale_top_key", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_reject_world_scale_top_key_diag",
                    strstr(diagnostics, "overlay key not allowed") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_space_mode_tiebreak_rejects_lexically_larger_producer(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_12\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"overlay_merge\":{"
            "\"space_mode_default\":{\"producer\":\"line_drawing\",\"logical_clock\":50}"
          "}"
        "}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":50},"
        "\"space_mode_default\":\"3d\","
        "\"extensions\":{\"ray_tracing\":{\"samples\":8}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_tiebreak_reject_larger_producer", !ok);
    if (!ok) {
        assert_true("runtime_scene_writeback_tiebreak_reject_larger_producer_diag",
                    strstr(diagnostics, "tie-break lost") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_writeback_space_mode_tiebreak_accepts_lexically_smaller_producer(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_writeback_13\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"overlay_merge\":{"
            "\"space_mode_default\":{\"producer\":\"ray_tracing\",\"logical_clock\":50}"
          "}"
        "}"
        "}";
    const char *overlay_json =
        "{"
        "\"overlay_meta\":{\"producer\":\"ray_tracing\",\"logical_clock\":51},"
        "\"space_mode_default\":\"3d\","
        "\"extensions\":{\"ray_tracing\":{\"samples\":32}}"
        "}";
    char *merged = NULL;
    char diagnostics[256];
    bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                              overlay_json,
                                                              &merged,
                                                              diagnostics,
                                                              sizeof(diagnostics));
    assert_true("runtime_scene_writeback_tiebreak_accept_same_producer_newer_clock", ok);
    if (ok && merged) {
        assert_true("runtime_scene_writeback_tiebreak_accept_space_mode_updated",
                    strstr(merged, "\"space_mode_default\":\"3d\"") != NULL);
    }
    free(merged);
    return 0;
}

static int test_runtime_scene_bridge_trio_fixture_compile_writeback_apply(void) {
    size_t authoring_size = 0;
    size_t overlay_size = 0;
    char *authoring_json = read_text_file_alloc("third_party/codework_shared/assets/scenes/trio_contract/scene_authoring_interop_min.json",
                                                &authoring_size);
    char *overlay_json = read_text_file_alloc("third_party/codework_shared/assets/scenes/trio_contract/ray_overlay_min.json",
                                              &overlay_size);
    char diagnostics[256];
    char *runtime_json = NULL;
    char *merged_json = NULL;
    RuntimeSceneBridgePreflight preflight;
    CoreResult r;

    assert_true("trio_fixture_authoring_read_ok", authoring_json != NULL && authoring_size > 0);
    assert_true("trio_fixture_ray_overlay_read_ok", overlay_json != NULL && overlay_size > 0);
    if (!authoring_json || !overlay_json) {
        free(authoring_json);
        free(overlay_json);
        return 0;
    }

    r = core_scene_compile_authoring_to_runtime(authoring_json,
                                                &runtime_json,
                                                diagnostics,
                                                sizeof(diagnostics));
    assert_true("trio_fixture_compile_ok", r.code == CORE_OK);
    if (r.code != CORE_OK || !runtime_json) {
        free(authoring_json);
        free(overlay_json);
        free(runtime_json);
        return 0;
    }

    {
        bool ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_json,
                                                                  overlay_json,
                                                                  &merged_json,
                                                                  diagnostics,
                                                                  sizeof(diagnostics));
        assert_true("trio_fixture_ray_writeback_ok", ok);
    }

    if (!merged_json) {
        free(authoring_json);
        free(overlay_json);
        free(runtime_json);
        free(merged_json);
        return 0;
    }

    assert_true("trio_fixture_preserve_line_drawing_ext",
                strstr(merged_json, "\"line_drawing\"") != NULL);
    assert_true("trio_fixture_preserve_physics_ext",
                strstr(merged_json, "\"physics_sim\"") != NULL);
    assert_true("trio_fixture_has_ray_ext",
                strstr(merged_json, "\"ray_tracing\"") != NULL);

    {
        bool ok = runtime_scene_bridge_preflight_json(merged_json, &preflight);
        assert_true("trio_fixture_preflight_after_writeback_ok", ok);
    }
    {
        bool ok = runtime_scene_bridge_apply_json(merged_json, &preflight);
        assert_true("trio_fixture_apply_after_writeback_ok", ok);
    }

    free(authoring_json);
    free(overlay_json);
    free(runtime_json);
    free(merged_json);
    return 0;
}

static int test_runtime_scene_bridge_apply_hydrates_ray_authoring_paths(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_anim = animSettings;
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_authoring_hydrate_1\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0}}],"
        "\"cameras\":[{\"position\":{\"x\":1.0,\"y\":1.0,\"z\":2.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"light_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{"
                    "\"x\":1.25,"
                    "\"y\":-0.75,"
                    "\"rotation\":0.0,"
                    "\"handleLink\":false,"
                    "\"velocity1\":{\"vx\":0.5,\"vy\":0.25}"
                  "},"
                  "{"
                    "\"x\":2.50,"
                    "\"y\":1.00,"
                    "\"rotation\":0.0,"
                    "\"handleLink\":false,"
                    "\"velocity2\":{\"vx\":-0.5,\"vy\":-0.25}"
                  "}"
                "]"
              "},"
              "\"camera_path\":{"
                "\"mode\":\"BEZIER_CUBIC\","
                "\"points\":["
                  "{"
                    "\"x\":2.0,"
                    "\"y\":3.0,"
                    "\"rotation\":0.5,"
                    "\"handleLink\":false"
                  "}"
                "]"
              "}"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_authoring_hydrate_apply_ok", ok);
    if (!ok) {
        sceneSettings = saved_scene;
        animSettings = saved_anim;
        return 0;
    }
    assert_true("runtime_scene_authoring_hydrate_bezier_points",
                sceneSettings.bezierPath.numPoints == 2);
    assert_close("runtime_scene_authoring_hydrate_light_p0_x",
                 sceneSettings.bezierPath.points[0].x,
                 2.5,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_light_p0_y",
                 sceneSettings.bezierPath.points[0].y,
                 -1.5,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_light_handle_vx",
                 sceneSettings.bezierPath.handles[0][0].vx,
                 1.0,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_light_handle_vy",
                 sceneSettings.bezierPath.handles[0][0].vy,
                 0.5,
                 1e-6);
    assert_true("runtime_scene_authoring_hydrate_camera_points",
                sceneSettings.cameraPath.numPoints == 1);
    assert_close("runtime_scene_authoring_hydrate_camera_p0_x",
                 sceneSettings.cameraPath.points[0].x,
                 4.0,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_camera_p0_y",
                 sceneSettings.cameraPath.points[0].y,
                 6.0,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_camera_x",
                 sceneSettings.camera.x,
                 4.0,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_camera_y",
                 sceneSettings.camera.y,
                 6.0,
                 1e-6);
    assert_close("runtime_scene_authoring_hydrate_camera_rot",
                 sceneSettings.camera.rotation,
                 0.5,
                 1e-6);
    sceneSettings = saved_scene;
    animSettings = saved_anim;
    return 0;
}


int run_test_runtime_scene_bridge_writeback_tests(void) {
    int before = test_support_failures();

    test_runtime_scene_bridge_writeback_overlay_preserves_non_ray_state();
    test_runtime_scene_bridge_writeback_rejects_foreign_extension_namespace();
    test_runtime_scene_bridge_writeback_rejects_forbidden_top_level_overlay_key();
    test_runtime_scene_bridge_writeback_rejects_invalid_space_mode_value();
    test_runtime_scene_bridge_writeback_rejects_non_object_ray_extension_payload();
    test_runtime_scene_bridge_writeback_rejects_missing_overlay_meta();
    test_runtime_scene_bridge_writeback_rejects_stale_logical_clock();
    test_runtime_scene_bridge_writeback_rejects_wrong_overlay_producer();
    test_runtime_scene_bridge_writeback_rejects_negative_logical_clock();
    test_runtime_scene_bridge_writeback_rejects_runtime_core_unit_system_overlay();
    test_runtime_scene_bridge_writeback_rejects_runtime_core_world_scale_overlay();
    test_runtime_scene_bridge_writeback_space_mode_tiebreak_rejects_lexically_larger_producer();
    test_runtime_scene_bridge_writeback_space_mode_tiebreak_accepts_lexically_smaller_producer();
    test_runtime_scene_bridge_trio_fixture_compile_writeback_apply();
    test_runtime_scene_bridge_apply_hydrates_ray_authoring_paths();

    return test_support_failures() - before;
}
