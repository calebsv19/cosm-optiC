#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_CURVE_EDIT_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_CURVE_EDIT_H

#include "scene_editor_light_timeline_view.h"

typedef enum SceneEditorLightTimelineHandle {
    SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_NONE = 0,
    SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_INCOMING = -1,
    SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING = 1
} SceneEditorLightTimelineHandle;

TimelineStatus scene_editor_light_timeline_set_interpolation(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    TimelineInterpolation interpolation,
    TimelineTrack* out_track);

bool scene_editor_light_timeline_handle_point(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    SceneEditorLightTimelineHandle handle,
    const SDL_Rect* graph,
    int* out_x,
    int* out_y);

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
    int* out_y);

SceneEditorLightTimelineHandle scene_editor_light_timeline_pick_handle(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    const SDL_Rect* graph,
    int x,
    int y);

SceneEditorLightTimelineHandle scene_editor_light_timeline_pick_scalar_handle(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    const SDL_Rect* graph,
    double minimum,
    double maximum,
    int x,
    int y);

TimelineStatus scene_editor_light_timeline_move_handle(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    size_t key_index,
    SceneEditorLightTimelineHandle handle,
    const SDL_Rect* graph,
    int x,
    int y,
    TimelineTrack* out_track);

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
    TimelineTrack* out_track);

TimelineStatus scene_editor_light_timeline_constrain_adjacent_handles(
    const RuntimeSceneLightTimelineDocument* document,
    TimelineTrack* track,
    size_t key_index);

#endif
