#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_TRACKS_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_TRACKS_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "editor/scene_editor_light_timeline.h"
#include "import/runtime_scene_light_timeline_io.h"

const char* scene_editor_light_timeline_lane_property_id(
    SceneEditorLightTimelineLane lane);

TimelineStatus scene_editor_light_timeline_lane_track(
    RuntimeSceneLightTimelineDocument* document,
    SceneEditorLightTimelineLane lane,
    TimelineTrack** out_track,
    size_t* out_track_index);

TimelineStatus scene_editor_light_timeline_ensure_intensity_track(
    RuntimeSceneLightTimelineDocument* document,
    double base_intensity,
    size_t* out_track_index);

TimelineStatus scene_editor_light_timeline_validate_lane_track(
    const RuntimeSceneLightTimelineDocument* document,
    SceneEditorLightTimelineLane lane,
    const TimelineTrack* track);

void scene_editor_light_timeline_lane_value_range(
    SceneEditorLightTimelineLane lane,
    const TimelineTrack* track,
    double fallback_value,
    double* out_minimum,
    double* out_maximum);

double scene_editor_light_timeline_lane_value_at_y(
    const SDL_Rect* graph,
    int y,
    double minimum,
    double maximum);

int scene_editor_light_timeline_lane_y_at_value(
    const SDL_Rect* graph,
    double value,
    double minimum,
    double maximum);

#endif
