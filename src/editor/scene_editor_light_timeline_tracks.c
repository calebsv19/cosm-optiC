#include "scene_editor_light_timeline_tracks.h"

#include "animation/timeline_light_motion.h"
#include "animation/timeline_property_registry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static double clamp_double(double value, double minimum, double maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

const char* scene_editor_light_timeline_lane_property_id(
    SceneEditorLightTimelineLane lane) {
    return lane == SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY
        ? "light/intensity"
        : "light/path_progress";
}

TimelineStatus scene_editor_light_timeline_lane_track(
    RuntimeSceneLightTimelineDocument* document,
    SceneEditorLightTimelineLane lane,
    TimelineTrack** out_track,
    size_t* out_track_index) {
    size_t index = SIZE_MAX;
    TimelineStatus status;
    if (!document || !out_track) return TIMELINE_STATUS_INVALID_ARGUMENT;
    status = RuntimeSceneLightTimelineFindTrack(
        document, scene_editor_light_timeline_lane_property_id(lane), &index);
    if (status != TIMELINE_STATUS_OK) return status;
    *out_track = &document->timeline.tracks[index];
    if (out_track_index) *out_track_index = index;
    return TIMELINE_STATUS_OK;
}

TimelineStatus scene_editor_light_timeline_ensure_intensity_track(
    RuntimeSceneLightTimelineDocument* document,
    double base_intensity,
    size_t* out_track_index) {
    TimelineTrack track;
    TimelineStatus status;
    size_t index = SIZE_MAX;
    int64_t last_frame;
    const TimelineTrack* progress;
    if (!document || !document->valid || !isfinite(base_intensity) ||
        base_intensity < 0.0 ||
        document->progress_track_index >= document->timeline.track_count) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    status = RuntimeSceneLightTimelineFindTrack(
        document, "light/intensity", &index);
    if (status == TIMELINE_STATUS_OK) {
        document->has_intensity_track = true;
        document->intensity_track_index = index;
        if (out_track_index) *out_track_index = index;
        return TIMELINE_STATUS_OK;
    }
    if (status != TIMELINE_STATUS_TARGET_NOT_FOUND) return status;
    if (document->timeline.track_count >= 2u) {
        return TIMELINE_STATUS_CAPACITY_EXCEEDED;
    }
    progress = &document->timeline.tracks[document->progress_track_index];
    memset(&track, 0, sizeof(track));
    status = TimelineTrackInit(
        &track, "selected_light_intensity", progress->target_id,
        "light/intensity", TIMELINE_VALUE_SCALAR);
    if (status != TIMELINE_STATUS_OK) return status;
    status = TimelineTrackSetUnit(
        &track, TIMELINE_UNIT_RELATIVE_INTENSITY);
    if (status != TIMELINE_STATUS_OK) return status;
    last_frame = document->timeline.range.start_frame +
        (int64_t)document->timeline.range.frame_count - 1;
    status = TimelineTrackAddKey(
        &track, document->timeline.range.start_frame,
        TimelineValueScalar(base_intensity), TIMELINE_INTERPOLATION_LINEAR);
    if (status == TIMELINE_STATUS_OK) {
        status = TimelineTrackAddKey(
            &track, last_frame, TimelineValueScalar(base_intensity),
            TIMELINE_INTERPOLATION_STEP);
    }
    if (status == TIMELINE_STATUS_OK) {
        status = TimelineDocumentAddTrack(&document->timeline, &track);
    }
    if (status != TIMELINE_STATUS_OK) return status;
    document->loaded_schema_version =
        RUNTIME_SCENE_LIGHT_TIMELINE_SCHEMA_VERSION;
    document->has_intensity_track = true;
    document->intensity_track_index = document->timeline.track_count - 1u;
    status = RuntimeSceneLightTimelineValidateDocument(document);
    if (status != TIMELINE_STATUS_OK) return status;
    if (out_track_index) *out_track_index = document->intensity_track_index;
    return TIMELINE_STATUS_OK;
}

TimelineStatus scene_editor_light_timeline_validate_lane_track(
    const RuntimeSceneLightTimelineDocument* document,
    SceneEditorLightTimelineLane lane,
    const TimelineTrack* track) {
    TimelinePropertyRegistry registry;
    TimelineStatus status;
    if (!document || !track ||
        strcmp(track->property_id,
               scene_editor_light_timeline_lane_property_id(lane)) != 0) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if (lane == SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION) {
        return TimelineLightMotionValidateProgressTrack(
            track, &document->timeline.range);
    }
    status = TimelinePropertyRegistryInitFoundationDefaults(&registry);
    if (status != TIMELINE_STATUS_OK) return status;
    status = TimelinePropertyRegistryValidateTrack(
        &registry, track, &document->timeline.range);
    if (status != TIMELINE_STATUS_OK) return status;
    for (size_t i = 0u; i + 1u < track->key_count; ++i) {
        if (track->keys[i].interpolation_to_next ==
            TIMELINE_INTERPOLATION_CUBIC_BEZIER) {
            double outgoing =
                track->keys[i].value.as.scalar +
                track->keys[i].outgoing_value_offset;
            double incoming =
                track->keys[i + 1u].value.as.scalar +
                track->keys[i + 1u].incoming_value_offset;
            if (!isfinite(outgoing) || !isfinite(incoming) ||
                outgoing < 0.0 || incoming < 0.0) {
                return TIMELINE_STATUS_VALUE_OUT_OF_RANGE;
            }
        }
    }
    return TIMELINE_STATUS_OK;
}

void scene_editor_light_timeline_lane_value_range(
    SceneEditorLightTimelineLane lane,
    const TimelineTrack* track,
    double fallback_value,
    double* out_minimum,
    double* out_maximum) {
    double minimum = 0.0;
    double maximum = lane == SCENE_EDITOR_LIGHT_TIMELINE_LANE_MOTION
        ? 1.0
        : (isfinite(fallback_value) && fallback_value > 0.0
               ? fallback_value
               : 1.0);
    if (lane == SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY && track) {
        for (size_t i = 0u; i < track->key_count; ++i) {
            double value = track->keys[i].value.as.scalar;
            double incoming = value + track->keys[i].incoming_value_offset;
            double outgoing = value + track->keys[i].outgoing_value_offset;
            if (isfinite(value) && value > maximum) maximum = value;
            if (isfinite(incoming) && incoming > maximum) maximum = incoming;
            if (isfinite(outgoing) && outgoing > maximum) maximum = outgoing;
        }
        maximum *= 1.25;
        if (maximum < 1.0) maximum = 1.0;
    }
    if (out_minimum) *out_minimum = minimum;
    if (out_maximum) *out_maximum = maximum;
}

double scene_editor_light_timeline_lane_value_at_y(
    const SDL_Rect* graph,
    int y,
    double minimum,
    double maximum) {
    double normalized;
    if (!graph || graph->h <= 0 || !isfinite(minimum) ||
        !isfinite(maximum) || maximum <= minimum) {
        return minimum;
    }
    normalized = clamp_double(
        1.0 - (double)(y - graph->y) / (double)graph->h, 0.0, 1.0);
    return minimum + normalized * (maximum - minimum);
}

int scene_editor_light_timeline_lane_y_at_value(
    const SDL_Rect* graph,
    double value,
    double minimum,
    double maximum) {
    double normalized;
    if (!graph || graph->h <= 0 || !isfinite(value) ||
        !isfinite(minimum) || !isfinite(maximum) || maximum <= minimum) {
        return graph ? graph->y + graph->h : 0;
    }
    normalized = clamp_double(
        (value - minimum) / (maximum - minimum), 0.0, 1.0);
    return graph->y + graph->h -
        (int)llround(normalized * (double)graph->h);
}
