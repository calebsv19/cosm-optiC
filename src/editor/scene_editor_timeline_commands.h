#ifndef SCENE_EDITOR_TIMELINE_COMMANDS_H
#define SCENE_EDITOR_TIMELINE_COMMANDS_H
#include "animation/timeline_document.h"
/* Internal bridge: one retained-document owner for UI and semantic commands. */
const TimelineDocument* SceneEditorTimelineDocumentView(size_t* selected);
bool SceneEditorTimelineCommitTrack(const TimelineTrack* track,unsigned long long revision);
#endif
