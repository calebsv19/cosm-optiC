#include "editor/scene_editor_shared_text.h"
#include "editor/scene_editor_shared_ui.h"
#include "editor/scene_editor_shared_fields.h"
#include "editor/scene_editor_render_control.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_timeline.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static bool open,result_verified;
static int field=-1;
static SceneEditorRenderOptions draft_options={320,200,1,0,0,0};
static SDL_Rect toggle,panel,fields[4],mode,start,cancel,results_button,close_button;
static char panel_job_id[64];
static char input[32],rows[4][96],status[512],feedback[256],result_summary[128];
bool SceneEditorRenderPanelDraftActive(void) {return field>=0;}
bool SceneEditorRenderPanelResultVerified(void) {return result_verified;}
static bool hit(SDL_Rect r,int x,int y) {return r.w>0 && x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
static void reset(void) {field=-1;do{scene_editor_shared_field_end(input);SDL_StopTextInput();}while(0);}
void SceneEditorRenderPanelReset(void) {reset();open=false;feedback[0]=0;}
bool SceneEditorRenderPanelControl(const char* name,SDL_Rect* out) {
    if(!name || !out) return false;
    if(!strcmp(name,"toggle")) *out=toggle;else if(!strcmp(name,"start")) *out=start;else if(!strcmp(name,"cancel")) *out=cancel;else if(!strcmp(name,"results")) *out=results_button;else if(!strcmp(name,"mode")) *out=mode;else if(!strcmp(name,"close")) *out=close_button;else {
        const char* names[]={"width","height","temporal_frames","frame"};for(int i=0;i<4;++i) if(!strcmp(name,names[i])) {*out=fields[i];return out->w>0;}return false;
    }return out->w>0;
}
bool SceneEditorRenderPanelEvent(SDL_Event* e) {
    if(!e) return false;
    if((field>=0) && scene_editor_shared_field_event(input,sizeof(input),e))return true;
    if(field>=0 && e->type==SDL_TEXTINPUT) {if(strlen(input)+strlen(e->text.text)<sizeof(input)) strcat(input,e->text.text);return true;}
    if(e->type==SDL_KEYDOWN && field>=0) {
        if(e->key.keysym.sym==SDLK_ESCAPE) {reset();return true;}
        if(e->key.keysym.sym==SDLK_BACKSPACE) {size_t n=strlen(input);if(n) input[n-1]=0;return true;}
        if(e->key.keysym.sym==SDLK_RETURN) {char* end;errno=0;long value=strtol(input,&end,10);SceneEditorRenderOptions copy=draft_options;
            int* values[]={&copy.width,&copy.height,&copy.temporal_frames,&copy.frame};
            if(!errno && end!=input && !*end && value>=0 && value<=1000000000) {*values[field]=(int)value;if(SceneEditorRenderOptionsValid(&copy)) {draft_options=copy;reset();}}
            if(field>=0) snprintf(feedback,sizeof(feedback),"Invalid render setting; Escape cancels.");return true;
        }return true;
    }
    if(e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_ESCAPE && open) {if(SceneEditorRenderActive()) SceneEditorRenderCancel(SceneEditorRenderLatestId(),feedback,sizeof(feedback));else open=false;return true;}
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT) return false;
    int x=e->button.x,y=e->button.y;
    if(hit(toggle,x,y)) {reset();open=!open;if(open) {draft_options=SceneEditorRenderOptionsGet();TimelineSample sample;if(SceneEditorTimelineCurrentSample(&sample)) draft_options.frame=(int)sample.absolute_frame;}return true;}
    if(!open) return false;
    if(hit(close_button,x,y)) {reset();open=false;return true;}
    if(hit(cancel,x,y)) {reset();SceneEditorRenderCancel(SceneEditorRenderLatestId(),feedback,sizeof(feedback));return true;}
    if(!SceneEditorRenderActive()) {
        if(hit(results_button,x,y)) {
            json_object* job=SceneEditorRenderJobRead(SceneEditorRenderLatestId(),true,feedback,sizeof(feedback));result_verified=false;
            if(job) {json_object *verified=NULL,*settings=NULL,*width=NULL,*height=NULL,*hash=NULL;json_object_object_get_ex(job,"artifact_integrity",&verified);result_verified=json_object_get_boolean(verified);json_object_object_get_ex(job,"settings",&settings);json_object_object_get_ex(settings,"width",&width);json_object_object_get_ex(settings,"height",&height);json_object_object_get_ex(job,"image_sha256",&hash);snprintf(result_summary,sizeof(result_summary),result_verified?"Verified BMP %dx%d | SHA %.12s":"No verified final image (%dx%d).",json_object_get_int(width),json_object_get_int(height),json_object_get_string(hash));json_object_put(job);}return true;
        }
        for(int i=0;i<4;++i) if(hit(fields[i],x,y)) {field=i;input[0]=0;do{scene_editor_shared_field_begin(input,sizeof(input),0);SDL_StartTextInput();}while(0);return true;}
        if(hit(mode,x,y)) {reset();draft_options.integrator=draft_options.integrator==0?5:0;if(draft_options.integrator==0) draft_options.temporal_frames=1;return true;}
        if(hit(start,x,y)) {reset();char id[64];struct timespec ts;clock_gettime(CLOCK_REALTIME,&ts);snprintf(id,sizeof(id),"ui-%d-%lld-%ld",getpid(),(long long)ts.tv_sec,ts.tv_nsec);SceneEditorRenderStart(id,&draft_options,SceneEditorDocumentRevision(),feedback,sizeof(feedback));return true;}
    }
    if(hit(panel,x,y)) return true;return false;
}
static void label(SDL_Renderer* r,const char* text,int x,int y) {scene_editor_shared_label_at(r,text,x,y,(SDL_Color){225,230,240,255},12);}
void SceneEditorRenderPanelDraw(SDL_Renderer* r) {
    memset(fields,0,sizeof(fields));mode=start=cancel=results_button=close_button=toggle=(SDL_Rect){0};SceneEditorPaneLayout layout;
    if(!r || !SceneEditorGetPaneLayout(&layout) || (SceneEditorWorkspaceProfileGet()!=SCENE_WORKSPACE_RENDER && !SceneEditorRenderActive())) return;
    const char* current=SceneEditorRenderLatestId();
    if(strcmp(current,panel_job_id)) {snprintf(panel_job_id,sizeof(panel_job_id),"%s",current);draft_options=SceneEditorRenderOptionsGet();result_summary[0]=0;result_verified=false;if(SceneEditorRenderActive()) open=true;}
    toggle=(SDL_Rect){layout.viewport_rect.x+8,layout.viewport_rect.y+8,132,28};SceneEditorSharedUiPanelButton(r,6,128,toggle,"Render frame",open,true);if(!open) return;
    panel=(SDL_Rect){toggle.x,toggle.y+34,360,350};SDL_SetRenderDrawColor(r,30,34,43,255);SDL_RenderFillRect(r,&panel);
    int values[]={draft_options.width,draft_options.height,draft_options.temporal_frames,draft_options.frame};const char* names[]={"Width","Height","Passes","Frame"};
    for(int i=0;i<4;++i) {fields[i]=(SDL_Rect){panel.x+8,panel.y+8+i*32,170,28};if(field==i) snprintf(rows[i],sizeof(rows[i]),"%s: %s_",names[i],input);else snprintf(rows[i],sizeof(rows[i]),"%s: %d",names[i],values[i]);SceneEditorSharedUiPanelButton(r,6,256+(i),fields[i],rows[i],field==i,!SceneEditorRenderActive());}
    mode=(SDL_Rect){panel.x+184,panel.y+8,166,28};SceneEditorSharedUiPanelButton(r,6,384,mode,draft_options.integrator==0?"Direct Light":"Disney V2",false,!SceneEditorRenderActive());
    start=(SDL_Rect){panel.x+8,panel.y+144,80,28};cancel=(SDL_Rect){panel.x+96,panel.y+144,80,28};results_button=(SDL_Rect){panel.x+184,panel.y+144,80,28};close_button=(SDL_Rect){panel.x+272,panel.y+144,78,28};SceneEditorSharedUiPanelButton(r,6,512,start,"Start",false,!SceneEditorRenderActive());SceneEditorSharedUiPanelButton(r,6,640,cancel,"Cancel",false,SceneEditorRenderActive());SceneEditorSharedUiPanelButton(r,6,768,results_button,"Results",false,!SceneEditorRenderActive() && *SceneEditorRenderLatestId());SceneEditorSharedUiPanelButton(r,6,896,close_button,"Close",false,true);
    const char* id=SceneEditorRenderLatestId();json_object* job=NULL;char message[256];if(*id) job=SceneEditorRenderJobRead(id,false,message,sizeof(message));
    if(job) {json_object *state=NULL,*passes=NULL;json_object_object_get_ex(job,"state",&state);json_object_object_get_ex(job,"completed_subpasses",&passes);snprintf(status,sizeof(status),"%s | committed passes: %d",json_object_get_string(state),json_object_get_int(passes));json_object_put(job);}else snprintf(status,sizeof(status),"No render job.");
    label(r,status,panel.x+8,panel.y+184);label(r,id,panel.x+8,panel.y+208);label(r,"Output: .optic-renders/<job ID>/frame.bmp",panel.x+8,panel.y+232);label(r,"Scene stays unsaved; edits pause during render.",panel.x+8,panel.y+256);label(r,result_summary,panel.x+8,panel.y+286);label(r,feedback,panel.x+8,panel.y+310);
}
