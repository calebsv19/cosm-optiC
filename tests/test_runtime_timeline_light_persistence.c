#include "test_runtime_timeline_light_persistence.h"

#include "config/config_manager.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "import/runtime_scene_bridge.h"
#include "editor/scene_editor_light_timeline.h"
#include "test_support.h"

#include <json-c/json.h>
#include <string.h>

static const char* light_timeline_json(bool embedded_spatial) {
    return embedded_spatial
        ? "{\"light_timeline\":{\"version\":1,"
          "\"rate\":{\"numerator\":20,\"denominator\":1},"
          "\"range\":{\"start_frame\":0,\"frame_count\":21},"
          "\"target_id\":\"light/key\","
          "\"spatial_path\":{"
          "\"path\":{\"mode\":\"BEZIER_CUBIC\",\"points\":["
          "{\"x\":0,\"y\":0,\"velocity1\":{\"vx\":1,\"vy\":0}},"
          "{\"x\":3,\"y\":0,\"velocity2\":{\"vx\":-1,\"vy\":0}}]},"
          "\"depth\":{\"points\":["
          "{\"z\":0,\"velocity1\":{\"vz\":1.3333333333333333}},"
          "{\"z\":4,\"velocity2\":{\"vz\":-1.3333333333333333}}]}},"
          "\"progress_track\":{\"id\":\"key_progress\",\"enabled\":true,\"keys\":["
          "{\"frame\":0,\"value\":0,\"interpolation\":\"linear\","
          "\"incoming_handle\":{\"frame_offset\":0,\"value_offset\":0},"
          "\"outgoing_handle\":{\"frame_offset\":0,\"value_offset\":0}},"
          "{\"frame\":20,\"value\":1,\"interpolation\":\"step\","
          "\"incoming_handle\":{\"frame_offset\":0,\"value_offset\":0},"
          "\"outgoing_handle\":{\"frame_offset\":0,\"value_offset\":0}}]}}}"
        : "{\"light_path\":{\"mode\":\"BEZIER_CUBIC\",\"points\":["
          "{\"x\":0,\"y\":0,\"velocity1\":{\"vx\":1,\"vy\":0}},"
          "{\"x\":3,\"y\":0,\"velocity2\":{\"vx\":-1,\"vy\":0}}]},"
          "\"light_path_depth\":{\"points\":[{\"z\":0},{\"z\":4}]},"
          "\"light_timeline\":{\"version\":1,"
          "\"rate\":{\"numerator\":20,\"denominator\":1},"
          "\"range\":{\"start_frame\":0,\"frame_count\":21},"
          "\"target_id\":\"light/key\","
          "\"progress_track\":{\"id\":\"key_progress\",\"keys\":["
          "{\"frame\":0,\"value\":0,\"interpolation\":\"linear\"},"
          "{\"frame\":20,\"value\":1,\"interpolation\":\"step\"}]}}}";
}

static int test_light_timeline_roundtrip_and_evaluation(void) {
    const int old_space_mode = animSettings.spaceMode;
    RuntimeSceneLightTimelineDocument document;
    RuntimeSceneLightTimelineDocument reopened;
    TimelineLightMotionSample first;
    TimelineLightMotionSample second;
    char diagnostics[64];
    json_object* authoring = json_tokener_parse(light_timeline_json(true));
    json_object* encoded = NULL;
    json_object* wrapper = NULL;
    memset(&document, 0, sizeof(document));
    memset(&reopened, 0, sizeof(reopened));
    animSettings.spaceMode = SPACE_MODE_3D;
    assert_true("light_persistence_parse",
                RuntimeSceneLightTimelineParseAuthoring(authoring, 1.0, &document,
                                                        diagnostics, sizeof(diagnostics)) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_persistence_parse_diag", strcmp(diagnostics, "ok") == 0);
    assert_true("light_persistence_evaluate",
                RuntimeSceneLightTimelineEvaluate(&document,
                                                  (TimelineSample){5, 0u, 1u},
                                                  &first) == TIMELINE_STATUS_OK);
    assert_close("light_persistence_progress", first.progress, 0.25, 1e-12);
    assert_close("light_persistence_x", first.position.x, 0.75, 1e-5);
    assert_close("light_persistence_z", first.position.z, 1.0, 1e-5);
    encoded = RuntimeSceneLightTimelineToJsonObject(&document, 1.0);
    assert_true("light_persistence_encode", encoded != NULL);
    wrapper = json_object_new_object();
    json_object_object_add(wrapper, "light_timeline", encoded);
    assert_true("light_persistence_reopen",
                RuntimeSceneLightTimelineParseAuthoring(wrapper, 1.0, &reopened,
                                                        diagnostics, sizeof(diagnostics)) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_persistence_reopen_evaluate",
                RuntimeSceneLightTimelineEvaluate(&reopened,
                                                  (TimelineSample){5, 0u, 1u},
                                                  &second) == TIMELINE_STATUS_OK);
    assert_close("light_persistence_roundtrip_progress", second.progress,
                 first.progress, 1e-12);
    assert_close("light_persistence_roundtrip_position", second.position.z,
                 first.position.z, 1e-9);
    assert_close("light_persistence_roundtrip_speed", second.world_speed_per_second,
                 first.world_speed_per_second, 1e-9);
    json_object_put(wrapper);
    json_object_put(authoring);
    animSettings.spaceMode = old_space_mode;
    return 0;
}

static int test_light_timeline_legacy_path_and_transactional_refusal(void) {
    RuntimeSceneLightTimelineDocument document;
    RuntimeSceneLightTimelineDocument sentinel;
    RuntimeSceneLightTimelineDocument original;
    char diagnostics[64];
    json_object* authoring = json_tokener_parse(light_timeline_json(false));
    json_object* bad = json_tokener_parse(
        "{\"light_timeline\":{\"version\":1,\"rate\":{\"numerator\":20,"
        "\"denominator\":1},\"range\":{\"start_frame\":0,\"frame_count\":0}}}");
    assert_true("light_persistence_legacy_parse",
                RuntimeSceneLightTimelineParseAuthoring(authoring, 2.0, &document,
                                                        diagnostics, sizeof(diagnostics)) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_persistence_legacy_flag", document.migrated_legacy_spatial_path);
    assert_close("light_persistence_world_scale", document.spatial_path.points[1].x,
                 6.0, 1e-12);
    memset(&sentinel, 0x5a, sizeof(sentinel));
    original = sentinel;
    assert_true("light_persistence_bad_refused",
                RuntimeSceneLightTimelineParseAuthoring(bad, 1.0, &sentinel,
                                                        diagnostics, sizeof(diagnostics)) ==
                    TIMELINE_STATUS_INVALID_RANGE);
    assert_true("light_persistence_bad_nonmutation",
                memcmp(&sentinel, &original, sizeof(sentinel)) == 0);
    json_object_put(bad);
    json_object_put(authoring);
    return 0;
}

static json_object* runtime_scene_with_light_timeline(const char* target_id) {
    json_object* root = json_object_new_object();
    json_object* lights = json_object_new_array();
    json_object* light = json_object_new_object();
    json_object* position = json_object_new_object();
    json_object* extensions = json_object_new_object();
    json_object* ray = json_object_new_object();
    json_object* source_authoring = json_tokener_parse(light_timeline_json(true));
    json_object* source_timeline = NULL;
    json_object* authoring = json_object_new_object();
    json_object_object_get_ex(source_authoring, "light_timeline", &source_timeline);
    json_object_object_add(root, "schema_family", json_object_new_string("codework_scene"));
    json_object_object_add(root, "schema_variant", json_object_new_string("scene_runtime_v1"));
    json_object_object_add(root, "schema_version", json_object_new_int(1));
    json_object_object_add(root, "scene_id", json_object_new_string("timeline_light_scene"));
    json_object_object_add(root, "unit_system", json_object_new_string("meters"));
    json_object_object_add(root, "world_scale", json_object_new_double(1.0));
    json_object_object_add(root, "space_mode_default", json_object_new_string("3d"));
    json_object_object_add(root, "objects", json_object_new_array());
    json_object_object_add(root, "materials", json_object_new_array());
    json_object_object_add(root, "cameras", json_object_new_array());
    json_object_object_add(light, "id", json_object_new_string("key"));
    json_object_object_add(position, "x", json_object_new_double(0.0));
    json_object_object_add(position, "y", json_object_new_double(0.0));
    json_object_object_add(position, "z", json_object_new_double(0.0));
    json_object_object_add(light, "position", position);
    json_object_array_add(lights, light);
    json_object_object_add(root, "lights", lights);
    json_object_get(source_timeline);
    json_object_object_add(authoring, "light_timeline", source_timeline);
    json_object* target = NULL;
    json_object_object_get_ex(source_timeline, "target_id", &target);
    json_object_set_string(target, target_id);
    json_object_object_add(ray, "authoring", authoring);
    json_object_object_add(extensions, "ray_tracing", ray);
    json_object_object_add(root, "extensions", extensions);
    json_object_put(source_authoring);
    return root;
}

static int test_light_timeline_runtime_bridge_headless_inspection(void) {
    RuntimeSceneBridgePreflight summary;
    TimelineLightMotionSample sample;
    json_object* valid = runtime_scene_with_light_timeline("light/key");
    json_object* stale = runtime_scene_with_light_timeline("light/missing");
    assert_true("light_persistence_bridge_apply",
                runtime_scene_bridge_apply_json(
                    json_object_to_json_string_ext(valid, JSON_C_TO_STRING_PLAIN),
                    &summary));
    assert_true("light_persistence_bridge_inspect",
                RuntimeSceneLightTimelineInspectLast((TimelineSample){5, 0u, 1u},
                                                     &sample) == TIMELINE_STATUS_OK);
    assert_true("light_persistence_bridge_target",
                strcmp(sample.target_id, "light/key") == 0);
    assert_close("light_persistence_bridge_progress", sample.progress, 0.25, 1e-12);
    {
        SceneEditorPaneHost pane_host;
        const SceneEditorPaneLayout* layout;
        SDL_Event event;
        memset(&pane_host, 0, sizeof(pane_host));
        memset(&event, 0, sizeof(event));
        SceneEditorLightTimelineReset();
        assert_true("light_persistence_ui_select",
                    SceneEditorLightTimelineSelectTargetId("light/key") ==
                        TIMELINE_STATUS_OK);
        assert_true("light_persistence_ui_pane_init",
                    scene_editor_pane_host_init(&pane_host, 1280, 760));
        assert_true("light_persistence_ui_open",
                    SceneEditorLightTimelineToggle(&pane_host));
        layout = scene_editor_pane_host_layout(&pane_host);
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.x = layout->timeline_rect.x + layout->timeline_rect.w / 2;
        event.button.y = layout->timeline_rect.y + layout->timeline_rect.h / 2;
        assert_true("light_persistence_ui_scrub",
                    SceneEditorLightTimelineHandleEvent(&event, &pane_host,
                                                        layout, NULL));
        assert_true("light_persistence_ui_scrub_inspect",
                    RuntimeSceneLightTimelineInspectLast((TimelineSample){10, 0u, 1u},
                                                         &sample) ==
                        TIMELINE_STATUS_OK);
        assert_close("light_persistence_ui_scrub_progress", sample.progress, 0.5, 1e-12);
        assert_true("light_persistence_ui_insert_key",
                    SceneEditorLightTimelineInsertKey(8, 0.3) == TIMELINE_STATUS_OK);
        RuntimeSceneLightTimelineDocument edited;
        assert_true("light_persistence_ui_insert_readback",
                    RuntimeSceneLightTimelineGetLast(&edited) &&
                    edited.timeline.tracks[edited.progress_track_index].key_count == 3u);
        assert_true("light_persistence_ui_undo", SceneEditorLightTimelineUndo());
        assert_true("light_persistence_ui_undo_readback",
                    RuntimeSceneLightTimelineGetLast(&edited) &&
                    edited.timeline.tracks[edited.progress_track_index].key_count == 2u);
        assert_true("light_persistence_ui_redo", SceneEditorLightTimelineRedo());
        assert_true("light_persistence_ui_delete_selected",
                    SceneEditorLightTimelineDeleteSelectedKey() == TIMELINE_STATUS_OK);
        assert_true("light_persistence_ui_delete_readback",
                    RuntimeSceneLightTimelineGetLast(&edited) &&
                    edited.timeline.tracks[edited.progress_track_index].key_count == 2u);
        assert_true("light_persistence_ui_delete_undo", SceneEditorLightTimelineUndo());
    }
    assert_true("light_persistence_bridge_apply_stale_scene",
                runtime_scene_bridge_apply_json(
                    json_object_to_json_string_ext(stale, JSON_C_TO_STRING_PLAIN),
                    &summary));
    assert_true("light_persistence_bridge_stale_refused",
                RuntimeSceneLightTimelineInspectLast((TimelineSample){5, 0u, 1u},
                                                     &sample) ==
                    TIMELINE_STATUS_TARGET_NOT_FOUND);
    json_object_put(stale);
    json_object_put(valid);
    return 0;
}

int run_test_runtime_timeline_light_persistence_tests(void) {
    test_light_timeline_roundtrip_and_evaluation();
    test_light_timeline_legacy_path_and_transactional_refusal();
    test_light_timeline_runtime_bridge_headless_inspection();
    return test_support_failures();
}
