#ifndef RAY_TRACING_SCENE_TIMELINE_SESSION_H
#define RAY_TRACING_SCENE_TIMELINE_SESSION_H

#include "animation/timeline_entity_binding.h"
#include "app/preview_transport.h"

/* Presentation state only. No authored data, persistence or scene mutation. */
typedef struct SceneTimelineSession {
    PreviewTransport transport;
    uint64_t scene_revision;
    uint64_t timeline_revision;
    char selected_entity[TIMELINE_ENTITY_ID_CAPACITY];
    char selected_target[TIMELINE_ID_CAPACITY];
    char selected_property[TIMELINE_ID_CAPACITY];
    bool selection_missing;
    bool selection_editable;
    bool editing;
} SceneTimelineSession;

TimelineStatus SceneTimelineSessionInit(SceneTimelineSession* session,
    TimelineRate rate, TimelineRange range, uint64_t scene_revision,
    uint64_t timeline_revision);
TimelineStatus SceneTimelineSessionSelect(SceneTimelineSession* session,
    const TimelineEntityBindings* bindings, const TimelinePropertyRegistry* registry,
    const char* target_id, const char* property_id);
/* Refresh after an authored edit. A removed target remains visibly unresolved,
 * never replaced by another array entry. A changed binding cancels a gesture. */
TimelineStatus SceneTimelineSessionReconcile(SceneTimelineSession* session,
    const TimelineEntityBindings* bindings, uint64_t scene_revision,
    uint64_t timeline_revision);
TimelineStatus SceneTimelineSessionSeek(SceneTimelineSession* session, TimelineSample sample);
TimelineStatus SceneTimelineSessionSetPlaying(SceneTimelineSession* session, bool playing);
TimelineStatus SceneTimelineSessionAdvance(SceneTimelineSession* session, double elapsed_seconds);
TimelineStatus SceneTimelineSessionBeginEdit(SceneTimelineSession* session,
    uint64_t scene_revision, uint64_t timeline_revision);
void SceneTimelineSessionEndEdit(SceneTimelineSession* session);

#endif
