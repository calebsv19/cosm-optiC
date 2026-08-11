#include "scene_editor_light_timeline_selection.h"

#include <stdio.h>
#include <string.h>

void scene_editor_light_timeline_selection_reset(
    SceneEditorLightTimelineSelection* selection) {
    if (!selection) return;
    memset(selection, 0, sizeof(*selection));
}

TimelineStatus scene_editor_light_timeline_selection_bind(
    SceneEditorLightTimelineSelection* selection,
    const char* target_id) {
    int written;
    if (!selection || !target_id || strncmp(target_id, "light/", 6u) != 0 ||
        target_id[6] == '\0') {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    written = snprintf(selection->target_id, sizeof(selection->target_id),
                       "%s", target_id);
    if (written < 0 || (size_t)written >= sizeof(selection->target_id)) {
        scene_editor_light_timeline_selection_reset(selection);
        return TIMELINE_STATUS_INVALID_ID;
    }
    return TIMELINE_STATUS_OK;
}

TimelineStatus scene_editor_light_timeline_selection_select_index(
    SceneEditorLightTimelineSelection* selection,
    const RuntimeLightSource3D* lights,
    size_t light_count,
    size_t light_index) {
    char target_id[TIMELINE_ID_CAPACITY];
    RuntimeSceneLightTimelineTarget resolved;
    TimelineStatus status;
    if (!selection || !lights || light_index >= light_count ||
        lights[light_index].id[0] == '\0') {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if (snprintf(target_id, sizeof(target_id), "light/%s",
                 lights[light_index].id) >= (int)sizeof(target_id)) {
        return TIMELINE_STATUS_INVALID_ID;
    }
    status = RuntimeSceneLightTimelineResolveTarget(
        lights, light_count, target_id, &resolved);
    if (status != TIMELINE_STATUS_OK) return status;
    return scene_editor_light_timeline_selection_bind(selection, target_id);
}

TimelineStatus scene_editor_light_timeline_selection_resolve(
    const SceneEditorLightTimelineSelection* selection,
    const RuntimeLightSource3D* lights,
    size_t light_count,
    size_t* out_light_index) {
    RuntimeSceneLightTimelineTarget resolved;
    TimelineStatus status;
    if (!selection || !selection->target_id[0] || !lights || !out_light_index) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    status = RuntimeSceneLightTimelineResolveTarget(
        lights, light_count, selection->target_id, &resolved);
    if (status != TIMELINE_STATUS_OK) return status;
    *out_light_index = resolved.light_index;
    return TIMELINE_STATUS_OK;
}

const char* scene_editor_light_timeline_selection_target_id(
    const SceneEditorLightTimelineSelection* selection) {
    return selection ? selection->target_id : "";
}
