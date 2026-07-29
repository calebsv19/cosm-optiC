#include "test_runtime_timeline_foundation.h"

#include <limits.h>
#include <string.h>

#include "animation/timeline_document.h"
#include "test_support.h"

static TimelineRate make_rate(uint32_t numerator, uint32_t denominator) {
    TimelineRate value = {numerator, denominator};
    return value;
}

static TimelineRange make_range(int64_t start, uint64_t count) {
    TimelineRange value = {start, count};
    return value;
}

static TimelineSample make_sample(int64_t frame, uint32_t numerator,
                                  uint32_t denominator) {
    TimelineSample value = {frame, numerator, denominator};
    return value;
}

static TimelineEvaluationContext context_at(TimelineRate timeline_rate,
                                            TimelineRange timeline_range,
                                            int64_t frame,
                                            uint32_t sub_num,
                                            uint32_t sub_den) {
    TimelineEvaluationContext context;
    memset(&context, 0, sizeof(context));
    assert_true("timeline_context_build",
                TimelineEvaluationContextBuild(
                    timeline_rate, timeline_range,
                    make_sample(frame, sub_num, sub_den), &context) ==
                    TIMELINE_STATUS_OK);
    return context;
}

static int test_timeline_clock_exact_frames(void) {
    TimelineEvaluationContext first =
        context_at(make_rate(20u, 1u), make_range(0, 100u), 0, 0u, 1u);
    TimelineEvaluationContext last =
        context_at(make_rate(20u, 1u), make_range(0, 100u), 99, 0u, 1u);
    assert_true("timeline_first_local_frame", first.local_frame == 0u);
    assert_close("timeline_100_frames_20fps_duration", first.duration_seconds,
                 5.0, 1e-12);
    assert_close("timeline_first_normalized", first.normalized_t, 0.0, 1e-12);
    assert_true("timeline_last_local_frame", last.local_frame == 99u);
    assert_close("timeline_last_sample_seconds", last.local_time_seconds,
                 4.95, 1e-12);
    assert_close("timeline_last_normalized", last.normalized_t, 1.0, 1e-12);
    return 0;
}

static int test_timeline_clock_offsets_subframes_and_chunks(void) {
    TimelineRate timeline_rate = make_rate(24u, 1u);
    TimelineRange timeline_range = make_range(100, 48u);
    TimelineEvaluationContext direct =
        context_at(timeline_rate, timeline_range, 112, 1u, 2u);
    TimelineEvaluationContext chunked;
    memset(&chunked, 0, sizeof(chunked));
    assert_true("timeline_chunk_context_build",
                TimelineEvaluationContextBuildFromChunk(
                    timeline_rate, timeline_range, 108, 4u, 1u, 2u,
                    &chunked) == TIMELINE_STATUS_OK);
    assert_true("timeline_chunk_same_sample",
                TimelineEvaluationContextsReferToSameSample(&direct, &chunked));
    assert_true("timeline_offset_local_frame", direct.local_frame == 12u);
    assert_close("timeline_subframe", direct.subframe, 0.5, 1e-12);
    assert_close("timeline_offset_local_seconds", direct.local_time_seconds,
                 12.5 / 24.0, 1e-12);
    assert_close("timeline_absolute_seconds", direct.absolute_time_seconds,
                 112.5 / 24.0, 1e-12);
    return 0;
}

static int test_timeline_clock_invalid_inputs(void) {
    TimelineEvaluationContext sentinel;
    TimelineEvaluationContext original;
    int64_t end_frame = 0;
    memset(&sentinel, 0x5a, sizeof(sentinel));
    original = sentinel;
    assert_true("timeline_invalid_rate",
                TimelineEvaluationContextBuild(
                    make_rate(0u, 1u), make_range(0, 10u),
                    make_sample(0, 0u, 1u), &sentinel) ==
                    TIMELINE_STATUS_INVALID_RATE);
    assert_true("timeline_invalid_rate_no_mutation",
                memcmp(&sentinel, &original, sizeof(sentinel)) == 0);
    assert_true("timeline_invalid_empty_range",
                TimelineEvaluationContextBuild(
                    make_rate(24u, 1u), make_range(0, 0u),
                    make_sample(0, 0u, 1u), &sentinel) ==
                    TIMELINE_STATUS_INVALID_RANGE);
    assert_true("timeline_invalid_subframe",
                TimelineEvaluationContextBuild(
                    make_rate(24u, 1u), make_range(0, 10u),
                    make_sample(0, 1u, 1u), &sentinel) ==
                    TIMELINE_STATUS_INVALID_ARGUMENT);
    assert_true("timeline_frame_out_of_range",
                TimelineEvaluationContextBuild(
                    make_rate(24u, 1u), make_range(10, 10u),
                    make_sample(9, 0u, 1u), &sentinel) ==
                    TIMELINE_STATUS_FRAME_OUT_OF_RANGE);
    assert_true("timeline_range_overflow",
                TimelineRangeEndFrame(make_range(INT64_MAX - 1, 3u),
                                      &end_frame) ==
                    TIMELINE_STATUS_ARITHMETIC_OVERFLOW);
    assert_true("timeline_chunk_overflow",
                TimelineEvaluationContextBuildFromChunk(
                    make_rate(24u, 1u), make_range(0, 10u), INT64_MAX, 1u,
                    0u, 1u, &sentinel) == TIMELINE_STATUS_ARITHMETIC_OVERFLOW);
    return 0;
}

static TimelineTrack make_scalar_track(
    const char* track_id, const char* target_id, const char* property_id,
    int64_t start_frame, double start_value,
    TimelineInterpolation interpolation, int64_t end_frame, double end_value) {
    TimelineTrack track;
    memset(&track, 0, sizeof(track));
    assert_true("timeline_scalar_track_init",
                TimelineTrackInit(&track, track_id, target_id, property_id,
                                  TIMELINE_VALUE_SCALAR) == TIMELINE_STATUS_OK);
    assert_true("timeline_scalar_key_start",
                TimelineTrackAddKey(&track, start_frame,
                                    TimelineValueScalar(start_value),
                                    interpolation) == TIMELINE_STATUS_OK);
    assert_true("timeline_scalar_key_end",
                TimelineTrackAddKey(&track, end_frame,
                                    TimelineValueScalar(end_value),
                                    TIMELINE_INTERPOLATION_STEP) ==
                    TIMELINE_STATUS_OK);
    return track;
}

static int test_timeline_track_scalar_step_and_linear(void) {
    TimelineRange timeline_range = make_range(10, 21u);
    TimelineEvaluationContext before =
        context_at(make_rate(20u, 1u), timeline_range, 10, 0u, 1u);
    TimelineEvaluationContext exact =
        context_at(make_rate(20u, 1u), timeline_range, 12, 0u, 1u);
    TimelineEvaluationContext middle =
        context_at(make_rate(20u, 1u), timeline_range, 20, 0u, 1u);
    TimelineEvaluationContext subframe =
        context_at(make_rate(20u, 1u), timeline_range, 19, 1u, 2u);
    TimelineEvaluationContext after =
        context_at(make_rate(20u, 1u), timeline_range, 30, 0u, 1u);
    TimelineTrack linear = make_scalar_track(
        "track_light_intensity", "light/key", "light/intensity", 12, 2.0,
        TIMELINE_INTERPOLATION_LINEAR, 28, 10.0);
    TimelineTrack step = make_scalar_track(
        "track_visibility", "object/cube", "object/visibility", 12, 0.0,
        TIMELINE_INTERPOLATION_STEP, 28, 1.0);
    TimelineEvaluationResult result;

    assert_true("timeline_linear_before_first",
                TimelineTrackEvaluate(&linear, &before, &result) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_linear_before_held", result.held);
    assert_close("timeline_linear_before_value", result.value.as.scalar,
                 2.0, 1e-12);
    assert_true("timeline_linear_exact_key",
                TimelineTrackEvaluate(&linear, &exact, &result) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_linear_exact_key_flag", result.exact_key);
    assert_true("timeline_linear_exact_key_not_held", !result.held);
    assert_close("timeline_linear_exact_key_value", result.value.as.scalar,
                 2.0, 1e-12);
    assert_true("timeline_linear_middle",
                TimelineTrackEvaluate(&linear, &middle, &result) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_linear_middle_interpolated", result.interpolated);
    assert_close("timeline_linear_middle_value", result.value.as.scalar,
                 6.0, 1e-12);
    assert_true("timeline_linear_subframe",
                TimelineTrackEvaluate(&linear, &subframe, &result) ==
                    TIMELINE_STATUS_OK);
    assert_close("timeline_linear_subframe_value", result.value.as.scalar,
                 5.75, 1e-12);
    assert_true("timeline_linear_after_last",
                TimelineTrackEvaluate(&linear, &after, &result) ==
                    TIMELINE_STATUS_OK);
    assert_close("timeline_linear_after_value", result.value.as.scalar,
                 10.0, 1e-12);
    assert_true("timeline_step_middle",
                TimelineTrackEvaluate(&step, &middle, &result) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_step_held", result.held);
    assert_close("timeline_step_value", result.value.as.scalar, 0.0, 1e-12);
    assert_true("timeline_result_authored",
                result.source == TIMELINE_CHANNEL_SOURCE_AUTHORED);
    return 0;
}

static int test_timeline_track_vec3_and_validation(void) {
    TimelineTrack track;
    TimelineTrack corrupt;
    TimelineEvaluationResult result;
    TimelineRange timeline_range = make_range(0, 11u);
    TimelineEvaluationContext middle =
        context_at(make_rate(10u, 1u), timeline_range, 5, 0u, 1u);
    memset(&track, 0, sizeof(track));
    assert_true("timeline_vec3_track_init",
                TimelineTrackInit(&track, "track_object_position",
                                  "object/cube", "transform/position",
                                  TIMELINE_VALUE_VEC3) == TIMELINE_STATUS_OK);
    assert_true("timeline_vec3_key_start",
                TimelineTrackAddKey(&track, 0,
                                    TimelineValueVec3(0.0, 2.0, 4.0),
                                    TIMELINE_INTERPOLATION_LINEAR) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_vec3_type_mismatch",
                TimelineTrackAddKey(&track, 5, TimelineValueScalar(2.0),
                                    TIMELINE_INTERPOLATION_LINEAR) ==
                    TIMELINE_STATUS_TYPE_MISMATCH);
    assert_true("timeline_vec3_key_end",
                TimelineTrackAddKey(&track, 10,
                                    TimelineValueVec3(10.0, 12.0, 14.0),
                                    TIMELINE_INTERPOLATION_STEP) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_vec3_evaluate",
                TimelineTrackEvaluate(&track, &middle, &result) ==
                    TIMELINE_STATUS_OK);
    assert_close("timeline_vec3_x", result.value.as.vec3.x, 5.0, 1e-12);
    assert_close("timeline_vec3_y", result.value.as.vec3.y, 7.0, 1e-12);
    assert_close("timeline_vec3_z", result.value.as.vec3.z, 9.0, 1e-12);
    assert_true("timeline_duplicate_add_refused",
                TimelineTrackAddKey(&track, 10,
                                    TimelineValueVec3(0.0, 0.0, 0.0),
                                    TIMELINE_INTERPOLATION_STEP) ==
                    TIMELINE_STATUS_DUPLICATE_KEY);
    assert_true("timeline_unsorted_add_refused",
                TimelineTrackAddKey(&track, 9,
                                    TimelineValueVec3(0.0, 0.0, 0.0),
                                    TIMELINE_INTERPOLATION_STEP) ==
                    TIMELINE_STATUS_UNSORTED_KEYS);
    assert_true("timeline_cubic_reserved_refused",
                TimelineTrackAddKey(&track, 11,
                                    TimelineValueVec3(0.0, 0.0, 0.0),
                                    TIMELINE_INTERPOLATION_CUBIC_BEZIER) ==
                    TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION);
    corrupt = track;
    corrupt.keys[1].frame = -1;
    assert_true("timeline_corrupt_range_detected",
                TimelineTrackValidate(&corrupt, &timeline_range) ==
                    TIMELINE_STATUS_FRAME_OUT_OF_RANGE);
    corrupt = track;
    corrupt.keys[1].frame = corrupt.keys[0].frame;
    assert_true("timeline_corrupt_duplicate_detected",
                TimelineTrackValidate(&corrupt, &timeline_range) ==
                    TIMELINE_STATUS_DUPLICATE_KEY);
    return 0;
}

static int test_timeline_track_scalar_cubic_temporal_handles(void) {
    TimelineTrack track = make_scalar_track(
        "track_light_progress", "light/key", "light/path_progress", 0, 0.0,
        TIMELINE_INTERPOLATION_CUBIC_BEZIER, 20, 1.0);
    TimelineRange range = make_range(0, 21u);
    TimelineEvaluationContext middle =
        context_at(make_rate(20u, 1u), range, 10, 0u, 1u);
    TimelineEvaluationContext early =
        context_at(make_rate(20u, 1u), range, 5, 0u, 1u);
    TimelineEvaluationResult result;
    TimelineTrack invalid;

    assert_true("timeline_cubic_left_handles",
                TimelineTrackSetScalarTemporalHandles(&track, 0u, 0.0, 0.0,
                                                      4.0, 0.0) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_cubic_right_handles",
                TimelineTrackSetScalarTemporalHandles(&track, 1u, -4.0, 0.0,
                                                      0.0, 0.0) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_cubic_validate",
                TimelineTrackValidate(&track, &range) == TIMELINE_STATUS_OK);
    assert_true("timeline_cubic_middle",
                TimelineTrackEvaluate(&track, &middle, &result) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_cubic_interpolated", result.interpolated);
    assert_true("timeline_cubic_derivative_valid", result.derivative_valid);
    assert_close("timeline_cubic_middle_value", result.value.as.scalar, 0.5,
                 1e-9);
    assert_true("timeline_cubic_early",
                TimelineTrackEvaluate(&track, &early, &result) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_cubic_ease_in", result.value.as.scalar < 0.25);

    invalid = track;
    invalid.keys[0].outgoing_frame_offset = 15.0;
    invalid.keys[1].incoming_frame_offset = -15.0;
    assert_true("timeline_cubic_crossed_handles_refused",
                TimelineTrackValidate(&invalid, &range) ==
                    TIMELINE_STATUS_INVALID_TRACK);
    assert_true("timeline_cubic_positive_incoming_refused",
                TimelineTrackSetScalarTemporalHandles(&track, 1u, 1.0, 0.0,
                                                      0.0, 0.0) ==
                    TIMELINE_STATUS_INVALID_TRACK);
    return 0;
}

static int test_timeline_document_multitrack_and_capacity(void) {
    TimelineDocument document;
    TimelineDocument duplicate_document;
    TimelineEvaluationContext context =
        context_at(make_rate(20u, 1u), make_range(0, 21u), 10, 0u, 1u);
    TimelineTrack intensity = make_scalar_track(
        "track_intensity", "light/key", "light/intensity", 0, 0.0,
        TIMELINE_INTERPOLATION_LINEAR, 20, 20.0);
    TimelineTrack roughness = make_scalar_track(
        "track_roughness", "material/glass", "material/roughness", 0, 0.1,
        TIMELINE_INTERPOLATION_LINEAR, 20, 0.9);
    TimelineEvaluationResult results[2];
    size_t result_count = 0u;
    assert_true("timeline_document_init",
                TimelineDocumentInit(&document, make_rate(20u, 1u),
                                     make_range(0, 21u)) == TIMELINE_STATUS_OK);
    assert_true("timeline_document_add_intensity",
                TimelineDocumentAddTrack(&document, &intensity) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_document_add_roughness",
                TimelineDocumentAddTrack(&document, &roughness) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_document_evaluate",
                TimelineDocumentEvaluate(&document, &context, results, 2u,
                                         &result_count) == TIMELINE_STATUS_OK);
    assert_true("timeline_document_result_count", result_count == 2u);
    assert_true("timeline_document_independent_ids",
                strcmp(results[0].track_id, results[1].track_id) != 0);
    assert_close("timeline_document_intensity", results[0].value.as.scalar,
                 10.0, 1e-12);
    assert_close("timeline_document_roughness", results[1].value.as.scalar,
                 0.5, 1e-12);
    assert_true("timeline_document_result_capacity",
                TimelineDocumentEvaluate(&document, &context, results, 1u,
                                         &result_count) ==
                    TIMELINE_STATUS_CAPACITY_EXCEEDED);
    duplicate_document = document;
    duplicate_document.tracks[1] = duplicate_document.tracks[0];
    assert_true("timeline_document_duplicate_id",
                TimelineDocumentValidate(&duplicate_document) ==
                    TIMELINE_STATUS_DUPLICATE_ID);
    return 0;
}

static int test_timeline_frame_rate_equivalence(void) {
    TimelineTrack track_20 = make_scalar_track(
        "track_20", "object/cube", "weight", 0, 0.0,
        TIMELINE_INTERPOLATION_LINEAR, 20, 1.0);
    TimelineTrack track_40 = make_scalar_track(
        "track_40", "object/cube", "weight", 0, 0.0,
        TIMELINE_INTERPOLATION_LINEAR, 40, 1.0);
    TimelineEvaluationContext context_20 =
        context_at(make_rate(20u, 1u), make_range(0, 21u), 10, 0u, 1u);
    TimelineEvaluationContext context_40 =
        context_at(make_rate(40u, 1u), make_range(0, 41u), 20, 0u, 1u);
    TimelineEvaluationResult result_20;
    TimelineEvaluationResult result_40;
    assert_true("timeline_rate_20_evaluate",
                TimelineTrackEvaluate(&track_20, &context_20, &result_20) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_rate_40_evaluate",
                TimelineTrackEvaluate(&track_40, &context_40, &result_40) ==
                    TIMELINE_STATUS_OK);
    assert_close("timeline_rate_same_seconds", context_20.local_time_seconds,
                 context_40.local_time_seconds, 1e-12);
    assert_close("timeline_rate_same_value", result_20.value.as.scalar,
                 result_40.value.as.scalar, 1e-12);
    return 0;
}

static int test_timeline_track_transactional_key_mutation(void) {
    TimelineTrack track = make_scalar_track(
        "track_edit", "light/key", "light/path_progress", 0, 0.0,
        TIMELINE_INTERPOLATION_LINEAR, 20, 1.0);
    TimelineTrack original;
    TimelineKeyframe key;
    size_t index = 99u;
    memset(&key, 0, sizeof(key));
    key.frame = 10;
    key.value = TimelineValueScalar(0.4);
    key.interpolation_to_next = TIMELINE_INTERPOLATION_CUBIC_BEZIER;
    assert_true("timeline_insert_middle",
                TimelineTrackInsertKey(&track, key, &index) == TIMELINE_STATUS_OK);
    assert_true("timeline_insert_index", index == 1u && track.key_count == 3u);
    assert_true("timeline_insert_sorted", track.keys[1].frame == 10);
    original = track;
    assert_true("timeline_move_cross_refused",
                TimelineTrackMoveScalarKey(&track, 1u, 20, 0.5) ==
                    TIMELINE_STATUS_UNSORTED_KEYS);
    assert_true("timeline_move_cross_nonmutation",
                memcmp(&track, &original, sizeof(track)) == 0);
    assert_true("timeline_move_valid",
                TimelineTrackMoveScalarKey(&track, 1u, 12, 0.6) ==
                    TIMELINE_STATUS_OK);
    assert_true("timeline_move_value",
                track.keys[1].frame == 12 && track.keys[1].value.as.scalar == 0.6);
    assert_true("timeline_remove_middle",
                TimelineTrackRemoveKey(&track, 1u) == TIMELINE_STATUS_OK);
    assert_true("timeline_remove_restores_count",
                track.key_count == 2u && track.keys[1].frame == 20);
    return 0;
}

int run_test_runtime_timeline_foundation_tests(void) {
    test_timeline_clock_exact_frames();
    test_timeline_clock_offsets_subframes_and_chunks();
    test_timeline_clock_invalid_inputs();
    test_timeline_track_scalar_step_and_linear();
    test_timeline_track_vec3_and_validation();
    test_timeline_track_scalar_cubic_temporal_handles();
    test_timeline_document_multitrack_and_capacity();
    test_timeline_frame_rate_equivalence();
    test_timeline_track_transactional_key_mutation();
    return test_support_failures();
}
