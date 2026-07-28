#include "scene_editor_light_timeline_edit.h"

#include "animation/timeline_document.h"
#include "path/path_system.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

bool scene_editor_light_timeline_ensure_editable_default_range(
    RuntimeSceneLightTimelineDocument* document) {
    TimelineTrack* track;
    uint64_t frame_count;
    uint64_t fps;
    if (!document ||
        document->progress_track_index >= document->timeline.track_count) {
        return false;
    }
    track = &document->timeline.tracks[document->progress_track_index];
    if (document->timeline.range.frame_count >= 3u ||
        strcmp(track->track_id, "selected_light_progress") != 0 ||
        track->key_count != 2u) {
        return true;
    }
    fps = document->timeline.rate.frames_per_second_denominator > 0u
        ? document->timeline.rate.frames_per_second_numerator /
              document->timeline.rate.frames_per_second_denominator
        : 0u;
    if (fps == 0u) fps = 30u;
    frame_count = fps * 5u + 1u;
    if (frame_count < 3u) frame_count = 3u;
    document->timeline.range.frame_count = frame_count;
    track->keys[track->key_count - 1u].frame =
        document->timeline.range.start_frame + (int64_t)frame_count - 1;
    if (TimelineDocumentValidate(&document->timeline) != TIMELINE_STATUS_OK ||
        RuntimeSceneLightTimelineSetLast(document) != TIMELINE_STATUS_OK) {
        return false;
    }
    return RuntimeSceneLightTimelineGetLast(document);
}

static bool track_contains_frame(const TimelineTrack* track, int64_t frame) {
    if (!track) return false;
    for (size_t i = 0u; i < track->key_count; ++i) {
        if (track->keys[i].frame == frame) return true;
    }
    return false;
}

bool scene_editor_light_timeline_nearest_open_frame(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    int64_t requested,
    int64_t* out_frame) {
    int64_t first;
    int64_t last;
    if (!document || !track || !out_frame ||
        document->timeline.range.frame_count < 3u) return false;
    first = document->timeline.range.start_frame + 1;
    last = document->timeline.range.start_frame +
           (int64_t)document->timeline.range.frame_count - 2;
    if (requested < first) requested = first;
    if (requested > last) requested = last;
    for (int64_t distance = 0; distance <= last - first; ++distance) {
        int64_t right = requested + distance;
        int64_t left = requested - distance;
        if (right <= last && !track_contains_frame(track, right)) {
            *out_frame = right;
            return true;
        }
        if (distance > 0 && left >= first &&
            !track_contains_frame(track, left)) {
            *out_frame = left;
            return true;
        }
    }
    return false;
}

bool scene_editor_light_timeline_evaluate_progress_at_frame(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    int64_t frame,
    double* out_progress) {
    TimelineEvaluationContext context;
    TimelineEvaluationResult result;
    if (!document || !track || !out_progress ||
        TimelineEvaluationContextBuild(
            document->timeline.rate,
            document->timeline.range,
            (TimelineSample){frame, 0u, 1u},
            &context) != TIMELINE_STATUS_OK ||
        TimelineTrackEvaluate(track, &context, &result) != TIMELINE_STATUS_OK ||
        result.value.type != TIMELINE_VALUE_SCALAR) {
        return false;
    }
    *out_progress = result.value.as.scalar;
    return isfinite(*out_progress);
}

bool scene_editor_light_timeline_path_anchor_progress(
    const RuntimeSceneLightTimelineDocument* document,
    int anchor_index,
    double* out_progress) {
    double target_global_t;
    double low = 0.0;
    double high = 1.0;
    if (!document || !out_progress ||
        document->spatial_path.numPoints < 2 ||
        anchor_index < 0 ||
        anchor_index >= document->spatial_path.numPoints) {
        return false;
    }
    if (anchor_index == 0) {
        *out_progress = 0.0;
        return true;
    }
    if (anchor_index + 1 == document->spatial_path.numPoints) {
        *out_progress = 1.0;
        return true;
    }
    target_global_t =
        (double)anchor_index /
        (double)(document->spatial_path.numPoints - 1);
    for (int iteration = 0; iteration < 48; ++iteration) {
        double middle = (low + high) * 0.5;
        double global_t = PathResolveNormalizedGlobalT(
            &document->spatial_path, middle);
        if (global_t < target_global_t) {
            low = middle;
        } else {
            high = middle;
        }
    }
    *out_progress = (low + high) * 0.5;
    return isfinite(*out_progress);
}

double scene_editor_light_timeline_snap_progress_to_anchor(
    const RuntimeSceneLightTimelineDocument* document,
    double progress,
    double tolerance,
    int* out_anchor_index) {
    double best = progress;
    double best_distance = tolerance;
    int best_index = -1;
    if (out_anchor_index) *out_anchor_index = -1;
    if (!document || !isfinite(progress) || !isfinite(tolerance) ||
        tolerance < 0.0) {
        return progress;
    }
    for (int i = 0; i < document->spatial_path.numPoints; ++i) {
        double anchor_progress;
        double distance;
        if (!scene_editor_light_timeline_path_anchor_progress(
                document, i, &anchor_progress)) {
            continue;
        }
        distance = fabs(progress - anchor_progress);
        if (distance <= best_distance) {
            best_distance = distance;
            best = anchor_progress;
            best_index = i;
        }
    }
    if (out_anchor_index) *out_anchor_index = best_index;
    return best;
}

static bool evaluate_progress_at_frame_position(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    double frame_position,
    double* out_progress) {
    int64_t frame;
    uint32_t subframe;
    TimelineEvaluationContext context;
    TimelineEvaluationResult result;
    if (!document || !track || !out_progress ||
        !isfinite(frame_position)) {
        return false;
    }
    frame = (int64_t)floor(frame_position);
    subframe =
        (uint32_t)llround((frame_position - (double)frame) * 1000000.0);
    if (subframe >= 1000000u) {
        frame += 1;
        subframe = 0u;
    }
    if (TimelineEvaluationContextBuild(
            document->timeline.rate, document->timeline.range,
            (TimelineSample){frame, subframe, 1000000u}, &context) !=
            TIMELINE_STATUS_OK ||
        TimelineTrackEvaluate(track, &context, &result) !=
            TIMELINE_STATUS_OK ||
        result.value.type != TIMELINE_VALUE_SCALAR) {
        return false;
    }
    *out_progress = result.value.as.scalar;
    return isfinite(*out_progress);
}

bool scene_editor_light_timeline_frame_at_progress(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    double progress,
    double* out_frame) {
    double low;
    double high;
    double first_progress;
    double last_progress;
    if (!document || !track || !out_frame || track->key_count < 2u ||
        !isfinite(progress) || progress < 0.0 || progress > 1.0) {
        return false;
    }
    low = (double)document->timeline.range.start_frame;
    high = low + (double)document->timeline.range.frame_count - 1.0;
    if (!evaluate_progress_at_frame_position(
            document, track, low, &first_progress) ||
        !evaluate_progress_at_frame_position(
            document, track, high, &last_progress) ||
        progress < first_progress - 1e-9 ||
        progress > last_progress + 1e-9) {
        return false;
    }
    if (progress <= first_progress + 1e-9) {
        *out_frame = low;
        return true;
    }
    if (progress >= last_progress - 1e-9) {
        *out_frame = high;
        return true;
    }
    for (int iteration = 0; iteration < 48; ++iteration) {
        double middle = (low + high) * 0.5;
        double sampled;
        if (!evaluate_progress_at_frame_position(
                document, track, middle, &sampled)) {
            return false;
        }
        if (sampled < progress) {
            low = middle;
        } else {
            high = middle;
        }
    }
    *out_frame = (low + high) * 0.5;
    return true;
}

static bool frame_is_used_except(
    const TimelineTrack* track,
    int64_t frame,
    size_t except_index) {
    if (!track) return false;
    for (size_t i = 0u; i < track->key_count; ++i) {
        if (i != except_index && track->keys[i].frame == frame) return true;
    }
    return false;
}

static bool nearest_open_frame_in_range(
    const TimelineTrack* track,
    int64_t requested,
    int64_t minimum,
    int64_t maximum,
    size_t except_index,
    int64_t* out_frame) {
    if (!track || !out_frame || minimum > maximum) return false;
    if (requested < minimum) requested = minimum;
    if (requested > maximum) requested = maximum;
    for (int64_t distance = 0; distance <= maximum - minimum; ++distance) {
        int64_t right = requested + distance;
        int64_t left = requested - distance;
        if (right <= maximum &&
            !frame_is_used_except(track, right, except_index)) {
            *out_frame = right;
            return true;
        }
        if (distance > 0 && left >= minimum &&
            !frame_is_used_except(track, left, except_index)) {
            *out_frame = left;
            return true;
        }
    }
    return false;
}

bool scene_editor_light_timeline_set_path_anchor_frame(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* source,
    int anchor_index,
    int64_t requested_frame,
    TimelineTrack* out_track,
    size_t* out_key_index) {
    const double epsilon = 1e-7;
    TimelineTrack candidate;
    TimelineKeyframe key;
    double progress;
    int64_t minimum;
    int64_t maximum;
    int64_t frame;
    size_t exact_index = SIZE_MAX;
    size_t key_index = 0u;
    if (!document || !source || !out_track ||
        anchor_index <= 0 ||
        anchor_index + 1 >= document->spatial_path.numPoints ||
        !scene_editor_light_timeline_path_anchor_progress(
            document, anchor_index, &progress)) {
        return false;
    }
    minimum = document->timeline.range.start_frame + 1;
    maximum = document->timeline.range.start_frame +
              (int64_t)document->timeline.range.frame_count - 2;
    for (size_t i = 0u; i < source->key_count; ++i) {
        double value = source->keys[i].value.as.scalar;
        if (fabs(value - progress) <= epsilon) {
            if (exact_index == SIZE_MAX ||
                llabs(source->keys[i].frame - requested_frame) <
                    llabs(source->keys[exact_index].frame -
                          requested_frame)) {
                exact_index = i;
            }
        } else if (value < progress) {
            if (source->keys[i].frame + 1 > minimum) {
                minimum = source->keys[i].frame + 1;
            }
        } else if (value > progress) {
            if (source->keys[i].frame - 1 < maximum) {
                maximum = source->keys[i].frame - 1;
            }
        }
    }
    if (exact_index != SIZE_MAX) {
        if (exact_index > 0u &&
            source->keys[exact_index - 1u].frame + 1 > minimum) {
            minimum = source->keys[exact_index - 1u].frame + 1;
        }
        if (exact_index + 1u < source->key_count &&
            source->keys[exact_index + 1u].frame - 1 < maximum) {
            maximum = source->keys[exact_index + 1u].frame - 1;
        }
    }
    if (!nearest_open_frame_in_range(
            source, requested_frame, minimum, maximum,
            exact_index, &frame)) {
        return false;
    }
    candidate = *source;
    if (exact_index != SIZE_MAX) {
        if (TimelineTrackMoveScalarKey(
                &candidate, exact_index, frame, progress) !=
            TIMELINE_STATUS_OK) {
            return false;
        }
        key_index = exact_index;
    } else {
        memset(&key, 0, sizeof(key));
        key.frame = frame;
        key.value = TimelineValueScalar(progress);
        key.interpolation_to_next = TIMELINE_INTERPOLATION_LINEAR;
        if (TimelineTrackInsertKey(&candidate, key, &key_index) !=
            TIMELINE_STATUS_OK) {
            return false;
        }
    }
    if (TimelineTrackValidate(
            &candidate, &document->timeline.range) != TIMELINE_STATUS_OK) {
        return false;
    }
    *out_track = candidate;
    if (out_key_index) *out_key_index = key_index;
    return true;
}

static bool add_traversal_key(
    TimelineTrack* track,
    int64_t frame,
    double progress,
    bool last) {
    return track &&
        TimelineTrackAddKey(
            track, frame, TimelineValueScalar(progress),
            last ? TIMELINE_INTERPOLATION_STEP
                 : TIMELINE_INTERPOLATION_LINEAR) ==
            TIMELINE_STATUS_OK;
}

bool scene_editor_light_timeline_build_traversal_track(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* source,
    SceneEditorLightTimelineTraversalMode mode,
    TimelineTrack* out_track) {
    TimelineTrack candidate;
    int point_count;
    int64_t first_frame;
    int64_t last_frame;
    if (!document || !source || !out_track ||
        document->timeline.range.frame_count < 2u ||
        (mode != SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED &&
         mode != SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS)) {
        return false;
    }
    point_count = document->spatial_path.numPoints;
    if (mode == SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS &&
        (point_count < 2 ||
         (uint64_t)point_count > document->timeline.range.frame_count ||
         (size_t)point_count > TIMELINE_TRACK_KEY_CAPACITY)) {
        return false;
    }
    candidate = *source;
    candidate.key_count = 0u;
    memset(candidate.keys, 0, sizeof(candidate.keys));
    first_frame = document->timeline.range.start_frame;
    last_frame = first_frame +
        (int64_t)document->timeline.range.frame_count - 1;
    if (mode == SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED) {
        if (!add_traversal_key(
                &candidate, first_frame, 0.0, false) ||
            !add_traversal_key(
                &candidate, last_frame, 1.0, true)) {
            return false;
        }
    } else {
        for (int i = 0; i < point_count; ++i) {
            double progress;
            double fraction =
                (double)i / (double)(point_count - 1);
            int64_t frame = first_frame +
                (int64_t)llround(
                    fraction *
                    (double)(document->timeline.range.frame_count - 1u));
            int remaining = point_count - i - 1;
            if (i > 0 &&
                frame <= candidate.keys[i - 1].frame) {
                frame = candidate.keys[i - 1].frame + 1;
            }
            if (frame > last_frame - remaining) {
                frame = last_frame - remaining;
            }
            if (!scene_editor_light_timeline_path_anchor_progress(
                    document, i, &progress) ||
                !add_traversal_key(
                    &candidate, frame, progress,
                    i + 1 == point_count)) {
                return false;
            }
        }
    }
    if (TimelineTrackValidate(
            &candidate, &document->timeline.range) !=
        TIMELINE_STATUS_OK) {
        return false;
    }
    *out_track = candidate;
    return true;
}

static bool traversal_tracks_match(
    const TimelineTrack* left,
    const TimelineTrack* right) {
    const double epsilon = 1e-7;
    if (!left || !right || left->key_count != right->key_count) {
        return false;
    }
    for (size_t i = 0u; i < left->key_count; ++i) {
        if (left->keys[i].frame != right->keys[i].frame ||
            left->keys[i].interpolation_to_next !=
                right->keys[i].interpolation_to_next ||
            fabs(left->keys[i].value.as.scalar -
                 right->keys[i].value.as.scalar) > epsilon ||
            fabs(left->keys[i].outgoing_frame_offset -
                 right->keys[i].outgoing_frame_offset) > epsilon ||
            fabs(left->keys[i].outgoing_value_offset -
                 right->keys[i].outgoing_value_offset) > epsilon ||
            fabs(left->keys[i].incoming_frame_offset -
                 right->keys[i].incoming_frame_offset) > epsilon ||
            fabs(left->keys[i].incoming_value_offset -
                 right->keys[i].incoming_value_offset) > epsilon) {
            return false;
        }
    }
    return true;
}

SceneEditorLightTimelineTraversalMode
scene_editor_light_timeline_classify_traversal(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track) {
    TimelineTrack expected;
    if (!document || !track) {
        return SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CUSTOM;
    }
    if (scene_editor_light_timeline_build_traversal_track(
            document, track,
            SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED,
            &expected) &&
        traversal_tracks_match(track, &expected)) {
        return SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED;
    }
    if (scene_editor_light_timeline_build_traversal_track(
            document, track,
            SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS,
            &expected) &&
        traversal_tracks_match(track, &expected)) {
        return SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS;
    }
    return SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CUSTOM;
}
