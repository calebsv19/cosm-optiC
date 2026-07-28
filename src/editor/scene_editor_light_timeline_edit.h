#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_EDIT_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_EDIT_H

#include <stdbool.h>
#include <stdint.h>

#include "import/runtime_scene_light_timeline_io.h"

typedef enum SceneEditorLightTimelineTraversalMode {
    SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED = 0,
    SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS = 1,
    SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CUSTOM = 2
} SceneEditorLightTimelineTraversalMode;

bool scene_editor_light_timeline_ensure_editable_default_range(
    RuntimeSceneLightTimelineDocument* document);

bool scene_editor_light_timeline_nearest_open_frame(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    int64_t requested,
    int64_t* out_frame);

bool scene_editor_light_timeline_evaluate_progress_at_frame(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    int64_t frame,
    double* out_progress);

bool scene_editor_light_timeline_path_anchor_progress(
    const RuntimeSceneLightTimelineDocument* document,
    int anchor_index,
    double* out_progress);

double scene_editor_light_timeline_snap_progress_to_anchor(
    const RuntimeSceneLightTimelineDocument* document,
    double progress,
    double tolerance,
    int* out_anchor_index);

bool scene_editor_light_timeline_frame_at_progress(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    double progress,
    double* out_frame);

bool scene_editor_light_timeline_set_path_anchor_frame(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* source,
    int anchor_index,
    int64_t requested_frame,
    TimelineTrack* out_track,
    size_t* out_key_index);

bool scene_editor_light_timeline_build_traversal_track(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* source,
    SceneEditorLightTimelineTraversalMode mode,
    TimelineTrack* out_track);

SceneEditorLightTimelineTraversalMode
scene_editor_light_timeline_classify_traversal(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track);

#endif
