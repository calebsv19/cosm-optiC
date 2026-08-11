#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_HISTORY_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_HISTORY_H

#include <stdbool.h>

#include "import/runtime_scene_light_timeline_io.h"

#define SCENE_EDITOR_LIGHT_TIMELINE_HISTORY_CAPACITY 24u

typedef struct SceneEditorLightTimelineChange {
    char target_id[TIMELINE_ID_CAPACITY];
    char property_id[TIMELINE_ID_CAPACITY];
    bool before_exists;
    TimelineTrack before;
    bool after_exists;
    TimelineTrack after;
} SceneEditorLightTimelineChange;

typedef struct SceneEditorLightTimelineHistory {
    SceneEditorLightTimelineChange
        undo[SCENE_EDITOR_LIGHT_TIMELINE_HISTORY_CAPACITY];
    size_t undo_count;
    SceneEditorLightTimelineChange
        redo[SCENE_EDITOR_LIGHT_TIMELINE_HISTORY_CAPACITY];
    size_t redo_count;
} SceneEditorLightTimelineHistory;

void scene_editor_light_timeline_history_reset(
    SceneEditorLightTimelineHistory* history);

TimelineStatus scene_editor_light_timeline_history_record(
    SceneEditorLightTimelineHistory* history,
    const TimelineTrack* before,
    const TimelineTrack* after);

TimelineStatus scene_editor_light_timeline_history_update_after(
    SceneEditorLightTimelineHistory* history,
    const TimelineTrack* after);

TimelineStatus scene_editor_light_timeline_history_undo(
    SceneEditorLightTimelineHistory* history,
    RuntimeSceneLightTimelineDocument* document);

TimelineStatus scene_editor_light_timeline_history_redo(
    SceneEditorLightTimelineHistory* history,
    RuntimeSceneLightTimelineDocument* document);

#endif
