#include "test_runtime_timeline_light_persistence.h"

#include "config/config_manager.h"
#include "animation/timeline_property_registry.h"
#include "app/evaluated_scene_service.h"
#include "app/preview_retained_scene_quality.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_evaluated_scene_3d.h"
#include "editor/scene_editor_light_timeline.h"
#include "editor/scene_editor_light_timeline_edit.h"
#include "editor/scene_editor_light_timeline_view.h"
#include "editor/scene_editor_light_timeline_curve_edit.h"
#include "editor/scene_editor_light_timeline_selection.h"
#include "editor/scene_editor_light_timeline_tracks.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "test_support.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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
        light_timeline_rect_inside(panel, &geometry.motion_lane_button) &&
        light_timeline_rect_inside(panel, &geometry.intensity_lane_button) &&
        light_timeline_rect_inside(
            panel, &geometry.constant_speed_button) &&
        light_timeline_rect_inside(
            panel, &geometry.equal_segments_button) &&
        light_timeline_rect_inside(
            panel, &geometry.custom_mode_indicator) &&
        light_timeline_rect_inside(panel, &geometry.step_button) &&
        light_timeline_rect_inside(panel, &geometry.linear_button) &&
        light_timeline_rect_inside(panel, &geometry.bezier_button) &&
        geometry.metrics_line.y + geometry.metrics_line.h <=
            geometry.constant_speed_button.y &&
        geometry.constant_speed_button.x +
                geometry.constant_speed_button.w <=
            geometry.equal_segments_button.x &&
        geometry.equal_segments_button.x +
                geometry.equal_segments_button.w <=
            geometry.custom_mode_indicator.x &&
        geometry.custom_mode_indicator.x +
                geometry.custom_mode_indicator.w <=
            geometry.step_button.x &&
        geometry.step_button.x + geometry.step_button.w <=
            geometry.linear_button.x &&
        geometry.linear_button.x + geometry.linear_button.w <=
            geometry.bezier_button.x &&
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
    json_object_object_add(light, "intensity", json_object_new_double(2.0));
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

static bool light_timeline_add_intensity_track(
    RuntimeSceneLightTimelineDocument* document,
    double start_intensity,
    double end_intensity) {
    TimelineTrack track;
    int64_t first;
    int64_t last;
    const TimelineTrack* progress;
    if (!document ||
        document->progress_track_index >= document->timeline.track_count) {
        return false;
    }
    progress = &document->timeline.tracks[document->progress_track_index];
    first = document->timeline.range.start_frame;
    last = first + (int64_t)document->timeline.range.frame_count - 1;
    memset(&track, 0, sizeof(track));
    if (TimelineTrackInit(
            &track, "key_intensity", progress->target_id,
            "light/intensity", TIMELINE_VALUE_SCALAR) !=
            TIMELINE_STATUS_OK ||
        TimelineTrackSetUnit(
            &track, TIMELINE_UNIT_RELATIVE_INTENSITY) !=
            TIMELINE_STATUS_OK ||
        TimelineTrackAddKey(
            &track, first, TimelineValueScalar(start_intensity),
            TIMELINE_INTERPOLATION_LINEAR) != TIMELINE_STATUS_OK ||
        TimelineTrackAddKey(
            &track, last, TimelineValueScalar(end_intensity),
            TIMELINE_INTERPOLATION_STEP) != TIMELINE_STATUS_OK ||
        TimelineDocumentAddTrack(&document->timeline, &track) !=
            TIMELINE_STATUS_OK) {
        return false;
    }
    document->loaded_schema_version =
        RUNTIME_SCENE_LIGHT_TIMELINE_SCHEMA_VERSION;
    document->has_intensity_track = true;
    document->intensity_track_index =
        document->timeline.track_count - 1u;
    return RuntimeSceneLightTimelineValidateDocument(document) ==
        TIMELINE_STATUS_OK;
}

static int test_light_timeline_schema_v2_multitrack_contract(void) {
    RuntimeSceneLightTimelineDocument legacy;
    RuntimeSceneLightTimelineDocument reopened;
    RuntimeSceneLightTimelineDocument invalid;
    RuntimeSceneLightTimelineDocument sentinel;
    RuntimeSceneLightTimelineDocument original;
    char diagnostics[96];
    json_object* v1 = json_tokener_parse(light_timeline_json(true));
    json_object* encoded = NULL;
    json_object* wrapper = NULL;
    json_object* version = NULL;
    json_object* tracks = NULL;
    memset(&legacy, 0, sizeof(legacy));
    assert_true("light_v2_legacy_parse",
                RuntimeSceneLightTimelineParseAuthoring(
                    v1, 1.0, &legacy, diagnostics,
                    sizeof(diagnostics)) == TIMELINE_STATUS_OK);
    assert_true("light_v2_legacy_schema_recorded",
                legacy.loaded_schema_version ==
                    RUNTIME_SCENE_LIGHT_TIMELINE_SCHEMA_VERSION_LEGACY &&
                !legacy.has_intensity_track);
    assert_true("light_v2_add_intensity",
                light_timeline_add_intensity_track(&legacy, 2.0, 6.0));
    encoded = RuntimeSceneLightTimelineToJsonObject(&legacy, 1.0);
    assert_true("light_v2_encode", encoded != NULL);
    assert_true("light_v2_writer_schema",
                json_object_object_get_ex(encoded, "version", &version) &&
                json_object_get_int(version) ==
                    RUNTIME_SCENE_LIGHT_TIMELINE_SCHEMA_VERSION);
    assert_true("light_v2_writer_typed_tracks",
                json_object_object_get_ex(encoded, "tracks", &tracks) &&
                json_object_array_length(tracks) == 2u);
    wrapper = json_object_new_object();
    json_object_object_add(wrapper, "light_timeline", encoded);
    assert_true("light_v2_reopen",
                RuntimeSceneLightTimelineParseAuthoring(
                    wrapper, 1.0, &reopened, diagnostics,
                    sizeof(diagnostics)) == TIMELINE_STATUS_OK);
    assert_true("light_v2_reopen_exact",
                reopened.has_intensity_track &&
                reopened.timeline.track_count == 2u &&
                memcmp(&legacy.timeline.tracks[
                           legacy.progress_track_index],
                       &reopened.timeline.tracks[
                           reopened.progress_track_index],
                       sizeof(TimelineTrack)) == 0 &&
                memcmp(&legacy.timeline.tracks[
                           legacy.intensity_track_index],
                       &reopened.timeline.tracks[
                           reopened.intensity_track_index],
                       sizeof(TimelineTrack)) == 0);

    invalid = legacy;
    invalid.timeline.tracks[invalid.intensity_track_index]
        .keys[0].value.as.scalar = -1.0;
    assert_true("light_v2_negative_intensity_refused",
                RuntimeSceneLightTimelineValidateDocument(&invalid) ==
                    TIMELINE_STATUS_VALUE_OUT_OF_RANGE);
    invalid = legacy;
    invalid.timeline.tracks[invalid.intensity_track_index].unit =
        TIMELINE_UNIT_UNITLESS;
    assert_true("light_v2_wrong_intensity_unit_refused",
                RuntimeSceneLightTimelineValidateDocument(&invalid) ==
                    TIMELINE_STATUS_UNIT_MISMATCH);
    invalid = legacy;
    snprintf(
        invalid.timeline.tracks[invalid.intensity_track_index].target_id,
        TIMELINE_ID_CAPACITY, "light/other");
    assert_true("light_v2_target_mismatch_refused",
                RuntimeSceneLightTimelineValidateDocument(&invalid) ==
                    TIMELINE_STATUS_OWNERSHIP_MISMATCH);
    invalid = legacy;
    snprintf(
        invalid.timeline.tracks[invalid.intensity_track_index].property_id,
        TIMELINE_ID_CAPACITY, "light/path_progress");
    assert_true("light_v2_duplicate_property_refused",
                RuntimeSceneLightTimelineValidateDocument(&invalid) ==
                    TIMELINE_STATUS_DUPLICATE_OWNERSHIP);
    invalid = legacy;
    invalid.timeline.tracks[invalid.intensity_track_index]
        .keys[0].value.as.scalar = NAN;
    assert_true("light_v2_nonfinite_intensity_refused",
                RuntimeSceneLightTimelineValidateDocument(&invalid) !=
                    TIMELINE_STATUS_OK);
    {
        TimelineEvaluationContext context;
        TimelineEvaluationResult result;
        TimelineTrack descending =
            legacy.timeline.tracks[legacy.intensity_track_index];
        descending.keys[0].value.as.scalar = 6.0;
        descending.keys[1].value.as.scalar = 2.0;
        descending.keys[0].interpolation_to_next =
            TIMELINE_INTERPOLATION_CUBIC_BEZIER;
        descending.keys[0].outgoing_frame_offset = 6.0;
        descending.keys[0].outgoing_value_offset = -1.0;
        descending.keys[1].incoming_frame_offset = -6.0;
        descending.keys[1].incoming_value_offset = 1.0;
        assert_true("light_v2_descending_cubic_valid",
                    scene_editor_light_timeline_validate_lane_track(
                        &legacy,
                        SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY,
                        &descending) == TIMELINE_STATUS_OK);
        assert_true("light_v2_descending_cubic_context",
                    TimelineEvaluationContextBuild(
                        legacy.timeline.rate, legacy.timeline.range,
                        (TimelineSample){10, 0u, 1u}, &context) ==
                        TIMELINE_STATUS_OK);
        assert_true("light_v2_descending_cubic_evaluate",
                    TimelineTrackEvaluate(
                        &descending, &context, &result) ==
                        TIMELINE_STATUS_OK);
        assert_true("light_v2_descending_cubic_nonnegative",
                    isfinite(result.value.as.scalar) &&
                    result.value.as.scalar >= 0.0 &&
                    result.value.as.scalar < 6.0 &&
                    result.value.as.scalar > 2.0);
    }
    sentinel = legacy;
    original = sentinel;
    assert_true("light_v2_set_last_valid",
                RuntimeSceneLightTimelineSetLast(&legacy) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_v2_set_last_invalid_transactional",
                RuntimeSceneLightTimelineSetLast(&invalid) !=
                    TIMELINE_STATUS_OK);
    assert_true("light_v2_set_last_preserves_prior",
                RuntimeSceneLightTimelineGetLast(&sentinel) &&
                memcmp(&sentinel, &original, sizeof(sentinel)) == 0);
    json_object_put(wrapper);
    json_object_put(v1);
    return 0;
}

static int test_light_timeline_intensity_evaluated_scene_parity(void) {
    RuntimeSceneBridgePreflight summary;
    RuntimeSceneLightTimelineDocument document;
    RayEvaluatedSceneServiceResult base;
    RayEvaluatedSceneServiceResult first;
    RayEvaluatedSceneServiceResult middle;
    RayEvaluatedSceneServiceResult last;
    char diagnostics[64];
    json_object* scene =
        runtime_scene_with_light_timeline("light/key");
    json_object* v1 = json_tokener_parse(light_timeline_json(true));
    assert_true("light_intensity_parity_apply",
                runtime_scene_bridge_apply_json(
                    json_object_to_json_string_ext(
                        scene, JSON_C_TO_STRING_PLAIN),
                    &summary));
    assert_true("light_intensity_parity_parse",
                RuntimeSceneLightTimelineParseAuthoring(
                    v1, 1.0, &document, diagnostics,
                    sizeof(diagnostics)) == TIMELINE_STATUS_OK);
    assert_true("light_intensity_parity_base_set",
                RuntimeSceneLightTimelineSetLast(&document) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_intensity_parity_base_capture",
                RayEvaluatedSceneCaptureAuthoredSample(
                    (TimelineSample){10, 0u, 1u}, &base));
    assert_close("light_intensity_parity_missing_uses_base",
                 base.snapshot.light.intensity, 2.0, 1e-12);
    assert_true("light_intensity_parity_missing_provenance",
                !base.snapshot.light.intensity_authored &&
                !base.snapshot.light.intensity_provenance.valid);
    assert_true("light_intensity_parity_add_track",
                light_timeline_add_intensity_track(
                    &document, 2.0, 6.0));
    assert_true("light_intensity_parity_set_v2",
                RuntimeSceneLightTimelineSetLast(&document) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_intensity_parity_first",
                RayEvaluatedSceneCaptureAuthoredSample(
                    (TimelineSample){0, 0u, 1u}, &first));
    assert_true("light_intensity_parity_middle",
                RayEvaluatedSceneCaptureAuthoredSample(
                    (TimelineSample){10, 0u, 1u}, &middle));
    assert_true("light_intensity_parity_last",
                RayEvaluatedSceneCaptureAuthoredSample(
                    (TimelineSample){20, 0u, 1u}, &last));
    assert_close("light_intensity_parity_first_value",
                 first.snapshot.light.intensity, 2.0, 1e-12);
    assert_close("light_intensity_parity_middle_value",
                 middle.snapshot.light.intensity, 4.0, 1e-12);
    assert_close("light_intensity_parity_last_value",
                 last.snapshot.light.intensity, 6.0, 1e-12);
    assert_true("light_intensity_parity_provenance",
                middle.snapshot.light.intensity_authored &&
                middle.snapshot.light.intensity_provenance.valid &&
                strcmp(
                    middle.snapshot.light.intensity_provenance.property_id,
                    "light/intensity") == 0);
    assert_close("light_intensity_parity_motion_identity",
                 middle.snapshot.light.progress,
                 base.snapshot.light.progress, 1e-12);
    assert_close("light_intensity_parity_position_identity",
                 middle.snapshot.light.position.x,
                 base.snapshot.light.position.x, 1e-12);
    assert_true("light_intensity_parity_one_snapshot_contract",
                middle.snapshot.schema_version ==
                    RAY_EVALUATED_SCENE_SNAPSHOT_SCHEMA_VERSION &&
                (middle.snapshot.invalidation_domains &
                 TIMELINE_INVALIDATION_LIGHTING) != 0u);
    {
        RuntimeScene3D* final_scene =
            (RuntimeScene3D*)calloc(1u, sizeof(RuntimeScene3D));
        RuntimeScene3D* headless_scene =
            (RuntimeScene3D*)calloc(1u, sizeof(RuntimeScene3D));
        RuntimeLightSource3D consumer_light;
        PreviewRetainedSceneFrame preview_first;
        PreviewRetainedSceneFrame preview_middle;
        PreviewRetainedSceneFrame preview_last;
        SDL_Color albedo = {160, 160, 160, 255};
        SDL_Color pixel_first;
        SDL_Color pixel_middle;
        SDL_Color pixel_last;
        double luminance_first;
        double luminance_middle;
        double luminance_last;
        assert_true("light_intensity_parity_consumer_alloc",
                    final_scene != NULL && headless_scene != NULL);
        if (!final_scene || !headless_scene) {
            free(final_scene);
            free(headless_scene);
            json_object_put(v1);
            json_object_put(scene);
            return 0;
        }
        RuntimeScene3D_Init(final_scene);
        RuntimeScene3D_Init(headless_scene);
        RuntimeLightSource3D_Init(&consumer_light);
        consumer_light.enabled = true;
        snprintf(consumer_light.id, sizeof(consumer_light.id), "key");
        assert_true("light_intensity_parity_final_light_append",
                    RuntimeLightSet3D_Append(
                        &final_scene->lightSet, &consumer_light, NULL));
        assert_true("light_intensity_parity_headless_light_append",
                    RuntimeLightSet3D_Append(
                        &headless_scene->lightSet, &consumer_light, NULL));
        assert_true("light_intensity_parity_final_consumer",
                    RuntimeEvaluatedScene3DApply(
                        final_scene, &middle.snapshot));
        assert_true("light_intensity_parity_headless_consumer",
                    RuntimeEvaluatedScene3DApply(
                        headless_scene, &middle.snapshot));
        assert_close("light_intensity_parity_final_value",
                     final_scene->lightSet.lights[0].intensity,
                     middle.snapshot.light.intensity, 1e-12);
        assert_close("light_intensity_parity_headless_value",
                     headless_scene->lightSet.lights[0].intensity,
                     middle.snapshot.light.intensity, 1e-12);
        assert_true("light_intensity_fixed_exposure_first",
                    PreviewRetainedSceneFrameBuild(
                        PREVIEW_RETAINED_SCENE_QUALITY_INTERACTIVE_SHADED,
                        &first.snapshot, &preview_first));
        assert_true("light_intensity_fixed_exposure_middle",
                    PreviewRetainedSceneFrameBuild(
                        PREVIEW_RETAINED_SCENE_QUALITY_INTERACTIVE_SHADED,
                        &middle.snapshot, &preview_middle));
        assert_true("light_intensity_fixed_exposure_last",
                    PreviewRetainedSceneFrameBuild(
                        PREVIEW_RETAINED_SCENE_QUALITY_INTERACTIVE_SHADED,
                        &last.snapshot, &preview_last));
        pixel_first = PreviewRetainedSceneShadeColor(
            albedo, 0.0, 0.0, 1.0,
            first.snapshot.light.position.x,
            first.snapshot.light.position.y,
            first.snapshot.light.position.z - 5.0,
            &preview_first);
        pixel_middle = PreviewRetainedSceneShadeColor(
            albedo, 0.0, 0.0, 1.0,
            middle.snapshot.light.position.x,
            middle.snapshot.light.position.y,
            middle.snapshot.light.position.z - 5.0,
            &preview_middle);
        pixel_last = PreviewRetainedSceneShadeColor(
            albedo, 0.0, 0.0, 1.0,
            last.snapshot.light.position.x,
            last.snapshot.light.position.y,
            last.snapshot.light.position.z - 5.0,
            &preview_last);
        luminance_first =
            (double)pixel_first.r + pixel_first.g + pixel_first.b;
        luminance_middle =
            (double)pixel_middle.r + pixel_middle.g + pixel_middle.b;
        luminance_last =
            (double)pixel_last.r + pixel_last.g + pixel_last.b;
        assert_true("light_intensity_fixed_exposure_response_direction",
                    luminance_first < luminance_middle &&
                    luminance_middle < luminance_last);
        RuntimeScene3D_Free(final_scene);
        RuntimeScene3D_Free(headless_scene);
        free(final_scene);
        free(headless_scene);
    }
    json_object_put(v1);
    json_object_put(scene);
    return 0;
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
    static RuntimeSceneLightTimelineDocument before_endpoint_drag;
    static RuntimeSceneLightTimelineDocument after_endpoint_drag;
    static RuntimeSceneLightTimelineDocument after_add;
    static RuntimeSceneLightTimelineDocument before_key_drag;
    static RuntimeSceneLightTimelineDocument after_key_drag;
    static RuntimeSceneLightTimelineDocument motion_before_intensity;
    static RuntimeSceneLightTimelineDocument intensity_authored;
    static RuntimeSceneLightTimelineDocument current_before_save;
    static RuntimeSceneLightTimelineDocument reopened;
    TimelineLightMotionSample before_save;
    TimelineLightMotionSample after_reopen;
    RuntimeSceneBridge3DLightSeedState lights_before_play;
    RuntimeSceneBridge3DLightSeedState lights_after_play;
    static RuntimeSceneLightTimelineDocument timeline_before_play;
    static RuntimeSceneLightTimelineDocument timeline_after_play;
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
    assert_true("light_acceptance_open_pane_reselection_refused",
                SceneEditorLightTimelineSelectTargetId("light/other") ==
                    TIMELINE_STATUS_OWNERSHIP_MISMATCH);
    assert_true("light_acceptance_open_pane_target_retained",
                strcmp(SceneEditorLightTimelineSelectedTargetId(),
                       "light/key") == 0);
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

    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.clicks = 1;
    event.button.x =
        geometry.bezier_button.x + geometry.bezier_button.w / 2;
    event.button.y =
        geometry.bezier_button.y + geometry.bezier_button.h / 2;
    assert_true("light_acceptance_bezier_selector",
                SceneEditorLightTimelineHandleEvent(
                    &event, &pane_host, layout, NULL));
    assert_true("light_acceptance_bezier_selected",
                RuntimeSceneLightTimelineGetLast(&reopened) &&
                reopened.timeline.tracks[reopened.progress_track_index]
                        .keys[1].interpolation_to_next ==
                    TIMELINE_INTERPOLATION_CUBIC_BEZIER);
    assert_true("light_acceptance_bezier_undo",
                SceneEditorLightTimelineUndo());
    assert_true("light_acceptance_bezier_undo_linear",
                RuntimeSceneLightTimelineGetLast(&reopened) &&
                reopened.timeline.tracks[reopened.progress_track_index]
                        .keys[1].interpolation_to_next ==
                    TIMELINE_INTERPOLATION_LINEAR);
    assert_true("light_acceptance_bezier_redo",
                SceneEditorLightTimelineRedo());
    assert_true("light_acceptance_bezier_redo_exact",
                RuntimeSceneLightTimelineGetLast(&reopened) &&
                reopened.timeline.tracks[reopened.progress_track_index]
                        .keys[1].outgoing_frame_offset > 0.0 &&
                reopened.timeline.tracks[reopened.progress_track_index]
                        .keys[2].incoming_frame_offset < 0.0);
    {
        int handle_x = 0;
        int handle_y = 0;
        TimelineTrack before_handle =
            reopened.timeline.tracks[reopened.progress_track_index];
        assert_true("light_acceptance_handle_visible",
                    scene_editor_light_timeline_handle_point(
                        &(SceneEditorLightTimelineView){0.0, 1.0},
                        &reopened, &before_handle, 1u,
                        SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING,
                        &timing_graph, &handle_x, &handle_y));
        memset(&event, 0, sizeof(event));
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.clicks = 1;
        event.button.x = handle_x;
        event.button.y = handle_y;
        assert_true("light_acceptance_handle_down",
                    SceneEditorLightTimelineHandleEvent(
                        &event, &pane_host, layout, NULL));
        event.type = SDL_MOUSEMOTION;
        event.motion.x = handle_x + 10;
        event.motion.y = handle_y - 8;
        assert_true("light_acceptance_handle_drag",
                    SceneEditorLightTimelineHandleEvent(
                        &event, &pane_host, layout, NULL));
        event.type = SDL_MOUSEBUTTONUP;
        event.button.button = SDL_BUTTON_LEFT;
        assert_true("light_acceptance_handle_up",
                    SceneEditorLightTimelineHandleEvent(
                        &event, &pane_host, layout, NULL));
        assert_true("light_acceptance_handle_changed",
                    RuntimeSceneLightTimelineGetLast(&after_key_drag) &&
                    memcmp(&before_handle,
                           &after_key_drag.timeline.tracks[
                               after_key_drag.progress_track_index],
                           sizeof(TimelineTrack)) != 0);
        assert_true("light_acceptance_handle_undo",
                    SceneEditorLightTimelineUndo());
        assert_true("light_acceptance_handle_undo_exact",
                    RuntimeSceneLightTimelineGetLast(&reopened) &&
                    memcmp(&before_handle,
                           &reopened.timeline.tracks[
                               reopened.progress_track_index],
                           sizeof(TimelineTrack)) == 0);
        assert_true("light_acceptance_handle_redo",
                    SceneEditorLightTimelineRedo());
        assert_true("light_acceptance_handle_redo_exact",
                    RuntimeSceneLightTimelineGetLast(&reopened) &&
                    memcmp(&after_key_drag.timeline.tracks[
                               after_key_drag.progress_track_index],
                           &reopened.timeline.tracks[
                               reopened.progress_track_index],
                           sizeof(TimelineTrack)) == 0);
    }

    assert_true("light_acceptance_motion_before_intensity",
                RuntimeSceneLightTimelineGetLast(
                    &motion_before_intensity));
    assert_true("light_acceptance_select_intensity_lane",
                SceneEditorLightTimelineSelectLane(
                    SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_acceptance_lane_keeps_target",
                strcmp(SceneEditorLightTimelineSelectedTargetId(),
                       "light/key") == 0);
    assert_true("light_acceptance_lazy_endpoint_insert",
                SceneEditorLightTimelineInsertIntensityKey(0, 3.5) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_acceptance_lazy_endpoint_updates_endpoint",
                RuntimeSceneLightTimelineGetLast(&reopened) &&
                reopened.has_intensity_track &&
                reopened.timeline.tracks[
                    reopened.intensity_track_index].key_count == 2u &&
                reopened.timeline.tracks[
                    reopened.intensity_track_index]
                    .keys[0].value.as.scalar == 3.5);
    assert_true("light_acceptance_lazy_endpoint_single_undo",
                SceneEditorLightTimelineUndo());
    assert_true("light_acceptance_lazy_endpoint_undo_removes_track",
                RuntimeSceneLightTimelineGetLast(&reopened) &&
                !reopened.has_intensity_track);
    assert_true("light_acceptance_lazy_intensity_insert",
                SceneEditorLightTimelineInsertIntensityKey(8, 3.5) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_acceptance_intensity_created",
                RuntimeSceneLightTimelineGetLast(&intensity_authored) &&
                intensity_authored.has_intensity_track &&
                intensity_authored.timeline.tracks[
                    intensity_authored.intensity_track_index].key_count ==
                    3u);
    assert_true("light_acceptance_intensity_motion_untouched",
                memcmp(&motion_before_intensity.timeline.tracks[
                           motion_before_intensity.progress_track_index],
                       &intensity_authored.timeline.tracks[
                           intensity_authored.progress_track_index],
                       sizeof(TimelineTrack)) == 0);
    assert_close("light_acceptance_intensity_authored_value",
                 intensity_authored.timeline.tracks[
                     intensity_authored.intensity_track_index]
                     .keys[1].value.as.scalar,
                 3.5, 1e-12);
    assert_true("light_acceptance_intensity_lazy_undo",
                SceneEditorLightTimelineUndo());
    assert_true("light_acceptance_intensity_undo_removes_track",
                RuntimeSceneLightTimelineGetLast(&reopened) &&
                !reopened.has_intensity_track &&
                memcmp(&motion_before_intensity.timeline.tracks[
                           motion_before_intensity.progress_track_index],
                       &reopened.timeline.tracks[
                           reopened.progress_track_index],
                       sizeof(TimelineTrack)) == 0);
    assert_true("light_acceptance_intensity_lazy_redo",
                SceneEditorLightTimelineRedo());
    assert_true("light_acceptance_intensity_redo_exact",
                RuntimeSceneLightTimelineGetLast(&reopened) &&
                reopened.has_intensity_track &&
                memcmp(&intensity_authored.timeline.tracks[
                           intensity_authored.intensity_track_index],
                       &reopened.timeline.tracks[
                           reopened.intensity_track_index],
                       sizeof(TimelineTrack)) == 0);
    {
        double minimum = 0.0;
        double maximum = 1.0;
        TimelineTrack* intensity = &reopened.timeline.tracks[
            reopened.intensity_track_index];
        scene_editor_light_timeline_lane_value_range(
            SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY,
            intensity, 2.0, &minimum, &maximum);
        memset(&event, 0, sizeof(event));
        event.type = SDL_MOUSEBUTTONDOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.clicks = 1;
        event.button.x = timing_graph.x + (int)llround(
            8.0 / 20.0 * (double)timing_graph.w);
        event.button.y = scene_editor_light_timeline_lane_y_at_value(
            &timing_graph, 3.5, minimum, maximum);
        assert_true("light_acceptance_intensity_select_key",
                    SceneEditorLightTimelineHandleEvent(
                        &event, &pane_host, layout, NULL));
        event.type = SDL_MOUSEBUTTONUP;
        event.button.button = SDL_BUTTON_LEFT;
        assert_true("light_acceptance_intensity_select_key_up",
                    SceneEditorLightTimelineHandleEvent(
                        &event, &pane_host, layout, NULL));
    }
    assert_true("light_acceptance_intensity_bezier",
                SceneEditorLightTimelineSetSelectedInterpolation(
                    TIMELINE_INTERPOLATION_CUBIC_BEZIER) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_acceptance_intensity_bezier_persisted",
                RuntimeSceneLightTimelineGetLast(&intensity_authored) &&
                intensity_authored.timeline.tracks[
                    intensity_authored.intensity_track_index]
                        .keys[1].interpolation_to_next ==
                    TIMELINE_INTERPOLATION_CUBIC_BEZIER);
    {
        TimelineTrack before_handle =
            intensity_authored.timeline.tracks[
                intensity_authored.intensity_track_index];
        TimelineTrack moved_handle;
        double minimum = 0.0;
        double maximum = 1.0;
        int handle_x = 0;
        int handle_y = 0;
        scene_editor_light_timeline_lane_value_range(
            SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY,
            &before_handle, 2.0, &minimum, &maximum);
        assert_true("light_acceptance_intensity_handle_visible",
                    scene_editor_light_timeline_scalar_handle_point(
                        &(SceneEditorLightTimelineView){0.0, 1.0},
                        &intensity_authored, &before_handle, 1u,
                        SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING,
                        &timing_graph, minimum, maximum,
                        &handle_x, &handle_y));
        assert_true("light_acceptance_intensity_handle_move",
                    scene_editor_light_timeline_move_scalar_handle(
                        &(SceneEditorLightTimelineView){0.0, 1.0},
                        &intensity_authored, &before_handle, 1u,
                        SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING,
                        &timing_graph, minimum, maximum,
                        handle_x + 8, handle_y - 6,
                        &moved_handle) == TIMELINE_STATUS_OK);
        assert_true("light_acceptance_intensity_handle_operable",
                    memcmp(&before_handle, &moved_handle,
                           sizeof(TimelineTrack)) != 0);
    }
    assert_true("light_acceptance_intensity_bezier_undo",
                SceneEditorLightTimelineUndo());
    assert_true("light_acceptance_intensity_bezier_redo",
                SceneEditorLightTimelineRedo());
    assert_true("light_acceptance_select_motion_lane",
                SceneEditorLightTimelineSelectLane(
                    SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_acceptance_current_before_save",
                RuntimeSceneLightTimelineGetLast(&current_before_save));
    assert_true("light_acceptance_before_save_sample",
                RuntimeSceneLightTimelineEvaluate(&current_before_save,
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
    assert_true("light_acceptance_roundtrip_intensity_track",
                reopened.has_intensity_track &&
                memcmp(&current_before_save.timeline.tracks[
                           current_before_save.intensity_track_index],
                       &reopened.timeline.tracks[
                           reopened.intensity_track_index],
                       sizeof(TimelineTrack)) == 0);

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
    assert_true("light_short_default_track_unit",
                TimelineTrackSetUnit(
                    &track, TIMELINE_UNIT_UNITLESS) ==
                    TIMELINE_STATUS_OK);
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
    {
        double progress = 0.0;
        assert_true("light_traversal_constant_declared_frame",
                    scene_editor_light_timeline_evaluate_progress_at_frame(
                        &document, &constant_track, 40, &progress));
        assert_close("light_traversal_constant_visible_evaluated_parity",
                     progress, 0.5, 1e-9);
    }
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
    {
        double progress = 0.0;
        assert_true("light_traversal_equal_declared_frame",
                    scene_editor_light_timeline_evaluate_progress_at_frame(
                        &document, &equal_track, 40, &progress));
        assert_close("light_traversal_equal_visible_evaluated_parity",
                     progress, 0.25, 0.01);
        assert_true("light_traversal_custom_declared_frame",
                    scene_editor_light_timeline_evaluate_progress_at_frame(
                        &document, &anchor_track, 50, &progress));
        assert_close("light_traversal_custom_visible_evaluated_parity",
                     progress, 0.25, 0.01);
    }
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

static int test_light_timeline_stable_selection_identity(void) {
    SceneEditorLightTimelineSelection selection;
    RuntimeLightSource3D lights[2];
    size_t index = SIZE_MAX;
    memset(&selection, 0, sizeof(selection));
    memset(lights, 0, sizeof(lights));
    snprintf(lights[0].id, sizeof(lights[0].id), "a");
    snprintf(lights[1].id, sizeof(lights[1].id), "b");
    assert_true("light_selection_select_index",
                scene_editor_light_timeline_selection_select_index(
                    &selection, lights, 2u, 1u) == TIMELINE_STATUS_OK);
    assert_true("light_selection_stores_stable_target",
                strcmp(scene_editor_light_timeline_selection_target_id(
                           &selection),
                       "light/b") == 0);
    {
        RuntimeLightSource3D reordered[2] = {lights[1], lights[0]};
        assert_true("light_selection_reorder_resolves_same_id",
                    scene_editor_light_timeline_selection_resolve(
                        &selection, reordered, 2u, &index) ==
                        TIMELINE_STATUS_OK &&
                    index == 0u);
    }
    assert_true("light_selection_disappearance_refused",
                scene_editor_light_timeline_selection_resolve(
                    &selection, lights, 1u, &index) ==
                    TIMELINE_STATUS_TARGET_NOT_FOUND);
    {
        RuntimeLightSource3D duplicate[2] = {lights[1], lights[1]};
        assert_true("light_selection_duplicate_refused",
                    scene_editor_light_timeline_selection_resolve(
                        &selection, duplicate, 2u, &index) ==
                        TIMELINE_STATUS_DUPLICATE_ID);
    }
    assert_true("light_selection_stale_identity_retained",
                strcmp(scene_editor_light_timeline_selection_target_id(
                           &selection),
                       "light/b") == 0);
    return 0;
}

static int test_light_timeline_cubic_operability_and_validity(void) {
    static RuntimeSceneLightTimelineDocument document;
    static RuntimeSceneLightTimelineDocument reopened;
    static TimelineTrack track;
    static TimelineTrack cubic;
    static TimelineTrack moved;
    static TimelineTrack invalid;
    SceneEditorLightTimelineView view = {0.0, 1.0};
    SDL_Rect graph = {40, 20, 400, 200};
    int handle_x = 0;
    int handle_y = 0;
    double previous = -1.0;
    char diagnostics[64];
    json_object* encoded = NULL;
    json_object* wrapper = NULL;
    memset(&document, 0, sizeof(document));
    memset(&reopened, 0, sizeof(reopened));
    memset(&track, 0, sizeof(track));
    memset(&cubic, 0, sizeof(cubic));
    memset(&moved, 0, sizeof(moved));
    memset(&invalid, 0, sizeof(invalid));
    assert_true("light_cubic_document_init",
                TimelineDocumentInit(
                    &document.timeline, (TimelineRate){20u, 1u},
                    (TimelineRange){0, 21u}) == TIMELINE_STATUS_OK);
    assert_true("light_cubic_track_init",
                TimelineTrackInit(
                    &track, "progress", "light/key",
                    "light/path_progress", TIMELINE_VALUE_SCALAR) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_cubic_unit",
                TimelineTrackSetUnit(&track, TIMELINE_UNIT_UNITLESS) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_cubic_key_0",
                TimelineTrackAddKey(
                    &track, 0, TimelineValueScalar(0.0),
                    TIMELINE_INTERPOLATION_LINEAR) == TIMELINE_STATUS_OK);
    assert_true("light_cubic_key_1",
                TimelineTrackAddKey(
                    &track, 10, TimelineValueScalar(0.4),
                    TIMELINE_INTERPOLATION_LINEAR) == TIMELINE_STATUS_OK);
    assert_true("light_cubic_key_2",
                TimelineTrackAddKey(
                    &track, 20, TimelineValueScalar(1.0),
                    TIMELINE_INTERPOLATION_STEP) == TIMELINE_STATUS_OK);
    assert_true("light_cubic_add_track",
                TimelineDocumentAddTrack(&document.timeline, &track) ==
                    TIMELINE_STATUS_OK);
    document.progress_track_index = 0u;
    document.spatial_path.numPoints = 2;
    document.spatial_path.points[0] = (Point){0.0, 0.0};
    document.spatial_path.points[1] = (Point){4.0, 0.0};
    CameraPath3D_Reset(&document.spatial_path_3d);
    CameraPath3D_SyncDefaults(
        &document.spatial_path_3d, &document.spatial_path, 0.0);
    document.valid = true;
    {
        TimelineEvaluationContext context;
        TimelineEvaluationResult result;
        TimelineTrack step = track;
        assert_true("light_linear_declared_frame_context",
                    TimelineEvaluationContextBuild(
                        document.timeline.rate, document.timeline.range,
                        (TimelineSample){5, 0u, 1u}, &context) ==
                        TIMELINE_STATUS_OK);
        assert_true("light_linear_declared_frame_evaluate",
                    TimelineTrackEvaluate(&track, &context, &result) ==
                        TIMELINE_STATUS_OK);
        assert_close("light_linear_visible_evaluated_parity",
                     result.value.as.scalar, 0.2, 1e-9);
        step.keys[0].interpolation_to_next =
            TIMELINE_INTERPOLATION_STEP;
        assert_true("light_step_declared_frame_evaluate",
                    TimelineTrackEvaluate(&step, &context, &result) ==
                        TIMELINE_STATUS_OK);
        assert_close("light_step_visible_evaluated_parity",
                     result.value.as.scalar, 0.0, 1e-9);
    }
    assert_true("light_cubic_selector_activates",
                scene_editor_light_timeline_set_interpolation(
                    &document, &track, 0u,
                    TIMELINE_INTERPOLATION_CUBIC_BEZIER,
                    &cubic) == TIMELINE_STATUS_OK);
    assert_true("light_cubic_default_handles_nonzero",
                cubic.keys[0].outgoing_frame_offset > 0.0 &&
                cubic.keys[1].incoming_frame_offset < 0.0);
    assert_true("light_cubic_handle_point",
                scene_editor_light_timeline_handle_point(
                    &view, &document, &cubic, 0u,
                    SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING,
                    &graph, &handle_x, &handle_y));
    assert_true("light_cubic_handle_pick",
                scene_editor_light_timeline_pick_handle(
                    &view, &document, &cubic, 0u, &graph,
                    handle_x, handle_y) ==
                    SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING);
    assert_true("light_cubic_handle_move",
                scene_editor_light_timeline_move_handle(
                    &view, &document, &cubic, 0u,
                    SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING,
                    &graph, graph.x + 80, graph.y + 120,
                    &moved) == TIMELINE_STATUS_OK);
    assert_true("light_cubic_track_valid",
                TimelineLightMotionValidateProgressTrack(
                    &moved, &document.timeline.range) ==
                    TIMELINE_STATUS_OK);
    {
        TimelineTrack inserted = moved;
        TimelineKeyframe key;
        size_t index = 0u;
        memset(&key, 0, sizeof(key));
        key.frame = 5;
        key.value = TimelineValueScalar(0.2);
        key.interpolation_to_next = TIMELINE_INTERPOLATION_LINEAR;
        assert_true("light_cubic_insert_key",
                    TimelineTrackInsertKey(
                        &inserted, key, &index) == TIMELINE_STATUS_OK);
        assert_true("light_cubic_insert_constrained",
                    scene_editor_light_timeline_constrain_adjacent_handles(
                        &document, &inserted, index) ==
                        TIMELINE_STATUS_OK);
    }
    document.timeline.tracks[0] = moved;
    for (int64_t frame = 0; frame <= 20; ++frame) {
        TimelineLightMotionSample sample;
        assert_true("light_cubic_declared_frame_evaluates",
                    RuntimeSceneLightTimelineEvaluate(
                        &document, (TimelineSample){frame, 0u, 1u},
                        &sample) == TIMELINE_STATUS_OK);
        assert_true("light_cubic_declared_frame_monotonic",
                    sample.progress >= previous &&
                    sample.progress >= 0.0 && sample.progress <= 1.0);
        previous = sample.progress;
    }
    invalid = moved;
    invalid.keys[0].outgoing_value_offset = 0.8;
    assert_true("light_cubic_overshoot_refused",
                TimelineLightMotionValidateProgressTrack(
                    &invalid, &document.timeline.range) ==
                    TIMELINE_STATUS_INVALID_TRACK);
    assert_true("light_cubic_set_last",
                RuntimeSceneLightTimelineSetLast(&document) ==
                    TIMELINE_STATUS_OK);
    {
        document.timeline.tracks[document.progress_track_index] = invalid;
        assert_true("light_cubic_invalid_set_last_refused",
                    RuntimeSceneLightTimelineSetLast(&document) ==
                        TIMELINE_STATUS_INVALID_TRACK);
        assert_true("light_cubic_invalid_set_last_transactional",
                    RuntimeSceneLightTimelineGetLast(&reopened) &&
                    memcmp(&reopened.timeline.tracks[
                               reopened.progress_track_index],
                           &moved,
                           sizeof(TimelineTrack)) == 0);
        document.timeline.tracks[document.progress_track_index] = moved;
    }
    encoded = RuntimeSceneLightTimelineToJsonObject(&document, 1.0);
    assert_true("light_cubic_encode", encoded != NULL);
    wrapper = json_object_new_object();
    json_object_object_add(wrapper, "light_timeline", encoded);
    assert_true("light_cubic_reopen",
                RuntimeSceneLightTimelineParseAuthoring(
                    wrapper, 1.0, &reopened, diagnostics,
                    sizeof(diagnostics)) == TIMELINE_STATUS_OK);
    assert_true("light_cubic_reopen_exact_track",
                memcmp(&document.timeline.tracks[0],
                       &reopened.timeline.tracks[0],
                       sizeof(TimelineTrack)) == 0);
    json_object_put(wrapper);
    {
        json_object* invalid_encoded = NULL;
        document.timeline.tracks[document.progress_track_index] = invalid;
        assert_true("light_cubic_invalid_encode_refused",
                    RuntimeSceneLightTimelineToJsonObject(
                        &document, 1.0) == NULL);
        document.timeline.tracks[document.progress_track_index] = moved;
        invalid_encoded = RuntimeSceneLightTimelineToJsonObject(
            &document, 1.0);
        assert_true("light_cubic_valid_reencode",
                    invalid_encoded != NULL);
        if (invalid_encoded) {
            json_object* tracks = NULL;
            json_object* track_object = NULL;
            json_object* keys = NULL;
            json_object* key = NULL;
            json_object* outgoing = NULL;
            json_object_object_get_ex(
                invalid_encoded, "tracks", &tracks);
            track_object = json_object_array_get_idx(tracks, 0u);
            json_object_object_get_ex(track_object, "keys", &keys);
            key = json_object_array_get_idx(keys, 0u);
            json_object_object_get_ex(key, "outgoing_handle", &outgoing);
            json_object_object_add(
                outgoing, "value_offset", json_object_new_double(0.8));
            wrapper = json_object_new_object();
            json_object_object_add(
                wrapper, "light_timeline", invalid_encoded);
            memset(diagnostics, 0, sizeof(diagnostics));
            assert_true("light_cubic_invalid_parse_refused",
                        RuntimeSceneLightTimelineParseAuthoring(
                            wrapper, 1.0, &reopened, diagnostics,
                            sizeof(diagnostics)) ==
                            TIMELINE_STATUS_INVALID_TRACK);
            assert_true("light_cubic_invalid_parse_diagnostic",
                        strcmp(diagnostics,
                               "invalid_path_progress_track") == 0);
            json_object_put(wrapper);
        }
    }
    return 0;
}

int run_test_runtime_timeline_light_persistence_tests(void) {
    test_light_timeline_responsive_geometry();
    test_light_timeline_roundtrip_and_evaluation();
    test_light_timeline_legacy_path_and_transactional_refusal();
    test_light_timeline_schema_v2_multitrack_contract();
    test_light_timeline_intensity_evaluated_scene_parity();
    test_light_timeline_runtime_bridge_headless_inspection();
    test_short_default_timeline_expands_for_authoring();
    test_new_timeline_keys_are_independent_of_path_points();
    test_light_timeline_stable_selection_identity();
    test_light_timeline_cubic_operability_and_validity();
    test_light_timeline_ui_acceptance_roundtrip();
    return test_support_failures();
}
