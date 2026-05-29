#include <stdlib.h>
#include <string.h>

#include "app/animation.h"
#include "core_scene_compile.h"
#include "import/runtime_scene_bridge.h"
#include "test_runtime_scene_bridge_core.h"
#include "test_support.h"

static int test_runtime_scene_bridge_preflight_accepts_runtime_contract(void) {
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_file("third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json",
                                                  &preflight);
    assert_true("runtime_scene_preflight_accept_fixture", ok);
    if (ok) {
        assert_true("runtime_scene_preflight_valid_contract", preflight.valid_contract);
        assert_true("runtime_scene_preflight_scene_id",
                    strcmp(preflight.scene_id, "scene_trio_min") == 0);
        assert_true("runtime_scene_preflight_object_count", preflight.object_count == 1);
    }
    return 0;
}

static int test_runtime_scene_bridge_rejects_authoring_variant(void) {
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_file("third_party/codework_shared/assets/scenes/trio_contract/scene_authoring_min.json",
                                                  &preflight);
    assert_true("runtime_scene_preflight_reject_authoring", !ok);
    if (!ok) {
        assert_true("runtime_scene_preflight_diag_schema_variant",
                    strstr(preflight.diagnostics, "scene_runtime_v1") != NULL);
    }
    return 0;
}

static int test_runtime_scene_bridge_rejects_malformed_runtime_payload(void) {
    const char *runtime_json_missing_variant =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_missing_variant\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_json(runtime_json_missing_variant, &preflight);
    assert_true("runtime_scene_preflight_reject_missing_variant", !ok);
    if (!ok) {
        assert_true("runtime_scene_preflight_diag_missing_variant",
                    strstr(preflight.diagnostics, "missing schema_variant") != NULL);
    }
    return 0;
}

static int test_runtime_scene_bridge_optional_lanes_default_deterministic(void) {
    const char *runtime_json_missing_optional_lanes =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_optional_lanes\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[{"
          "\"object_id\":\"obj_optional_1\","
          "\"object_type\":\"circle\","
          "\"transform\":{"
            "\"position\":{\"x\":3.0,\"y\":4.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "}"
        "}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight preflight;
    RuntimeSceneBridgePreflight summary;
    bool ok = runtime_scene_bridge_preflight_json(runtime_json_missing_optional_lanes, &preflight);
    assert_true("runtime_scene_preflight_optional_lanes_ok", ok);
    if (ok) {
        assert_true("runtime_scene_preflight_optional_material_count_zero", preflight.material_count == 0);
        assert_true("runtime_scene_preflight_optional_light_count_zero", preflight.light_count == 0);
        assert_true("runtime_scene_preflight_optional_camera_count_zero", preflight.camera_count == 0);
        assert_true("runtime_scene_preflight_optional_object_count_one", preflight.object_count == 1);
    }

    ok = runtime_scene_bridge_apply_json(runtime_json_missing_optional_lanes, &summary);
    assert_true("runtime_scene_apply_optional_lanes_ok", ok);
    if (ok) {
        assert_true("runtime_scene_apply_optional_object_count_one", sceneSettings.objectCount == 1);
        assert_true("runtime_scene_apply_optional_material_count_zero", summary.material_count == 0);
        assert_true("runtime_scene_apply_optional_light_count_zero", summary.light_count == 0);
        assert_true("runtime_scene_apply_optional_camera_count_zero", summary.camera_count == 0);
        assert_close("runtime_scene_apply_optional_object_x", sceneSettings.sceneObjects[0].x, 3.0, 1e-9);
        assert_close("runtime_scene_apply_optional_object_y", sceneSettings.sceneObjects[0].y, 4.0, 1e-9);
        assert_close("runtime_scene_apply_optional_camera_x_default", sceneSettings.camera.x, 0.0, 1e-9);
        assert_close("runtime_scene_apply_optional_camera_y_default", sceneSettings.camera.y, 0.0, 1e-9);
        assert_close("runtime_scene_apply_optional_light_path_x_default",
                     sceneSettings.bezierPath.points[0].x,
                     0.0,
                     1e-9);
        assert_close("runtime_scene_apply_optional_light_path_y_default",
                     sceneSettings.bezierPath.points[0].y,
                     0.0,
                     1e-9);
    }
    return 0;
}

static int test_runtime_scene_bridge_rejects_noncanonical_unit_system(void) {
    const char *runtime_json_noncanonical_units =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_units_invalid\","
        "\"unit_system\":\"centimeters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"2d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    RuntimeSceneBridgePreflight preflight;
    bool ok = runtime_scene_bridge_preflight_json(runtime_json_noncanonical_units, &preflight);
    assert_true("runtime_scene_preflight_reject_noncanonical_units", !ok);
    if (!ok) {
        assert_true("runtime_scene_preflight_diag_noncanonical_units",
                    strstr(preflight.diagnostics, "unit_system must be meters") != NULL);
    }
    return 0;
}

static int test_runtime_scene_bridge_apply_uses_world_scale_mapping(void) {
    const char *runtime_json_scaled =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_world_scale\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"obj_scale_1\","
          "\"object_type\":\"circle\","
          "\"transform\":{"
            "\"position\":{\"x\":2.0,\"y\":3.0,\"z\":4.0},"
            "\"scale\":{\"x\":1.0,\"y\":2.0,\"z\":3.0}"
          "}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":5.0,\"y\":6.0,\"z\":7.0}}],"
        "\"cameras\":[{\"position\":{\"x\":8.0,\"y\":9.0,\"z\":10.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    bool ok = runtime_scene_bridge_apply_json(runtime_json_scaled, &summary);
    assert_true("runtime_scene_apply_world_scale_ok", ok);
    if (!ok) return 0;
    assert_true("runtime_scene_apply_world_scale_object_count_one", sceneSettings.objectCount == 1);
    assert_close("runtime_scene_apply_world_scale_object_x", sceneSettings.sceneObjects[0].x, 4.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_object_y", sceneSettings.sceneObjects[0].y, 6.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_object_z", sceneSettings.sceneObjects[0].z, 8.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_object_scale", sceneSettings.sceneObjects[0].scale, 4.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_light_x", sceneSettings.bezierPath.points[0].x, 10.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_light_y", sceneSettings.bezierPath.points[0].y, 12.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_camera_x", sceneSettings.camera.x, 16.0, 1e-9);
    assert_close("runtime_scene_apply_world_scale_camera_y", sceneSettings.camera.y, 18.0, 1e-9);
    return 0;
}

static int test_runtime_scene_bridge_apply_preserves_editor_mode_state(void) {
    const char *runtime_json_no_editor_mutation =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_editor_state\","
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
    RuntimeSceneBridgePreflight summary;
    bool ok = false;
    int original_editor_mode = animSettings.editorMode;
    animSettings.editorMode = 2;
    ok = runtime_scene_bridge_apply_json(runtime_json_no_editor_mutation, &summary);
    assert_true("runtime_scene_apply_preserve_editor_mode_apply_ok", ok);
    assert_true("runtime_scene_apply_preserve_editor_mode_unchanged", animSettings.editorMode == 2);
    animSettings.editorMode = original_editor_mode;
    return 0;
}

static int test_runtime_scene_bridge_apply_3d_primitives_scaffold(void) {
    const char *runtime_json_primitives =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_3d_primitives\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_box\","
            "\"object_type\":\"box\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_plane\","
            "\"object_type\":\"plane\","
            "\"transform\":{\"position\":{\"x\":5.0,\"y\":0.0,\"z\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"obj_mesh\","
            "\"object_type\":\"triangle_mesh\","
            "\"transform\":{\"position\":{\"x\":10.0,\"y\":0.0,\"z\":-1.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":20.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneBridge3DScaffoldState scaffold;
    RuntimeSceneBridge3DPrimitiveSeedState seeds;
    bool ok = runtime_scene_bridge_apply_json(runtime_json_primitives, &summary);
    assert_true("runtime_scene_apply_3d_primitives_ok", ok);
    if (!ok) return 0;

    assert_true("runtime_scene_apply_3d_primitives_object_count", sceneSettings.objectCount == 3);
    assert_true("runtime_scene_apply_3d_primitives_box_polygon",
                sceneSettings.sceneObjects[0].numPoints == 4);
    assert_true("runtime_scene_apply_3d_primitives_plane_polygon",
                sceneSettings.sceneObjects[1].numPoints == 4);
    assert_true("runtime_scene_apply_3d_primitives_mesh_triangle",
                sceneSettings.sceneObjects[2].numPoints == 3);

    runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
    assert_true("runtime_scene_apply_3d_primitives_scaffold_valid", scaffold.valid);
    assert_true("runtime_scene_apply_3d_primitives_scaffold_has_camera", scaffold.has_camera_seed);
    assert_close("runtime_scene_apply_3d_primitives_scaffold_camera_z", scaffold.camera_z, 20.0, 1e-9);
    assert_true("runtime_scene_apply_3d_primitives_scaffold_box_count", scaffold.box_count == 1);
    assert_true("runtime_scene_apply_3d_primitives_scaffold_plane_count", scaffold.plane_count == 1);
    assert_true("runtime_scene_apply_3d_primitives_scaffold_mesh_count", scaffold.triangle_mesh_count == 1);

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    assert_true("runtime_scene_apply_3d_primitives_seeds_valid", seeds.valid);
    assert_true("runtime_scene_apply_3d_primitives_seeds_plane_count", seeds.plane_primitive_count == 1);
    assert_true("runtime_scene_apply_3d_primitives_seeds_rect_prism_count",
                seeds.rect_prism_primitive_count == 0);
    assert_true("runtime_scene_apply_3d_primitives_seeds_retained_count", seeds.primitive_count == 1);
    assert_true("runtime_scene_apply_3d_primitives_seeds_excluded_count",
                seeds.excluded_primitive_count == 2);
    return 0;
}

static int test_runtime_scene_bridge_apply_authoring_helpers_do_not_block_native_3d(void) {
    const char *runtime_json_helpers =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_helpers_native_3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane_primitive\","
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
          "},"
          "{"
            "\"object_id\":\"author_points\","
            "\"object_type\":\"point_set\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"author_curve\","
            "\"object_type\":\"curve_path\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"author_edges\","
            "\"object_type\":\"edge_set\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":-1.5,\"z\":2.0}}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridge3DPrimitiveSeedState seeds;
    bool ok = runtime_scene_bridge_apply_json(runtime_json_helpers, &summary);
    assert_true("runtime_scene_apply_helpers_native3d_ok", ok);
    if (!ok) return 0;

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    assert_true("runtime_scene_apply_helpers_native3d_seeds_valid", seeds.valid);
    assert_true("runtime_scene_apply_helpers_native3d_retained_count", seeds.primitive_count == 2);
    assert_true("runtime_scene_apply_helpers_native3d_plane_count", seeds.plane_primitive_count == 1);
    assert_true("runtime_scene_apply_helpers_native3d_prism_count",
                seeds.rect_prism_primitive_count == 1);
    assert_true("runtime_scene_apply_helpers_native3d_excluded_count",
                seeds.excluded_primitive_count == 0);
    return 0;
}

static int test_runtime_scene_bridge_apply_authoring_tag_does_not_mark_real_geometry_guide_only(void) {
    const char *runtime_json_authored_geometry =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_authored_geometry_native_3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane_primitive\","
            "\"tags\":[\"authoring\",\"preview\"],"
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"panel\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"tags\":[\"authoring\"],"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.5},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":1.0,\"height\":1.0,\"depth\":2.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":0.0,\"y\":1.0,\"z\":3.0},\"intensity\":1.0}],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    RuntimeSceneBridge3DDigestState digest = {0};
    bool ok = runtime_scene_bridge_apply_json(runtime_json_authored_geometry, &summary);
    assert_true("runtime_scene_apply_authoring_tag_geometry_ok", ok);
    if (!ok) return 0;

    assert_true("runtime_scene_apply_authoring_tag_geometry_object_count_two",
                sceneSettings.objectCount == 2);
    assert_true("runtime_scene_apply_authoring_tag_floor_not_guide",
                !SceneObjectIsGuideOnly(&sceneSettings.sceneObjects[0]));
    assert_true("runtime_scene_apply_authoring_tag_panel_not_guide",
                !SceneObjectIsGuideOnly(&sceneSettings.sceneObjects[1]));
    assert_true("runtime_scene_apply_authoring_tag_floor_render_participant",
                SceneObjectParticipatesInRender(&sceneSettings.sceneObjects[0]));
    assert_true("runtime_scene_apply_authoring_tag_panel_render_participant",
                SceneObjectParticipatesInRender(&sceneSettings.sceneObjects[1]));

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    assert_true("runtime_scene_apply_authoring_tag_seeds_valid", seeds.valid);
    assert_true("runtime_scene_apply_authoring_tag_seed_count_two",
                seeds.primitive_count == 2);

    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    assert_true("runtime_scene_apply_authoring_tag_digest_valid", digest.valid);
    assert_true("runtime_scene_apply_authoring_tag_digest_count_two",
                digest.primitive_count == 2);
    assert_true("runtime_scene_apply_authoring_tag_floor_digest_not_guide",
                !digest.primitives[0].guide_only);
    assert_true("runtime_scene_apply_authoring_tag_panel_digest_not_guide",
                !digest.primitives[1].guide_only);
    return 0;
}

static int test_runtime_scene_bridge_apply_physics_emitter_overlay_marks_guide_only(void) {
    const char *runtime_json_emitter_helper =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_emitter_helper_native_3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane_primitive\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"emitter_guide\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.5},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":1.0,\"height\":1.0,\"depth\":2.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"physics_sim\":{"
            "\"object_overlays\":[{"
              "\"object_id\":\"emitter_guide\","
              "\"motion_mode\":\"Static\","
              "\"initial_velocity\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"emitter\":{"
                "\"active\":true,"
                "\"type\":\"Jet\","
                "\"radius\":1.0,"
                "\"strength\":1.0,"
                "\"direction\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}"
              "}"
            "}]"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    RuntimeSceneBridge3DDigestState digest = {0};
    bool ok = runtime_scene_bridge_apply_json(runtime_json_emitter_helper, &summary);
    assert_true("runtime_scene_apply_emitter_helper_ok", ok);
    if (!ok) return 0;

    assert_true("runtime_scene_apply_emitter_helper_object_count_one",
                sceneSettings.objectCount == 1);
    assert_true("runtime_scene_apply_emitter_helper_floor_not_guide",
                !SceneObjectIsGuideOnly(&sceneSettings.sceneObjects[0]));
    assert_true("runtime_scene_apply_emitter_helper_floor_render_participant",
                SceneObjectParticipatesInRender(&sceneSettings.sceneObjects[0]));

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    assert_true("runtime_scene_apply_emitter_helper_seeds_valid", seeds.valid);
    assert_true("runtime_scene_apply_emitter_helper_retained_seed_count_one",
                seeds.primitive_count == 1);
    assert_true("runtime_scene_apply_emitter_helper_seed_floor_index_zero",
                seeds.primitives[0].scene_object_index == 0);

    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    assert_true("runtime_scene_apply_emitter_helper_digest_valid", digest.valid);
    assert_true("runtime_scene_apply_emitter_helper_digest_count_two",
                digest.primitive_count == 2);
    assert_true("runtime_scene_apply_emitter_helper_digest_floor_scene_object_index_zero",
                digest.primitives[0].scene_object_index == 0);
    assert_true("runtime_scene_apply_emitter_helper_digest_emitter_scene_object_index_one",
                digest.primitives[1].scene_object_index == 1);
    assert_true("runtime_scene_apply_emitter_helper_digest_emitter_guide_only",
                digest.primitives[1].guide_only);
    assert_true("runtime_scene_apply_emitter_helper_digest_guide_flag",
                digest.primitives[1].guide_only);
    return 0;
}

static int test_runtime_scene_bridge_authoring_overlay_does_not_override_helper_tint(void) {
    const char *runtime_json_emitter_helper =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_emitter_helper_tint_lock\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"floor\","
            "\"object_type\":\"plane_primitive\","
            "\"primitive\":{\"kind\":\"plane\",\"width\":6.0,\"height\":6.0,"
              "\"frame\":{\"origin\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":0.0,\"z\":1.0},"
              "\"normal\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}}},"
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":-5.0,\"z\":0.0},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}"
          "},"
          "{"
            "\"object_id\":\"emitter_guide\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"transform\":{\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.5},"
              "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
            "\"primitive\":{\"kind\":\"rect_prism_primitive\","
              "\"width\":1.0,\"height\":1.0,\"depth\":2.0}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[{\"position\":{\"x\":0.0,\"y\":2.0,\"z\":8.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"physics_sim\":{"
            "\"object_overlays\":[{"
              "\"object_id\":\"emitter_guide\","
              "\"motion_mode\":\"Static\","
              "\"initial_velocity\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"emitter\":{"
                "\"active\":true,"
                "\"type\":\"Jet\","
                "\"radius\":1.0,"
                "\"strength\":1.0,"
                "\"direction\":{\"x\":0.0,\"y\":1.0,\"z\":0.0}"
              "}"
            "}]"
          "},"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"object_materials\":[{"
                "\"object_id\":\"emitter_guide\","
                "\"material_id\":0,"
                "\"object_color\":16777215,"
                "\"alpha\":1.0,"
                "\"emissive_strength\":1.0"
              "}]"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    bool ok = runtime_scene_bridge_apply_json(runtime_json_emitter_helper, &summary);
    assert_true("runtime_scene_apply_emitter_helper_tint_lock_ok", ok);
    if (!ok) return 0;
    assert_true("runtime_scene_apply_emitter_helper_tint_lock_stays_guide_only",
                SceneObjectIsGuideOnly(&sceneSettings.sceneObjects[1]));
    assert_true("runtime_scene_apply_emitter_helper_tint_lock_keeps_jet_green",
                sceneSettings.sceneObjects[1].color == SceneObjectPackRGBBytes(74, 232, 124));
    return 0;
}

static int test_runtime_scene_bridge_apply_ps4d_fixture_retains_digest_truth(void) {
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneBridge3DScaffoldState scaffold;
    RuntimeSceneBridge3DDigestState digest;
    bool ok = runtime_scene_bridge_apply_file("../physics_sim/config/samples/ps4d_runtime_scene_visual_test.json",
                                              &summary);
    assert_true("runtime_scene_ps4d_apply_ok", ok);
    if (!ok) return 0;

    assert_true("runtime_scene_ps4d_summary_valid", summary.valid_contract);
    assert_true("runtime_scene_ps4d_object_count", sceneSettings.objectCount == 3);
    assert_true("runtime_scene_ps4d_space_mode_3d", animSettings.spaceMode == SPACE_MODE_3D);
    assert_true("runtime_scene_ps4d_light_path_empty_without_light_seed",
                sceneSettings.bezierPath.numPoints == 0);

    runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
    assert_true("runtime_scene_ps4d_scaffold_valid", scaffold.valid);
    assert_true("runtime_scene_ps4d_scaffold_plane_count", scaffold.plane_count == 1);
    assert_true("runtime_scene_ps4d_scaffold_box_count", scaffold.box_count == 2);

    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    assert_true("runtime_scene_ps4d_digest_valid", digest.valid);
    assert_true("runtime_scene_ps4d_digest_has_bounds", digest.has_scene_bounds);
    assert_true("runtime_scene_ps4d_digest_bounds_enabled", digest.bounds_enabled);
    assert_true("runtime_scene_ps4d_digest_bounds_clamp", digest.bounds_clamp_on_edit);
    assert_close("runtime_scene_ps4d_digest_bounds_min_x", digest.bounds_min_x, -6.0, 1e-9);
    assert_close("runtime_scene_ps4d_digest_bounds_min_y", digest.bounds_min_y, -5.0, 1e-9);
    assert_close("runtime_scene_ps4d_digest_bounds_min_z", digest.bounds_min_z, -2.5, 1e-9);
    assert_close("runtime_scene_ps4d_digest_bounds_max_x", digest.bounds_max_x, 6.0, 1e-9);
    assert_close("runtime_scene_ps4d_digest_bounds_max_y", digest.bounds_max_y, 5.0, 1e-9);
    assert_close("runtime_scene_ps4d_digest_bounds_max_z", digest.bounds_max_z, 4.0, 1e-9);
    assert_true("runtime_scene_ps4d_digest_has_construction_plane", digest.has_construction_plane);
    assert_true("runtime_scene_ps4d_digest_construction_mode",
                strcmp(digest.construction_plane_mode, "axis_aligned") == 0);
    assert_true("runtime_scene_ps4d_digest_construction_axis",
                strcmp(digest.construction_plane_axis, "xy") == 0);
    assert_close("runtime_scene_ps4d_digest_construction_offset",
                 digest.construction_plane_offset,
                 -1.0,
                 1e-9);
    assert_true("runtime_scene_ps4d_digest_primitive_count", digest.primitive_count == 3);
    assert_true("runtime_scene_ps4d_digest_plane_count", digest.plane_primitive_count == 1);
    assert_true("runtime_scene_ps4d_digest_prism_count", digest.rect_prism_primitive_count == 2);
    assert_true("runtime_scene_ps4d_digest_plane_kind_count",
                digest_count_kind(&digest, RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE) == 1);
    assert_true("runtime_scene_ps4d_digest_prism_kind_count",
                digest_count_kind(&digest, RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM) == 2);

    return 0;
}

static int test_runtime_scene_bridge_apply_ps4d_fixture_retains_primitive_seed_truth(void) {
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneBridge3DPrimitiveSeedState seeds;
    const RuntimeSceneBridgePrimitiveSeed *plane_seed = NULL;
    const RuntimeSceneBridgePrimitiveSeed *center_seed = NULL;
    const RuntimeSceneBridgePrimitiveSeed *offset_seed = NULL;
    bool ok = runtime_scene_bridge_apply_file("../physics_sim/config/samples/ps4d_runtime_scene_visual_test.json",
                                              &summary);
    assert_true("runtime_scene_ps4d_seed_apply_ok", ok);
    if (!ok) return 0;

    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    assert_true("runtime_scene_ps4d_seed_valid", seeds.valid);
    assert_true("runtime_scene_ps4d_seed_retained_count", seeds.primitive_count == 3);
    assert_true("runtime_scene_ps4d_seed_plane_count", seeds.plane_primitive_count == 1);
    assert_true("runtime_scene_ps4d_seed_rect_prism_count", seeds.rect_prism_primitive_count == 2);
    assert_true("runtime_scene_ps4d_seed_excluded_count", seeds.excluded_primitive_count == 0);

    plane_seed = find_primitive_seed_by_object_id(&seeds, "plane_floor");
    center_seed = find_primitive_seed_by_object_id(&seeds, "prism_center");
    offset_seed = find_primitive_seed_by_object_id(&seeds, "prism_offset");

    assert_true("runtime_scene_ps4d_seed_plane_present", plane_seed != NULL);
    assert_true("runtime_scene_ps4d_seed_center_present", center_seed != NULL);
    assert_true("runtime_scene_ps4d_seed_offset_present", offset_seed != NULL);
    if (!plane_seed || !center_seed || !offset_seed) return 0;

    assert_true("runtime_scene_ps4d_seed_plane_kind",
                plane_seed->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_PLANE);
    assert_true("runtime_scene_ps4d_seed_center_kind",
                center_seed->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM);
    assert_true("runtime_scene_ps4d_seed_offset_kind",
                offset_seed->kind == RUNTIME_SCENE_BRIDGE_PRIMITIVE_RECT_PRISM);
    assert_true("runtime_scene_ps4d_seed_plane_dimensions", plane_seed->has_dimensions);
    assert_true("runtime_scene_ps4d_seed_center_dimensions", center_seed->has_dimensions);
    assert_true("runtime_scene_ps4d_seed_offset_dimensions", offset_seed->has_dimensions);

    assert_close("runtime_scene_ps4d_seed_plane_origin_z", plane_seed->origin_z, -1.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_plane_width", plane_seed->width, 8.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_plane_height", plane_seed->height, 6.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_plane_axis_u_x", plane_seed->axis_u_x, 1.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_plane_axis_v_y", plane_seed->axis_v_y, 1.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_plane_normal_z", plane_seed->normal_z, 1.0, 1e-9);

    assert_close("runtime_scene_ps4d_seed_center_origin_z", center_seed->origin_z, 0.25, 1e-9);
    assert_close("runtime_scene_ps4d_seed_center_width", center_seed->width, 2.0, 1e-9);
    assert_close("runtime_scene_ps4d_seed_center_height", center_seed->height, 1.5, 1e-9);
    assert_close("runtime_scene_ps4d_seed_center_depth", center_seed->depth, 2.5, 1e-9);

    assert_close("runtime_scene_ps4d_seed_offset_origin_x", offset_seed->origin_x, 2.5, 1e-9);
    assert_close("runtime_scene_ps4d_seed_offset_origin_y", offset_seed->origin_y, 1.5, 1e-9);
    assert_close("runtime_scene_ps4d_seed_offset_width", offset_seed->width, 1.25, 1e-9);
    assert_close("runtime_scene_ps4d_seed_offset_height", offset_seed->height, 1.25, 1e-9);
    assert_close("runtime_scene_ps4d_seed_offset_depth", offset_seed->depth, 1.5, 1e-9);
    return 0;
}

static int test_scene_compile_and_preflight_roundtrip(void) {
    const char *authoring_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_compile_rt\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{\"object_id\":\"obj_a\"}],"
        "\"hierarchy\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{\"ray_tracing\":{\"seed\":1}}"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    RuntimeSceneBridgePreflight preflight;
    CoreResult r = core_scene_compile_authoring_to_runtime(authoring_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    assert_true("scene_compile_roundtrip_compile_ok", r.code == CORE_OK);
    if (r.code == CORE_OK && runtime_json) {
        bool ok = runtime_scene_bridge_preflight_json(runtime_json, &preflight);
        assert_true("scene_compile_roundtrip_preflight_ok", ok);
        if (ok) {
            assert_true("scene_compile_roundtrip_scene_id",
                        strcmp(preflight.scene_id, "scene_compile_rt") == 0);
            assert_true("scene_compile_roundtrip_objects", preflight.object_count == 1);
        }
    }
    free(runtime_json);
    return 0;
}

static int test_runtime_scene_bridge_apply_runtime_fixture(void) {
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneBridgePreflight alias_summary;
    bool ok = runtime_scene_bridge_apply_file("third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json",
                                              &summary);
    assert_true("runtime_scene_apply_fixture_ok", ok);
    if (!ok) return 0;

    assert_true("runtime_scene_apply_valid_contract", summary.valid_contract);
    assert_true("runtime_scene_apply_object_count", sceneSettings.objectCount == 1);
    assert_close("runtime_scene_apply_object_x", sceneSettings.sceneObjects[0].x, 0.0, 1e-9);
    assert_close("runtime_scene_apply_object_y", sceneSettings.sceneObjects[0].y, 0.0, 1e-9);
    assert_close("runtime_scene_apply_object_z", sceneSettings.sceneObjects[0].z, 0.0, 1e-9);
    assert_true("runtime_scene_apply_color_from_material",
                sceneSettings.sceneObjects[0].color == 0xFFFFFF);
    assert_true("runtime_scene_apply_light_point_count_seeded",
                sceneSettings.bezierPath.numPoints == 1);
    assert_close("runtime_scene_apply_light_x", sceneSettings.bezierPath.points[0].x, 5.0, 1e-9);
    assert_close("runtime_scene_apply_light_y", sceneSettings.bezierPath.points[0].y, 8.0, 1e-9);
    assert_close("runtime_scene_apply_camera_x", sceneSettings.camera.x, 0.0, 1e-9);
    assert_close("runtime_scene_apply_camera_y", sceneSettings.camera.y, 0.0, 1e-9);
    assert_true("runtime_scene_apply_space_mode_2d", animSettings.spaceMode == SPACE_MODE_2D);
    assert_true("runtime_scene_apply_source_runtime_scene",
                animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
    assert_true("runtime_scene_apply_source_runtime_scene_path",
                strcmp(animSettings.runtimeScenePath,
                       "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json") == 0);

    // Regression: menu apply path passes animSettings.runtimeScenePath as input.
    ok = runtime_scene_bridge_apply_file(animSettings.runtimeScenePath, &alias_summary);
    assert_true("runtime_scene_apply_alias_buffer_ok", ok);
    if (ok) {
        assert_true("runtime_scene_apply_alias_buffer_valid_contract", alias_summary.valid_contract);
        assert_true("runtime_scene_apply_alias_buffer_path_retained",
                    strcmp(animSettings.runtimeScenePath,
                           "third_party/codework_shared/assets/scenes/trio_contract/scene_runtime_min.json") == 0);
    }
    return 0;
}

static int test_runtime_scene_bridge_authoring_overlay_object_color_preserved(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_overlay_object_color\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"obj_color\","
          "\"object_type\":\"rect_prism_primitive\","
          "\"transform\":{"
            "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "},"
          "\"primitive\":{"
            "\"kind\":\"rect_prism_primitive\","
            "\"width\":1.0,"
            "\"height\":1.0,"
            "\"depth\":1.0"
          "}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"object_materials\":[{"
              "\"object_id\":\"obj_color\","
              "\"material_id\":3,"
              "\"object_color\":255,"
              "\"alpha\":0.4,"
              "\"reflectivity\":0.72,"
              "\"roughness\":0.18,"
              "\"emissive_strength\":0.7"
            "}]"
          "}"
        "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_apply_authoring_overlay_object_color_ok", ok);
    if (ok) {
        assert_true("runtime_scene_apply_authoring_overlay_material_id",
                    sceneSettings.sceneObjects[0].material_id == 3);
        assert_true("runtime_scene_apply_authoring_overlay_object_color",
                    sceneSettings.sceneObjects[0].color == 0x0000FF);
        assert_close("runtime_scene_apply_authoring_overlay_alpha",
                     sceneSettings.sceneObjects[0].alpha,
                     0.4,
                     1e-9);
        assert_close("runtime_scene_apply_authoring_overlay_reflectivity",
                     sceneSettings.sceneObjects[0].reflectivity,
                     0.72,
                     1e-9);
        assert_close("runtime_scene_apply_authoring_overlay_roughness",
                     sceneSettings.sceneObjects[0].roughness,
                     0.18,
                     1e-9);
        assert_close("runtime_scene_apply_authoring_overlay_emissive_strength",
                     sceneSettings.sceneObjects[0].emissiveStrength,
                     0.7,
                     1e-9);
    }
    return 0;
}

static int test_runtime_scene_bridge_authoring_overlay_light_settings_preserved(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_overlay_light_settings\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[{\"position\":{\"x\":1.0,\"y\":2.0,\"z\":3.0}}],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"light_settings\":{"
                "\"intensity\":7.5,"
                "\"radius\":1.25"
              "}"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_apply_authoring_overlay_light_settings_ok", ok);
    if (!ok) return 0;
    assert_close("runtime_scene_apply_authoring_overlay_light_intensity",
                 animSettings.lightIntensity,
                 7.5,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_light_radius",
                 animSettings.lightRadius,
                 1.25,
                 1e-9);
    return 0;
}

static int test_runtime_scene_bridge_authoring_overlay_environment_settings_preserved(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_overlay_environment_settings\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"environment\":{"
                "\"light_mode\":2,"
                "\"ambient_strength\":0.375,"
                "\"top_fill_strength\":3.5"
              "}"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_apply_authoring_overlay_environment_settings_ok", ok);
    if (!ok) return 0;
    assert_true("runtime_scene_apply_authoring_overlay_environment_mode",
                animSettings.environmentLightMode == ENVIRONMENT_LIGHT_MODE_AMBIENT);
    assert_close("runtime_scene_apply_authoring_overlay_environment_ambient_strength",
                 animSettings.environmentBrightness,
                 0.375 * 255.0,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_environment_top_fill_strength",
                 animSettings.topFillStrength,
                 3.5,
                 1e-9);
    return 0;
}

static int test_runtime_scene_bridge_authoring_overlay_procedural_texture_shorthand_preserved(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_overlay_procedural_texture\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"obj_tex\","
          "\"object_type\":\"rect_prism_primitive\","
          "\"transform\":{"
            "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "},"
          "\"primitive\":{"
            "\"kind\":\"rect_prism_primitive\","
            "\"width\":1.0,"
            "\"height\":1.0,"
            "\"depth\":1.0"
          "}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"object_materials\":[{"
                "\"object_id\":\"obj_tex\","
                "\"material_id\":0,"
                "\"texture_id\":1,"
                "\"texture_strength\":0.35,"
                "\"texture_scale\":2.5,"
                "\"texture_offset_u\":0.12,"
                "\"texture_offset_v\":0.18,"
                "\"texture_pattern_mode\":2,"
                "\"texture_coverage\":0.62,"
                "\"texture_grain\":0.41,"
                "\"texture_edge_softness\":0.27,"
                "\"texture_contrast\":0.73,"
                "\"texture_flow\":0.35,"
                "\"texture_color_depth\":0.58,"
                "\"texture_surface_damage\":0.44,"
                "\"texture_seed\":77"
              "}]"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_apply_authoring_overlay_procedural_texture_ok", ok);
    if (!ok) return 0;
    assert_true("runtime_scene_apply_authoring_overlay_texture_id",
                sceneSettings.sceneObjects[0].textureId == 1);
    assert_close("runtime_scene_apply_authoring_overlay_texture_strength",
                 sceneSettings.sceneObjects[0].textureStrength,
                 0.35,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_texture_scale",
                 sceneSettings.sceneObjects[0].textureScale,
                 2.5,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_texture_offset_u",
                 sceneSettings.sceneObjects[0].textureOffsetU,
                 0.12,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_texture_offset_v",
                 sceneSettings.sceneObjects[0].textureOffsetV,
                 0.18,
                 1e-9);
    assert_true("runtime_scene_apply_authoring_overlay_texture_pattern_mode",
                sceneSettings.sceneObjects[0].texturePatternMode == 2);
    assert_close("runtime_scene_apply_authoring_overlay_texture_coverage",
                 sceneSettings.sceneObjects[0].textureCoverage,
                 0.62,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_texture_grain",
                 sceneSettings.sceneObjects[0].textureGrain,
                 0.41,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_texture_edge_softness",
                 sceneSettings.sceneObjects[0].textureEdgeSoftness,
                 0.27,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_texture_contrast",
                 sceneSettings.sceneObjects[0].textureContrast,
                 0.73,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_texture_flow",
                 sceneSettings.sceneObjects[0].textureFlow,
                 0.35,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_texture_color_depth",
                 sceneSettings.sceneObjects[0].textureColorDepth,
                 0.58,
                 1e-9);
    assert_close("runtime_scene_apply_authoring_overlay_texture_surface_damage",
                 sceneSettings.sceneObjects[0].textureSurfaceDamage,
                 0.44,
                 1e-9);
    assert_true("runtime_scene_apply_authoring_overlay_texture_seed",
                sceneSettings.sceneObjects[0].textureSeed == 77);
    return 0;
}

static int test_runtime_scene_bridge_authoring_overlay_default_emissive_strength_zero(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_authoring_default_emissive_zero\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"obj_overlay\","
          "\"object_type\":\"box\","
          "\"transform\":{"
            "\"position\":{\"x\":1.0,\"y\":2.0,\"z\":3.0},"
            "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
          "}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"ray_tracing\":{"
            "\"authoring\":{"
              "\"object_materials\":[{"
                "\"object_id\":\"obj_overlay\","
                "\"material_id\":5,"
                "\"alpha\":0.4,"
                "\"reflectivity\":0.72,"
                "\"roughness\":0.18"
              "}]"
            "}"
          "}"
        "}"
        "}";
    RuntimeSceneBridgePreflight summary = {0};
    bool ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
    assert_true("runtime_scene_apply_authoring_default_emissive_zero_ok", ok);
    if (ok) {
        assert_true("runtime_scene_apply_authoring_default_emissive_zero_material",
                    sceneSettings.sceneObjects[0].material_id == 5);
        assert_close("runtime_scene_apply_authoring_default_emissive_zero_strength",
                     sceneSettings.sceneObjects[0].emissiveStrength,
                     0.0,
                     1e-9);
    }
    return 0;
}

static int test_runtime_scene_bridge_apply_compile_output(void) {
    const char *authoring_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_apply_compile\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
          "\"object_id\":\"obj_apply\","
          "\"object_type\":\"circle\","
          "\"transform\":{"
             "\"position\":{\"x\":4.0,\"y\":6.0,\"z\":2.5},"
             "\"rotation\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
             "\"scale\":{\"x\":1.2,\"y\":1.2,\"z\":1.2}"
          "},"
          "\"material_ref\":{\"id\":\"mat_apply\"}"
        "}],"
        "\"hierarchy\":[],"
        "\"materials\":[{\"material_id\":\"mat_apply\",\"albedo\":[0.25,0.5,1.0]}],"
        "\"lights\":[{\"position\":{\"x\":2.0,\"y\":3.0,\"z\":1.0}}],"
        "\"cameras\":[{\"position\":{\"x\":7.0,\"y\":8.0,\"z\":9.0}}],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    RuntimeSceneBridgePreflight summary;
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(authoring_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    assert_true("runtime_scene_apply_compile_compile_ok", r.code == CORE_OK);
    if (r.code != CORE_OK || !runtime_json) {
        free(runtime_json);
        return 0;
    }

    {
        bool ok = runtime_scene_bridge_apply_json(runtime_json, &summary);
        assert_true("runtime_scene_apply_compile_apply_ok", ok);
        if (ok) {
            assert_true("runtime_scene_apply_compile_count", sceneSettings.objectCount == 1);
            assert_close("runtime_scene_apply_compile_x", sceneSettings.sceneObjects[0].x, 4.0, 1e-9);
            assert_close("runtime_scene_apply_compile_y", sceneSettings.sceneObjects[0].y, 6.0, 1e-9);
            assert_close("runtime_scene_apply_compile_z", sceneSettings.sceneObjects[0].z, 2.5, 1e-9);
            assert_true("runtime_scene_apply_compile_space3d", animSettings.spaceMode == SPACE_MODE_3D);
            assert_true("runtime_scene_apply_compile_source_runtime_scene",
                        animSettings.sceneSource == SCENE_SOURCE_RUNTIME_SCENE);
            assert_true("runtime_scene_apply_compile_runtime_path_empty",
                        animSettings.runtimeScenePath[0] == '\0');
        }
    }

    free(runtime_json);
    return 0;
}


int run_test_runtime_scene_bridge_core_tests(void) {
    int before = test_support_failures();

    test_runtime_scene_bridge_preflight_accepts_runtime_contract();
    test_runtime_scene_bridge_rejects_authoring_variant();
    test_runtime_scene_bridge_rejects_malformed_runtime_payload();
    test_runtime_scene_bridge_optional_lanes_default_deterministic();
    test_runtime_scene_bridge_rejects_noncanonical_unit_system();
    test_runtime_scene_bridge_apply_uses_world_scale_mapping();
    test_runtime_scene_bridge_apply_preserves_editor_mode_state();
    test_runtime_scene_bridge_apply_3d_primitives_scaffold();
    test_runtime_scene_bridge_apply_authoring_helpers_do_not_block_native_3d();
    test_runtime_scene_bridge_apply_authoring_tag_does_not_mark_real_geometry_guide_only();
    test_runtime_scene_bridge_apply_physics_emitter_overlay_marks_guide_only();
    test_runtime_scene_bridge_authoring_overlay_does_not_override_helper_tint();
    test_runtime_scene_bridge_apply_ps4d_fixture_retains_digest_truth();
    test_runtime_scene_bridge_apply_ps4d_fixture_retains_primitive_seed_truth();
    test_scene_compile_and_preflight_roundtrip();
    test_runtime_scene_bridge_apply_runtime_fixture();
    test_runtime_scene_bridge_authoring_overlay_object_color_preserved();
    test_runtime_scene_bridge_authoring_overlay_light_settings_preserved();
    test_runtime_scene_bridge_authoring_overlay_environment_settings_preserved();
    test_runtime_scene_bridge_authoring_overlay_procedural_texture_shorthand_preserved();
    test_runtime_scene_bridge_authoring_overlay_default_emissive_strength_zero();
    test_runtime_scene_bridge_apply_compile_output();

    return test_support_failures() - before;
}
