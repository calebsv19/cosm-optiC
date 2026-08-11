#include "animation/timeline_light_motion.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "animation/timeline_property_registry.h"

#define TIMELINE_LIGHT_PATH_SAMPLE_CAPACITY 2049u

typedef struct TimelineLightPathArcSample {
    double global_t;
    double cumulative_length;
    TimelineVec3 position;
} TimelineLightPathArcSample;

typedef struct TimelineLightPathArcTable {
    size_t count;
    double total_length;
    TimelineLightPathArcSample samples[TIMELINE_LIGHT_PATH_SAMPLE_CAPACITY];
} TimelineLightPathArcTable;

static TimelineVec3 timeline_light_path_position(const Path* path,
                                                 const CameraPath3D* path3d,
                                                 double global_t) {
    Point xy = GetPositionAlongPath((Path*)path, global_t);
    TimelineVec3 position = {xy.x, xy.y, 0.0};
    if (path3d) {
        position.z = CameraPath3D_GetPositionZ(path, path3d, global_t);
    }
    return position;
}

static double timeline_light_distance(TimelineVec3 a, TimelineVec3 b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double dz = b.z - a.z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

static TimelineStatus timeline_light_build_arc_table(
    const Path* path,
    const CameraPath3D* path3d,
    TimelineLightPathArcTable* out_table) {
    TimelineLightPathArcTable table;
    if (!path || !out_table || path->numPoints < 2 ||
        path->numPoints > MAX_BEZIER_POINTS) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    memset(&table, 0, sizeof(table));
    table.count = TIMELINE_LIGHT_PATH_SAMPLE_CAPACITY;
    for (size_t i = 0u; i < table.count; ++i) {
        TimelineLightPathArcSample* sample = &table.samples[i];
        sample->global_t = (double)i / (double)(table.count - 1u);
        sample->position = timeline_light_path_position(path, path3d,
                                                        sample->global_t);
        if (!isfinite(sample->position.x) || !isfinite(sample->position.y) ||
            !isfinite(sample->position.z)) {
            return TIMELINE_STATUS_INVALID_TRACK;
        }
        if (i > 0u) {
            sample->cumulative_length =
                table.samples[i - 1u].cumulative_length +
                timeline_light_distance(table.samples[i - 1u].position,
                                        sample->position);
        }
    }
    table.total_length = table.samples[table.count - 1u].cumulative_length;
    if (!isfinite(table.total_length) || table.total_length <= 1e-12) {
        return TIMELINE_STATUS_INVALID_TRACK;
    }
    *out_table = table;
    return TIMELINE_STATUS_OK;
}

static TimelineVec3 timeline_light_vec3_lerp(TimelineVec3 a, TimelineVec3 b,
                                             double alpha) {
    TimelineVec3 value = {
        a.x + (b.x - a.x) * alpha,
        a.y + (b.y - a.y) * alpha,
        a.z + (b.z - a.z) * alpha
    };
    return value;
}

static TimelineStatus timeline_light_sample_arc_table(
    const TimelineLightPathArcTable* table,
    double progress,
    TimelineVec3* out_position,
    double* out_global_t) {
    double target_length = 0.0;
    size_t low = 1u;
    size_t high = 0u;
    if (!table || !out_position || !out_global_t || table->count < 2u ||
        !isfinite(progress) || progress < 0.0 || progress > 1.0) {
        return TIMELINE_STATUS_VALUE_OUT_OF_RANGE;
    }
    if (progress <= 0.0) {
        *out_position = table->samples[0].position;
        *out_global_t = 0.0;
        return TIMELINE_STATUS_OK;
    }
    if (progress >= 1.0) {
        *out_position = table->samples[table->count - 1u].position;
        *out_global_t = 1.0;
        return TIMELINE_STATUS_OK;
    }
    target_length = progress * table->total_length;
    high = table->count - 1u;
    while (low <= high) {
        const size_t middle = low + (high - low) / 2u;
        if (table->samples[middle].cumulative_length < target_length) {
            low = middle + 1u;
        } else {
            if (middle == 0u) break;
            high = middle - 1u;
        }
    }
    if (low >= table->count) low = table->count - 1u;
    {
        const TimelineLightPathArcSample* left = &table->samples[low - 1u];
        const TimelineLightPathArcSample* right = &table->samples[low];
        const double length_span =
            right->cumulative_length - left->cumulative_length;
        double alpha = 0.0;
        if (length_span > 1e-12) {
            alpha = (target_length - left->cumulative_length) / length_span;
        }
        *out_position = timeline_light_vec3_lerp(left->position, right->position,
                                                 alpha);
        *out_global_t =
            left->global_t + (right->global_t - left->global_t) * alpha;
    }
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineLightMotionValidateProgressTrack(
    const TimelineTrack* progress_track,
    const TimelineRange* range) {
    TimelineStatus status;
    if (!progress_track || !range) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (strncmp(progress_track->target_id, "light/", 6u) != 0 ||
        progress_track->target_id[6] == '\0' ||
        strcmp(progress_track->property_id, "light/path_progress") != 0 ||
        progress_track->value_type != TIMELINE_VALUE_SCALAR ||
        progress_track->unit != TIMELINE_UNIT_UNITLESS) {
        return TIMELINE_STATUS_TARGET_KIND_MISMATCH;
    }
    status = TimelineTrackValidate(progress_track, range);
    if (status != TIMELINE_STATUS_OK) return status;
    for (size_t i = 0u; i < progress_track->key_count; ++i) {
        const TimelineKeyframe* key = &progress_track->keys[i];
        const double value = key->value.as.scalar;
        if (!isfinite(value) || value < 0.0 || value > 1.0) {
            return TIMELINE_STATUS_VALUE_OUT_OF_RANGE;
        }
        if (i > 0u &&
            value < progress_track->keys[i - 1u].value.as.scalar) {
            return TIMELINE_STATUS_INVALID_TRACK;
        }
        if (i + 1u < progress_track->key_count &&
            key->interpolation_to_next ==
                TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
            const TimelineKeyframe* right =
                &progress_track->keys[i + 1u];
            const double y0 = value;
            const double y1 = y0 + key->outgoing_value_offset;
            const double y3 = right->value.as.scalar;
            const double y2 = y3 + right->incoming_value_offset;
            if (!isfinite(y1) || !isfinite(y2) ||
                y1 < y0 || y2 < y1 || y3 < y2 ||
                y1 < 0.0 || y2 > 1.0) {
                return TIMELINE_STATUS_INVALID_TRACK;
            }
        }
    }
    return TIMELINE_STATUS_OK;
}

TimelineStatus TimelineLightMotionEvaluate(
    const TimelineTrack* progress_track,
    const Path* path,
    const CameraPath3D* path3d,
    const TimelineEvaluationContext* context,
    TimelineLightMotionSample* out_sample) {
    TimelineEvaluationResult progress_result;
    TimelineStatus status;
    if (!progress_track || !path || !context || !out_sample) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    status = TimelineLightMotionValidateProgressTrack(progress_track,
                                                      &context->range);
    if (status != TIMELINE_STATUS_OK) return status;
    memset(&progress_result, 0, sizeof(progress_result));
    status = TimelineTrackEvaluate(progress_track, context, &progress_result);
    if (status != TIMELINE_STATUS_OK) return status;
    return TimelineLightMotionEvaluateResult(
        progress_track, &progress_result, path, path3d, context, out_sample);
}

TimelineStatus TimelineLightMotionEvaluateResult(
    const TimelineTrack* progress_track,
    const TimelineEvaluationResult* progress_result,
    const Path* path,
    const CameraPath3D* path3d,
    const TimelineEvaluationContext* context,
    TimelineLightMotionSample* out_sample) {
    TimelineLightMotionSample sample;
    TimelineLightPathArcTable arc_table;
    TimelineStatus status;
    double frames_per_second = 0.0;
    if (!progress_track || !progress_result || !path || !context ||
        !out_sample || !progress_result->valid ||
        progress_result->status != TIMELINE_STATUS_OK ||
        progress_result->value.type != TIMELINE_VALUE_SCALAR ||
        strcmp(progress_result->track_id, progress_track->track_id) != 0 ||
        strcmp(progress_result->target_id, progress_track->target_id) != 0 ||
        strcmp(progress_result->property_id, progress_track->property_id) != 0) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    status = TimelineLightMotionValidateProgressTrack(progress_track,
                                                      &context->range);
    if (status != TIMELINE_STATUS_OK) return status;
    memset(&sample, 0, sizeof(sample));
    memset(&arc_table, 0, sizeof(arc_table));
    sample.progress = progress_result->value.as.scalar;
    if (!isfinite(sample.progress) || sample.progress < 0.0 ||
        sample.progress > 1.0) {
        return TIMELINE_STATUS_VALUE_OUT_OF_RANGE;
    }
    status = timeline_light_build_arc_table(path, path3d, &arc_table);
    if (status != TIMELINE_STATUS_OK) return status;
    status = timeline_light_sample_arc_table(&arc_table, sample.progress,
                                             &sample.position,
                                             &sample.global_path_t);
    if (status != TIMELINE_STATUS_OK) return status;
    frames_per_second =
        (double)context->rate.frames_per_second_numerator /
        (double)context->rate.frames_per_second_denominator;
    sample.valid = true;
    sample.path_length_world = arc_table.total_length;
    sample.progress_per_frame = progress_result->derivative_per_frame;
    sample.speed_valid = progress_result->derivative_valid;
    if (sample.speed_valid) {
        sample.world_speed_per_second =
            fabs(sample.path_length_world * sample.progress_per_frame *
                 frames_per_second);
    }
    sample.invalidation_domains = TIMELINE_INVALIDATION_LIGHTING;
    snprintf(sample.target_id, sizeof(sample.target_id), "%s",
             progress_track->target_id);
    *out_sample = sample;
    return TIMELINE_STATUS_OK;
}
