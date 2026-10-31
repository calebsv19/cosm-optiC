#ifndef RAY_TRACING_SEQUENCE_CLOCK_H
#define RAY_TRACING_SEQUENCE_CLOCK_H

#include "animation/timeline_clock.h"
#include <stddef.h>

#define SEQUENCE_CLOCK_VERSION 1u
#define SEQUENCE_SEGMENT_CAPACITY 64u
#define SEQUENCE_ID_CAPACITY 64u
#define SEQUENCE_REVISION_CAPACITY 65u

typedef enum SequenceStatus {
    SEQUENCE_OK = 0,
    SEQUENCE_INVALID_ARGUMENT,
    SEQUENCE_INVALID_VERSION,
    SEQUENCE_INVALID_TIME,
    SEQUENCE_NUMERIC_RANGE,
    SEQUENCE_INVALID_RANGE,
    SEQUENCE_INVALID_IDENTITY,
    SEQUENCE_INVALID_SEGMENTS,
    SEQUENCE_DISCONTINUOUS_SOURCE,
    SEQUENCE_SOURCE_OUT_OF_RANGE,
    SEQUENCE_UNSUPPORTED_SOURCE_SAMPLE,
    SEQUENCE_SAMPLE_OUT_OF_RANGE
} SequenceStatus;

/* Nonnegative rational; components <= INT64_MAX. Results are reduced. */
typedef struct SequenceTime { uint64_t numerator, denominator; } SequenceTime;
typedef enum SequenceSegmentKind { SEQUENCE_PLAY = 1, SEQUENCE_HOLD = 2 } SequenceSegmentKind;
typedef struct SequenceSegment {
    char id[SEQUENCE_ID_CAPACITY];
    uint64_t start_frame, frame_count; /* Local offsets, half-open interval. */
    SequenceSegmentKind kind;
    SequenceTime source_start;
    SequenceTime speed; /* Source seconds per presentation second. */
} SequenceSegment;
typedef struct SequenceSource {
    char id[SEQUENCE_ID_CAPACITY];
    char revision[SEQUENCE_REVISION_CAPACITY]; /* 64 hexadecimal characters. */
    TimelineRate rate;
    uint64_t first_tick, last_tick; /* Inclusive, declared contiguous envelope. */
} SequenceSource;
typedef struct SequenceClock {
    uint32_t version;
    TimelineRate rate;
    TimelineRange range;
    SequenceSource source;
    size_t segment_count;
    SequenceSegment segments[SEQUENCE_SEGMENT_CAPACITY];
} SequenceClock;
typedef struct SequenceEvaluation {
    uint32_t version;
    TimelineEvaluationContext presentation_context;
    SequenceTime presentation_time, segment_time, simulation_time;
    uint64_t source_tick;
    size_t segment_index;
    SequenceSource source;
} SequenceEvaluation;
typedef enum SequenceMotionClock {
    SEQUENCE_MOTION_PRESENTATION = 1,
    SEQUENCE_MOTION_SIMULATION,
    SEQUENCE_MOTION_SEGMENT_LOCAL
} SequenceMotionClock;

/* Semantic equality; C struct padding is never provenance. */
bool SequenceClockEvaluationsEqual(const SequenceEvaluation *a,const SequenceEvaluation *b);
const char *SequenceStatusLabel(SequenceStatus status);
SequenceStatus SequenceClockValidate(const SequenceClock *clock);
SequenceStatus SequenceClockDuration(const SequenceClock *clock, SequenceTime *out);
SequenceStatus SequenceClockEvaluate(const SequenceClock *clock,
                                    TimelineSample sample,
                                    SequenceEvaluation *out);
SequenceStatus SequenceClockEvaluateChunk(const SequenceClock *clock,
    int64_t chunk_start, uint64_t chunk_offset, uint32_t subframe_numerator,
    uint32_t subframe_denominator, SequenceEvaluation *out);
/* This selects an evaluated time only; it does not implement motion bindings. */
SequenceStatus SequenceClockMotionTime(const SequenceEvaluation *evaluation,
                                      SequenceMotionClock domain,
                                      SequenceTime *out);
#endif
