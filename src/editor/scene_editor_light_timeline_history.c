#include "scene_editor_light_timeline_history.h"

#include <stdio.h>
#include <string.h>

static void push_change(SceneEditorLightTimelineChange* stack,
                        size_t* count,
                        const SceneEditorLightTimelineChange* change) {
    if (*count >= SCENE_EDITOR_LIGHT_TIMELINE_HISTORY_CAPACITY) {
        memmove(&stack[0], &stack[1],
                (SCENE_EDITOR_LIGHT_TIMELINE_HISTORY_CAPACITY - 1u) *
                    sizeof(stack[0]));
        *count = SCENE_EDITOR_LIGHT_TIMELINE_HISTORY_CAPACITY - 1u;
    }
    stack[(*count)++] = *change;
}

static bool same_identity(const TimelineTrack* track,
                          const SceneEditorLightTimelineChange* change) {
    return track && change &&
        strcmp(track->target_id, change->target_id) == 0 &&
        strcmp(track->property_id, change->property_id) == 0;
}

static TimelineStatus remove_track(RuntimeSceneLightTimelineDocument* document,
                                   size_t index) {
    if (!document || index >= document->timeline.track_count) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    if (index + 1u < document->timeline.track_count) {
        memmove(&document->timeline.tracks[index],
                &document->timeline.tracks[index + 1u],
                (document->timeline.track_count - index - 1u) *
                    sizeof(document->timeline.tracks[0]));
    }
    document->timeline.track_count -= 1u;
    memset(&document->timeline.tracks[document->timeline.track_count], 0,
           sizeof(document->timeline.tracks[0]));
    return TIMELINE_STATUS_OK;
}

static TimelineStatus apply_change(
    RuntimeSceneLightTimelineDocument* document,
    const SceneEditorLightTimelineChange* change,
    bool forward) {
    const bool expected_exists =
        forward ? change->before_exists : change->after_exists;
    const bool replacement_exists =
        forward ? change->after_exists : change->before_exists;
    const TimelineTrack* expected = forward ? &change->before : &change->after;
    const TimelineTrack* replacement = forward ? &change->after : &change->before;
    size_t index = SIZE_MAX;
    TimelineStatus status;
    RuntimeSceneLightTimelineDocument candidate;
    if (!document || !change) return TIMELINE_STATUS_INVALID_ARGUMENT;
    candidate = *document;
    status = RuntimeSceneLightTimelineFindTrack(
        &candidate, change->property_id, &index);
    if (expected_exists) {
        if (status != TIMELINE_STATUS_OK ||
            !same_identity(&candidate.timeline.tracks[index], change) ||
            memcmp(&candidate.timeline.tracks[index], expected,
                   sizeof(*expected)) != 0) {
            return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
        }
    } else if (status != TIMELINE_STATUS_TARGET_NOT_FOUND) {
        return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
    }
    if (expected_exists && replacement_exists) {
        candidate.timeline.tracks[index] = *replacement;
    } else if (expected_exists) {
        status = remove_track(&candidate, index);
        if (status != TIMELINE_STATUS_OK) return status;
    } else if (replacement_exists) {
        status = TimelineDocumentAddTrack(&candidate.timeline, replacement);
        if (status != TIMELINE_STATUS_OK) return status;
    } else {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    candidate.loaded_schema_version =
        RUNTIME_SCENE_LIGHT_TIMELINE_SCHEMA_VERSION;
    status = RuntimeSceneLightTimelineFindTrack(
        &candidate, "light/path_progress", &candidate.progress_track_index);
    if (status != TIMELINE_STATUS_OK) return status;
    status = RuntimeSceneLightTimelineFindTrack(
        &candidate, "light/intensity", &candidate.intensity_track_index);
    candidate.has_intensity_track = status == TIMELINE_STATUS_OK;
    if (!candidate.has_intensity_track) {
        candidate.intensity_track_index = 0u;
        if (status != TIMELINE_STATUS_TARGET_NOT_FOUND) return status;
    }
    status = RuntimeSceneLightTimelineValidateDocument(&candidate);
    if (status != TIMELINE_STATUS_OK) return status;
    *document = candidate;
    return TIMELINE_STATUS_OK;
}

void scene_editor_light_timeline_history_reset(
    SceneEditorLightTimelineHistory* history) {
    if (history) memset(history, 0, sizeof(*history));
}

TimelineStatus scene_editor_light_timeline_history_record(
    SceneEditorLightTimelineHistory* history,
    const TimelineTrack* before,
    const TimelineTrack* after) {
    SceneEditorLightTimelineChange change;
    const TimelineTrack* identity = before ? before : after;
    if (!history || !identity || (!before && !after) ||
        (before && after &&
         (strcmp(before->target_id, after->target_id) != 0 ||
          strcmp(before->property_id, after->property_id) != 0))) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    memset(&change, 0, sizeof(change));
    snprintf(change.target_id, sizeof(change.target_id), "%s",
             identity->target_id);
    snprintf(change.property_id, sizeof(change.property_id), "%s",
             identity->property_id);
    change.before_exists = before != NULL;
    if (before) change.before = *before;
    change.after_exists = after != NULL;
    if (after) change.after = *after;
    push_change(history->undo, &history->undo_count, &change);
    history->redo_count = 0u;
    return TIMELINE_STATUS_OK;
}

TimelineStatus scene_editor_light_timeline_history_update_after(
    SceneEditorLightTimelineHistory* history,
    const TimelineTrack* after) {
    SceneEditorLightTimelineChange* change;
    if (!history || !after || history->undo_count == 0u) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    change = &history->undo[history->undo_count - 1u];
    if (strcmp(change->target_id, after->target_id) != 0 ||
        strcmp(change->property_id, after->property_id) != 0) {
        return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
    }
    change->after_exists = true;
    change->after = *after;
    return TIMELINE_STATUS_OK;
}

static TimelineStatus restore(SceneEditorLightTimelineHistory* history,
                              RuntimeSceneLightTimelineDocument* document,
                              bool undo) {
    SceneEditorLightTimelineChange* source =
        undo ? history->undo : history->redo;
    size_t* source_count =
        undo ? &history->undo_count : &history->redo_count;
    SceneEditorLightTimelineChange* destination =
        undo ? history->redo : history->undo;
    size_t* destination_count =
        undo ? &history->redo_count : &history->undo_count;
    SceneEditorLightTimelineChange change;
    TimelineStatus status;
    if (!history || !document || *source_count == 0u) {
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    }
    change = source[*source_count - 1u];
    status = apply_change(document, &change, !undo);
    if (status != TIMELINE_STATUS_OK) return status;
    *source_count -= 1u;
    push_change(destination, destination_count, &change);
    return TIMELINE_STATUS_OK;
}

TimelineStatus scene_editor_light_timeline_history_undo(
    SceneEditorLightTimelineHistory* history,
    RuntimeSceneLightTimelineDocument* document) {
    return restore(history, document, true);
}

TimelineStatus scene_editor_light_timeline_history_redo(
    SceneEditorLightTimelineHistory* history,
    RuntimeSceneLightTimelineDocument* document) {
    return restore(history, document, false);
}
