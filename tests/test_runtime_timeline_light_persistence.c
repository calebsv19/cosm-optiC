#include "test_runtime_timeline_light_persistence.h"

#include "config/config_manager.h"
#include "app/evaluated_scene_service.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "import/runtime_scene_bridge.h"
#include "editor/scene_editor_light_timeline.h"
#include "editor/scene_editor_light_timeline_edit.h"
#include "editor/scene_editor_light_timeline_view.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "test_support.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static bool light_timeline_rect_inside(
    const SDL_Rect* outer,
    const SDL_Rect* inner) {
    return outer && inner && inner->w > 0 && inner->h > 0 &&
        inner->x >= outer->x && inner->y >= outer->y &&
        inner->x + inner->w <= outer->x + outer->w &&
        inner->y + inner->h <= outer->y + outer->h;
}

static bool light_timeline_geometry_fits(const SDL_Rect* panel) {
    SceneEditorLightTimelinePanelGeometry geometry;
    scene_editor_light_timeline_panel_geometry(panel, &geometry);
    return light_timeline_rect_inside(panel, &geometry.metrics_line) &&
        light_timeline_rect_inside(panel, &geometry.timing_graph) &&
        light_timeline_rect_inside(panel, &geometry.path_point_strip) &&
        light_timeline_rect_inside(panel, &geometry.speed_strip) &&
        light_timeline_rect_inside(panel, &geometry.footer_hint) &&
        light_timeline_rect_inside(panel, &geometry.play_button) &&
        light_timeline_rect_inside(panel, &geometry.add_key_button) &&
        light_timeline_rect_inside(
            panel, &geometry.constant_speed_button) &&
        light_timeline_rect_inside(
            panel, &geometry.equal_segments_button) &&
        light_timeline_rect_inside(
            panel, &geometry.custom_mode_indicator) &&
        geometry.metrics_line.y + geometry.metrics_line.h <=
            geometry.constant_speed_button.y &&
        geometry.constant_speed_button.x +
                geometry.constant_speed_button.w <=
            geometry.equal_segments_button.x &&
        geometry.equal_segments_button.x +
                geometry.equal_segments_button.w <=
            geometry.custom_mode_indicator.x &&
        geometry.constant_speed_button.y +
                geometry.constant_speed_button.h <=
            geometry.timing_graph.y &&
        geometry.timing_graph.y + geometry.timing_graph.h <=
            geometry.path_point_strip.y &&
        geometry.path_point_strip.y + geometry.path_point_strip.h <=
            geometry.speed_strip.y &&
        geometry.speed_strip.y + geometry.speed_strip.h <=
            geometry.footer_hint.y;
}

static int test_light_timeline_responsive_geometry(void) {
    const SDL_Rect compact_panel = {10, 20, 340, 220};
    const SDL_Rect standard_panel = {10, 20, 760, 300};
    SceneEditorLightTimelinePanelGeometry compact;
    SceneEditorLightTimelinePanelGeometry standard;
    int widths[] = {340, 360, 420, 620, 760, 1040};
    int heights[] = {220, 239, 260, 300, 400};
    scene_editor_light_timeline_panel_geometry(
        &compact_panel, &compact);
    scene_editor_light_timeline_panel_geometry(
        &standard_panel, &standard);
    assert_true("light_layout_compact_fits",
                light_timeline_geometry_fits(&compact_panel));
    assert_true("light_layout_standard_fits",
                light_timeline_geometry_fits(&standard_panel));
    assert_true("light_layout_compact_graph_usable",
                compact.timing_graph.h >= 44);
    assert_true("light_layout_standard_graph_dominant",
                standard.timing_graph.h >= 96);
    assert_true("light_layout_standard_speed_visible",
                standard.speed_strip.h >= 60);
    for (size_t width_index = 0u;
         width_index < sizeof(widths) / sizeof(widths[0]);
         ++width_index) {
        for (size_t height_index = 0u;
             height_index < sizeof(heights) / sizeof(heights[0]);
             ++height_index) {
            SDL_Rect supported_panel = {
                10, 20, widths[width_index], heights[height_index]
            };
            assert_true(
                "light_layout_supported_matrix_fits",
                light_timeline_geometry_fits(&supported_panel));
        }
    }
    assert_close("light_speed_axis_zero_fallback",
                 scene_editor_light_timeline_nice_ceiling(0.0),
                 1.0, 1e-12);
    assert_close("light_speed_axis_rounds_tenths",
                 scene_editor_light_timeline_nice_ceiling(0.14),
                 0.2, 1e-12);
    assert_close("light_speed_axis_rounds_units",
                 scene_editor_light_timeline_nice_ceiling(3.2),
                 5.0, 1e-12);
    assert_close("light_speed_axis_rounds_hundreds",
                 scene_editor_light_timeline_nice_ceiling(86.821),
                 100.0, 1e-12);
    return 0;
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

static int test_light_timeline_ui_acceptance_roundtrip(void) {
    const char* runtime_path = "/tmp/ray_tracing_light_timeline_ui_acceptance_runtime.json";
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_animation = animSettings;
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneLightTimelineDocument before_endpoint_drag;
    RuntimeSceneLightTimelineDocument after_endpoint_drag;
    RuntimeSceneLightTimelineDocument after_add;
    RuntimeSceneLightTimelineDocument before_key_drag;
    RuntimeSceneLightTimelineDocument after_key_drag;
    RuntimeSceneLightTimelineDocument reopened;
    TimelineLightMotionSample before_save;
    TimelineLightMotionSample after_reopen;
    RuntimeSceneBridge3DLightSeedState lights_before_play;
    RuntimeSceneBridge3DLightSeedState lights_after_play;
    RuntimeSceneLightTimelineDocument timeline_before_play;
    RuntimeSceneLightTimelineDocument timeline_after_play;
    RayEvaluatedSceneSnapshot evaluated_before_play;
    RayEvaluatedSceneSnapshot evaluated_after_play;
    RayEvaluatedSceneServiceResult direct_after_play;
    SceneConfig scene_before_play;
    AnimationConfig animation_before_play;
    SceneEditorPaneHost pane_host;
    const SceneEditorPaneLayout* layout;
    SDL_Rect timing_graph;
    SceneEditorLightTimelinePanelGeometry geometry;
    SDL_Event event;
    char diagnostics[128];
    json_object* scene = runtime_scene_with_light_timeline("light/key");
    FILE* file = fopen(runtime_path, "wb");
    const char* serialized = json_object_to_json_string_ext(
        scene, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);

    assert_true("light_acceptance_open_tmp", file != NULL);
    if (!file) {
        json_object_put(scene);
        return 0;
    }
    assert_true("light_acceptance_write_tmp",
                fwrite(serialized, 1u, strlen(serialized), file) == strlen(serialized));
    fclose(file);

    memset(&summary, 0, sizeof(summary));
    memset(&pane_host, 0, sizeof(pane_host));
    memset(&event, 0, sizeof(event));
    assert_true("light_acceptance_apply_scene",
                runtime_scene_bridge_apply_file(runtime_path, &summary));
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    animSettings.spaceMode = SPACE_MODE_3D;
    snprintf(animSettings.runtimeScenePath, sizeof(animSettings.runtimeScenePath),
             "%s", runtime_path);
    SceneEditorLightTimelineReset();
    assert_true("light_acceptance_select",
                SceneEditorLightTimelineSelectTargetId("light/key") ==
                    TIMELINE_STATUS_OK);
    assert_true("light_acceptance_pane_init",
                scene_editor_pane_host_init(&pane_host, 1280, 760));
    assert_true("light_acceptance_open_pane",
                SceneEditorLightTimelineToggle(&pane_host));
    layout = scene_editor_pane_host_layout(&pane_host);
    scene_editor_light_timeline_panel_geometry(
        &layout->timeline_rect, &geometry);
    timing_graph = geometry.timing_graph;
    assert_true("light_acceptance_layout_header_clear",
                timing_graph.y >= layout->timeline_rect.y + 54);
    assert_true("light_acceptance_layout_point_lane_after_graph",
                geometry.path_point_strip.y >=
                    timing_graph.y + timing_graph.h);
    assert_true("light_acceptance_layout_speed_after_points",
                geometry.speed_strip.y >=
                    geometry.path_point_strip.y +
                        geometry.path_point_strip.h);

    runtime_scene_bridge_get_last_3d_light_seed_state(
        &lights_before_play);
    assert_true("light_acceptance_initial_evaluated_scene",
                SceneEditorLightTimelineCopyEvaluatedScene(
                    &evaluated_before_play));
    assert_true("light_acceptance_timeline_before_play",
                RuntimeSceneLightTimelineGetLast(&timeline_before_play));
    scene_before_play = sceneSettings;
    animation_before_play = animSettings;
    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = 1;
    event.button.x =
        geometry.play_button.x + geometry.play_button.w / 2;
    event.button.y =
        geometry.play_button.y + geometry.play_button.h / 2;
    assert_true("light_acceptance_play_button",
                SceneEditorLightTimelineHandleEvent(
                    &event, &pane_host, layout, NULL));
    assert_true("light_acceptance_playing",
                SceneEditorLightTimelinePlaying());
    SDL_Delay(60u);
    assert_true("light_acceptance_playback_advanced",
                SceneEditorLightTimelineAdvancePlayback());
    assert_true("light_acceptance_advanced_evaluated_scene",
                SceneEditorLightTimelineCopyEvaluatedScene(
                    &evaluated_after_play));
    runtime_scene_bridge_get_last_3d_light_seed_state(
        &lights_after_play);
    assert_true("light_acceptance_playback_moves_evaluated_proxy",
                fabs(evaluated_before_play.light.position.x -
                     evaluated_after_play.light.position.x) > 1e-6);
    assert_true("light_acceptance_playback_exact_subframe",
                evaluated_after_play.frame.sample.subframe_denominator ==
                    1000000u);
    assert_true("light_acceptance_direct_service_after_play",
                RayEvaluatedSceneCaptureAuthoredSample(
                    evaluated_after_play.frame.sample,
                    &direct_after_play));
    assert_true("light_acceptance_play_direct_snapshot_parity",
                memcmp(&evaluated_after_play,
                       &direct_after_play.snapshot,
                       sizeof(evaluated_after_play)) == 0);
    assert_true("light_acceptance_playback_preserves_retained_lights",
                lights_before_play.valid && lights_after_play.valid &&
                memcmp(&lights_before_play, &lights_after_play,
                       sizeof(lights_before_play)) == 0);
    assert_true("light_acceptance_playback_preserves_scene",
                memcmp(&scene_before_play, &sceneSettings,
                       sizeof(scene_before_play)) == 0);
    assert_true("light_acceptance_playback_preserves_animation",
                memcmp(&animation_before_play, &animSettings,
                       sizeof(animation_before_play)) == 0);
    assert_true("light_acceptance_timeline_after_play",
                RuntimeSceneLightTimelineGetLast(&timeline_after_play));
    assert_true("light_acceptance_playback_preserves_timeline",
                memcmp(&timeline_before_play, &timeline_after_play,
                       sizeof(timeline_before_play)) == 0);
    assert_true("light_acceptance_pause_button",
                SceneEditorLightTimelineHandleEvent(
                    &event, &pane_host, layout, NULL));
    assert_true("light_acceptance_paused",
                !SceneEditorLightTimelinePlaying());

    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = 1;
    event.button.x = timing_graph.x + (timing_graph.w * 3) / 4;
    event.button.y = timing_graph.y + timing_graph.h / 2;
    assert_true("light_acceptance_scrub_down",
                SceneEditorLightTimelineHandleEvent(&event, &pane_host, layout, NULL));
    event.type = SDL_MOUSEMOTION;
    event.motion.x = timing_graph.x + timing_graph.w / 4;
    event.motion.y = timing_graph.y + timing_graph.h / 2;
    assert_true("light_acceptance_scrub_drag",
                SceneEditorLightTimelineHandleEvent(&event, &pane_host, layout, NULL));
    event.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_LEFT;
    assert_true("light_acceptance_scrub_up",
                SceneEditorLightTimelineHandleEvent(&event, &pane_host, layout, NULL));

    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = 1;
    event.button.x = timing_graph.x;
    event.button.y = timing_graph.y + timing_graph.h;
    assert_true("light_acceptance_select_first_key",
                SceneEditorLightTimelineHandleEvent(&event, &pane_host, layout, NULL));
    assert_true("light_acceptance_before_endpoint_drag",
                RuntimeSceneLightTimelineGetLast(&before_endpoint_drag));
    event.type = SDL_MOUSEMOTION;
    event.motion.x = timing_graph.x + timing_graph.w / 3;
    event.motion.y = timing_graph.y;
    assert_true("light_acceptance_endpoint_drag_consumed",
                SceneEditorLightTimelineHandleEvent(&event, &pane_host, layout, NULL));
    event.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_LEFT;
    assert_true("light_acceptance_finish_key_select",
                SceneEditorLightTimelineHandleEvent(&event, &pane_host, layout, NULL));
    assert_true("light_acceptance_after_endpoint_drag",
                RuntimeSceneLightTimelineGetLast(&after_endpoint_drag));
    assert_true("light_acceptance_endpoint_locked",
                memcmp(&before_endpoint_drag.timeline.tracks[
                           before_endpoint_drag.progress_track_index],
                       &after_endpoint_drag.timeline.tracks[
                           after_endpoint_drag.progress_track_index],
                       sizeof(TimelineTrack)) == 0);
    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = 2;
    event.button.x = timing_graph.x + timing_graph.w / 2;
    event.button.y = timing_graph.y + (timing_graph.h * 3) / 4;
    assert_true("light_acceptance_double_click_add",
                SceneEditorLightTimelineHandleEvent(&event, &pane_host, layout, NULL));
    assert_true("light_acceptance_after_add",
                RuntimeSceneLightTimelineGetLast(&after_add));
    assert_true("light_acceptance_add_count",
                after_add.timeline.tracks[after_add.progress_track_index].key_count ==
                    3u);
    assert_close("light_acceptance_add_authored_progress",
                 after_add.timeline.tracks[after_add.progress_track_index]
                     .keys[1].value.as.scalar,
                 0.25, 0.02);

    assert_true("light_acceptance_before_key_drag",
                RuntimeSceneLightTimelineGetLast(&before_key_drag));

    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = 1;
    event.button.x = timing_graph.x + (int)llround(
        (double)(after_add.timeline.tracks[after_add.progress_track_index]
                     .keys[1].frame -
                 after_add.timeline.range.start_frame) /
        (double)(after_add.timeline.range.frame_count - 1u) *
        timing_graph.w);
    event.button.y = timing_graph.y + timing_graph.h - (int)llround(
        after_add.timeline.tracks[after_add.progress_track_index]
            .keys[1].value.as.scalar * timing_graph.h);
    assert_true("light_acceptance_key_down",
                SceneEditorLightTimelineHandleEvent(&event, &pane_host, layout, NULL));
    event.type = SDL_MOUSEMOTION;
    event.motion.x = event.button.x + 36;
    event.motion.y = event.button.y - timing_graph.h / 5;
    assert_true("light_acceptance_key_drag",
                SceneEditorLightTimelineHandleEvent(&event, &pane_host, layout, NULL));
    event.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_LEFT;
    assert_true("light_acceptance_key_up",
                SceneEditorLightTimelineHandleEvent(&event, &pane_host, layout, NULL));
    assert_true("light_acceptance_after_key_drag",
                RuntimeSceneLightTimelineGetLast(&after_key_drag));
    assert_true("light_acceptance_key_changed",
                memcmp(&before_key_drag.timeline.tracks[
                           before_key_drag.progress_track_index],
                       &after_key_drag.timeline.tracks[
                           after_key_drag.progress_track_index],
                       sizeof(TimelineTrack)) != 0);
    assert_true("light_acceptance_progress_changed",
                after_key_drag.timeline.tracks[
                    after_key_drag.progress_track_index]
                    .keys[1].value.as.scalar >
                before_key_drag.timeline.tracks[
                    before_key_drag.progress_track_index]
                    .keys[1].value.as.scalar);
    assert_true("light_acceptance_key_undo", SceneEditorLightTimelineUndo());
    assert_true("light_acceptance_key_undo_state",
                RuntimeSceneLightTimelineGetLast(&reopened) &&
                memcmp(&before_key_drag.timeline.tracks[
                           before_key_drag.progress_track_index],
                       &reopened.timeline.tracks[reopened.progress_track_index],
                       sizeof(TimelineTrack)) == 0);
    assert_true("light_acceptance_key_redo", SceneEditorLightTimelineRedo());
    assert_true("light_acceptance_key_redo_state",
                RuntimeSceneLightTimelineGetLast(&reopened) &&
                memcmp(&after_key_drag.timeline.tracks[
                           after_key_drag.progress_track_index],
                       &reopened.timeline.tracks[reopened.progress_track_index],
                       sizeof(TimelineTrack)) == 0);

    assert_true("light_acceptance_before_save_sample",
                RuntimeSceneLightTimelineEvaluate(&after_key_drag,
                                                  (TimelineSample){5, 0u, 1u},
                                                  &before_save) == TIMELINE_STATUS_OK);
    assert_true("light_acceptance_persist",
                SceneEditorRuntimeScenePersistAuthoring(diagnostics,
                                                        sizeof(diagnostics)));
    assert_true("light_acceptance_persist_diag", strcmp(diagnostics, "ok") == 0);
    assert_true("light_acceptance_reapply",
                runtime_scene_bridge_apply_file(runtime_path, &summary));
    assert_true("light_acceptance_reopened_document",
                RuntimeSceneLightTimelineGetLast(&reopened));
    assert_true("light_acceptance_reopened_sample",
                RuntimeSceneLightTimelineEvaluate(&reopened,
                                                  (TimelineSample){5, 0u, 1u},
                                                  &after_reopen) == TIMELINE_STATUS_OK);
    assert_close("light_acceptance_roundtrip_progress", after_reopen.progress,
                 before_save.progress, 1e-9);
    assert_close("light_acceptance_roundtrip_position_x", after_reopen.position.x,
                 before_save.position.x, 1e-9);
    assert_close("light_acceptance_roundtrip_speed",
                 after_reopen.world_speed_per_second,
                 before_save.world_speed_per_second, 1e-9);

    unlink(runtime_path);
    json_object_put(scene);
    sceneSettings = saved_scene;
    animSettings = saved_animation;
    SceneEditorLightTimelineReset();
    return 0;
}

static int test_short_default_timeline_expands_for_authoring(void) {
    RuntimeSceneLightTimelineDocument document;
    TimelineTrack track;
    TimelineTrack anchor_track;
    TimelineTrack constant_track;
    TimelineTrack equal_track;
    double anchor_progress = 0.0;
    double anchor_frame = 0.0;
    size_t anchor_key_index = 0u;
    int snapped_anchor = -1;
    memset(&document, 0, sizeof(document));
    memset(&track, 0, sizeof(track));
    assert_true("light_short_default_document_init",
                TimelineDocumentInit(
                    &document.timeline,
                    (TimelineRate){16u, 1u},
                    (TimelineRange){0, 2u}) == TIMELINE_STATUS_OK);
    assert_true("light_short_default_track_init",
                TimelineTrackInit(
                    &track,
                    "selected_light_progress",
                    "light/key",
                    "light/path_progress",
                    TIMELINE_VALUE_SCALAR) == TIMELINE_STATUS_OK);
    assert_true("light_short_default_start_key",
                TimelineTrackAddKey(
                    &track, 0, TimelineValueScalar(0.0),
                    TIMELINE_INTERPOLATION_LINEAR) == TIMELINE_STATUS_OK);
    assert_true("light_short_default_end_key",
                TimelineTrackAddKey(
                    &track, 1, TimelineValueScalar(1.0),
                    TIMELINE_INTERPOLATION_STEP) == TIMELINE_STATUS_OK);
    assert_true("light_short_default_add_track",
                TimelineDocumentAddTrack(&document.timeline, &track) ==
                    TIMELINE_STATUS_OK);
    document.progress_track_index = 0u;
    document.spatial_path.numPoints = 3;
    document.spatial_path.points[0] = (Point){0.0, 0.0};
    document.spatial_path.points[1] = (Point){1.0, 0.0};
    document.spatial_path.points[2] = (Point){4.0, 0.0};
    document.valid = true;
    assert_true("light_short_default_expand",
                scene_editor_light_timeline_ensure_editable_default_range(
                    &document));
    assert_true("light_short_default_five_seconds",
                document.timeline.range.frame_count == 81u);
    assert_true("light_short_default_endpoint_rebased",
                document.timeline.tracks[0].keys[1].frame == 80);
    assert_true("light_path_anchor_progress",
                scene_editor_light_timeline_path_anchor_progress(
                    &document, 1, &anchor_progress));
    assert_close("light_path_anchor_arc_fraction",
                 anchor_progress, 0.25, 0.01);
    assert_close("light_path_anchor_snap",
                 scene_editor_light_timeline_snap_progress_to_anchor(
                     &document, anchor_progress + 0.01, 0.02,
                     &snapped_anchor),
                 anchor_progress, 1e-9);
    assert_true("light_path_anchor_snap_index", snapped_anchor == 1);
    assert_true("light_path_anchor_frame_from_timing",
                scene_editor_light_timeline_frame_at_progress(
                    &document, &document.timeline.tracks[0],
                    anchor_progress, &anchor_frame));
    assert_close("light_path_anchor_linear_arrival",
                 anchor_frame, 20.0, 0.01);
    assert_true("light_path_anchor_set_arrival",
                scene_editor_light_timeline_set_path_anchor_frame(
                    &document, &document.timeline.tracks[0],
                    1, 40, &anchor_track, &anchor_key_index));
    assert_true("light_path_anchor_track_count",
                anchor_track.key_count == 3u);
    assert_true("light_path_anchor_track_mid_frame",
                anchor_track.keys[1].frame == 40);
    assert_true("light_path_anchor_track_selected_key",
                anchor_key_index == 1u);
    assert_close("light_path_anchor_track_mid_progress",
                 anchor_track.keys[1].value.as.scalar, 0.25, 0.01);
    assert_true("light_path_anchor_move_arrival",
                scene_editor_light_timeline_set_path_anchor_frame(
                    &document, &anchor_track,
                    1, 50, &anchor_track, &anchor_key_index));
    assert_true("light_path_anchor_move_keeps_count",
                anchor_track.key_count == 3u);
    assert_true("light_path_anchor_move_frame",
                anchor_track.keys[1].frame == 50);
    assert_true("light_traversal_constant_build",
                scene_editor_light_timeline_build_traversal_track(
                    &document, &anchor_track,
                    SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED,
                    &constant_track));
    assert_true("light_traversal_constant_endpoint_only",
                constant_track.key_count == 2u &&
                constant_track.keys[0].frame == 0 &&
                constant_track.keys[1].frame == 80);
    assert_true("light_traversal_constant_classify",
                scene_editor_light_timeline_classify_traversal(
                    &document, &constant_track) ==
                SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED);
    assert_true("light_traversal_equal_build",
                scene_editor_light_timeline_build_traversal_track(
                    &document, &constant_track,
                    SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS,
                    &equal_track));
    assert_true("light_traversal_equal_count",
                equal_track.key_count == 3u);
    assert_true("light_traversal_equal_mid_frame",
                equal_track.keys[1].frame == 40);
    assert_close("light_traversal_equal_keeps_arc_progress",
                 equal_track.keys[1].value.as.scalar, 0.25, 0.01);
    assert_true("light_traversal_equal_classify",
                scene_editor_light_timeline_classify_traversal(
                    &document, &equal_track) ==
                SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS);
    assert_true("light_traversal_custom_classify",
                scene_editor_light_timeline_classify_traversal(
                    &document, &anchor_track) ==
                SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CUSTOM);
    return 0;
}

static int test_new_timeline_keys_are_independent_of_path_points(void) {
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_animation = animSettings;
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneLightTimelineDocument document;
    SceneEditorPaneHost pane_host;
    const SceneEditorPaneLayout* layout;
    SceneEditorLightTimelinePanelGeometry geometry;
    SDL_Event event;
    json_object* scene = runtime_scene_with_light_timeline("light/key");
    memset(&pane_host, 0, sizeof(pane_host));
    memset(&event, 0, sizeof(event));
    assert_true("light_independent_keys_apply_scene",
                runtime_scene_bridge_apply_json(
                    json_object_to_json_string_ext(
                        scene, JSON_C_TO_STRING_PLAIN),
                    &summary));
    sceneSettings.bezierPath.numPoints = 5;
    for (int i = 0; i < 5; ++i) {
        sceneSettings.bezierPath.points[i] =
            (Point){(double)i, 0.0};
        sceneSettings.bezierPath3D.point_z[i] = (double)i;
    }
    animSettings.fps = 20;
    animSettings.frameLimit = 101;
    animSettings.framesForTravel = 218;
    RuntimeSceneLightTimelineResetLast();
    SceneEditorLightTimelineReset();
    assert_true("light_independent_keys_select",
                SceneEditorLightTimelineSelectTargetId("light/key") ==
                    TIMELINE_STATUS_OK);
    assert_true("light_independent_keys_pane_init",
                scene_editor_pane_host_init(&pane_host, 1280, 760));
    assert_true("light_independent_keys_open",
                SceneEditorLightTimelineToggle(&pane_host));
    assert_true("light_independent_keys_readback",
                RuntimeSceneLightTimelineGetLast(&document));
    assert_true("light_independent_keys_only_endpoints",
                document.spatial_path.numPoints == 5 &&
                document.timeline.range.frame_count == 218u &&
                document.timeline.tracks[
                    document.progress_track_index].key_count == 2u);
    layout = scene_editor_pane_host_layout(&pane_host);
    scene_editor_light_timeline_panel_geometry(
        &layout->timeline_rect, &geometry);
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = 1;
    event.button.x = geometry.equal_segments_button.x +
        geometry.equal_segments_button.w / 2;
    event.button.y = geometry.equal_segments_button.y +
        geometry.equal_segments_button.h / 2;
    assert_true("light_independent_keys_equal_mode_click",
                SceneEditorLightTimelineHandleEvent(
                    &event, &pane_host, layout, NULL));
    assert_true("light_independent_keys_equal_mode_readback",
                RuntimeSceneLightTimelineGetLast(&document) &&
                document.timeline.tracks[
                    document.progress_track_index].key_count == 5u &&
                scene_editor_light_timeline_classify_traversal(
                    &document,
                    &document.timeline.tracks[
                        document.progress_track_index]) ==
                    SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS);
    assert_true("light_independent_keys_manual_custom",
                SceneEditorLightTimelineInsertKey(10, 0.05) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_independent_keys_custom_readback",
                RuntimeSceneLightTimelineGetLast(&document) &&
                scene_editor_light_timeline_classify_traversal(
                    &document,
                    &document.timeline.tracks[
                        document.progress_track_index]) ==
                    SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CUSTOM);
    event.button.x = geometry.constant_speed_button.x +
        geometry.constant_speed_button.w / 2;
    event.button.y = geometry.constant_speed_button.y +
        geometry.constant_speed_button.h / 2;
    assert_true("light_independent_keys_constant_mode_click",
                SceneEditorLightTimelineHandleEvent(
                    &event, &pane_host, layout, NULL));
    assert_true("light_independent_keys_constant_mode_readback",
                RuntimeSceneLightTimelineGetLast(&document) &&
                document.timeline.tracks[
                    document.progress_track_index].key_count == 2u &&
                scene_editor_light_timeline_classify_traversal(
                    &document,
                    &document.timeline.tracks[
                        document.progress_track_index]) ==
                    SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED);
    assert_true("light_independent_keys_mode_undo",
                SceneEditorLightTimelineUndo());
    assert_true("light_independent_keys_mode_undo_custom",
                RuntimeSceneLightTimelineGetLast(&document) &&
                scene_editor_light_timeline_classify_traversal(
                    &document,
                    &document.timeline.tracks[
                        document.progress_track_index]) ==
                    SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CUSTOM);
    SceneEditorLightTimelineReset();
    RuntimeSceneLightTimelineResetLast();
    sceneSettings = saved_scene;
    animSettings = saved_animation;
    json_object_put(scene);
    return 0;
}

int run_test_runtime_timeline_light_persistence_tests(void) {
    test_light_timeline_responsive_geometry();
    test_light_timeline_roundtrip_and_evaluation();
    test_light_timeline_legacy_path_and_transactional_refusal();
    test_light_timeline_runtime_bridge_headless_inspection();
    test_short_default_timeline_expands_for_authoring();
    test_new_timeline_keys_are_independent_of_path_points();
    test_light_timeline_ui_acceptance_roundtrip();
    return test_support_failures();
}
