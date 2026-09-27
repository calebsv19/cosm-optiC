#include "app/scene_timeline_session.h"
#include <stdio.h>
#include <string.h>

TimelineStatus SceneTimelineSessionInit(SceneTimelineSession* session,
    TimelineRate rate, TimelineRange range, uint64_t scene_revision,
    uint64_t timeline_revision) {
    if (!session) return TIMELINE_STATUS_INVALID_ARGUMENT;
    SceneTimelineSession candidate = {0};
    TimelineStatus status = PreviewTransportInit(&candidate.transport, rate, range);
    if (status != TIMELINE_STATUS_OK) return status;
    candidate.scene_revision = scene_revision;
    candidate.timeline_revision = timeline_revision;
    *session = candidate;
    return TIMELINE_STATUS_OK;
}

TimelineStatus SceneTimelineSessionSelect(SceneTimelineSession* session,
    const TimelineEntityBindings* bindings, const TimelinePropertyRegistry* registry,
    const char* target_id, const char* property_id) {
    if (!session || !session->transport.valid || !property_id ||
        strnlen(property_id, TIMELINE_ID_CAPACITY) >= TIMELINE_ID_CAPACITY)
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (session->editing) return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
    const TimelineEntityBinding* binding = NULL;
    TimelineStatus status = TimelineEntityBindingsFind(bindings, target_id, &binding);
    if (status != TIMELINE_STATUS_OK) return status;
    if (property_id[0]) {
        const TimelinePropertyDescriptor* descriptor = NULL;
        status = TimelinePropertyRegistryFind(registry, property_id, &descriptor);
        if (status != TIMELINE_STATUS_OK) return status;
        if (descriptor->target_kind != binding->kind)
            return TIMELINE_STATUS_TARGET_KIND_MISMATCH;
    }
    snprintf(session->selected_entity, sizeof(session->selected_entity), "%s", binding->entity_id);
    snprintf(session->selected_target, sizeof(session->selected_target), "%s", target_id);
    snprintf(session->selected_property, sizeof(session->selected_property), "%s", property_id);
    session->selection_missing = false;
    session->selection_editable = TimelineEntityBindingCanAuthor(binding, property_id);
    return TIMELINE_STATUS_OK;
}

TimelineStatus SceneTimelineSessionReconcile(SceneTimelineSession* session,
    const TimelineEntityBindings* bindings, uint64_t scene_revision,
    uint64_t timeline_revision) {
    if (!session || !session->transport.valid || !bindings)
        return TIMELINE_STATUS_INVALID_ARGUMENT;
    const TimelineEntityBinding* binding = NULL;
    TimelineStatus status = TIMELINE_STATUS_OK;
    if (session->selected_target[0]) {
        status = TimelineEntityBindingsFind(bindings, session->selected_target, &binding);
        if (status == TIMELINE_STATUS_OK &&
            strcmp(binding->entity_id, session->selected_entity) != 0)
            status = TIMELINE_STATUS_TARGET_NOT_FOUND;
    }
    if (status != TIMELINE_STATUS_OK && status != TIMELINE_STATUS_TARGET_NOT_FOUND &&
        status != TIMELINE_STATUS_DUPLICATE_ID) return status;
    if (scene_revision != session->scene_revision ||
        timeline_revision != session->timeline_revision || status != TIMELINE_STATUS_OK)
        session->editing = false;
    session->scene_revision = scene_revision;
    session->timeline_revision = timeline_revision;
    session->selection_missing = status != TIMELINE_STATUS_OK;
    session->selection_editable = status == TIMELINE_STATUS_OK &&
        TimelineEntityBindingCanAuthor(binding, session->selected_property);
    if (!session->selection_editable) session->editing = false;
    return status;
}

TimelineStatus SceneTimelineSessionSeek(SceneTimelineSession* session, TimelineSample sample) {
    if (!session) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (session->editing) return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
    return PreviewTransportSeek(&session->transport, sample, session->transport.direction);
}

TimelineStatus SceneTimelineSessionSetPlaying(SceneTimelineSession* session, bool playing) {
    if (!session) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (session->editing && playing) return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
    return playing ? PreviewTransportPlay(&session->transport) : PreviewTransportPause(&session->transport);
}

TimelineStatus SceneTimelineSessionAdvance(SceneTimelineSession* session, double elapsed_seconds) {
    if (!session) return TIMELINE_STATUS_INVALID_ARGUMENT;
    /* The transport implementation owns playback math. Commit only success. */
    PreviewTransport candidate = session->transport;
    TimelineStatus status = PreviewTransportAdvance(&candidate, elapsed_seconds);
    if (status == TIMELINE_STATUS_OK) session->transport = candidate;
    return status;
}

TimelineStatus SceneTimelineSessionBeginEdit(SceneTimelineSession* session,
    uint64_t scene_revision, uint64_t timeline_revision) {
    if (!session || !session->transport.valid) return TIMELINE_STATUS_INVALID_ARGUMENT;
    if (session->selection_missing || !session->selected_target[0])
        return TIMELINE_STATUS_TARGET_NOT_FOUND;
    if (!session->selection_editable) return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
    if (session->editing || session->scene_revision != scene_revision ||
        session->timeline_revision != timeline_revision)
        return TIMELINE_STATUS_OWNERSHIP_MISMATCH;
    PreviewTransportPause(&session->transport);
    session->editing = true;
    return TIMELINE_STATUS_OK;
}

void SceneTimelineSessionEndEdit(SceneTimelineSession* session) {
    if (session) session->editing = false;
}
