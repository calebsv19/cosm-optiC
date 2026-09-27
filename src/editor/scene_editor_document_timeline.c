#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_document.h"
#include "import/scene_timeline_document_io.h"
#include "scene_editor_document_transaction.h"
#include <stdio.h>

static json_object* member(json_object* root, const char* key, bool create) {
    json_object* value = NULL;
    if (!root || !json_object_is_type(root, json_type_object)) return NULL;
    if (json_object_object_get_ex(root, key, &value))
        return json_object_is_type(value, json_type_object) ? value : NULL;
    if (!create) return NULL;
    value = json_object_new_object();
    if (value && json_object_object_add(root, key, value) != 0) {
        json_object_put(value);
        return NULL;
    }
    return value;
}
static json_object* authoring(bool create) {
    return member(member(member(document_authoring_root(), "extensions", create),
        "ray_tracing", create), "authoring", create);
}
TimelineStatus SceneEditorDocumentGetTimeline(TimelineDocument* out) {
    if (!out || !SceneEditorDocumentIsOpen()) return TIMELINE_STATUS_INVALID_ARGUMENT;
    json_object* root = authoring(false);
    json_object* timeline = NULL;
    if (!root || !json_object_object_get_ex(root, "scene_timeline", &timeline))
        return TIMELINE_STATUS_TARGET_NOT_FOUND;
    return SceneTimelineDocumentFromJson(timeline, out);
}
bool SceneEditorDocumentSetTimelineWithLight(const TimelineDocument* document,
    const RuntimeSceneLightTimelineDocument* light, unsigned long long expected_revision, char* diagnostics, size_t size) {
    if (!SceneEditorDocumentIsOpen() || SceneEditorDocumentRevision() != expected_revision) {
        if (diagnostics && size) snprintf(diagnostics, size, "scene revision changed; refresh timeline before editing");
        return false;
    }
    json_object* timeline = SceneTimelineDocumentToJson(document);
    if (!timeline) {
        if (diagnostics && size) snprintf(diagnostics, size, "invalid scene timeline document");
        return false;
    }
    if (!document_begin_command(diagnostics, size)) {
        json_object_put(timeline);
        return false;
    }
    json_object* root = authoring(true);
    if (!root || json_object_object_add(root, "scene_timeline", timeline) != 0) {
        json_object_put(timeline);
        document_rollback_command();
        if (diagnostics && size) snprintf(diagnostics, size, "cannot retain scene timeline");
        return false;
    }
    if(light) {
        json_object* spatial=RuntimeSceneLightTimelineToJsonObject(light,SceneEditorDocumentWorldScale());
        if(!spatial) {document_rollback_command();return false;}
        json_object_object_add(root,"light_timeline",spatial);
    }
    return document_finish_command(diagnostics, size);
}

bool SceneEditorDocumentSetTimeline(const TimelineDocument* document,
    unsigned long long revision,char* diagnostics,size_t size) {
    return SceneEditorDocumentSetTimelineWithLight(document,NULL,revision,diagnostics,size);
}
