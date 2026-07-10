#include <string.h>

#include "app/animation.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_motion_bridge.h"
#include "test_runtime_object_motion_tracks.h"
#include "test_support.h"

static int test_object_motion_missing_tracks_resets_to_empty_contract(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_object_motion_missing\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight bridge_summary;
    RuntimeMotionTrack3DSummary motion_summary;
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &bridge_summary);
    assert_true("object_motion_missing_apply_ok", ok);
    runtime_scene_motion_bridge_get_last_summary(&motion_summary);
    assert_true("object_motion_missing_valid", motion_summary.valid);
    assert_true("object_motion_missing_has_tracks_false",
                !motion_summary.has_object_motion_tracks);
    assert_true("object_motion_missing_total_zero", motion_summary.total_tracks == 0);
    assert_true("object_motion_missing_diag",
                strcmp(motion_summary.diagnostics, "object_motion_tracks_missing") == 0);
    return 0;
}

static int test_object_motion_rejects_non_array_tracks(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_object_motion_invalid\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{\"ray_tracing\":{\"authoring\":{\"object_motion_tracks\":{}}}}"
        "}";
    RuntimeSceneBridgePreflight bridge_summary;
    RuntimeMotionTrack3DSummary motion_summary;
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &bridge_summary);
    assert_true("object_motion_invalid_apply_ok", ok);
    runtime_scene_motion_bridge_get_last_summary(&motion_summary);
    assert_true("object_motion_invalid_has_tracks_true",
                motion_summary.has_object_motion_tracks);
    assert_true("object_motion_invalid_valid_false", !motion_summary.valid);
    assert_true("object_motion_invalid_diag",
                strcmp(motion_summary.diagnostics, "object_motion_tracks_not_array") == 0);
    return 0;
}

static int test_object_motion_summarizes_authored_tracks_without_moving_geometry(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_object_motion_contract\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"plane_glider\","
            "\"object_type\":\"plane\","
            "\"transform\":{"
              "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
            "}"
          "},"
          "{"
            "\"object_id\":\"box_target\","
            "\"object_type\":\"box\","
            "\"transform\":{"
              "\"position\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
            "}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{\"ray_tracing\":{\"authoring\":{\"object_motion_tracks\":["
          "{"
            "\"object_id\":\"plane_glider\","
            "\"enabled\":true,"
            "\"mode\":\"authored_path\","
            "\"timing_domain\":\"frame\","
            "\"wrap\":\"clamp\""
          "},"
          "{"
            "\"object_id\":\"missing_glider\","
            "\"enabled\":true,"
            "\"mode\":\"authored_path\""
          "},"
          "{"
            "\"object_id\":\"box_target\","
            "\"enabled\":false,"
            "\"mode\":\"physics\""
          "},"
          "{"
            "\"object_id\":\"plane_glider\","
            "\"enabled\":true,"
            "\"mode\":\"authored_path\""
          "},"
          "{"
            "\"object_id\":\"box_target\","
            "\"enabled\":true,"
            "\"mode\":\"physics\""
          "}"
        "]}}}"
        "}";
    RuntimeSceneBridgePreflight bridge_summary;
    RuntimeMotionTrack3DSummary motion_summary;
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &bridge_summary);
    assert_true("object_motion_contract_apply_ok", ok);
    runtime_scene_motion_bridge_get_last_summary(&motion_summary);
    assert_true("object_motion_contract_valid", motion_summary.valid);
    assert_true("object_motion_contract_has_tracks", motion_summary.has_object_motion_tracks);
    assert_true("object_motion_contract_total", motion_summary.total_tracks == 5);
    assert_true("object_motion_contract_stored", motion_summary.stored_tracks == 5);
    assert_true("object_motion_contract_enabled", motion_summary.enabled_tracks == 4);
    assert_true("object_motion_contract_disabled", motion_summary.disabled_tracks == 1);
    assert_true("object_motion_contract_matched", motion_summary.matched_tracks == 3);
    assert_true("object_motion_contract_unmatched", motion_summary.unmatched_tracks == 1);
    assert_true("object_motion_contract_unsupported", motion_summary.unsupported_tracks == 1);
    assert_true("object_motion_contract_duplicate", motion_summary.duplicate_tracks == 1);
    assert_true("object_motion_contract_authored", motion_summary.authored_path_tracks == 3);
    assert_true("object_motion_contract_physics", motion_summary.physics_tracks == 2);
    assert_true("object_motion_contract_first",
                strcmp(motion_summary.first_object_id, "plane_glider") == 0);
    assert_true("object_motion_contract_first_unmatched",
                strcmp(motion_summary.first_unmatched_object_id, "missing_glider") == 0);
    assert_true("object_motion_contract_first_unsupported",
                strcmp(motion_summary.first_unsupported_object_id, "box_target") == 0);
    assert_true("object_motion_contract_first_track_matched",
                motion_summary.tracks[0].matched_object);
    assert_true("object_motion_contract_duplicate_track_flag",
                motion_summary.tracks[3].duplicate_object);
    assert_true("object_motion_contract_no_geometry_motion",
                sceneSettings.sceneObjects[0].x == 0.0 &&
                sceneSettings.sceneObjects[0].y == 0.0 &&
                sceneSettings.sceneObjects[0].z == 0.0);
    return 0;
}

int run_test_runtime_object_motion_tracks_tests(void) {
    int before = test_support_failures();

    test_object_motion_missing_tracks_resets_to_empty_contract();
    test_object_motion_rejects_non_array_tracks();
    test_object_motion_summarizes_authored_tracks_without_moving_geometry();

    return test_support_failures() - before;
}
