#ifndef SCENE_EDITOR_MOTION_TRAIL_H
#define SCENE_EDITOR_MOTION_TRAIL_H
#include "editor/scene_editor.h"
#include "animation/timeline_document.h"
#define MOTION_TRAIL_KEY_CAPACITY (3*TIMELINE_TRACK_KEY_CAPACITY)
typedef struct SceneEditorMotionTrail {
  char target[TIMELINE_ID_CAPACITY];
  TimelineTrack axes[3];
  TimelineRate rate;TimelineRange range;
  int64_t frames[MOTION_TRAIL_KEY_CAPACITY];size_t count;
  unsigned long long revision;
} SceneEditorMotionTrail;
bool SceneEditorMotionTrailRead(const char *target,SceneEditorMotionTrail *out);
bool SceneEditorMotionTrailSample(const SceneEditorMotionTrail *trail,double frame,double xyz[3]);
bool SceneEditorMotionTrailSetKey(const char *target,int64_t frame,const double xyz[3],unsigned long long revision,char *message,size_t size);
void SceneEditorMotionTrailDraw(SceneEditor *editor,const SceneEditorPaneLayout *layout);
bool SceneEditorMotionTrailEvent(SceneEditor *editor,SDL_Event *event,const SceneEditorPaneLayout *layout);
bool SceneEditorMotionTrailControl(const char *name,SDL_Rect *out);
void SceneEditorMotionTrailReset(void);
#endif
