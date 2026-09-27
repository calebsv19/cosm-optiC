#ifndef SCENE_EDITOR_TIMELINE_UI_H
#define SCENE_EDITOR_TIMELINE_UI_H
#include "scene_editor_timeline_view.h"
#include "editor/scene_editor_pane_host.h"
#include "app/scene_timeline_session.h"
#define TL_ROW_CAPACITY (TIMELINE_DOCUMENT_TRACK_CAPACITY*2)
typedef struct {size_t track; bool group; char target[TIMELINE_ID_CAPACITY];} TimelineRow;
typedef struct {
    TimelineView view;
    TimelineLayout layout;
    TimelineRow rows[TL_ROW_CAPACITY];size_t row_count,row_offset;
    char collapsed[TIMELINE_DOCUMENT_TRACK_CAPACITY][TIMELINE_ID_CAPACITY];size_t collapsed_count;
    bool curves,focused,scrubbing,dragging,panning,menu,had_selection;
    int64_t shown_key;
    size_t numeric_count;
    int pan_x,numeric;char draft[64],feedback[256];
    int64_t drag_frame,drag_origin;unsigned long long revision;
    char drag_track[TIMELINE_ID_CAPACITY],shown_track[TIMELINE_ID_CAPACITY];
} TimelineUI;
void SceneEditorTimelineUIReset(void);
void SceneEditorTimelineUIReleaseFocus(void);
void SceneEditorTimelineUIFocus(void);
bool SceneEditorTimelineUIEvent(SDL_Event*,const SceneEditorPaneLayout*,const TimelineDocument*,const SceneTimelineSession*,size_t);
void SceneEditorTimelineUIDraw(SDL_Renderer*,const SceneEditorPaneLayout*,const TimelineDocument*,const SceneTimelineSession*,size_t);
void SceneEditorTimelineDrawDock(SDL_Renderer*,const TimelineUI*,const TimelineDocument*,const SceneTimelineSession*,size_t);
#endif
