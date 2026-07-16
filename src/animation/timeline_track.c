#include "animation/timeline_track.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool timeline_id_is_valid(const char* id) {
    size_t length = 0u;
    if (!id || id[0] == '\0') return false;
    while (length < TIMELINE_ID_CAPACITY && id[length] != '\0') length += 1u;
    return length > 0u && length < TIMELINE_ID_CAPACITY;
}

static TimelineStatus timeline_copy_id(char* out, const char* id) {
    if (!out || !timeline_id_is_valid(id)) return TIMELINE_STATUS_INVALID_ID;
    snprintf(out, TIMELINE_ID_CAPACITY, "%s", id);
    return TIMELINE_STATUS_OK;
}

const char* TimelineInterpolationLabel(TimelineInterpolation interpolation) {
    switch (interpolation) {
        case TIMELINE_INTERPOLATION_STEP: return "step";
        case TIMELINE_INTERPOLATION_LINEAR: return "linear";
        case TIMELINE_INTERPOLATION_CUBIC_RESERVED: return "cubic_reserved";
        default: return "unknown";
    }
}

const char* TimelineChannelSourceLabel(TimelineChannelSource source) {
    switch (source) {
        case TIMELINE_CHANNEL_SOURCE_AUTHORED: return "authored";
        case TIMELINE_CHANNEL_SOURCE_SIMULATION_RESERVED: return "simulation_reserved";
        case TIMELINE_CHANNEL_SOURCE_PROCEDURAL_RESERVED: return "procedural_reserved";
        case TIMELINE_CHANNEL_SOURCE_CONSTRAINT_RESERVED: return "constraint_reserved";
        case TIMELINE_CHANNEL_SOURCE_DERIVED_RESERVED: return "derived_reserved";
        default: return "unknown";
    }
}

TimelineStatus TimelineTrackInit(TimelineTrack* track,
                                 const char* track_id,
                                 const char* target_id,
                                 const char* property_id,
                                 TimelineValueType value_type) {
    TimelineTrack candidate;
    TimelineStatus status;
    if (!track) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (value_type != TIMELINE_VALUE_SCALAR && value_type != TIMELINE_VALUE_VEC3) {
        return TIMELINE_STATUS_TYPE_MISMATCH;
    }
    memset(&candidate, 0, sizeof(candidate));
    status = timeline_copy_id(candidate.track_id, track_id);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_copy_id(candidate.target_id, target_id);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_copy_id(candidate.property_id, property_id);
    if (status != TIMELINE_STATUS_OK) return status;
    candidate.value_type = value_type;
    candidate.unit = TIMELINE_UNIT_UNSPECIFIED;
    candidate.source = TIMELINE_CHANNEL_SOURCE_AUTHORED;
    candidate.enabled = true;
    *track = candidate;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineTrackSetUnit(TimelineTrack* track, TimelineUnit unit) {
    if (!track) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (!TimelineUnitIsValid(unit) || unit == TIMELINE_UNIT_UNSPECIFIED) {
        return TIMELINE_STATUS_UNIT_MISMATCH;
    }
    track->unit = unit;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineTrackAddKey(TimelineTrack* track,
                                   int64_t frame,
                                   TimelineValue value,
                                   TimelineInterpolation interpolation_to_next) {
    TimelineKeyframe key;
    if (!track) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (track->key_count >= TIMELINE_TRACK_KEY_CAPACITY) {
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    }
    if (value.type != track->value_type || !TimelineValueIsFinite(value)) {
        return TIMELINE_STATUS_TYPE_MISMATCH;
    }
    if (interpolation_to_next != TIMELINE_INTERPOLATION_STEP &&
        interpolation_to_next != TIMELINE_INTERPOLATION_LINEAR) {
        return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
    }
    if (track->key_count > 0u) {
        const int64_t last_frame = track->keys[track->key_count - 1u].frame;
        if (frame == last_frame) return TIMELINE_STATUS_DUPLICATE_KEY;
        if (frame < last_frame) return TIMELINE_STATUS_UNSORTED_KEYS;
    }
    memset(&key, 0, sizeof(key));
    key.frame = frame;
    key.value = value;
    key.interpolation_to_next = interpolation_to_next;
    track->keys[track->key_count++] = key;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineTrackValidate(const TimelineTrack* track,
                                     const TimelineRange* range) {
    int64_t end_frame = 0;
    TimelineStatus range_status;
    if (!track || !range) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (!timeline_id_is_valid(track->track_id) ||
        !timeline_id_is_valid(track->target_id) ||
        !timeline_id_is_valid(track->property_id)) {
        return TIMELINE_STATUS_INVALID_ID;
    }
    if (track->value_type != TIMELINE_VALUE_SCALAR &&
        track->value_type != TIMELINE_VALUE_VEC3) {
        return TIMELINE_STATUS_TYPE_MISMATCH;
    }
    if (!TimelineUnitIsValid(track->unit)) return TIMELINE_STATUS_UNIT_MISMATCH;
    if (track->source != TIMELINE_CHANNEL_SOURCE_AUTHORED || track->key_count == 0u ||
        track->key_count > TIMELINE_TRACK_KEY_CAPACITY) {
        return TIMELINE_STATUS_INVALID_TRACK;
    }
    range_status = TimelineRangeEndFrame(*range, &end_frame);
    if (range_status != TIMELINE_STATUS_OK) return range_status;
    for (size_t i = 0u; i < track->key_count; ++i) {
        const TimelineKeyframe* key = &track->keys[i];
        if (key->frame < range->start_frame || key->frame > end_frame) {
            return TIMELINE_STATUS_FRAME_OUT_OF_RANGE;
        }
        if (key->value.type != track->value_type || !TimelineValueIsFinite(key->value)) {
            return TIMELINE_STATUS_TYPE_MISMATCH;
        }
        if (key->interpolation_to_next != TIMELINE_INTERPOLATION_STEP &&
            key->interpolation_to_next != TIMELINE_INTERPOLATION_LINEAR) {
            return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
        }
        if (i > 0u) {
            if (key->frame == track->keys[i - 1u].frame) {
                return TIMELINE_STATUS_DUPLICATE_KEY;
            }
            if (key->frame < track->keys[i - 1u].frame) {
                return TIMELINE_STATUS_UNSORTED_KEYS;
            }
        }
    }
    return TIMELINE_STATUS_OK;
}

static void timeline_result_identity(TimelineEvaluationResult* result,
                                     const TimelineTrack* track) {
    snprintf(result->track_id, sizeof(result->track_id), "%s", track->track_id);
    snprintf(result->target_id, sizeof(result->target_id), "%s", track->target_id);
    snprintf(result->property_id, sizeof(result->property_id), "%s", track->property_id);
    result->source = track->source;
}

TimelineStatus TimelineTrackEvaluate(const TimelineTrack* track,
                                     const TimelineEvaluationContext* context,
                                     TimelineEvaluationResult* out_result) {
    TimelineEvaluationResult result;
    TimelineStatus status;
    double sample_frame;
    if (!track || !context || !out_result) return TIMELINE_STATUS_INVALID_ARGUMENT;
    memset(&result, 0, sizeof(result));
    status = TimelineTrackValidate(track, &context->range);
    if (status != TIMELINE_STATUS_OK) return status;
    if (!track->enabled) return TIMELINE_STATUS_INVALID_TRACK;
    sample_frame = context->absolute_frame_position;
    timeline_result_identity(&result, track);

    if (sample_frame <= (double)track->keys[0].frame) {
        result.valid = true;
        result.exact_key = sample_frame == (double)track->keys[0].frame;
        result.held = !result.exact_key;
        result.value = track->keys[0].value;
        result.left_frame = track->keys[0].frame;
        result.right_frame = track->keys[0].frame;
        result.status = TIMELINE_STATUS_OK;
        *out_result = result;
        return TIMELINE_STATUS_OK;
    }
    if (sample_frame >= (double)track->keys[track->key_count - 1u].frame) {
        const TimelineKeyframe* last = &track->keys[track->key_count - 1u];
        result.valid = true;
        result.exact_key = sample_frame == (double)last->frame;
        result.held = !result.exact_key;
        result.value = last->value;
        result.left_frame = last->frame;
        result.right_frame = last->frame;
        result.status = TIMELINE_STATUS_OK;
        *out_result = result;
        return TIMELINE_STATUS_OK;
    }

    for (size_t i = 0u; i + 1u < track->key_count; ++i) {
        const TimelineKeyframe* left = &track->keys[i];
        const TimelineKeyframe* right = &track->keys[i + 1u];
        if (sample_frame == (double)left->frame) {
            result.valid = true;
            result.exact_key = true;
            result.value = left->value;
            result.left_frame = left->frame;
            result.right_frame = left->frame;
            result.status = TIMELINE_STATUS_OK;
            *out_result = result;
            return TIMELINE_STATUS_OK;
        }
        if (sample_frame > (double)left->frame && sample_frame < (double)right->frame) {
            const double alpha = (sample_frame - (double)left->frame) /
                                 (double)(right->frame - left->frame);
            result.valid = true;
            result.left_frame = left->frame;
            result.right_frame = right->frame;
            result.alpha = alpha;
            if (left->interpolation_to_next == TIMELINE_INTERPOLATION_STEP) {
                result.held = true;
                result.value = left->value;
            } else if (left->interpolation_to_next == TIMELINE_INTERPOLATION_LINEAR) {
                status = TimelineValueInterpolateLinear(left->value, right->value,
                                                        alpha, &result.value);
                if (status != TIMELINE_STATUS_OK) return status;
                result.interpolated = true;
            } else {
                return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
            }
            result.status = TIMELINE_STATUS_OK;
            *out_result = result;
            return TIMELINE_STATUS_OK;
        }
    }
    return TIMELINE_STATUS_INVALID_TRACK;
}
