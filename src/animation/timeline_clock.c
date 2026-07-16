#include "animation/timeline_clock.h"

#include <limits.h>
#include <math.h>
#include <string.h>

const char* TimelineStatusLabel(TimelineStatus status) {
    switch (status) {
        case TIMELINE_STATUS_OK: return "ok";
        case TIMELINE_STATUS_INVALID_ARGUMENT: return "invalid_argument";
        case TIMELINE_STATUS_INVALID_RATE: return "invalid_rate";
        case TIMELINE_STATUS_INVALID_RANGE: return "invalid_range";
        case TIMELINE_STATUS_FRAME_OUT_OF_RANGE: return "frame_out_of_range";
        case TIMELINE_STATUS_ARITHMETIC_OVERFLOW: return "arithmetic_overflow";
        case TIMELINE_STATUS_CAPACITY_EXCEEDED: return "capacity_exceeded";
        case TIMELINE_STATUS_DUPLICATE_ID: return "duplicate_id";
        case TIMELINE_STATUS_INVALID_ID: return "invalid_id";
        case TIMELINE_STATUS_INVALID_TRACK: return "invalid_track";
        case TIMELINE_STATUS_DUPLICATE_KEY: return "duplicate_key";
        case TIMELINE_STATUS_UNSORTED_KEYS: return "unsorted_keys";
        case TIMELINE_STATUS_TYPE_MISMATCH: return "type_mismatch";
        case TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION:
            return "unsupported_interpolation";
        case TIMELINE_STATUS_INVALID_PROPERTY_DESCRIPTOR:
            return "invalid_property_descriptor";
        case TIMELINE_STATUS_UNKNOWN_PROPERTY: return "unknown_property";
        case TIMELINE_STATUS_TARGET_KIND_MISMATCH:
            return "target_kind_mismatch";
        case TIMELINE_STATUS_OWNERSHIP_MISMATCH:
            return "ownership_mismatch";
        case TIMELINE_STATUS_DUPLICATE_OWNERSHIP:
            return "duplicate_ownership";
        case TIMELINE_STATUS_UNIT_MISMATCH: return "unit_mismatch";
        case TIMELINE_STATUS_VALUE_OUT_OF_RANGE: return "value_out_of_range";
        default: return "unknown";
    }
}

bool TimelineRateIsValid(TimelineRate rate) {
    return rate.frames_per_second_numerator > 0u &&
           rate.frames_per_second_denominator > 0u;
}

TimelineStatus TimelineRangeEndFrame(TimelineRange range, int64_t* out_end_frame) {
    uint64_t offset;
    if (!out_end_frame) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (range.frame_count == 0u) return TIMELINE_STATUS_INVALID_RANGE;
    offset = range.frame_count - 1u;
    if (offset > (uint64_t)INT64_MAX) return TIMELINE_STATUS_ARITHMETIC_OVERFLOW;
    if (range.start_frame > INT64_MAX - (int64_t)offset) {
        return TIMELINE_STATUS_ARITHMETIC_OVERFLOW;
    }
    *out_end_frame = range.start_frame + (int64_t)offset;
    return TIMELINE_STATUS_OK;
}

bool TimelineRangeIsValid(TimelineRange range) {
    int64_t end_frame = 0;
    return TimelineRangeEndFrame(range, &end_frame) == TIMELINE_STATUS_OK;
}

static bool timeline_sample_subframe_is_valid(TimelineSample sample) {
    return sample.subframe_denominator > 0u &&
           sample.subframe_numerator < sample.subframe_denominator;
}

TimelineStatus TimelineEvaluationContextBuild(TimelineRate rate,
                                              TimelineRange range,
                                              TimelineSample sample,
                                              TimelineEvaluationContext* out_context) {
    TimelineEvaluationContext context;
    TimelineStatus range_status;
    int64_t end_frame = 0;
    uint64_t local_frame;
    double fps;
    double subframe;

    if (!out_context) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (!TimelineRateIsValid(rate)) return TIMELINE_STATUS_INVALID_RATE;
    range_status = TimelineRangeEndFrame(range, &end_frame);
    if (range_status != TIMELINE_STATUS_OK) return range_status;
    if (!timeline_sample_subframe_is_valid(sample)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if (sample.absolute_frame < range.start_frame || sample.absolute_frame > end_frame) {
        return TIMELINE_STATUS_FRAME_OUT_OF_RANGE;
    }

    local_frame = (uint64_t)(sample.absolute_frame - range.start_frame);
    fps = (double)rate.frames_per_second_numerator /
          (double)rate.frames_per_second_denominator;
    subframe = (double)sample.subframe_numerator /
               (double)sample.subframe_denominator;

    memset(&context, 0, sizeof(context));
    context.rate = rate;
    context.range = range;
    context.sample = sample;
    context.local_frame = local_frame;
    context.subframe = subframe;
    context.absolute_frame_position = (double)sample.absolute_frame + subframe;
    context.local_frame_position = (double)local_frame + subframe;
    context.absolute_time_seconds = context.absolute_frame_position / fps;
    context.local_time_seconds = context.local_frame_position / fps;
    context.duration_seconds = (double)range.frame_count / fps;
    if (range.frame_count <= 1u) {
        context.normalized_t = 0.0;
    } else {
        context.normalized_t = context.local_frame_position /
                               (double)(range.frame_count - 1u);
        if (context.normalized_t < 0.0) context.normalized_t = 0.0;
        if (context.normalized_t > 1.0) context.normalized_t = 1.0;
    }

    if (!isfinite(context.absolute_time_seconds) ||
        !isfinite(context.local_time_seconds) ||
        !isfinite(context.duration_seconds) ||
        !isfinite(context.normalized_t)) {
        return TIMELINE_STATUS_ARITHMETIC_OVERFLOW;
    }
    *out_context = context;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineEvaluationContextBuildFromChunk(
    TimelineRate rate,
    TimelineRange authored_range,
    int64_t chunk_start_frame,
    uint64_t chunk_local_frame,
    uint32_t subframe_numerator,
    uint32_t subframe_denominator,
    TimelineEvaluationContext* out_context) {
    TimelineSample sample;
    if (chunk_local_frame > (uint64_t)INT64_MAX) {
        return TIMELINE_STATUS_ARITHMETIC_OVERFLOW;
    }
    if (chunk_start_frame > INT64_MAX - (int64_t)chunk_local_frame) {
        return TIMELINE_STATUS_ARITHMETIC_OVERFLOW;
    }
    sample.absolute_frame = chunk_start_frame + (int64_t)chunk_local_frame;
    sample.subframe_numerator = subframe_numerator;
    sample.subframe_denominator = subframe_denominator;
    return TimelineEvaluationContextBuild(rate, authored_range, sample, out_context);
}

bool TimelineEvaluationContextsReferToSameSample(
    const TimelineEvaluationContext* a,
    const TimelineEvaluationContext* b) {
    if (!a || !b) return false;
    return a->rate.frames_per_second_numerator ==
               b->rate.frames_per_second_numerator &&
           a->rate.frames_per_second_denominator ==
               b->rate.frames_per_second_denominator &&
           a->range.start_frame == b->range.start_frame &&
           a->range.frame_count == b->range.frame_count &&
           a->sample.absolute_frame == b->sample.absolute_frame &&
           a->sample.subframe_numerator == b->sample.subframe_numerator &&
           a->sample.subframe_denominator == b->sample.subframe_denominator;
}
