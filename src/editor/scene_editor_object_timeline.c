#include "editor/scene_editor_timeline_selection.h"
#include "motion/scene_motion_paths.h"
#include "editor/scene_editor_object_timeline.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_object_commands.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_workspace_profile.h"
#include "import/runtime_scene_object_timeline.h"
#include "import/runtime_scene_timeline.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <stdio.h>
#include <string.h>
static SDL_Rect button,frame_button;
static char label[180],feedback[256];
void SceneEditorObjectTimelineBindings(TimelineEntityBindings* bindings) {
    SceneEditorDocumentObjectInfo object;
    for(int i=0;i<SceneEditorDocumentObjectCount();++i) if(SceneEditorDocumentObjectAt(i,&object)) {
        char target[TIMELINE_ID_CAPACITY];int n=snprintf(target,sizeof(target),"object/%s",object.id);
        if(n>0 && (size_t)n<sizeof(target)) TimelineEntityBindingsAdd(bindings,object.id,target,TIMELINE_PROPERTY_TARGET_OBJECT,true,false);
    }
}
bool SceneEditorObjectTimelineEditable(const char* target,char* diagnostics,size_t size) {
    if(strncmp(target,"object/",7)) return true;
    SceneEditorDocumentObjectInfo info;
    if(SceneEditorDocumentObjectById(target+7,&info) && !info.locked) return true;
    snprintf(diagnostics,size,"Object is missing or locked; unlock it in Scene.");return false;
}
bool SceneEditorObjectTimelineAdd(const char* id,char* diagnostics,size_t size) {
    SceneEditorDocumentObjectInfo info;SceneEditorDocumentTransform transform;
    static TimelineDocument doc;
    if(!SceneEditorDocumentObjectById(id,&info) || info.runtime_index<0 || info.locked || !info.visible ||
       !SceneEditorDocumentGetTransformForSceneIndex(info.runtime_index,&transform,diagnostics,size)) {
        snprintf(diagnostics,size,"Select an unlocked visible object in Scene first.");return false;
    }
    if(!SceneEditorTimelineActivate() || SceneEditorDocumentGetTimeline(&doc)!=TIMELINE_STATUS_OK) {
        snprintf(diagnostics,size,"%s",SceneEditorTimelineStatus());return false;
    }
    char target[TIMELINE_ID_CAPACITY];int n=snprintf(target,sizeof(target),"object/%s",id);
    if(n<=0 || (size_t)n>=sizeof(target)) {snprintf(diagnostics,size,"Object ID exceeds timeline capacity.");return false;}
    for(size_t i=0;i<doc.track_count;++i) if(doc.tracks[i].enabled && !strcmp(doc.tracks[i].target_id,target) && !strcmp(doc.tracks[i].property_id,MOTION_PROGRESS_PROPERTY)) {
        snprintf(diagnostics,size,"Object follows a path. Detach it in Paths to use XYZ.");return false;
    }
    for(size_t i=0;i<doc.track_count;++i) if(!strcmp(doc.tracks[i].target_id,target) && RuntimeObjectTimelineAxis(doc.tracks[i].property_id)>=0) {
        SceneEditorTimelineSelectTrack(i);SceneEditorRenderAuthoringSetTiming(true);return true;
    }
    size_t first=doc.track_count;
    const char* properties[]={"object/transform/position_x","object/transform/position_y","object/transform/position_z"};
    for(int axis=0;axis<3;++axis) {
        TimelineTrack track;char track_id[TIMELINE_ID_CAPACITY];unsigned number=0;bool collision;
        do {snprintf(track_id,sizeof(track_id),"object-channel-%u",number++);collision=false;
            for(size_t i=0;i<doc.track_count;++i) if(!strcmp(doc.tracks[i].track_id,track_id)) collision=true;
        } while(collision);
        if(TimelineTrackInit(&track,track_id,target,properties[axis],TIMELINE_VALUE_SCALAR)!=TIMELINE_STATUS_OK ||
           TimelineTrackSetUnit(&track,TIMELINE_UNIT_WORLD_DISTANCE)!=TIMELINE_STATUS_OK ||
           TimelineTrackAddKey(&track,doc.range.start_frame,TimelineValueScalar(transform.position[axis]),TIMELINE_INTERPOLATION_LINEAR)!=TIMELINE_STATUS_OK ||
           TimelineDocumentAddTrack(&doc,&track)!=TIMELINE_STATUS_OK) {snprintf(diagnostics,size,"Cannot add position channels; capacity or value invalid.");return false;}
    }
    if(!SceneEditorDocumentSetTimeline(&doc,SceneEditorDocumentRevision(),diagnostics,size)) return false;
    SceneEditorTimelineSelectTrack(first);SceneEditorRenderAuthoringSetTiming(true);SceneEditorTimelinePause();
    SceneEditorTimelineSeek(doc.range.start_frame);SceneEditorTimelineSelectKey(doc.range.start_frame,false);
    snprintf(diagnostics,size,"Position ready. Set XYZ at another frame.");return true;
}
bool SceneEditorObjectTimelinePosition(const char* id,double position[3]) {
    TimelineSample sample;TimelineRate rate;TimelineRange range;TimelineEvaluationContext context;TimelineVec3 value;
    if(!SceneEditorTimelineCurrentSample(&sample) ||
       RuntimeSceneTimelineClock(&rate,&range)!=TIMELINE_STATUS_OK ||
       TimelineEvaluationContextBuild(rate,range,sample,&context)!=TIMELINE_STATUS_OK ||
       RuntimeObjectTimelinePosition(id,&context,&value)!=TIMELINE_STATUS_OK) return false;
    position[0]=value.x;position[1]=value.y;position[2]=value.z;return true;
}
bool SceneEditorObjectTimelineRotation(const char* id,double rotation[3]) {
    TimelineSample sample;TimelineRate rate;TimelineRange range;TimelineEvaluationContext context;TimelineVec3 value;
    if(!SceneEditorTimelineCurrentSample(&sample) ||
       RuntimeSceneTimelineClock(&rate,&range)!=TIMELINE_STATUS_OK ||
       TimelineEvaluationContextBuild(rate,range,sample,&context)!=TIMELINE_STATUS_OK ||
       !RuntimeObjectTimelineRotation(id,&context,&value)) return false;
    rotation[0]=value.x;rotation[1]=value.y;rotation[2]=value.z;return true;
}
void SceneEditorObjectTimelineDraw(SDL_Renderer* renderer,SDL_Rect pane,int* y) {
    SceneEditorObjectReadback selected;SceneEditorObjectInspect(&selected);
    snprintf(label,sizeof(label),"Animate object: %s",selected.has_selection?selected.selection.name:"select in Scene");
    button=(SDL_Rect){pane.x+10,*y,pane.w-20,32};
    SceneEditorRenderButton(renderer,button,label,false,selected.has_selection);*y+=38;
    if(button.y<pane.y || button.y+button.h>pane.y+pane.h) button=(SDL_Rect){0};
    frame_button=(SDL_Rect){pane.x+10,*y,pane.w-20,30};
    SceneEditorRenderButton(renderer,frame_button,"Frame selected object",false,selected.has_selection);*y+=36;
    if(frame_button.y<pane.y || frame_button.y+frame_button.h>pane.y+pane.h) frame_button=(SDL_Rect){0};
    if(feedback[0]) {ray_tracing_text_draw_utf8_at(renderer,ray_tracing_font_runtime_get_ui_regular(renderer,11,8),feedback,pane.x+10,*y,(SDL_Color){210,215,225,255});*y+=26;}
}
bool SceneEditorObjectTimelineEvent(SceneEditor* editor,SDL_Event* event) {
    (void)editor;
    if(event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT &&
       frame_button.w>0 && SDL_PointInRect(&(SDL_Point){event->button.x,event->button.y},&frame_button)) {
        SceneEditorFrameViewport(true);return true;
    }
    if(event->type!=SDL_MOUSEBUTTONDOWN || event->button.button!=SDL_BUTTON_LEFT || button.w<=0 ||
       event->button.x<button.x || event->button.x>=button.x+button.w || event->button.y<button.y || event->button.y>=button.y+button.h) return false;
    SceneEditorObjectReadback selected;SceneEditorObjectInspect(&selected);
    if(selected.has_selection) SceneEditorObjectTimelineAdd(selected.selection.id,feedback,sizeof(feedback));
    return true;
}
bool SceneEditorObjectTimelineControl(const char* name,SDL_Rect* out) {
    if(!strcmp(name,"frame_object")) {*out=frame_button;return frame_button.w>0;}
    if(!strcmp(name,"animate_object")) {*out=button;return button.w>0;}return false;
}

bool SceneEditorObjectTimelineFrameOffset(int scene_index,double delta[3]) {
    SceneEditorDocumentObjectInfo info;SceneEditorDocumentTransform base;char diagnostic[128];
    for(int i=0;i<SceneEditorDocumentObjectCount();++i) if(SceneEditorDocumentObjectAt(i,&info) && info.runtime_index==scene_index) {
        if(!SceneEditorObjectTimelinePosition(info.id,delta) ||
           !SceneEditorDocumentGetTransformForSceneIndex(scene_index,&base,diagnostic,sizeof(diagnostic))) return false;
        double scale=SceneEditorDocumentWorldScale();
        for(int axis=0;axis<3;++axis) delta[axis]-=base.position[axis]*scale;
        return true;
    }
    return false;
}
