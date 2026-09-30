#ifndef SCENE_EDITOR_TIMELINE_SELECTION_H
#define SCENE_EDITOR_TIMELINE_SELECTION_H
#include "animation/timeline_document.h"
/* Session selection is separate from evaluated time. Keys are addressed by
 * stable track ID + frame and bound to the retained document revision. */
typedef struct {
    char track_id[TIMELINE_ID_CAPACITY];
    size_t count;
    int64_t frames[TIMELINE_TRACK_KEY_CAPACITY];
    TimelineKeyframe primary;
    unsigned long long revision;
} SceneEditorTimelineKeySelection;
bool SceneEditorTimelineSelectionRead(SceneEditorTimelineKeySelection* out);
bool SceneEditorTimelineSelectKey(int64_t frame,bool toggle);
void SceneEditorTimelineClearKeys(void);
bool SceneEditorTimelineKeySelected(const char* track_id,int64_t frame);
bool SceneEditorTimelineSelectAllKeys(void);
bool SceneEditorTimelineNavigateKey(int direction);
bool SceneEditorTimelineMoveSelectedKeys(int64_t primary_frame);
bool SceneEditorTimelineSetSelectedValue(double value);
bool SceneEditorTimelineMoveSelectedValue(int64_t frame,double value);
bool SceneEditorTimelineDeleteSelectedKeys(void);
bool SceneEditorTimelineSelectedTangentMode(TimelineTangentMode mode);
bool SceneEditorTimelineSelectedInterpolation(TimelineInterpolation mode);
bool SceneEditorTimelineSelectedHandles(double fi,double vi,double fo,double vo);
bool SceneEditorTimelineCopyKeys(void);
bool SceneEditorTimelineCanPasteKeys(void);
bool SceneEditorTimelinePasteKeys(void);
bool SceneEditorTimelineDuplicateKeys(void);
const char* SceneEditorTimelineSelectionStatus(void);
/* Called on document replacement, not on playhead movement or lost UI focus. */
void SceneEditorTimelineSelectionReset(void);
#endif
