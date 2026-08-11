#include "scene_editor_light_timeline_curve_edit.h"

#include "animation/timeline_light_motion.h"
#include "scene_editor_light_timeline_tracks.h"

#include <math.h>

static double clamp_double(double value, double minimum, double maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static bool handle_is_active(const TimelineTrack* track, size_t key_index,
                             SceneEditorLightTimelineHandle handle) {
    if (!track || key_index >= track->key_count) return false;
    if (handle == SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING) {
        return key_index + 1u < track->key_count &&
            track->keys[key_index].interpolation_to_next ==
                TIMELINE_INTERPOLATION_CUBIC_BEZIER;
    }
    if (handle == SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_INCOMING) {
        return key_index > 0u &&
            track->keys[key_index - 1u].interpolation_to_next ==
                TIMELINE_INTERPOLATION_CUBIC_BEZIER;
    }
    return false;
}

TimelineStatus scene_editor_light_timeline_set_interpolation(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    TimelineInterpolation interpolation,
    TimelineTrack* out_track) {
    TimelineTrack candidate;
    TimelineKeyframe* left;
    TimelineKeyframe* right;
    double frame_span;
    double value_span;
    if (!document || !track || !out_track ||
        key_index + 1u >= track->key_count ||
        (interpolation != TIMELINE_INTERPOLATION_STEP &&
         interpolation != TIMELINE_INTERPOLATION_LINEAR &&
         interpolation != TIMELINE_INTERPOLATION_CUBIC_BEZIER)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    candidate = *track;
    left = &candidate.keys[key_index];
    right = &candidate.keys[key_index + 1u];
    left->interpolation_to_next = interpolation;
    if (interpolation == TIMELINE_INTERPOLATION_CUBIC_BEZIER &&
        left->outgoing_frame_offset == 0.0 &&
        left->outgoing_value_offset == 0.0 &&
        right->incoming_frame_offset == 0.0 &&
        right->incoming_value_offset == 0.0) {
        frame_span = (double)(right->frame - left->frame);
        value_span = right->value.as.scalar - left->value.as.scalar;
        left->outgoing_frame_offset = frame_span / 3.0;
        left->outgoing_value_offset = value_span / 3.0;
        right->incoming_frame_offset = -frame_span / 3.0;
        right->incoming_value_offset = -value_span / 3.0;
    }
    SceneEditorLightTimelineLane lane =
        strcmp(track->property_id, "light/intensity") == 0
            ? SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY
            : SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION;
    if (scene_editor_light_timeline_validate_lane_track(
            document, lane, &candidate) != TIMELINE_STATUS_OK) {
        return TIMELINE_STATUS_INVALID_TRACK;
    }
    *out_track = candidate;
    return TIMELINE_STATUS_OK;
}

bool scene_editor_light_timeline_handle_point(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    SceneEditorLightTimelineHandle handle,
    const SDL_Rect* graph,
    int* out_x,
    int* out_y) {
    return scene_editor_light_timeline_scalar_handle_point(
        view, document, track, key_index, handle, graph,
        0.0, 1.0, out_x, out_y);
}

bool scene_editor_light_timeline_scalar_handle_point(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    SceneEditorLightTimelineHandle handle,
    const SDL_Rect* graph,
    double minimum,
    double maximum,
    int* out_x,
    int* out_y) {
    const TimelineKeyframe* key;
    double frame;
    double value;
    double normalized;
    if (!view || !document || !track || !graph ||
        !handle_is_active(track, key_index, handle) ||
        document->timeline.range.frame_count < 2u) {
        return false;
    }
    key = &track->keys[key_index];
    frame = (double)key->frame +
        (handle == SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_INCOMING
             ? key->incoming_frame_offset
             : key->outgoing_frame_offset);
    value = key->value.as.scalar +
        (handle == SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_INCOMING
             ? key->incoming_value_offset
             : key->outgoing_value_offset);
    normalized = (frame -
        (double)document->timeline.range.start_frame) /
        (double)(document->timeline.range.frame_count - 1u);
    if (out_x) {
        *out_x = scene_editor_light_timeline_view_x_at_normalized(
            view, graph, normalized);
    }
    if (out_y) {
        *out_y = scene_editor_light_timeline_y_at_value(
            graph, value, minimum, maximum);
    }
    return true;
}

SceneEditorLightTimelineHandle scene_editor_light_timeline_pick_handle(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    const SDL_Rect* graph,
    int x,
    int y) {
    return scene_editor_light_timeline_pick_scalar_handle(
        view, document, track, key_index, graph, 0.0, 1.0, x, y);
}

SceneEditorLightTimelineHandle scene_editor_light_timeline_pick_scalar_handle(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    const SDL_Rect* graph,
    double minimum,
    double maximum,
    int x,
    int y) {
    const SceneEditorLightTimelineHandle handles[] = {
        SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_INCOMING,
        SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING
    };
    SceneEditorLightTimelineHandle best =
        SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_NONE;
    double best_distance = 9.0 * 9.0;
    for (size_t i = 0u; i < sizeof(handles) / sizeof(handles[0]); ++i) {
        int hx = 0;
        int hy = 0;
        double dx;
        double dy;
        double distance;
        if (!scene_editor_light_timeline_scalar_handle_point(
                view, document, track, key_index, handles[i], graph,
                minimum, maximum,
                &hx, &hy)) continue;
        dx = (double)(hx - x);
        dy = (double)(hy - y);
        distance = dx * dx + dy * dy;
        if (distance <= best_distance) {
            best_distance = distance;
            best = handles[i];
        }
    }
    return best;
}

TimelineStatus scene_editor_light_timeline_move_handle(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    SceneEditorLightTimelineHandle handle,
    const SDL_Rect* graph,
    int x,
    int y,
    TimelineTrack* out_track) {
    return scene_editor_light_timeline_move_scalar_handle(
        view, document, track, key_index, handle, graph,
        0.0, 1.0, x, y, out_track);
}

TimelineStatus scene_editor_light_timeline_move_scalar_handle(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    SceneEditorLightTimelineHandle handle,
    const SDL_Rect* graph,
    double minimum,
    double maximum,
    int x,
    int y,
    TimelineTrack* out_track) {
    TimelineTrack candidate;
    TimelineKeyframe* key;
    TimelineKeyframe* left;
    TimelineKeyframe* right;
    double normalized;
    double frame;
    double value;
    double left_control_frame;
    double right_control_frame;
    double left_control_value;
    double right_control_value;
    if (!view || !document || !track || !graph || !out_track ||
        !handle_is_active(track, key_index, handle)) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    normalized = scene_editor_light_timeline_view_normalized_at_x(
        view, graph, x);
    frame = (double)document->timeline.range.start_frame +
        normalized *
            (double)(document->timeline.range.frame_count - 1u);
    value = scene_editor_light_timeline_value_at_y(
        graph, y, minimum, maximum);
    candidate = *track;
    key = &candidate.keys[key_index];
    if (handle == SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING) {
        left = key;
        right = &candidate.keys[key_index + 1u];
        right_control_frame =
            (double)right->frame + right->incoming_frame_offset;
        right_control_value =
            right->value.as.scalar + right->incoming_value_offset;
        frame = clamp_double(frame, (double)left->frame,
                             right_control_frame);
        if (strcmp(track->property_id, "light/intensity") == 0) {
            value = clamp_double(value, minimum, maximum);
        } else {
            value = clamp_double(value, left->value.as.scalar,
                                 right_control_value);
        }
        left->outgoing_frame_offset = frame - (double)left->frame;
        left->outgoing_value_offset = value - left->value.as.scalar;
    } else {
        right = key;
        left = &candidate.keys[key_index - 1u];
        left_control_frame =
            (double)left->frame + left->outgoing_frame_offset;
        left_control_value =
            left->value.as.scalar + left->outgoing_value_offset;
        frame = clamp_double(frame, left_control_frame,
                             (double)right->frame);
        if (strcmp(track->property_id, "light/intensity") == 0) {
            value = clamp_double(value, minimum, maximum);
        } else {
            value = clamp_double(value, left_control_value,
                                 right->value.as.scalar);
        }
        right->incoming_frame_offset = frame - (double)right->frame;
        right->incoming_value_offset = value - right->value.as.scalar;
    }
    SceneEditorLightTimelineLane lane =
        strcmp(track->property_id, "light/intensity") == 0
            ? SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY
            : SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION;
    if (scene_editor_light_timeline_validate_lane_track(
            document, lane, &candidate) != TIMELINE_STATUS_OK) {
        return TIMELINE_STATUS_INVALID_TRACK;
    }
    *out_track = candidate;
    return TIMELINE_STATUS_OK;
}

static void constrain_cubic_segment(
    TimelineTrack* track,
    size_t left_index,
    SceneEditorLightTimelineLane lane) {
    TimelineKeyframe* left;
    TimelineKeyframe* right;
    double x0;
    double x1;
    double x2;
    double x3;
    double y0;
    double y1;
    double y2;
    double y3;
    if (!track || left_index + 1u >= track->key_count ||
        track->keys[left_index].interpolation_to_next !=
            TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
        return;
    }
    left = &track->keys[left_index];
    right = &track->keys[left_index + 1u];
    x0 = (double)left->frame;
    x3 = (double)right->frame;
    x1 = clamp_double(x0 + left->outgoing_frame_offset, x0, x3);
    x2 = clamp_double(x3 + right->incoming_frame_offset, x1, x3);
    y0 = left->value.as.scalar;
    y3 = right->value.as.scalar;
    if (lane == SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY) {
        y1 = fmax(0.0, y0 + left->outgoing_value_offset);
        y2 = fmax(0.0, y3 + right->incoming_value_offset);
    } else {
        y1 = clamp_double(y0 + left->outgoing_value_offset, y0, y3);
        y2 = clamp_double(y3 + right->incoming_value_offset, y1, y3);
    }
    left->outgoing_frame_offset = x1 - x0;
    left->outgoing_value_offset = y1 - y0;
    right->incoming_frame_offset = x2 - x3;
    right->incoming_value_offset = y2 - y3;
}

TimelineStatus scene_editor_light_timeline_constrain_adjacent_handles(
    const RuntimeSceneLightTimelineDocument* document,
    TimelineTrack* track,
    size_t key_index) {
    if (!document || !track || key_index >= track->key_count) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    SceneEditorLightTimelineLane lane =
        strcmp(track->property_id, "light/intensity") == 0
            ? SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY
            : SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION;
    if (key_index > 0u) {
        constrain_cubic_segment(track, key_index - 1u, lane);
    }
    constrain_cubic_segment(track, key_index, lane);
    return scene_editor_light_timeline_validate_lane_track(
        document, lane, track);
}
