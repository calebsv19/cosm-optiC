#include "test_runtime_evaluated_scene_preview.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "animation/evaluated_scene_snapshot.h"
#include "animation/timeline_property_registry.h"
#include "app/evaluated_scene_service.h"
#include "app/preview_retained_scene_quality.h"
#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_light_timeline_io.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_native_3d_render_request_snapshot.h"
#include "test_support.h"

static TimelineTrack evaluated_progress_track(bool equal_segment_time) {
    TimelineTrack track;
    memset(&track, 0, sizeof(track));
    assert_true("evaluated_track_init",
                TimelineTrackInit(&track, "evaluated_progress", "light/key",
                                  "light/path_progress",
                                  TIMELINE_VALUE_SCALAR) == TIMELINE_STATUS_OK);
    assert_true("evaluated_track_unit",
                TimelineTrackSetUnit(&track, TIMELINE_UNIT_UNITLESS) ==
                    TIMELINE_STATUS_OK);
    assert_true("evaluated_track_start",
                TimelineTrackAddKey(&track, 0, TimelineValueScalar(0.0),
                                    TIMELINE_INTERPOLATION_LINEAR) ==
                    TIMELINE_STATUS_OK);
    if (equal_segment_time) {
        assert_true("evaluated_track_middle",
                    TimelineTrackAddKey(&track, 10, TimelineValueScalar(0.1),
                                        TIMELINE_INTERPOLATION_LINEAR) ==
                        TIMELINE_STATUS_OK);
    }
    assert_true("evaluated_track_end",
                TimelineTrackAddKey(&track, 20, TimelineValueScalar(1.0),
                                    TIMELINE_INTERPOLATION_STEP) ==
                    TIMELINE_STATUS_OK);
    return track;
}

static void evaluated_unequal_path(Path* path, CameraPath3D* path3d) {
    memset(path, 0, sizeof(*path));
    CameraPath3D_Reset(path3d);
    path->mode = BEZIER_CUBIC;
    path->numPoints = 3;
    path->points[0] = (Point){0.0, 0.0};
    path->points[1] = (Point){1.0, 0.0};
    path->points[2] = (Point){10.0, 0.0};
    path->handles[0][0] = (Velocity){1.0 / 3.0, 0.0};
    path->handles[0][1] = (Velocity){-1.0 / 3.0, 0.0};
    path->handles[1][0] = (Velocity){3.0, 0.0};
    path->handles[1][1] = (Velocity){-3.0, 0.0};
}

static TimelineEvaluationContext evaluated_context(int64_t frame) {
    TimelineEvaluationContext context;
    memset(&context, 0, sizeof(context));
    assert_true("evaluated_context_build",
                TimelineEvaluationContextBuild(
                    (TimelineRate){20u, 1u},
                    (TimelineRange){0, 21u},
                    (TimelineSample){frame, 0u, 1u},
                    &context) == TIMELINE_STATUS_OK);
    return context;
}

static int test_evaluated_elapsed_frame_mapping(void) {
    TimelineRate rate = {10u, 1u};
    TimelineRange range = {5, 4u};
    TimelineSample sample = {0};
    bool reverse = false;
    bool clamped = false;
    assert_true("evaluated_stop_fraction",
                RayEvaluatedTimelineSampleFromElapsed(
                    rate, range, 0.15, RAY_EVALUATED_PLAYBACK_STOP,
                    &sample, &reverse, &clamped) == TIMELINE_STATUS_OK);
    assert_true("evaluated_stop_frame", sample.absolute_frame == 6);
    assert_true("evaluated_stop_subframe",
                sample.subframe_numerator == 500000u &&
                sample.subframe_denominator == 1000000u);
    assert_true("evaluated_stop_direction", !reverse && !clamped);

    assert_true("evaluated_loop_fraction",
                RayEvaluatedTimelineSampleFromElapsed(
                    rate, range, 0.45, RAY_EVALUATED_PLAYBACK_LOOP,
                    &sample, &reverse, &clamped) == TIMELINE_STATUS_OK);
    assert_true("evaluated_loop_frame", sample.absolute_frame == 5);
    assert_true("evaluated_loop_subframe", sample.subframe_numerator == 500000u);

    assert_true("evaluated_bounce_fraction",
                RayEvaluatedTimelineSampleFromElapsed(
                    rate, range, 0.45, RAY_EVALUATED_PLAYBACK_BOUNCE,
                    &sample, &reverse, &clamped) == TIMELINE_STATUS_OK);
    assert_true("evaluated_bounce_frame", sample.absolute_frame == 6);
    assert_true("evaluated_bounce_reverse", reverse && !clamped);

    assert_true("evaluated_stop_clamp",
                RayEvaluatedTimelineSampleFromElapsed(
                    rate, range, 1.0, RAY_EVALUATED_PLAYBACK_STOP,
                    &sample, &reverse, &clamped) == TIMELINE_STATUS_OK);
    assert_true("evaluated_stop_last_frame",
                sample.absolute_frame == 8 && sample.subframe_numerator == 0u);
    assert_true("evaluated_stop_clamped", clamped);
    return 0;
}

static int test_evaluated_snapshot_detachment(void) {
    RayEvaluatedSceneSnapshotInputs inputs;
    RayEvaluatedSceneSnapshot snapshot;
    memset(&inputs, 0, sizeof(inputs));
    inputs.source = RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE;
    inputs.frame = evaluated_context(5);
    inputs.light.valid = true;
    inputs.light.enabled = true;
    inputs.light.position = (TimelineVec3){1.0, 2.0, 3.0};
    inputs.light.property_provenance.valid = true;
    inputs.camera.valid = true;
    inputs.camera.position = (TimelineVec3){4.0, 5.0, 6.0};
    inputs.camera.zoom = 1.0;
    inputs.simulation.source = RAY_EVALUATED_SIMULATION_NONE;
    inputs.simulation.valid = false;
    inputs.invalidation_domains = TIMELINE_INVALIDATION_LIGHTING;
    inputs.diagnostics = "detachment proof";
    assert_true("evaluated_snapshot_build",
                RayEvaluatedSceneSnapshotBuild(&inputs, &snapshot) ==
                    TIMELINE_STATUS_OK);
    inputs.light.position.x = 99.0;
    inputs.camera.position.z = 88.0;
    assert_close("evaluated_snapshot_light_detached",
                 snapshot.light.position.x, 1.0, 1e-12);
    assert_close("evaluated_snapshot_camera_detached",
                 snapshot.camera.position.z, 6.0, 1e-12);
    assert_true("evaluated_snapshot_sim_none",
                snapshot.simulation.source == RAY_EVALUATED_SIMULATION_NONE &&
                !snapshot.simulation.valid);
    return 0;
}

static int test_preview_quality_preserves_evaluated_snapshot(void) {
    RayEvaluatedSceneSnapshotInputs inputs;
    RayEvaluatedSceneSnapshot snapshot;
    PreviewRetainedSceneFrame solid;
    PreviewRetainedSceneFrame shaded;
    memset(&inputs, 0, sizeof(inputs));
    inputs.source = RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE;
    inputs.playback_mode = RAY_EVALUATED_PLAYBACK_BOUNCE;
    inputs.reverse_direction = true;
    inputs.frame = evaluated_context(7);
    inputs.identity.scene_revision = 41u;
    inputs.identity.timeline_revision = 73u;
    inputs.light.valid = true;
    inputs.light.enabled = true;
    inputs.light.position = (TimelineVec3){1.0, 2.0, 8.0};
    inputs.light.color = (TimelineVec3){1.0, 0.5, 0.25};
    inputs.light.intensity = 4.0;
    inputs.light.property_provenance.valid = true;
    inputs.light.property_provenance.left_frame = 5;
    inputs.light.property_provenance.right_frame = 10;
    inputs.camera.valid = true;
    inputs.camera.position = (TimelineVec3){4.0, 5.0, 6.0};
    inputs.camera.zoom = 1.0;
    inputs.simulation.source = RAY_EVALUATED_SIMULATION_CACHE;
    inputs.simulation.valid = true;
    snprintf(inputs.simulation.cache_id,
             sizeof(inputs.simulation.cache_id),
             "preview-cache-proof");
    inputs.simulation.cache_revision = 9u;
    inputs.simulation.frame_index = 7;
    inputs.invalidation_domains =
        TIMELINE_INVALIDATION_LIGHTING | TIMELINE_INVALIDATION_CAMERA;
    inputs.diagnostics = "preview quality snapshot parity";
    assert_true("preview_quality_snapshot_fixture",
                RayEvaluatedSceneSnapshotBuild(&inputs, &snapshot) ==
                    TIMELINE_STATUS_OK);
    assert_true("preview_quality_solid_frame",
                PreviewRetainedSceneFrameBuild(
                    PREVIEW_RETAINED_SCENE_QUALITY_SOLID,
                    &snapshot,
                    &solid));
    assert_true("preview_quality_shaded_frame",
                PreviewRetainedSceneFrameBuild(
                    PREVIEW_RETAINED_SCENE_QUALITY_INTERACTIVE_SHADED,
                    &snapshot,
                    &shaded));
    assert_true("preview_quality_snapshot_byte_parity_solid",
                memcmp(&solid.evaluated_scene,
                       &snapshot,
                       sizeof(snapshot)) == 0);
    assert_true("preview_quality_snapshot_byte_parity_shaded",
                memcmp(&shaded.evaluated_scene,
                       &snapshot,
                       sizeof(snapshot)) == 0);
    assert_true("preview_quality_only_changes_presentation",
                solid.quality != shaded.quality &&
                memcmp(&solid.evaluated_scene,
                       &shaded.evaluated_scene,
                       sizeof(snapshot)) == 0);
    assert_true("preview_quality_simulation_identity_preserved",
                shaded.evaluated_scene.simulation.valid &&
                shaded.evaluated_scene.simulation.cache_revision == 9u &&
                shaded.evaluated_scene.simulation.frame_index == 7);
    return 0;
}

static int test_preview_quality_light_first_shading(void) {
    RayEvaluatedSceneSnapshotInputs inputs;
    RayEvaluatedSceneSnapshot snapshot;
    PreviewRetainedSceneFrame solid;
    PreviewRetainedSceneFrame shaded;
    SDL_Color base = {200, 180, 160, 255};
    SDL_Color solid_color;
    SDL_Color shaded_color;
    SDL_Color moved_light_color;
    memset(&inputs, 0, sizeof(inputs));
    inputs.source = RAY_EVALUATED_SCENE_SOURCE_AUTHORED_TIMELINE;
    inputs.frame = evaluated_context(8);
    inputs.light.valid = true;
    inputs.light.enabled = true;
    inputs.light.position = (TimelineVec3){0.0, 0.0, 5.0};
    inputs.light.color = (TimelineVec3){1.0, 1.0, 1.0};
    inputs.light.intensity = 8.0;
    inputs.light.property_provenance.valid = true;
    inputs.light.property_provenance.left_frame = 8;
    inputs.light.property_provenance.right_frame = 8;
    inputs.camera.valid = true;
    inputs.camera.zoom = 1.0;
    inputs.diagnostics = "preview light-first shading";
    assert_true("preview_shading_snapshot_fixture",
                RayEvaluatedSceneSnapshotBuild(&inputs, &snapshot) ==
                    TIMELINE_STATUS_OK);
    assert_true("preview_shading_solid_frame",
                PreviewRetainedSceneFrameBuild(
                    PREVIEW_RETAINED_SCENE_QUALITY_SOLID,
                    &snapshot,
                    &solid));
    assert_true("preview_shading_interactive_frame",
                PreviewRetainedSceneFrameBuild(
                    PREVIEW_RETAINED_SCENE_QUALITY_INTERACTIVE_SHADED,
                    &snapshot,
                    &shaded));
    solid_color = PreviewRetainedSceneShadeColor(
        base, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, &solid);
    shaded_color = PreviewRetainedSceneShadeColor(
        base, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, &shaded);
    snapshot.light.position.z = -5.0;
    assert_true("preview_shading_moved_light_frame",
                PreviewRetainedSceneFrameBuild(
                    PREVIEW_RETAINED_SCENE_QUALITY_INTERACTIVE_SHADED,
                    &snapshot,
                    &shaded));
    moved_light_color = PreviewRetainedSceneShadeColor(
        base, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, &shaded);
    assert_true("preview_shading_solid_is_light_independent",
                solid_color.r == 164 && solid_color.g == 148 &&
                solid_color.b == 131);
    assert_true("preview_shading_uses_evaluated_light",
                shaded_color.r > moved_light_color.r &&
                shaded_color.g > moved_light_color.g &&
                shaded_color.b > moved_light_color.b);
    return 0;
}

static int test_evaluated_equal_time_vs_constant_speed(void) {
    const int previous_space_mode = animSettings.spaceMode;
    TimelineTrack constant_speed = evaluated_progress_track(false);
    TimelineTrack equal_segment_time = evaluated_progress_track(true);
    TimelineEvaluationContext context = evaluated_context(5);
    TimelineLightMotionSample constant_motion = {0};
    TimelineLightMotionSample equal_time_motion = {0};
    Path path;
    CameraPath3D path3d;
    evaluated_unequal_path(&path, &path3d);
    animSettings.spaceMode = SPACE_MODE_3D;
    assert_true("evaluated_constant_speed_motion",
                TimelineLightMotionEvaluate(&constant_speed, &path, &path3d,
                                            &context, &constant_motion) ==
                    TIMELINE_STATUS_OK);
    assert_true("evaluated_equal_time_motion",
                TimelineLightMotionEvaluate(&equal_segment_time, &path, &path3d,
                                            &context, &equal_time_motion) ==
                    TIMELINE_STATUS_OK);
    assert_close("evaluated_same_authored_frame",
                 context.absolute_frame_position, 5.0, 1e-12);
    assert_close("evaluated_constant_speed_progress",
                 constant_motion.progress, 0.25, 1e-12);
    assert_close("evaluated_equal_time_progress",
                 equal_time_motion.progress, 0.05, 1e-12);
    assert_true("evaluated_modes_visibly_differ",
                fabs(constant_motion.position.x - equal_time_motion.position.x) >
                    1.0);
    assert_true("evaluated_modes_preserve_provenance",
                strcmp(constant_speed.target_id, equal_segment_time.target_id) == 0 &&
                strcmp(constant_speed.property_id,
                       equal_segment_time.property_id) == 0);
    animSettings.spaceMode = previous_space_mode;
    return 0;
}

static int test_evaluated_explicit_legacy_fallback(void) {
    RuntimeSceneLightTimelineDocument previous_document = {0};
    RayEvaluatedSceneServiceResult result = {0};
    SceneConfig before_scene = sceneSettings;
    AnimationConfig before_animation = animSettings;
    bool had_previous_document =
        RuntimeSceneLightTimelineGetLast(&previous_document);
    RuntimeSceneLightTimelineResetLast();
    assert_true("evaluated_legacy_capture",
                RayEvaluatedSceneCaptureForElapsed(0.25, &result));
    assert_true("evaluated_legacy_source_visible",
                result.snapshot.source ==
                    RAY_EVALUATED_SCENE_SOURCE_LEGACY_PREVIEW_FALLBACK &&
                strstr(result.status_line, "legacy-fallback") != NULL);
    assert_true("evaluated_legacy_scene_nonmutation",
                memcmp(&before_scene, &sceneSettings, sizeof(before_scene)) == 0);
    assert_true("evaluated_legacy_animation_nonmutation",
                memcmp(&before_animation, &animSettings,
                       sizeof(before_animation)) == 0);
    if (had_previous_document) {
        (void)RuntimeSceneLightTimelineSetLast(&previous_document);
    }
    return 0;
}

static int test_evaluated_service_parity_and_nonmutation(void) {
    const char* runtime_path =
        "config/samples/light_timeline_editor_demo_runtime.json";
    SceneConfig saved_scene = sceneSettings;
    AnimationConfig saved_animation = animSettings;
    RuntimeSceneLightTimelineDocument previous_document = {0};
    RuntimeSceneLightTimelineDocument before_document = {0};
    RuntimeSceneLightTimelineDocument after_document = {0};
    RuntimeSceneBridge3DLightSeedState before_lights = {0};
    RuntimeSceneBridge3DLightSeedState after_lights = {0};
    RuntimeSceneBridgePreflight summary = {0};
    TimelineLightMotionSample final_sample = {0};
    RayEvaluatedSceneServiceResult preview_sample = {0};
    RayEvaluatedSceneServiceResult retained_preview_sample = {0};
    RuntimeNative3DPreparedFrame native_frame = {0};
    RuntimeNative3DRenderRequestSnapshot render_snapshot = {0};
    RuntimeNative3DRenderRequestSnapshotDesc render_snapshot_desc = {0};
    RayEvaluatedSceneSnapshot unresolved_snapshot = {0};
    const RuntimeLightSource3D* native_light = NULL;
    SceneConfig before_scene;
    AnimationConfig before_animation;
    bool had_previous_document =
        RuntimeSceneLightTimelineGetLast(&previous_document);

    assert_true("evaluated_fixture_apply",
                runtime_scene_bridge_apply_file(runtime_path, &summary));
    assert_true("evaluated_fixture_timeline",
                RuntimeSceneLightTimelineGetLast(&before_document));
    runtime_scene_bridge_get_last_3d_light_seed_state(&before_lights);
    before_scene = sceneSettings;
    before_animation = animSettings;
    assert_true("evaluated_final_reference",
                RuntimeSceneLightTimelineInspectLast(
                    (TimelineSample){35, 0u, 1u},
                    &final_sample) == TIMELINE_STATUS_OK);
    assert_true("evaluated_preview_capture",
                RayEvaluatedSceneCaptureAuthoredSample(
                    (TimelineSample){35, 0u, 1u}, &preview_sample));
    assert_true("evaluated_retained_preview_capture",
                RayEvaluatedSceneCaptureAuthoredSample(
                    (TimelineSample){35, 0u, 1u}, &retained_preview_sample));
    assert_true("evaluated_preview_snapshot_valid", preview_sample.snapshot.valid);
    assert_close("evaluated_preview_final_progress_parity",
                 preview_sample.snapshot.light.progress,
                 final_sample.progress, 1e-12);
    assert_close("evaluated_preview_final_x_parity",
                 preview_sample.snapshot.light.position.x,
                 final_sample.position.x, 1e-12);
    assert_close("evaluated_preview_final_y_parity",
                 preview_sample.snapshot.light.position.y,
                 final_sample.position.y, 1e-12);
    assert_close("evaluated_preview_final_z_parity",
                 preview_sample.snapshot.light.position.z,
                 final_sample.position.z, 1e-12);
    assert_true("evaluated_preview_provenance",
                preview_sample.snapshot.light.property_provenance.valid &&
                strcmp(preview_sample.snapshot.light.property_provenance.track_id,
                       "timeline_key_progress") == 0);
    assert_true("evaluated_preview_sim_identity_explicit",
                preview_sample.snapshot.simulation.source ==
                    RAY_EVALUATED_SIMULATION_NONE &&
                !preview_sample.snapshot.simulation.valid);
    assert_true("evaluated_ordinary_retained_snapshot_parity",
                memcmp(&preview_sample.snapshot,
                       &retained_preview_sample.snapshot,
                       sizeof(preview_sample.snapshot)) == 0);

    assert_true("evaluated_native_prepare",
                RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(
                    &native_frame,
                    64,
                    64,
                    &preview_sample.snapshot,
                    NULL));
    assert_true("evaluated_native_snapshot_bound",
                native_frame.evaluatedSceneBound);
    assert_true("evaluated_native_snapshot_exact",
                memcmp(&native_frame.evaluatedScene,
                       &preview_sample.snapshot,
                       sizeof(preview_sample.snapshot)) == 0);
    for (int i = 0; i < native_frame.scene.lightSet.lightCount; ++i) {
        if (strcmp(native_frame.scene.lightSet.lights[i].id,
                   preview_sample.snapshot.light.runtime_light_id) == 0) {
            native_light = &native_frame.scene.lightSet.lights[i];
            break;
        }
    }
    assert_true("evaluated_native_light_resolved", native_light != NULL);
    if (native_light) {
        assert_close("evaluated_native_light_x_parity",
                     native_light->position.x,
                     preview_sample.snapshot.light.position.x, 1e-12);
        assert_close("evaluated_native_light_y_parity",
                     native_light->position.y,
                     preview_sample.snapshot.light.position.y, 1e-12);
        assert_close("evaluated_native_light_z_parity",
                     native_light->position.z,
                     preview_sample.snapshot.light.position.z, 1e-12);
    }
    assert_close("evaluated_native_camera_x_parity",
                 native_frame.scene.camera.position.x,
                 preview_sample.snapshot.camera.position.x, 1e-12);
    assert_close("evaluated_native_camera_y_parity",
                 native_frame.scene.camera.position.y,
                 preview_sample.snapshot.camera.position.y, 1e-12);
    assert_close("evaluated_native_camera_z_parity",
                 native_frame.scene.camera.position.z,
                 preview_sample.snapshot.camera.position.z, 1e-12);
    assert_close("evaluated_native_camera_yaw_parity",
                 native_frame.scene.camera.rotation,
                 preview_sample.snapshot.camera.yaw_radians, 1e-12);
    assert_close("evaluated_native_camera_pitch_parity",
                 native_frame.scene.camera.lookPitch,
                 preview_sample.snapshot.camera.pitch_radians, 1e-12);
    assert_close("evaluated_native_camera_zoom_parity",
                 native_frame.scene.camera.zoom,
                 preview_sample.snapshot.camera.zoom, 1e-12);
    render_snapshot_desc.outputWidth = 64;
    render_snapshot_desc.outputHeight = 64;
    render_snapshot_desc.renderWidth = 64;
    render_snapshot_desc.renderHeight = 64;
    render_snapshot_desc.hostWidth = 64;
    render_snapshot_desc.hostHeight = 64;
    render_snapshot_desc.frameIndex =
        (int)preview_sample.snapshot.frame.sample.absolute_frame;
    render_snapshot_desc.frameCount =
        (int)preview_sample.snapshot.frame.range.frame_count;
    render_snapshot_desc.temporalFrames = 1;
    render_snapshot_desc.preparedFrame = &native_frame;
    assert_true("evaluated_render_snapshot_build",
                RuntimeNative3DRenderRequestSnapshot_Build(
                    &render_snapshot, &render_snapshot_desc));
    assert_true("evaluated_render_snapshot_bound",
                render_snapshot.evaluatedSceneBound);
    assert_true("evaluated_render_snapshot_exact",
                memcmp(&render_snapshot.evaluatedScene,
                       &preview_sample.snapshot,
                       sizeof(preview_sample.snapshot)) == 0);

    unresolved_snapshot = preview_sample.snapshot;
    snprintf(unresolved_snapshot.light.runtime_light_id,
             sizeof(unresolved_snapshot.light.runtime_light_id),
             "%s",
             "missing-authored-light");
    {
        RuntimeNative3DPreparedFrame unresolved_frame = {0};
        assert_true("evaluated_authored_light_id_fail_closed",
                    !RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(
                        &unresolved_frame,
                        64,
                        64,
                        &unresolved_snapshot,
                        NULL));
        RuntimeNative3DPreparedFrame_Free(&unresolved_frame);
    }
    assert_close("evaluated_headless_preflight_progress_parity",
                 final_sample.progress,
                 preview_sample.snapshot.light.progress, 1e-12);

    runtime_scene_bridge_get_last_3d_light_seed_state(&after_lights);
    assert_true("evaluated_preview_scene_nonmutation",
                memcmp(&before_scene, &sceneSettings, sizeof(before_scene)) == 0);
    assert_true("evaluated_preview_animation_nonmutation",
                memcmp(&before_animation, &animSettings,
                       sizeof(before_animation)) == 0);
    assert_true("evaluated_preview_light_seed_nonmutation",
                memcmp(&before_lights, &after_lights, sizeof(before_lights)) == 0);
    assert_true("evaluated_preview_timeline_nonmutation",
                RuntimeSceneLightTimelineGetLast(&after_document) &&
                memcmp(&before_document, &after_document,
                       sizeof(before_document)) == 0);

    sceneSettings = saved_scene;
    animSettings = saved_animation;
    if (had_previous_document) {
        (void)RuntimeSceneLightTimelineSetLast(&previous_document);
    } else {
        RuntimeSceneLightTimelineResetLast();
    }
    RuntimeNative3DPreparedFrame_Free(&native_frame);
    return 0;
}

int run_test_runtime_evaluated_scene_preview_tests(void) {
    test_evaluated_elapsed_frame_mapping();
    test_evaluated_snapshot_detachment();
    test_preview_quality_preserves_evaluated_snapshot();
    test_preview_quality_light_first_shading();
    test_evaluated_equal_time_vs_constant_speed();
    test_evaluated_explicit_legacy_fallback();
    test_evaluated_service_parity_and_nonmutation();
    return test_support_failures();
}
