#include "scene_editor_timeline_key_inspector.h"
#include "scene_editor_timeline_ui.h"
#include "editor/scene_editor_timeline_selection.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_chrome_shell.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
enum {KEY_FRAME,KEY_VALUE,PREVIOUS,NEXT,COPY,PASTE,DUPLICATE,ALL,CLEAR,DELETE_KEYS,CONTROL_COUNT};
static SDL_Rect controls[CONTROL_COUNT];
static int editing=-1;static char draft[64],labels[4][160],entry_feedback[128];
static unsigned long long revision;
static int64_t editing_frame;static size_t editing_count;
static char editing_track[TIMELINE_ID_CAPACITY];
static bool hit(SDL_Rect r,int x,int y) {return r.w>0 && x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h;}
void SceneEditorTimelineKeyInspectorReset(void) {if(editing>=0) SDL_StopTextInput();editing=-1;entry_feedback[0]=0;memset(controls,0,sizeof(controls));}
bool SceneEditorTimelineKeyInspectorEvent(SDL_Event* e) {
    if(!e) return false;
    if(e->type==SDL_WINDOWEVENT && (e->window.event==SDL_WINDOWEVENT_FOCUS_LOST || e->window.event==SDL_WINDOWEVENT_SIZE_CHANGED)) {SceneEditorTimelineKeyInspectorReset();return false;}
    SceneEditorTimelineKeySelection s;bool selected=SceneEditorTimelineSelectionRead(&s);
    if(editing>=0 && (!selected || revision!=SceneEditorDocumentRevision() || s.primary.frame!=editing_frame || s.count!=editing_count || strcmp(s.track_id,editing_track))) {editing=-1;SDL_StopTextInput();}
    if(editing>=0 && e->type==SDL_TEXTINPUT) {if(strlen(draft)+strlen(e->text.text)<sizeof(draft)) strcat(draft,e->text.text);return true;}
    if(editing>=0 && e->type==SDL_KEYDOWN) {
        SDL_Keycode key=e->key.keysym.sym;
        if(key==SDLK_ESCAPE) {editing=-1;SDL_StopTextInput();return true;}
        if(key==SDLK_BACKSPACE) {size_t n=strlen(draft);if(n) draft[n-1]=0;return true;}
        if(key==SDLK_RETURN || key==SDLK_KP_ENTER) {
            char* end;bool ok=false;errno=0;
            if(editing==KEY_FRAME) {long long f=strtoll(draft,&end,10);if(!errno && end!=draft && !*end) ok=SceneEditorTimelineMoveSelectedKeys(f);}
            else {double v=strtod(draft,&end);if(!errno && end!=draft && !*end && isfinite(v)) ok=SceneEditorTimelineSetSelectedValue(v);}
            if(ok) {editing=-1;entry_feedback[0]=0;SDL_StopTextInput();}
            else snprintf(entry_feedback,sizeof(entry_feedback),"Invalid edit. Check timeline message; Esc cancels.");
        }return true;
    }
    if(e->type!=SDL_MOUSEBUTTONDOWN || e->button.button!=SDL_BUTTON_LEFT) return false;
    if(editing>=0) {editing=-1;SDL_StopTextInput();}
    for(int i=0;i<CONTROL_COUNT;++i) if(hit(controls[i],e->button.x,e->button.y)) {
        SceneEditorTimelineReleaseFocus();SceneEditorTimelineUIFocus();SceneEditorTimelinePause();
        if(i<=KEY_VALUE) {
            if(selected) {entry_feedback[0]=0;editing=i;revision=s.revision;editing_frame=s.primary.frame;editing_count=s.count;snprintf(editing_track,sizeof(editing_track),"%s",s.track_id);draft[0]=0;SDL_StartTextInput();}
        } else switch(i) {
            case PREVIOUS:SceneEditorTimelineNavigateKey(-1);break;
            case NEXT:SceneEditorTimelineNavigateKey(1);break;
            case COPY:SceneEditorTimelineCopyKeys();break;
            case PASTE:SceneEditorTimelinePasteKeys();break;
            case DUPLICATE:SceneEditorTimelineDuplicateKeys();break;
            case ALL:SceneEditorTimelineSelectAllKeys();break;
            case CLEAR:SceneEditorTimelineClearKeys();break;
            case DELETE_KEYS:SceneEditorTimelineDeleteSelectedKeys();break;
        }
        return true;
    }return false;
}
void SceneEditorTimelineKeyInspectorDraw(SDL_Renderer* r,SDL_Rect rect) {
    memset(controls,0,sizeof(controls));
    SceneEditorTimelineKeySelection s;bool selected=SceneEditorTimelineSelectionRead(&s);
    SDL_Rect prior;SDL_bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&prior);
    SDL_Rect clip=rect;if(clipped) SDL_IntersectRect(&prior,&rect,&clip);SDL_RenderSetClipRect(r,&clip);
    int x=rect.x,y=rect.y,w=rect.w;
    snprintf(labels[0],sizeof(labels[0]),selected?"Selected keys: %zu":"Selected keys: none",selected?s.count:0);
    ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,12,9),labels[0],x,y,SceneEditorChromeShellResolvePalette().text_primary);y+=22;
    snprintf(labels[1],sizeof(labels[1]),selected?"%s: %lld":"Key frame: select a diamond",selected && s.count>1?"Anchor frame":"Key frame",selected?(long long)s.primary.frame:0);
    snprintf(labels[2],sizeof(labels[2]),selected?"%s: %.6g":"Key value: select a diamond",selected && s.count>1?"Set all values":"Key value",selected?s.primary.value.as.scalar:0);
    for(int i=0;i<2;++i) {
        if(editing==i) snprintf(labels[i+1],sizeof(labels[i+1]),"%s: %s_",i?"Value":"Key frame",draft);
        controls[i]=(SDL_Rect){x,y,w,28};SceneEditorRenderButton(r,controls[i],labels[i+1],editing==i,selected);y+=32;
    }
    controls[PREVIOUS]=(SDL_Rect){x,y,(w-4)/2,28};controls[NEXT]=(SDL_Rect){x+(w+4)/2,y,(w-4)/2,28};
    SceneEditorRenderButton(r,controls[PREVIOUS],"Previous key",false,true);SceneEditorRenderButton(r,controls[NEXT],"Next key",false,true);y+=32;
    const char* texts[]={"Copy","Paste","Dup here","All keys","Clear","Delete"};
    for(int i=COPY;i<CONTROL_COUNT;++i) {
        controls[i]=(SDL_Rect){x+((i-COPY)%3)*(w+4)/3,y,(w-8)/3,28};
        bool enabled=i==PASTE?SceneEditorTimelineCanPasteKeys():i==ALL || selected;
        SceneEditorRenderButton(r,controls[i],texts[i-COPY],false,enabled);
        if(i==DUPLICATE) y+=32;
    }
    if(editing>=0 && entry_feedback[0]) ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,11,8),entry_feedback,x,y+32,SceneEditorChromeShellResolvePalette().text_primary);
    /* Controls clipped out of the pane cannot receive hidden clicks. */
    for(int i=0;i<CONTROL_COUNT;++i) if(controls[i].y+controls[i].h>rect.y+rect.h) controls[i]=(SDL_Rect){0};
    SDL_RenderSetClipRect(r,clipped?&prior:NULL);
}
bool SceneEditorTimelineKeyInspectorControl(const char* name,SDL_Rect* out) {
    const char* names[]={"key_frame","key_value","previous_key","next_key","copy_keys","paste_keys","duplicate_keys","all_keys","clear_keys","delete_keys"};
    if(!name || !out) return false;
    for(int i=0;i<CONTROL_COUNT;++i) if(!strcmp(name,names[i])) {*out=controls[i];return out->w>0;}
    return false;
}
