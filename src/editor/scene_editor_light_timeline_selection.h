#ifndef RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_SELECTION_H
#define RAY_TRACING_SCENE_EDITOR_LIGHT_TIMELINE_SELECTION_H

#include "import/runtime_scene_light_timeline_bridge.h"

typedef struct SceneEditorLightTimelineSelection {
    char target_id[TIMELINE_ID_CAPACITY];
} SceneEditorLightTimelineSelection;

void scene_editor_light_timeline_selection_reset(
    SceneEditorLightTimelineSelection* selection);

TimelineStatus scene_editor_light_timeline_selection_bind(
    SceneEditorLightTimelineSelection* selection,
    const char* target_id);

TimelineStatus scene_editor_light_timeline_selection_select_index(
    SceneEditorLightTimelineSelection* selection,
    const RuntimeLightSource3D* lights,
    size_t light_count,
    size_t light_index);

TimelineStatus scene_editor_light_timeline_selection_resolve(
    const SceneEditorLightTimelineSelection* selection,
    const RuntimeLightSource3D* lights,
    size_t light_count,
    size_t* out_light_index);

const char* scene_editor_light_timeline_selection_target_id(
    const SceneEditorLightTimelineSelection* selection);

#endif
