#ifndef RAY_TRACING_TIMELINE_TRACK_H
#define RAY_TRACING_TIMELINE_TRACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "animation/timeline_value.h"

#define TIMELINE_ID_CAPACITY 64u
#define TIMELINE_TRACK_KEY_CAPACITY 128u

typedef enum TimelineInterpolation {
    TIMELINE_INTERPOLATION_STEP = 0,
    TIMELINE_INTERPOLATION_LINEAR,
    TIMELINE_INTERPOLATION_CUBIC_RESERVED
} TimelineInterpolation;

typedef enum TimelineChannelSource {
    TIMELINE_CHANNEL_SOURCE_AUTHORED = 0,
    TIMELINE_CHANNEL_SOURCE_SIMULATION_RESERVED,
    TIMELINE_CHANNEL_SOURCE_PROCEDURAL_RESERVED,
    TIMELINE_CHANNEL_SOURCE_CONSTRAINT_RESERVED,
    TIMELINE_CHANNEL_SOURCE_DERIVED_RESERVED
} TimelineChannelSource;

typedef struct TimelineKeyframe {
    int64_t frame;
    TimelineValue value;
    TimelineInterpolation interpolation_to_next;
} TimelineKeyframe;

typedef struct TimelineTrack {
    char track_id[TIMELINE_ID_CAPACITY];
    char target_id[TIMELINE_ID_CAPACITY];
    char property_id[TIMELINE_ID_CAPACITY];
    TimelineValueType value_type;
    TimelineUnit unit;
    TimelineChannelSource source;
    bool enabled;
    size_t key_count;
    TimelineKeyframe keys[TIMELINE_TRACK_KEY_CAPACITY];
} TimelineTrack;

typedef struct TimelineEvaluationResult {
    bool valid;
    bool exact_key;
    bool held;
    bool interpolated;
    size_t track_index;
    char track_id[TIMELINE_ID_CAPACITY];
    char target_id[TIMELINE_ID_CAPACITY];
    char property_id[TIMELINE_ID_CAPACITY];
    TimelineChannelSource source;
    TimelineValue value;
    int64_t left_frame;
    int64_t right_frame;
    double alpha;
    TimelineStatus status;
} TimelineEvaluationResult;

const char* TimelineInterpolationLabel(TimelineInterpolation interpolation);
const char* TimelineChannelSourceLabel(TimelineChannelSource source);
TimelineStatus TimelineTrackInit(TimelineTrack* track,
                                 const char* track_id,
                                 const char* target_id,
                                 const char* property_id,
                                 TimelineValueType value_type);
TimelineStatus TimelineTrackAddKey(TimelineTrack* track,
                                   int64_t frame,
                                   TimelineValue value,
                                   TimelineInterpolation interpolation_to_next);
TimelineStatus TimelineTrackSetUnit(TimelineTrack* track, TimelineUnit unit);
TimelineStatus TimelineTrackValidate(const TimelineTrack* track,
                                     const TimelineRange* range);
TimelineStatus TimelineTrackEvaluate(const TimelineTrack* track,
                                     const TimelineEvaluationContext* context,
                                     TimelineEvaluationResult* out_result);

#endif
