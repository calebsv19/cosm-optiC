#include "editor/scene_editor_shared_text.h"
#include "editor/scene_editor_shared_ui.h"
#include "editor/scene_editor_shared_fields.h"
/* Compact settings surface; shared retained commands own every mutation. */
#include "scene_editor_timeline_settings_panel.h"
#include "scene_editor_timeline_commands.h"
#include "editor/scene_editor_timeline_manage.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_object_timeline.h"
#include "import/runtime_scene_object_timeline.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_chrome_shell.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
enum {TOGGLE,NUMERATOR,DENOMINATOR,START,COUNT,PREVIOUS,NEXT,ENABLED,REMOVE,NCONTROLS};
static SDL_Rect controls[NCONTROLS];static char labels[NCONTROLS][160],draft[64],feedback[256],selected[TIMELINE_ID_CAPACITY],path[4096];
static bool expanded;static int editing=-1;static unsigned long long revision,shown_revision;
bool SceneEditorTimelineSettingsActive(void) {return editing>=0;}
void SceneEditorTimelineSettingsReset(void) {if(editing>=0) do{scene_editor_shared_field_end(draft);SDL_StopTextInput();}while(0);editing=-1;expanded=false;selected[0]=0;feedback[0]=0;memset(controls,0,sizeof(controls));}
static void text(SDL_Renderer* r,const char* s,int x,int y) {scene_editor_shared_label_at(r,s,x,y,SceneEditorChromeShellResolvePalette().text_primary,11);}
static size_t resolve(const TimelineDocument* doc) {
    for(size_t i=0;doc && i<doc->track_count;++i) if(!strcmp(selected,doc->tracks[i].track_id)) return i;
    if(doc && doc->track_count) {snprintf(selected,sizeof(selected),"%s",doc->tracks[0].track_id);return 0;}selected[0]=0;return SIZE_MAX;
}
int SceneEditorTimelineSettingsDraw(SDL_Renderer* r,SDL_Rect pane,int y) {
    if(strcmp(path,SceneEditorDocumentPath())) {SceneEditorTimelineSettingsReset();snprintf(path,sizeof(path),"%s",SceneEditorDocumentPath());}
    shown_revision=SceneEditorDocumentRevision();memset(controls,0,sizeof(controls));const TimelineDocument* doc=SceneEditorTimelineDocumentView(NULL);int x=pane.x+10,w=pane.w-20;
    controls[TOGGLE]=(SDL_Rect){x,y,w,30};SceneEditorSharedUiPanelButton(r,182,128+(TOGGLE),controls[TOGGLE],"Timeline settings",expanded,doc!=NULL);y+=36;
    if(!expanded || !doc) goto done;
    snprintf(labels[NUMERATOR],160,"FPS numerator: %u",doc->rate.frames_per_second_numerator);
    snprintf(labels[DENOMINATOR],160,"FPS denominator: %u",doc->rate.frames_per_second_denominator);
    snprintf(labels[START],160,"Start frame: %lld",(long long)doc->range.start_frame);
    snprintf(labels[COUNT],160,"Frame count: %llu",(unsigned long long)doc->range.frame_count);
    for(int i=NUMERATOR;i<=COUNT;++i) {controls[i]=(SDL_Rect){x,y,w,28};if(editing==i) snprintf(labels[i],160,"Enter integer: %s_",draft);SceneEditorSharedUiPanelButton(r,182,256+(i),controls[i],labels[i],editing==i,true);y+=32;}
    size_t index=resolve(doc);controls[PREVIOUS]=(SDL_Rect){x,y,(w-6)/2,28};controls[NEXT]=(SDL_Rect){x+(w+6)/2,y,(w-6)/2,28};
    SceneEditorSharedUiPanelButton(r,182,384+(PREVIOUS),controls[PREVIOUS],"Previous channel",false,index!=SIZE_MAX);SceneEditorSharedUiPanelButton(r,182,512+(NEXT),controls[NEXT],"Next channel",false,index!=SIZE_MAX);y+=34;
    snprintf(labels[0],160,"Channel: %s",selected[0]?selected:"none");text(r,labels[0],x,y);y+=22;
    if(index!=SIZE_MAX) {
        const TimelineTrack* t=&doc->tracks[index];bool xyz=RuntimeObjectTimelineAxis(t->property_id)>=0;
        snprintf(labels[ENABLED],160,"%s %s",t->enabled?"Disable":"Enable",xyz?"XYZ position channels":"channel");
        controls[ENABLED]=(SDL_Rect){x,y,w,28};SceneEditorSharedUiPanelButton(r,182,640+(ENABLED),controls[ENABLED],labels[ENABLED],false,true);y+=32;
        controls[REMOVE]=(SDL_Rect){x,y,w,28};SceneEditorSharedUiPanelButton(r,182,768+(REMOVE),controls[REMOVE],xyz?"Remove XYZ channels":"Remove channel",false,true);y+=34;
    }
    if(feedback[0]) {text(r,feedback,x,y);y+=24;}
    text(r,"Keys keep their frames. Undo restores edits.",x,y);y+=26;
done:
    for(int i=0;i<NCONTROLS;++i) if(controls[i].y<pane.y || controls[i].y+controls[i].h>pane.y+pane.h) controls[i]=(SDL_Rect){0};
    return y;
}
bool SceneEditorTimelineSettingsEvent(SDL_Event* e) {
    if(!e) return false;
    if(e->type==SDL_WINDOWEVENT && (e->window.event==SDL_WINDOWEVENT_FOCUS_LOST || e->window.event==SDL_WINDOWEVENT_SIZE_CHANGED)) {SceneEditorTimelineSettingsReset();return false;}
    if(editing>=0 && revision!=SceneEditorDocumentRevision()) {editing=-1;do{scene_editor_shared_field_end(draft);SDL_StopTextInput();}while(0);}
    if((editing>=0) && scene_editor_shared_field_event(draft,sizeof(draft),e))return true;
    if(editing>=0 && e->type==SDL_TEXTINPUT) {if(strlen(draft)+strlen(e->text.text)<sizeof(draft)) strcat(draft,e->text.text);return true;}
    if(editing>=0 && e->type==SDL_KEYDOWN) {
        SDL_Keycode key=e->key.keysym.sym;
        if(key==SDLK_ESCAPE) {editing=-1;do{scene_editor_shared_field_end(draft);SDL_StopTextInput();}while(0);return true;}
        if(key==SDLK_BACKSPACE) {size_t n=strlen(draft);if(n) draft[n-1]=0;return true;}
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER) {
            char* end;errno=0;long long v=strtoll(draft,&end,10);const TimelineDocument* doc=SceneEditorTimelineDocumentView(NULL);int field=editing;editing=-1;bool ok=false;
            if(!errno && end!=draft && !*end && doc && (field==START || v>0)) {
                TimelineRate rate=doc->rate;TimelineRange range=doc->range;
                if(field==NUMERATOR && v<=1000000) rate.frames_per_second_numerator=(uint32_t)v;
                else if(field==DENOMINATOR && v<=1000000) rate.frames_per_second_denominator=(uint32_t)v;
                else if(field==START) range.start_frame=v;
                else if(field==COUNT) range.frame_count=(uint64_t)v;
                else goto invalid;
                ok=SceneEditorTimelineSetClock(rate,range,revision,feedback,sizeof(feedback));
            }
invalid:
            if(ok) {do{scene_editor_shared_field_end(draft);SDL_StopTextInput();}while(0);snprintf(feedback,sizeof(feedback),"Clock applied. Keys unchanged.");}
            else {editing=field;if(!feedback[0]) snprintf(feedback,sizeof(feedback),"Invalid integer or range; Esc cancels.");}
        }return true;
    }
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT) return false;
    SDL_Point point={e->button.x,e->button.y};
    for(int i=0;i<NCONTROLS;++i) if(controls[i].w && SDL_PointInRect(&point,&controls[i])) {
        if(shown_revision!=SceneEditorDocumentRevision()) {snprintf(feedback,sizeof(feedback),"Scene changed; review the refreshed settings.");return true;}
        if(editing>=0) {editing=-1;do{scene_editor_shared_field_end(draft);SDL_StopTextInput();}while(0);}SceneEditorTimelineReleaseFocus();SceneEditorTimelinePause();
        const TimelineDocument* doc=SceneEditorTimelineDocumentView(NULL);if(!doc) return true;
        if(i==TOGGLE) expanded=!expanded;
        else if(i>=NUMERATOR && i<=COUNT) {editing=i;revision=SceneEditorDocumentRevision();draft[0]=0;feedback[0]=0;do{scene_editor_shared_field_begin(draft,sizeof(draft),0);SDL_StartTextInput();}while(0);}
        else {
            size_t index=resolve(doc);if(index==SIZE_MAX) return true;
            if(i==PREVIOUS || i==NEXT) {index=(index+doc->track_count+(i==PREVIOUS?-1:1))%doc->track_count;snprintf(selected,sizeof(selected),"%s",doc->tracks[index].track_id);}
            else {
                bool ok=i==REMOVE?SceneEditorTimelineRemoveChannel(selected,SceneEditorDocumentRevision(),feedback,sizeof(feedback)):
                    SceneEditorTimelineSetChannelEnabled(selected,!doc->tracks[index].enabled,SceneEditorDocumentRevision(),feedback,sizeof(feedback));
                if(ok) snprintf(feedback,sizeof(feedback),"Channel edit applied. Undo restores it.");
            }
        }return true;
    }
    return false;
}
bool SceneEditorTimelineSettingsControl(const char* name,SDL_Rect* out) {
    const char* names[]={"timeline_settings","clock_numerator","clock_denominator","clock_start","clock_count","channel_previous","channel_next","channel_enabled","channel_remove"};
    for(int i=0;i<NCONTROLS;++i) if(!strcmp(name,names[i])) {*out=controls[i];return out->w>0;}return false;
}
