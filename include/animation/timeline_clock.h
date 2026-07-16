#ifndef RAY_TRACING_TIMELINE_CLOCK_H
#define RAY_TRACING_TIMELINE_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum TimelineStatus {
    TIMELINE_STATUS_OK = 0,
    TIMELINE_STATUS_INVALID_ARGUMENT,
    TIMELINE_STATUS_INVALID_RATE,
    TIMELINE_STATUS_INVALID_RANGE,
    TIMELINE_STATUS_FRAME_OUT_OF_RANGE,
    TIMELINE_STATUS_ARITHMETIC_OVERFLOW,
    TIMELINE_STATUS_CAPACITY_EXCEEDED,
    TIMELINE_STATUS_DUPLICATE_ID,
    TIMELINE_STATUS_INVALID_ID,
    TIMELINE_STATUS_INVALID_TRACK,
    TIMELINE_STATUS_DUPLICATE_KEY,
    TIMELINE_STATUS_UNSORTED_KEYS,
    TIMELINE_STATUS_TYPE_MISMATCH,
    TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION
} TimelineStatus;

typedef struct TimelineRate {
    uint32_t frames_per_second_numerator;
    uint32_t frames_per_second_denominator;
} TimelineRate;

typedef struct TimelineRange {
    int64_t start_frame;
    uint64_t frame_count;
} TimelineRange;

typedef struct TimelineSample {
    int64_t absolute_frame;
    uint32_t subframe_numerator;
    uint32_t subframe_denominator;
} TimelineSample;

typedef struct TimelineEvaluationContext {
    TimelineRate rate;
    TimelineRange range;
    TimelineSample sample;
    uint64_t local_frame;
    double subframe;
    double absolute_frame_position;
    double local_frame_position;
    double absolute_time_seconds;
    double local_time_seconds;
    double duration_seconds;
    double normalized_t;
} TimelineEvaluationContext;

const char* TimelineStatusLabel(TimelineStatus status);
bool TimelineRateIsValid(TimelineRate rate);
bool TimelineRangeIsValid(TimelineRange range);
TimelineStatus TimelineRangeEndFrame(TimelineRange range, int64_t* out_end_frame);
TimelineStatus TimelineEvaluationContextBuild(TimelineRate rate,
                                              TimelineRange range,
                                              TimelineSample sample,
                                              TimelineEvaluationContext* out_context);
TimelineStatus TimelineEvaluationContextBuildFromChunk(
    TimelineRate rate,
    TimelineRange authored_range,
    int64_t chunk_start_frame,
    uint64_t chunk_local_frame,
    uint32_t subframe_numerator,
    uint32_t subframe_denominator,
    TimelineEvaluationContext* out_context);
bool TimelineEvaluationContextsReferToSameSample(
    const TimelineEvaluationContext* a,
    const TimelineEvaluationContext* b);

#endif
