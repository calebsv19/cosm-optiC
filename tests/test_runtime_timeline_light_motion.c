#include "test_runtime_timeline_light_motion.h"

#include <stdio.h>
#include <string.h>

#include "animation/timeline_light_motion.h"
#include "animation/timeline_property_registry.h"
#include "config/config_manager.h"
#include "import/runtime_scene_light_timeline_bridge.h"
#include "test_support.h"

static TimelineEvaluationContext light_context(int64_t frame) {
    TimelineEvaluationContext context;
    TimelineRate rate = {20u, 1u};
    TimelineRange range = {0, 21u};
    TimelineSample sample = {frame, 0u, 1u};
    memset(&context, 0, sizeof(context));
    assert_true("light_timeline_context",
                TimelineEvaluationContextBuild(rate, range, sample, &context) ==
                    TIMELINE_STATUS_OK);
    return context;
}

static TimelineTrack light_progress_track(TimelineInterpolation interpolation) {
    TimelineTrack track;
    memset(&track, 0, sizeof(track));
    assert_true("light_progress_init",
                TimelineTrackInit(&track, "track_light_progress", "light/key",
                                  "light/path_progress", TIMELINE_VALUE_SCALAR) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_progress_unit",
                TimelineTrackSetUnit(&track, TIMELINE_UNIT_UNITLESS) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_progress_start",
                TimelineTrackAddKey(&track, 0, TimelineValueScalar(0.0),
                                    interpolation) == TIMELINE_STATUS_OK);
    assert_true("light_progress_end",
                TimelineTrackAddKey(&track, 20, TimelineValueScalar(1.0),
                                    TIMELINE_INTERPOLATION_STEP) ==
                    TIMELINE_STATUS_OK);
    return track;
}

static void light_straight_path(Path* path, CameraPath3D* path3d) {
    memset(path, 0, sizeof(*path));
    CameraPath3D_Reset(path3d);
    path->mode = BEZIER_CUBIC;
    path->numPoints = 2;
    path->points[0] = (Point){0.0, 0.0};
    path->points[1] = (Point){3.0, 0.0};
    path->handles[0][0] = (Velocity){1.0, 0.0};
    path->handles[0][1] = (Velocity){-1.0, 0.0};
    path3d->point_z[0] = 0.0;
    path3d->point_z[1] = 4.0;
    path3d->handles_vz[0][0] = 4.0 / 3.0;
    path3d->handles_vz[0][1] = -4.0 / 3.0;
}

static int test_light_timeline_linear_progress_and_speed(void) {
    const int previous_space_mode = animSettings.spaceMode;
    TimelineTrack track = light_progress_track(TIMELINE_INTERPOLATION_LINEAR);
    TimelineEvaluationContext context = light_context(5);
    TimelineLightMotionSample motion;
    Path path;
    CameraPath3D path3d;
    memset(&motion, 0, sizeof(motion));
    animSettings.spaceMode = SPACE_MODE_3D;
    light_straight_path(&path, &path3d);
    assert_true("light_motion_evaluate",
                TimelineLightMotionEvaluate(&track, &path, &path3d, &context,
                                            &motion) == TIMELINE_STATUS_OK);
    assert_true("light_motion_valid", motion.valid);
    assert_true("light_motion_speed_valid", motion.speed_valid);
    assert_close("light_motion_progress", motion.progress, 0.25, 1e-12);
    assert_close("light_motion_length", motion.path_length_world, 5.0, 1e-6);
    assert_close("light_motion_position_x", motion.position.x, 0.75, 1e-5);
    assert_close("light_motion_position_z", motion.position.z, 1.0, 1e-5);
    assert_close("light_motion_speed", motion.world_speed_per_second, 5.0,
                 1e-6);
    assert_true("light_motion_lighting_invalidation",
                motion.invalidation_domains == TIMELINE_INVALIDATION_LIGHTING);
    animSettings.spaceMode = previous_space_mode;
    return 0;
}

static int test_light_timeline_cubic_ease_and_refusals(void) {
    const int previous_space_mode = animSettings.spaceMode;
    TimelineTrack track = light_progress_track(
        TIMELINE_INTERPOLATION_CUBIC_BEZIER);
    TimelineEvaluationContext context = light_context(5);
    TimelineLightMotionSample motion;
    TimelineLightMotionSample sentinel;
    TimelineLightMotionSample original;
    Path path;
    CameraPath3D path3d;
    animSettings.spaceMode = SPACE_MODE_3D;
    light_straight_path(&path, &path3d);
    assert_true("light_cubic_left_handles",
                TimelineTrackSetScalarTemporalHandles(&track, 0u, 0.0, 0.0,
                                                      4.0, 0.0) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_cubic_right_handles",
                TimelineTrackSetScalarTemporalHandles(&track, 1u, -4.0, 0.0,
                                                      0.0, 0.0) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_cubic_evaluate",
                TimelineLightMotionEvaluate(&track, &path, &path3d, &context,
                                            &motion) == TIMELINE_STATUS_OK);
    assert_true("light_cubic_ease_progress", motion.progress < 0.25);
    memset(&sentinel, 0x5a, sizeof(sentinel));
    original = sentinel;
    snprintf(track.target_id, sizeof(track.target_id), "%s", "object/key");
    assert_true("light_wrong_target_refused",
                TimelineLightMotionEvaluate(&track, &path, &path3d, &context,
                                            &sentinel) ==
                    TIMELINE_STATUS_TARGET_KIND_MISMATCH);
    assert_true("light_wrong_target_nonmutation",
                memcmp(&sentinel, &original, sizeof(sentinel)) == 0);
    animSettings.spaceMode = previous_space_mode;
    return 0;
}

static int test_light_target_resolve_and_apply(void) {
    RuntimeLightSource3D lights[2];
    RuntimeLightSource3D original[2];
    RuntimeSceneLightTimelineTarget target;
    TimelineLightMotionSample motion;
    memset(lights, 0, sizeof(lights));
    RuntimeLightSource3D_Init(&lights[0]);
    RuntimeLightSource3D_Init(&lights[1]);
    snprintf(lights[0].id, sizeof(lights[0].id), "%s", "key");
    snprintf(lights[1].id, sizeof(lights[1].id), "%s", "fill");
    assert_true("light_target_resolve",
                RuntimeSceneLightTimelineResolveTarget(lights, 2u, "light/key",
                                                       &target) ==
                    TIMELINE_STATUS_OK);
    assert_true("light_target_index", target.light_index == 0u);
    memset(&motion, 0, sizeof(motion));
    motion.valid = true;
    snprintf(motion.target_id, sizeof(motion.target_id), "%s", "light/key");
    motion.position = (TimelineVec3){2.0, 3.0, 4.0};
    assert_true("light_target_apply",
                RuntimeSceneLightTimelineApplyMotion(lights, 2u, &motion,
                                                     &target) ==
                    TIMELINE_STATUS_OK);
    assert_close("light_target_position_x", lights[0].position.x, 2.0, 1e-12);
    assert_close("light_target_other_unchanged", lights[1].position.x, 0.0,
                 1e-12);
    memcpy(original, lights, sizeof(lights));
    snprintf(lights[1].id, sizeof(lights[1].id), "%s", "key");
    assert_true("light_target_duplicate_refused",
                RuntimeSceneLightTimelineApplyMotion(lights, 2u, &motion,
                                                     &target) ==
                    TIMELINE_STATUS_DUPLICATE_ID);
    assert_close("light_target_duplicate_nonmutation", lights[0].position.x,
                 original[0].position.x, 1e-12);
    return 0;
}

int run_test_runtime_timeline_light_motion_tests(void) {
    test_light_timeline_linear_progress_and_speed();
    test_light_timeline_cubic_ease_and_refusals();
    test_light_target_resolve_and_apply();
    return test_support_failures();
}
