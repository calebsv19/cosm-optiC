#include <string.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include "app/animation.h"
#include "editor/object_editor_motion.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_motion_bridge.h"
#include "render/runtime_scene_3d_builder.h"
#include "test_runtime_object_motion_tracks.h"
#include "test_support.h"

static bool nearly_equal(double a, double b) {
    return fabs(a - b) <= 1e-6;
}

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

static int test_object_motion_samples_primitive_transform_at_normalized_t(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_object_motion_sampled_primitive\","
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
            "},"
            "\"primitive\":{\"kind\":\"plane\",\"width\":2.0,\"height\":1.0}"
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
            "\"timing\":{\"domain\":\"normalized_t\",\"wrap\":\"hold\"},"
            "\"path\":{\"mode\":\"BEZIER_CUBIC\",\"points\":["
              "{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "{\"x\":2.0,\"y\":0.0,\"z\":1.0}"
            "]},"
            "\"rotation_keyframes\":["
              "{\"t\":0.0,\"yaw_degrees\":0.0,\"pitch_degrees\":0.0,\"roll_degrees\":0.0},"
              "{\"t\":1.0,\"yaw_degrees\":90.0,\"pitch_degrees\":0.0,\"roll_degrees\":0.0}"
            "]"
          "}"
        "]}}}"
        "}";
    RuntimeSceneBridgePreflight bridge_summary;
    RuntimeMotionTrack3DSummary motion_summary;
    RuntimeMotionTrack3DSample sample;
    RuntimeScene3D scene0;
    RuntimeScene3D scene1;
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &bridge_summary);
    assert_true("object_motion_sampled_primitive_apply_ok", ok);
    runtime_scene_motion_bridge_get_last_summary(&motion_summary);
    assert_true("object_motion_sampled_primitive_executable",
                motion_summary.has_executable_motion);
    assert_true("object_motion_sampled_primitive_position_path",
                motion_summary.position_path_tracks == 1);
    assert_true("object_motion_sampled_primitive_rotation_path",
                motion_summary.rotation_keyframe_tracks == 1);
    assert_true("object_motion_sampled_primitive_sample_count",
                motion_summary.sampled_tracks == 1);
    assert_true("object_motion_sampled_primitive_sample_ok",
                runtime_scene_motion_bridge_sample_object("plane_glider", 1.0, &sample));
    assert_true("object_motion_sampled_primitive_sample_position",
                sample.has_position &&
                nearly_equal(sample.position_x, 2.0) &&
                nearly_equal(sample.position_z, 1.0));

    RuntimeScene3D_Init(&scene0);
    RuntimeScene3D_Init(&scene1);
    assert_true("object_motion_sampled_primitive_build_0",
                RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene0, 0.0));
    assert_true("object_motion_sampled_primitive_build_1",
                RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene1, 1.0));
    assert_true("object_motion_sampled_primitive_origin_moves",
                scene0.primitiveCount == 1 &&
                scene1.primitiveCount == 1 &&
                nearly_equal(scene0.primitives[0].shape.plane.origin.x, 0.0) &&
                nearly_equal(scene1.primitives[0].shape.plane.origin.x, 2.0) &&
                nearly_equal(scene1.primitives[0].shape.plane.origin.z, 1.0));
    assert_true("object_motion_sampled_primitive_basis_rotates",
                scene1.primitiveCount == 1 &&
                fabs(scene1.primitives[0].shape.plane.axisU.z) > 0.99);
    RuntimeScene3D_Free(&scene0);
    RuntimeScene3D_Free(&scene1);
    return 0;
}

static double scene_triangle_min_x_for_object(const RuntimeScene3D *scene,
                                              const char *object_id) {
    bool seeded = false;
    double min_x = 0.0;
    if (!scene || !object_id) return 0.0;
    for (int i = 0; i < scene->triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D *triangle = &scene->triangleMesh.triangles[i];
        const RuntimePrimitive3D *primitive = NULL;
        if (triangle->primitiveIndex < 0 || triangle->primitiveIndex >= scene->primitiveCount) {
            continue;
        }
        primitive = &scene->primitives[triangle->primitiveIndex];
        if (strcmp(primitive->source.objectId, object_id) != 0) {
            continue;
        }
        const double values[3] = {triangle->p0.x, triangle->p1.x, triangle->p2.x};
        for (int j = 0; j < 3; ++j) {
            if (!seeded || values[j] < min_x) {
                min_x = values[j];
                seeded = true;
            }
        }
    }
    return min_x;
}

static int test_object_motion_samples_mesh_asset_instance_transform(void) {
    char cwd[4096] = {0};
    char mesh_path[8192] = {0};
    const char *scene_path = "/private/tmp/ray_tracing_om1_mesh_motion_scene.json";
    FILE *file = NULL;
    RuntimeSceneBridgePreflight bridge_summary = {0};
    RuntimeScene3D scene0;
    RuntimeScene3D scene1;
    bool ok = false;
    double min_x0 = 0.0;
    double min_x1 = 0.0;

    assert_true("object_motion_mesh_getcwd", getcwd(cwd, sizeof(cwd)) != NULL);
    snprintf(mesh_path,
             sizeof(mesh_path),
             "%s/tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json",
             cwd);
    file = fopen(scene_path, "w");
    assert_true("object_motion_mesh_open_scene", file != NULL);
    if (!file) return 0;
    fprintf(file,
            "{"
            "\"schema_family\":\"codework_scene\","
            "\"schema_variant\":\"scene_runtime_v1\","
            "\"schema_version\":1,"
            "\"scene_id\":\"scene_object_motion_sampled_mesh\","
            "\"unit_system\":\"meters\","
            "\"world_scale\":1.0,"
            "\"space_mode_default\":\"3d\","
            "\"objects\":[{"
            "\"object_id\":\"mesh_glider\","
            "\"object_type\":\"mesh_asset_instance\","
            "\"dimensional_mode\":\"full_3d\","
            "\"transform\":{"
            "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"rotation\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":0.5,\"y\":0.5,\"z\":0.5}"
            "},"
            "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_sphere_8x4\"},"
            "\"extensions\":{\"line_drawing\":{\"runtime_mesh_path\":\"%s\"}}"
            "}],"
            "\"materials\":[],"
            "\"lights\":[],"
            "\"cameras\":[],"
            "\"constraints\":[],"
            "\"extensions\":{\"ray_tracing\":{\"authoring\":{\"object_motion_tracks\":[{"
            "\"object_id\":\"mesh_glider\","
            "\"enabled\":true,"
            "\"mode\":\"authored_path\","
            "\"path\":{\"mode\":\"BEZIER_CUBIC\",\"points\":["
            "{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "{\"x\":3.0,\"y\":0.0,\"z\":0.0}"
            "]}"
            "}]}}}"
            "}",
            mesh_path);
    fclose(file);

    ok = runtime_scene_bridge_apply_file_defer_mesh_assets(scene_path, &bridge_summary);
    assert_true("object_motion_mesh_apply_ok", ok);
    RuntimeScene3D_Init(&scene0);
    RuntimeScene3D_Init(&scene1);
    assert_true("object_motion_mesh_build_0",
                RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene0, 0.0));
    assert_true("object_motion_mesh_build_1",
                RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(&scene1, 1.0));
    min_x0 = scene_triangle_min_x_for_object(&scene0, "mesh_glider");
    min_x1 = scene_triangle_min_x_for_object(&scene1, "mesh_glider");
    assert_true("object_motion_mesh_bounds_move", min_x1 > min_x0 + 2.0);
    RuntimeScene3D_Free(&scene0);
    RuntimeScene3D_Free(&scene1);
    return 0;
}

static int test_object_motion_editor_store_selected_object_contract(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_object_motion_editor_selected\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"selected_glider\","
          "\"object_type\":\"plane\","
          "\"transform\":{"
            "\"position\":{\"x\":4.0,\"y\":-2.0,\"z\":3.0},"
            "\"rotation\":{\"x\":0.0,\"y\":0.0,\"z\":0.25},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "},"
          "\"primitive\":{\"kind\":\"plane\",\"width\":1.0,\"height\":1.0}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight bridge_summary;
    const RuntimeMotionTrack3D *track = NULL;
    json_object *tracks = NULL;
    const char *serialized = NULL;
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &bridge_summary);
    assert_true("object_motion_editor_selected_apply_ok", ok);

    ObjectEditorMotionReset();
    assert_true("object_motion_editor_selected_set_authored",
                ObjectEditorMotionSetSelectedObjectAuthored(0));
    track = ObjectEditorMotionFindTrack("selected_glider");
    assert_true("object_motion_editor_selected_find_track", track != NULL);
    if (track) {
        assert_true("object_motion_editor_selected_enabled", track->enabled);
        assert_true("object_motion_editor_selected_authored_mode",
                    track->mode == RUNTIME_MOTION_TRACK_3D_MODE_AUTHORED_PATH);
        assert_true("object_motion_editor_selected_path_seeded",
                    track->has_position_path && track->position_path.numPoints == 1);
        assert_close("object_motion_editor_selected_path_x",
                     track->position_path.points[0].x,
                     sceneSettings.sceneObjects[0].x,
                     1e-6);
        assert_close("object_motion_editor_selected_path_z",
                     track->position_path_3d.point_z[0],
                     sceneSettings.sceneObjects[0].z,
                     1e-6);
    }

    tracks = ObjectEditorMotionBuildAuthoringTracksJson(1.0);
    assert_true("object_motion_editor_selected_tracks_json", tracks != NULL);
    serialized = tracks ? json_object_to_json_string_ext(tracks,
                                                         JSON_C_TO_STRING_NOSLASHESCAPE)
                        : NULL;
    assert_true("object_motion_editor_selected_json_has_id",
                serialized && strstr(serialized, "selected_glider") != NULL);
    assert_true("object_motion_editor_selected_json_has_timing",
                serialized && strstr(serialized, "\"normalized_t\"") != NULL);
    if (tracks) json_object_put(tracks);
    ObjectEditorMotionReset();
    return 0;
}

static int test_object_motion_editor_store_persists_runtime_scaled_track(void) {
    char runtime_json[8192];
    RuntimeSceneBridgePreflight bridge_summary;
    RuntimeMotionTrack3DSummary motion_summary;
    RuntimeMotionTrack3DSample sample;
    json_object *tracks = NULL;
    const char *tracks_json = NULL;

    ObjectEditorMotionReset();
    assert_true("object_motion_editor_scale_set_authored",
                ObjectEditorMotionSetObjectAuthored("scaled_glider",
                                                    4.0,
                                                    -2.0,
                                                    6.0,
                                                    M_PI / 4.0));
    tracks = ObjectEditorMotionBuildAuthoringTracksJson(0.5);
    assert_true("object_motion_editor_scale_tracks_json", tracks != NULL);
    tracks_json = tracks ? json_object_to_json_string_ext(tracks,
                                                          JSON_C_TO_STRING_NOSLASHESCAPE)
                         : "[]";
    snprintf(runtime_json,
             sizeof(runtime_json),
             "{"
             "\"schema_family\":\"codework_scene\","
             "\"schema_variant\":\"scene_runtime_v1\","
             "\"schema_version\":1,"
             "\"scene_id\":\"scene_object_motion_editor_scaled\","
             "\"unit_system\":\"meters\","
             "\"world_scale\":2.0,"
             "\"space_mode_default\":\"3d\","
             "\"objects\":[{"
               "\"object_id\":\"scaled_glider\","
               "\"object_type\":\"plane\","
               "\"transform\":{"
                 "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
                 "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
               "},"
               "\"primitive\":{\"kind\":\"plane\",\"width\":1.0,\"height\":1.0}"
             "}],"
             "\"materials\":[],"
             "\"lights\":[],"
             "\"cameras\":[],"
             "\"constraints\":[],"
             "\"extensions\":{\"ray_tracing\":{\"authoring\":{\"object_motion_tracks\":%s}}}"
             "}",
             tracks_json);
    if (tracks) json_object_put(tracks);

    assert_true("object_motion_editor_scale_apply",
                runtime_scene_bridge_apply_json(runtime_json, &bridge_summary));
    runtime_scene_motion_bridge_get_last_summary(&motion_summary);
    assert_true("object_motion_editor_scale_has_tracks",
                motion_summary.has_object_motion_tracks);
    assert_true("object_motion_editor_scale_sampled", motion_summary.sampled_tracks == 1);
    assert_true("object_motion_editor_scale_sample",
                runtime_scene_motion_bridge_sample_object("scaled_glider", 0.0, &sample));
    assert_true("object_motion_editor_scale_sample_position",
                sample.has_position &&
                nearly_equal(sample.position_x, 4.0) &&
                nearly_equal(sample.position_y, -2.0) &&
                nearly_equal(sample.position_z, 6.0));
    assert_true("object_motion_editor_scale_sample_rotation",
                sample.has_rotation && nearly_equal(sample.yaw_radians, M_PI / 4.0));

    ObjectEditorMotionHydrateFromRuntimeSummary(&motion_summary);
    assert_true("object_motion_editor_scale_hydrate_count",
                ObjectEditorMotionTrackCount() == 1);
    assert_true("object_motion_editor_scale_hydrate_find",
                ObjectEditorMotionFindTrack("scaled_glider") != NULL);
    assert_true("object_motion_editor_scale_static_disable",
                ObjectEditorMotionSetObjectStatic("scaled_glider"));
    assert_true("object_motion_editor_scale_static_track_disabled",
                ObjectEditorMotionFindTrack("scaled_glider") &&
                !ObjectEditorMotionFindTrack("scaled_glider")->enabled);
    ObjectEditorMotionReset();
    return 0;
}

int run_test_runtime_object_motion_tracks_tests(void) {
    int before = test_support_failures();

    test_object_motion_missing_tracks_resets_to_empty_contract();
    test_object_motion_rejects_non_array_tracks();
    test_object_motion_summarizes_authored_tracks_without_moving_geometry();
    test_object_motion_samples_primitive_transform_at_normalized_t();
    test_object_motion_samples_mesh_asset_instance_transform();
    test_object_motion_editor_store_selected_object_contract();
    test_object_motion_editor_store_persists_runtime_scaled_track();

    return test_support_failures() - before;
}
