#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_document.h"
#include "scene_editor_object_timeline_panel.h"
#include "editor/scene_editor_object_timeline.h"
#include "editor/scene_editor_motion_trail.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_timeline_selection.h"
#include "editor/scene_editor_document_timeline.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_chrome_shell.h"
#include "import/runtime_scene_object_timeline.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    char target[TIMELINE_ID_CAPACITY],draft[64],labels[6][180],message[180];
    unsigned long long revision;
    int64_t frame;
    TimelineSample sample;
    double position[3];
    int editing;
    bool valid,changed;
    SDL_Rect axes[3],apply;
} panel={.editing=-1};

bool SceneEditorObjectTimelinePanelPending(void) {return SceneEditorWorkspaceProfileGet()==SCENE_WORKSPACE_RENDER && panel.valid && (panel.changed || panel.editing>=0);}
void SceneEditorObjectTimelinePanelReset(void) {
    if(panel.editing>=0) SDL_StopTextInput();
    memset(&panel,0,sizeof(panel));panel.editing=-1;
}
static bool sync_panel(void) {
    TimelineTrack selected;TimelineRate rate;TimelineRange range;TimelineSample sample;
    if(!SceneEditorTimelineSelectedTrack(&selected,&rate,&range,&sample) ||
       strncmp(selected.target_id,"object/",7)) {SceneEditorObjectTimelinePanelReset();return false;}
    unsigned long long revision=SceneEditorDocumentRevision();
    if(panel.valid && panel.revision==revision && panel.frame==sample.absolute_frame &&
       panel.sample.subframe_numerator==sample.subframe_numerator && panel.sample.subframe_denominator==sample.subframe_denominator &&
       !strcmp(panel.target,selected.target_id)) return true;
    SceneEditorObjectTimelinePanelReset();
    static TimelineDocument doc;
    TimelineEvaluationContext context;unsigned axes=0;
    if(SceneEditorDocumentGetTimeline(&doc)!=TIMELINE_STATUS_OK ||
       TimelineEvaluationContextBuild(rate,range,sample,&context)!=TIMELINE_STATUS_OK) return false;
    for(size_t i=0;i<doc.track_count;++i) {
        TimelineTrack* track=&doc.tracks[i];int axis=RuntimeObjectTimelineAxis(track->property_id);
        TimelineEvaluationResult result;
        if(axis>=0 && track->enabled && !strcmp(track->target_id,selected.target_id) &&
           TimelineTrackEvaluate(track,&context,&result)==TIMELINE_STATUS_OK) {
            panel.position[axis]=result.value.as.scalar;axes|=1u<<axis;
        }
    }
    if(axes!=7) return false;
    snprintf(panel.target,sizeof(panel.target),"%s",selected.target_id);
    panel.revision=revision;panel.sample=sample;panel.frame=sample.absolute_frame;panel.valid=true;return true;
}
static bool finish_field(void) {
    if(panel.editing<0) return true;
    char* end;double value=strtod(panel.draft,&end);
    if(end==panel.draft || *end || !isfinite(value)) {
        snprintf(panel.message,sizeof(panel.message),"Enter a finite number; Escape cancels.");return false;
    }
    panel.position[panel.editing]=value;panel.changed=true;panel.editing=-1;
    panel.message[0]=0;SDL_StopTextInput();return true;
}
static bool set_position_key(void) {
    int64_t frame=panel.frame;
    if(!SceneEditorMotionTrailSetKey(panel.target,frame,panel.position,panel.revision,panel.message,sizeof(panel.message)))return false;
    panel.valid=false;sync_panel();SceneEditorTimelineSelectKey(frame,false);
    snprintf(panel.message,sizeof(panel.message),"Keyed frame %lld. Scrub to preview; Save to keep.",(long long)frame);return true;
}
bool SceneEditorObjectTimelinePanelEvent(SDL_Event* e) {
    if(!sync_panel()) return false;
    if(e->type==SDL_WINDOWEVENT && e->window.event==SDL_WINDOWEVENT_FOCUS_LOST) {SceneEditorObjectTimelinePanelReset();return false;}
    if(e->type==SDL_TEXTINPUT && panel.editing>=0) {
        if(strlen(panel.draft)+strlen(e->text.text)<sizeof(panel.draft)) strcat(panel.draft,e->text.text);return true;
    }
    if(e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_ESCAPE && panel.changed && panel.editing<0) {SceneEditorObjectTimelinePanelReset();return true;}
    if(e->type==SDL_KEYDOWN && panel.editing>=0) {
        SDL_Keycode key=e->key.keysym.sym;
        if(key==SDLK_ESCAPE) {panel.editing=-1;SDL_StopTextInput();}
        else if(key==SDLK_BACKSPACE) {size_t n=strlen(panel.draft);if(n) panel.draft[n-1]=0;}
        else if(key==SDLK_RETURN || key==SDLK_KP_ENTER || key==SDLK_TAB) finish_field();
        return true;
    }
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT) return false;
    SDL_Point point={e->button.x,e->button.y};
    for(int i=0;i<3;++i) if(panel.axes[i].w>0 && SDL_PointInRect(&point,&panel.axes[i])) {
        if(!finish_field()) return true;
        SceneEditorTimelineReleaseFocus();SceneEditorTimelinePause();panel.editing=i;panel.draft[0]=0;SDL_StartTextInput();return true;
    }
    if(panel.apply.w>0 && SDL_PointInRect(&point,&panel.apply)) {
        if(finish_field()) set_position_key();return true;
    }
    if(panel.editing>=0 && !finish_field()) return true;
    return false;
}
int SceneEditorObjectTimelinePanelDraw(SDL_Renderer* r,SDL_Rect rect) {
    if(!sync_panel()) return rect.y;
    int y=rect.y;
    snprintf(panel.labels[0],sizeof(panel.labels[0]),"Position (%s) at playhead%s",SceneEditorDocumentUnitLabel(),panel.changed?" — draft":"");
    ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,11,8),panel.labels[0],rect.x,y,SceneEditorChromeShellResolvePalette().text_primary);y+=22;
    for(int i=0;i<3;++i) {
        if(panel.editing==i) snprintf(panel.labels[i+1],sizeof(panel.labels[i+1]),"%c: %s_",'X'+i,panel.draft);
        else snprintf(panel.labels[i+1],sizeof(panel.labels[i+1]),"%c: %.8g",'X'+i,panel.position[i]);
        panel.axes[i]=(SDL_Rect){rect.x,y,rect.w,26};
        SceneEditorRenderButton(r,panel.axes[i],panel.labels[i+1],panel.editing==i,true);y+=29;
    }
    snprintf(panel.labels[4],sizeof(panel.labels[4]),"%s position key at frame %lld",panel.changed?"Apply":"Set",(long long)panel.frame);
    panel.apply=(SDL_Rect){rect.x,y,rect.w,28};SceneEditorRenderButton(r,panel.apply,panel.labels[4],panel.changed,true);y+=32;
    snprintf(panel.labels[5],sizeof(panel.labels[5]),"%s",panel.message[0]?panel.message:panel.changed?"Draft ready — click Apply above. Escape cancels.":"Click XYZ; Enter stages edits; Set key applies.");
    ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,10,8),panel.labels[5],rect.x,y,SceneEditorChromeShellResolvePalette().text_primary);y+=24;
    for(int i=0;i<3;++i) if(panel.axes[i].y+panel.axes[i].h>rect.y+rect.h) panel.axes[i]=(SDL_Rect){0};
    if(panel.apply.y+panel.apply.h>rect.y+rect.h) panel.apply=(SDL_Rect){0};
    return y;
}
bool SceneEditorObjectTimelinePanelControl(const char* name,SDL_Rect* out) {
    const char* names[]={"position_x","position_y","position_z"};
    for(int i=0;i<3;++i) if(!strcmp(name,names[i])) {*out=panel.axes[i];return out->w>0;}
    if(!strcmp(name,"set_position_key")) {*out=panel.apply;return out->w>0;}
    return false;
}
