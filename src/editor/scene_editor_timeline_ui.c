#include "scene_editor_timeline_ui.h"
#include "scene_editor_timeline_curve.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_pointer_event.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
static TimelineUI ui;
static bool hit(SDL_Rect r,int x,int y) {return r.w>0 && r.h>0 && x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
void SceneEditorTimelineUIReleaseFocus(void) {
    if(ui.numeric) SDL_StopTextInput();
    ui.numeric=0;ui.focused=ui.dragging=ui.scrubbing=ui.panning=ui.menu=false;
    SceneEditorTimelineCurveCancel();
}
void SceneEditorTimelineUIReset(void) {SceneEditorTimelineUIReleaseFocus();memset(&ui,0,sizeof(ui));}
static bool collapsed(const char* target) {
    for(size_t i=0;i<ui.collapsed_count;++i) if(!strcmp(target,ui.collapsed[i])) return true;
    return false;
}
static void prepare(const SceneEditorPaneLayout* pane,const TimelineDocument* doc,size_t selected) {
    if(memcmp(&ui.layout.panel,&pane->timeline_rect,sizeof(SDL_Rect))) SceneEditorTimelineUIReleaseFocus();
    ui.layout=SceneEditorTimelineLayout(pane->timeline_rect);ui.row_count=0;
    if(!doc) return;
    if(!ui.view.valid) TimelineViewFit(&ui.view,doc->range);
    /* Group by stable target identity, camera first, preserving property order. */
    for(int camera=1;camera>=0;--camera) for(size_t i=0;i<doc->track_count;++i) {
        const char* target=doc->tracks[i].target_id;
        if((!strncmp(target,"camera/",7))!=camera) continue;
        bool seen=false;for(size_t j=0;j<i;++j) if(!strcmp(target,doc->tracks[j].target_id)) seen=true;
        if(seen) continue;
        TimelineRow* row=&ui.rows[ui.row_count++];row->track=i;row->group=true;snprintf(row->target,sizeof(row->target),"%s",target);
        if(!collapsed(target)) for(size_t j=0;j<doc->track_count;++j) if(!strcmp(target,doc->tracks[j].target_id)) {
            row=&ui.rows[ui.row_count++];row->track=j;row->group=false;snprintf(row->target,sizeof(row->target),"%s",target);
        }
    }
    size_t visible=(size_t)(ui.layout.body.h/ui.layout.row_height);
    if(selected<doc->track_count && strcmp(ui.shown_track,doc->tracks[selected].track_id)) {
        snprintf(ui.shown_track,sizeof(ui.shown_track),"%s",doc->tracks[selected].track_id);
        for(size_t row=0;row<ui.row_count;++row) if(!ui.rows[row].group && ui.rows[row].track==selected) {
            if(row<ui.row_offset) ui.row_offset=row;
            if(visible && row>=ui.row_offset+visible) ui.row_offset=row-visible+1;
        }
    }
    size_t max=ui.row_count>visible?ui.row_count-visible:0;if(ui.row_offset>max) ui.row_offset=max;
}
static int64_t clamp_frame(const TimelineDocument* d,double frame) {
    int64_t end;TimelineRangeEndFrame(d->range,&end);
    return (int64_t)llround(fmax((double)d->range.start_frame,fmin((double)end,frame)));
}
static double value_at(const TimelineDocument* d,const SceneTimelineSession* s,size_t track) {
    TimelineEvaluationContext c;TimelineEvaluationResult r;
    if(track<d->track_count && TimelineEvaluationContextBuild(d->rate,d->range,s->transport.sample,&c)==TIMELINE_STATUS_OK &&
       TimelineTrackEvaluate(&d->tracks[track],&c,&r)==TIMELINE_STATUS_OK) return r.value.as.scalar;
    return 0;
}
static size_t key_at(const TimelineDocument* d,size_t selected,int64_t frame) {
    if(selected<d->track_count) for(size_t k=0;k<d->tracks[selected].key_count;++k)
        if(d->tracks[selected].keys[k].frame==frame) return k;
    return SIZE_MAX;
}
static void seek(const TimelineDocument* d,double frame) {SceneEditorTimelinePause();SceneEditorTimelineSeek(clamp_frame(d,frame));}
static void fit_channel(const TimelineDocument* d,size_t selected) {
    if(selected>=d->track_count) return;
    const TimelineTrack* t=&d->tracks[selected];if(!t->key_count) return;
    ui.view.first=t->keys[0].frame;ui.view.span=fmax(1,(double)(t->keys[t->key_count-1].frame-t->keys[0].frame));
}
static void begin_numeric(int mode) {SceneEditorTimelinePause();ui.numeric=mode;ui.draft[0]=0;SDL_StartTextInput();}
static bool numeric_event(SDL_Event* e) {
    if(e->type==SDL_TEXTINPUT) {if(strlen(ui.draft)+strlen(e->text.text)<sizeof(ui.draft)) strcat(ui.draft,e->text.text);return true;}
    if(e->type!=SDL_KEYDOWN) return false;
    SDL_Keycode k=e->key.keysym.sym;
    if(k==SDLK_ESCAPE) {ui.numeric=0;SDL_StopTextInput();return true;}
    if(k==SDLK_BACKSPACE) {size_t n=strlen(ui.draft);if(n) ui.draft[n-1]=0;return true;}
    if(k==SDLK_RETURN || k==SDLK_KP_ENTER) {
        char* end;bool ok=false;errno=0;
        if(ui.numeric==1) {long long f=strtoll(ui.draft,&end,10);if(!errno && end!=ui.draft && !*end) ok=SceneEditorTimelineSeek(f);}
        else {double v=strtod(ui.draft,&end);if(!errno && end!=ui.draft && !*end && isfinite(v)) ok=SceneEditorTimelineSetKey(v);}
        if(ok) {ui.numeric=0;SDL_StopTextInput();ui.feedback[0]=0;}
        else snprintf(ui.feedback,sizeof(ui.feedback),"Invalid frame or value; Enter applies, Esc cancels.");
    }
    return true;
}
bool SceneEditorTimelineUIEvent(SDL_Event* e,const SceneEditorPaneLayout* pane,const TimelineDocument* d,const SceneTimelineSession* s,size_t selected) {
    if(!e || !pane || !pane->timeline_visible) return false;
    prepare(pane,d,selected);TimelineLayout* l=&ui.layout;
    if(e->type==SDL_WINDOWEVENT && (e->window.event==SDL_WINDOWEVENT_FOCUS_LOST || e->window.event==SDL_WINDOWEVENT_SIZE_CHANGED)) {SceneEditorTimelineUIReleaseFocus();return false;}
    if(ui.dragging && (ui.revision!=SceneEditorDocumentRevision() || !d || selected>=d->track_count || strcmp(ui.drag_track,d->tracks[selected].track_id))) ui.dragging=false;
    if(ui.numeric && numeric_event(e)) return true;
    if(e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_ESCAPE && ui.focused) {SceneEditorTimelineUIReleaseFocus();return true;}
    if(d && ui.curves && SceneEditorTimelineCurveEvent(e,l->grid,&ui.view)) {ui.focused=true;return true;}
    if(e->type==SDL_MOUSEWHEEL) {
        int x,y;SceneEditorWheelPosition(e,&x,&y);if(!hit(l->panel,x,y)) return false;
        if(!d) return true;
        SDL_Keymod mod=SDL_GetModState();
        SceneEditorTimelineCurveCancel();ui.dragging=false;
        if(mod&(KMOD_CTRL|KMOD_GUI)) TimelineViewZoom(&ui.view,pow(1.2,-e->wheel.y),(double)(x-l->grid.x)/l->grid.w);
        else if((mod&KMOD_SHIFT) || e->wheel.x) TimelineViewPan(&ui.view,(-e->wheel.y-e->wheel.x)*ui.view.span*.08);
        else {int n=(int)ui.row_offset-e->wheel.y;ui.row_offset=n>0?(size_t)n:0;prepare(pane,d,selected);}
        return true;
    }
    if(e->type==SDL_MOUSEMOTION && d) {
        if(ui.panning) {TimelineViewPan(&ui.view,(ui.pan_x-e->motion.x)*ui.view.span/l->grid.w);ui.pan_x=e->motion.x;return true;}
        if(ui.scrubbing) {seek(d,TimelineViewFrame(&ui.view,l->grid,e->motion.x));return true;}
        if(ui.dragging) {ui.drag_frame=clamp_frame(d,TimelineViewFrame(&ui.view,l->grid,e->motion.x));return true;}
    }
    if(e->type==SDL_MOUSEBUTTONUP) {
        if(e->button.button==SDL_BUTTON_MIDDLE && ui.panning) {ui.panning=false;return true;}
        if(e->button.button==SDL_BUTTON_LEFT && (ui.scrubbing || ui.dragging)) {
            ui.scrubbing=false;
            if(ui.dragging) {
                ui.dragging=false;
                if(ui.drag_frame!=ui.drag_origin && ui.revision==SceneEditorDocumentRevision()) SceneEditorTimelineMoveKey(ui.drag_frame);
            }return true;
        }
    }
    if(e->type==SDL_MOUSEBUTTONDOWN) {
        int x=e->button.x,y=e->button.y;
        if(!hit(l->panel,x,y)) {SceneEditorTimelineUIReleaseFocus();return false;}
        ui.focused=true;ui.feedback[0]=0;
        if(!d) {if(e->button.button==SDL_BUTTON_LEFT && y<l->panel.y+38) SceneEditorTimelineActivate();return true;}
        if(e->button.button==SDL_BUTTON_MIDDLE && x>=l->ruler.x) {ui.panning=true;ui.pan_x=x;return true;}
        if(e->button.button!=SDL_BUTTON_LEFT) return true;
        if(ui.numeric) {ui.numeric=0;SDL_StopTextInput();}
        if(ui.menu) {
            SDL_Rect menu=l->controls[TL_INTERPOLATION];menu.y-=72;menu.h=72;
            ui.menu=false;
            if(hit(menu,x,y)) {const TimelineInterpolation modes[]={TIMELINE_INTERPOLATION_STEP,TIMELINE_INTERPOLATION_LINEAR,TIMELINE_INTERPOLATION_CUBIC_BEZIER};SceneEditorTimelineSetInterpolation(modes[(y-menu.y)/24]);return true;}
        }
        int64_t end;TimelineRangeEndFrame(d->range,&end);
        for(int c=0;c<TL_CONTROL_COUNT;++c) if(hit(l->controls[c],x,y)) {
            switch(c) {
                case TL_START:seek(d,d->range.start_frame);break;
                case TL_PREVIOUS:seek(d,(double)s->transport.sample.absolute_frame-1);break;
                case TL_PLAY:SceneEditorTimelineTogglePlaying();break;
                case TL_NEXT:seek(d,(double)s->transport.sample.absolute_frame+1);break;
                case TL_END:seek(d,end);break;
                case TL_FRAME:begin_numeric(1);break;
                case TL_ADD:if(selected<d->track_count) SceneEditorTimelineSetKey(value_at(d,s,selected));break;
                case TL_KEYS:case TL_CURVES:ui.curves=c==TL_CURVES;SceneEditorTimelineCurveCancel();break;
                case TL_FIT:TimelineViewFit(&ui.view,d->range);break;
                case TL_FIT_CHANNEL:fit_channel(d,selected);break;
                case TL_ZOOM_OUT:case TL_ZOOM_IN:TimelineViewZoom(&ui.view,c==TL_ZOOM_IN?.5:2,.5);break;
                case TL_VALUE:if(selected<d->track_count) begin_numeric(2);break;
                case TL_INTERPOLATION:ui.menu=key_at(d,selected,s->transport.sample.absolute_frame)!=SIZE_MAX;break;
                case TL_DELETE:if(key_at(d,selected,s->transport.sample.absolute_frame)!=SIZE_MAX) SceneEditorTimelineDeleteKey();break;
            }return true;
        }
        if(hit(l->ruler,x,y)) {ui.scrubbing=true;seek(d,TimelineViewFrame(&ui.view,l->grid,x));return true;}
        if(hit(l->body,x,y)) {
            size_t visible=(size_t)(l->body.h/l->row_height);
            if(x>=l->gutter.x+l->gutter.w-10 && x<l->ruler.x && ui.row_count>visible) {
                ui.row_offset=(size_t)llround((double)(y-l->body.y)/l->body.h*(ui.row_count-visible));return true;
            }
            size_t row=ui.row_offset+(size_t)((y-l->body.y)/l->row_height);
            if(row>=ui.row_count) return true;
            TimelineRow* item=&ui.rows[row];
            if(x<l->ruler.x || !ui.curves) {
                SceneEditorTimelineSelectTrack(item->track);SceneEditorTimelinePause();
                SceneEditorRenderAuthoringSetTiming(!item->group);
                if(item->group && x<l->gutter.x+26) {
                    bool found=false;for(size_t i=0;i<ui.collapsed_count;++i) if(!strcmp(item->target,ui.collapsed[i])) {memmove(ui.collapsed[i],ui.collapsed[i+1],(--ui.collapsed_count-i)*sizeof(ui.collapsed[0]));found=true;break;}
                    if(!found && ui.collapsed_count<TIMELINE_DOCUMENT_TRACK_CAPACITY) snprintf(ui.collapsed[ui.collapsed_count++],TIMELINE_ID_CAPACITY,"%s",item->target);
                }
                if(!item->group && x>=l->ruler.x) {
                    const TimelineTrack* t=&d->tracks[item->track];
                    for(size_t k=0;k<t->key_count;++k) if(abs(x-TimelineViewX(&ui.view,l->grid,t->keys[k].frame))<=7) {
                        SceneEditorTimelineSeek(t->keys[k].frame);ui.dragging=true;ui.drag_frame=ui.drag_origin=t->keys[k].frame;
                        ui.revision=SceneEditorDocumentRevision();snprintf(ui.drag_track,sizeof(ui.drag_track),"%s",t->track_id);break;
                    }
                }
            }
        }return true;
    }
    if(e->type==SDL_KEYDOWN && ui.focused && d) {
        SDL_Keycode k=e->key.keysym.sym;SDL_Keymod mod=e->key.keysym.mod;
        SceneEditorTimelineCurveCancel();ui.dragging=false;
        if((mod&(KMOD_CTRL|KMOD_GUI)) && k==SDLK_z) {
            SceneEditorTimelineCurveCancel();ui.dragging=false;
            if(mod&KMOD_SHIFT) SceneEditorDocumentRedo(ui.feedback,sizeof(ui.feedback));else SceneEditorDocumentUndo(ui.feedback,sizeof(ui.feedback));return true;
        }
        if(k==SDLK_LEFT || k==SDLK_RIGHT) {
            int step=(mod&KMOD_SHIFT)?10:1;if(k==SDLK_LEFT) step=-step;
            if(mod&KMOD_ALT) SceneEditorTimelineMoveKey(clamp_frame(d,(double)s->transport.sample.absolute_frame+step));
            else seek(d,(double)s->transport.sample.absolute_frame+step);return true;
        }
        if(k==SDLK_HOME || k==SDLK_END) {int64_t end;TimelineRangeEndFrame(d->range,&end);seek(d,k==SDLK_HOME?d->range.start_frame:end);return true;}
        if(k==SDLK_SPACE) {SceneEditorTimelineTogglePlaying();return true;}
        if(k==SDLK_DELETE || k==SDLK_BACKSPACE) {SceneEditorTimelineDeleteKey();return true;}
        if(k==SDLK_f) {TimelineViewFit(&ui.view,d->range);return true;}
        return false;
    }return false;
}
void SceneEditorTimelineUIDraw(SDL_Renderer* r,const SceneEditorPaneLayout* l,const TimelineDocument* d,const SceneTimelineSession* s,size_t selected) {
    if(!r || !l || !l->timeline_visible) return;prepare(l,d,selected);SceneEditorTimelineDrawDock(r,&ui,d,s,selected);
}
bool SceneEditorTimelineControl(const char* name,SDL_Rect* out) {
    static const char* names[]={"start","previous","play","next","end","frame","add_key","keys","curves","fit","fit_channel","zoom_out","zoom_in","value","interpolation","delete"};
    if(!name || !out) return false;
    for(int i=0;i<TL_CONTROL_COUNT;++i) if(!strcmp(name,names[i])) {*out=ui.layout.controls[i];return out->w>0;}
    if(!strcmp(name,"graph")) {*out=ui.layout.grid;return out->w>0;}
    if(!strcmp(name,"ruler")) {*out=ui.layout.ruler;return out->w>0;}
    return false;
}
bool SceneEditorTimelineTrackRect(size_t track,SDL_Rect* out) {
    if(!out) return false;
    for(size_t i=ui.row_offset;i<ui.row_count;++i) if(!ui.rows[i].group && ui.rows[i].track==track) {
        *out=(SDL_Rect){ui.layout.gutter.x,ui.layout.body.y+(int)(i-ui.row_offset)*ui.layout.row_height,ui.layout.gutter.w,ui.layout.row_height};
        return out->y+out->h<=ui.layout.body.y+ui.layout.body.h;
    }return false;
}
int SceneEditorTimelineFrameX(int64_t frame) {return TimelineViewX(&ui.view,ui.layout.grid,(double)frame);}
