#ifndef SCENE_EDITOR_DOCUMENT_TIMELINE_H
#define SCENE_EDITOR_DOCUMENT_TIMELINE_H
#include "animation/timeline_document.h"

/* Authored units. Missing timeline returns TARGET_NOT_FOUND without changing out. */
TimelineStatus SceneEditorDocumentGetTimeline(TimelineDocument* out);
/* One revision-checked retained command, participates in scene undo/redo/save. */
bool SceneEditorDocumentSetTimeline(const TimelineDocument* document,
    unsigned long long expected_revision, char* diagnostics, size_t size);
#endif
