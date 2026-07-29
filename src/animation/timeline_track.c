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
        case TIMELINE_INTERPOLATION_CUBIC_BEZIER: return "cubic_bezier";
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
        interpolation_to_next != TIMELINE_INTERPOLATION_LINEAR &&
        interpolation_to_next != TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
        return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
    }
    if (interpolation_to_next == TIMELINE_INTERPOLATION_CUBIC_BEZIER &&
        track->value_type != TIMELINE_VALUE_SCALAR) {
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

TimelineStatus TimelineTrackInsertKey(TimelineTrack* track,
                                      TimelineKeyframe key,
                                      size_t* out_key_index) {
    TimelineTrack candidate;
    size_t index = 0u;
    if (!track || !out_key_index) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (track->key_count >= TIMELINE_TRACK_KEY_CAPACITY) {
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    }
    if (key.value.type != track->value_type || !TimelineValueIsFinite(key.value)) {
        return TIMELINE_STATUS_TYPE_MISMATCH;
    }
    if (key.interpolation_to_next != TIMELINE_INTERPOLATION_STEP &&
        key.interpolation_to_next != TIMELINE_INTERPOLATION_LINEAR &&
        key.interpolation_to_next != TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
        return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
    }
    if (key.interpolation_to_next == TIMELINE_INTERPOLATION_CUBIC_BEZIER &&
        track->value_type != TIMELINE_VALUE_SCALAR) {
        return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
    }
    while (index < track->key_count && track->keys[index].frame < key.frame) index += 1u;
    if (index < track->key_count && track->keys[index].frame == key.frame) {
        return TIMELINE_STATUS_DUPLICATE_KEY;
    }
    candidate = *track;
    memmove(&candidate.keys[index + 1u], &candidate.keys[index],
            (candidate.key_count - index) * sizeof(candidate.keys[0]));
    candidate.keys[index] = key;
    candidate.key_count += 1u;
    *track = candidate;
    *out_key_index = index;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineTrackRemoveKey(TimelineTrack* track, size_t key_index) {
    TimelineTrack candidate;
    if (!track || key_index >= track->key_count) return TIMELINE_STATUS_INVALID_ARGUMENT;
    candidate = *track;
    memmove(&candidate.keys[key_index], &candidate.keys[key_index + 1u],
            (candidate.key_count - key_index - 1u) * sizeof(candidate.keys[0]));
    candidate.key_count -= 1u;
    memset(&candidate.keys[candidate.key_count], 0, sizeof(candidate.keys[0]));
    *track = candidate;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineTrackMoveScalarKey(TimelineTrack* track,
                                          size_t key_index,
                                          int64_t frame,
                                          double value) {
    TimelineTrack candidate;
    if (!track || key_index >= track->key_count ||
        track->value_type != TIMELINE_VALUE_SCALAR || !isfinite(value)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if ((key_index > 0u && frame <= track->keys[key_index - 1u].frame) ||
        (key_index + 1u < track->key_count &&
         frame >= track->keys[key_index + 1u].frame)) {
        return TIMELINE_STATUS_UNSORTED_KEYS;
    }
    candidate = *track;
    candidate.keys[key_index].frame = frame;
    candidate.keys[key_index].value.as.scalar = value;
    *track = candidate;
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineTrackSetScalarTemporalHandles(
    TimelineTrack* track,
    size_t key_index,
    double incoming_frame_offset,
    double incoming_value_offset,
    double outgoing_frame_offset,
    double outgoing_value_offset) {
    TimelineKeyframe candidate;
    if (!track || key_index >= track->key_count) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if (track->value_type != TIMELINE_VALUE_SCALAR ||
        !isfinite(incoming_frame_offset) || !isfinite(incoming_value_offset) ||
        !isfinite(outgoing_frame_offset) || !isfinite(outgoing_value_offset) ||
        incoming_frame_offset > 0.0 || outgoing_frame_offset < 0.0) {
        return TIMELINE_STATUS_INVALID_TRACK;
    }
    candidate = track->keys[key_index];
    candidate.incoming_frame_offset = incoming_frame_offset;
    candidate.incoming_value_offset = incoming_value_offset;
    candidate.outgoing_frame_offset = outgoing_frame_offset;
    candidate.outgoing_value_offset = outgoing_value_offset;
    track->keys[key_index] = candidate;
    return TIMELINE_STATUS_OK;
}

static TimelineStatus timeline_validate_cubic_segment(const TimelineKeyframe* left,
                                                       const TimelineKeyframe* right) {
    const double frame_span = (double)(right->frame - left->frame);
    const double x1 = (double)left->frame + left->outgoing_frame_offset;
    const double x2 = (double)right->frame + right->incoming_frame_offset;
    if (!isfinite(left->outgoing_frame_offset) ||
        !isfinite(left->outgoing_value_offset) ||
        !isfinite(right->incoming_frame_offset) ||
        !isfinite(right->incoming_value_offset) ||
        left->outgoing_frame_offset < 0.0 ||
        left->outgoing_frame_offset > frame_span ||
        right->incoming_frame_offset > 0.0 ||
        right->incoming_frame_offset < -frame_span || x1 > x2) {
        return TIMELINE_STATUS_INVALID_TRACK;
    }
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
            key->interpolation_to_next != TIMELINE_INTERPOLATION_LINEAR &&
            key->interpolation_to_next != TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
            return TIMELINE_STATUS_UNSUPPORTED_INTERPOLATION;
        }
        if (key->interpolation_to_next == TIMELINE_INTERPOLATION_CUBIC_BEZIER &&
            track->value_type != TIMELINE_VALUE_SCALAR) {
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
        if (i + 1u < track->key_count &&
            key->interpolation_to_next == TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
            TimelineStatus cubic_status =
                timeline_validate_cubic_segment(key, &track->keys[i + 1u]);
            if (cubic_status != TIMELINE_STATUS_OK) return cubic_status;
        }
    }
    return TIMELINE_STATUS_OK;
}

static double timeline_cubic_bezier(double p0, double p1, double p2, double p3,
                                    double u) {
    const double one_minus_u = 1.0 - u;
    return one_minus_u * one_minus_u * one_minus_u * p0 +
           3.0 * one_minus_u * one_minus_u * u * p1 +
           3.0 * one_minus_u * u * u * p2 + u * u * u * p3;
}

static double timeline_cubic_bezier_derivative(double p0, double p1, double p2,
                                               double p3, double u) {
    const double one_minus_u = 1.0 - u;
    return 3.0 * one_minus_u * one_minus_u * (p1 - p0) +
           6.0 * one_minus_u * u * (p2 - p1) +
           3.0 * u * u * (p3 - p2);
}

static double timeline_solve_cubic_frame_parameter(double x0, double x1,
                                                   double x2, double x3,
                                                   double sample_frame) {
    double low = 0.0;
    double high = 1.0;
    for (int iteration = 0; iteration < 60; ++iteration) {
        const double middle = (low + high) * 0.5;
        const double x = timeline_cubic_bezier(x0, x1, x2, x3, middle);
        if (x < sample_frame) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return (low + high) * 0.5;
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
                result.derivative_valid = true;
                result.derivative_per_frame = 0.0;
            } else if (left->interpolation_to_next == TIMELINE_INTERPOLATION_LINEAR) {
                status = TimelineValueInterpolateLinear(left->value, right->value,
                                                        alpha, &result.value);
                if (status != TIMELINE_STATUS_OK) return status;
                result.interpolated = true;
                result.curve_parameter = alpha;
                result.derivative_valid = true;
                if (track->value_type == TIMELINE_VALUE_SCALAR) {
                    result.derivative_per_frame =
                        (right->value.as.scalar - left->value.as.scalar) /
                        (double)(right->frame - left->frame);
                }
            } else if (left->interpolation_to_next ==
                       TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
                const double x0 = (double)left->frame;
                const double x1 = x0 + left->outgoing_frame_offset;
                const double x3 = (double)right->frame;
                const double x2 = x3 + right->incoming_frame_offset;
                const double y0 = left->value.as.scalar;
                const double y1 = y0 + left->outgoing_value_offset;
                const double y3 = right->value.as.scalar;
                const double y2 = y3 + right->incoming_value_offset;
                const double u = timeline_solve_cubic_frame_parameter(
                    x0, x1, x2, x3, sample_frame);
                const double dx_du =
                    timeline_cubic_bezier_derivative(x0, x1, x2, x3, u);
                const double dy_du =
                    timeline_cubic_bezier_derivative(y0, y1, y2, y3, u);
                result.value = TimelineValueScalar(
                    timeline_cubic_bezier(y0, y1, y2, y3, u));
                result.interpolated = true;
                result.curve_parameter = u;
                if (fabs(dx_du) > 1e-12) {
                    result.derivative_valid = true;
                    result.derivative_per_frame = dy_du / dx_du;
                }
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
