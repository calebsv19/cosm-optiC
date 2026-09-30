#include "motion/scene_motion_plans.h"
#include "scene_editor_timeline_curve.h"
#include "animation/timeline_property_registry.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_timeline_selection.h"
#include "editor/scene_editor_document.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct {
    bool active;
    int side;
    int64_t anchor;
    size_t key;
    unsigned long long revision;
    TimelineTrack track;
    double minimum,maximum;
} drag;
void SceneEditorTimelineCurveCancel(void) {drag.active=false;}
static bool inside(SDL_Rect r,int x,int y) {return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
static void limits(const TimelineTrack* track,double* minimum,double* maximum) {
    if(MotionPlansRuntimeActive(track->target_id) && (!strcmp(track->property_id,MOTION_PROGRESS_PROPERTY)||!strcmp(track->property_id,MOTION_CAMERA_PROGRESS_PROPERTY)||!strcmp(track->property_id,MOTION_LIGHT_PROGRESS_PROPERTY))) {*minimum=-.05;*maximum=1.05;return;}
    *minimum=INFINITY;*maximum=-INFINITY;
    for(size_t i=0;i<track->key_count;++i) {
        const TimelineKeyframe* k=&track->keys[i];
        double values[]={k->value.as.scalar,k->value.as.scalar+k->incoming_value_offset,k->value.as.scalar+k->outgoing_value_offset};
        for(size_t j=0;j<3;++j) {*minimum=fmin(*minimum,values[j]);*maximum=fmax(*maximum,values[j]);}
    }
    double pad=fmax((*maximum-*minimum)*.15,.05);
    *minimum-=pad;*maximum+=pad;
}
static int py(SDL_Rect g,double low,double high,double value) {
    return g.y+g.h-(int)llround((value-low)/(high-low)*g.h);
}
static bool handle_active(const TimelineTrack* t,size_t k,int side) {
    return side<0 ? k>0 && t->keys[k-1].interpolation_to_next==TIMELINE_INTERPOLATION_CUBIC_BEZIER
        : k+1<t->key_count && t->keys[k].interpolation_to_next==TIMELINE_INTERPOLATION_CUBIC_BEZIER;
}
bool SceneEditorTimelineCurveEvent(SDL_Event* event,SDL_Rect graph,const TimelineView* view) {
    TimelineTrack track;TimelineRate rate;TimelineRange range;TimelineSample sample;
    if(!event || !SceneEditorTimelineSelectedTrack(&track,&rate,&range,&sample) || track.value_type!=TIMELINE_VALUE_SCALAR) {drag.active=false;return false;}
    SceneEditorTimelineKeySelection keys;bool have_keys=SceneEditorTimelineSelectionRead(&keys);
    if(drag.active && (drag.revision!=SceneEditorDocumentRevision() || !have_keys || keys.primary.frame!=drag.anchor || strcmp(track.track_id,drag.track.track_id))) drag.active=false;
    if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_ESCAPE && drag.active) {drag.active=false;return true;}
    double low,high;limits(&track,&low,&high);
    if(event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT && inside(graph,event->button.x,event->button.y)) {
        for(size_t k=0;k<track.key_count;++k) {
            const TimelineKeyframe* key=&track.keys[k];
            if(!have_keys || keys.count!=1 || key->frame!=keys.primary.frame) continue;
            for(int side=-1;side<=1;side+=2) {
                if(!handle_active(&track,k,side)) continue;
                double df=side<0?key->incoming_frame_offset:key->outgoing_frame_offset;
                double dv=side<0?key->incoming_value_offset:key->outgoing_value_offset;
                if(hypot(event->button.x-TimelineViewX(view,graph,(double)key->frame+df),event->button.y-py(graph,low,high,key->value.as.scalar+dv))<=10) {
                    drag.active=true;drag.side=side;drag.key=k;drag.revision=SceneEditorDocumentRevision();
                    drag.anchor=track.keys[k].frame;drag.track=track;drag.minimum=low;drag.maximum=high;SceneEditorTimelinePause();return true;
                }
            }
        }
        for(size_t k=0;k<track.key_count;++k)
            if(hypot(event->button.x-TimelineViewX(view,graph,(double)track.keys[k].frame),event->button.y-py(graph,low,high,track.keys[k].value.as.scalar))<=10) {
                SceneEditorTimelinePause();SceneEditorTimelineSelectKey(track.keys[k].frame,false);
                drag.active=true;drag.side=0;drag.key=k;drag.revision=SceneEditorDocumentRevision();
                drag.anchor=track.keys[k].frame;drag.track=track;drag.minimum=low;drag.maximum=high;return true;
            }
        return true;
    }
    if(event->type==SDL_MOUSEMOTION && drag.active) {
        if(drag.side==0) drag.track=track;
        TimelineKeyframe* key=&drag.track.keys[drag.key];
        double frame=TimelineViewFrame(view,graph,event->motion.x);
        double value=drag.minimum+(1-fmax(0,fmin(1,(double)(event->motion.y-graph.y)/graph.h)))*(drag.maximum-drag.minimum);
        TimelinePropertyRegistry registry;
        const TimelinePropertyDescriptor* descriptor=NULL;
        if(TimelinePropertyRegistryInitFoundationDefaults(&registry)==TIMELINE_STATUS_OK &&
            TimelinePropertyRegistryFind(&registry,track.property_id,&descriptor)==TIMELINE_STATUS_OK) {
            if(descriptor->has_minimum) value=fmax(value,descriptor->minimum.as.scalar);
            if(descriptor->has_maximum) value=fmin(value,descriptor->maximum.as.scalar);
        }
        if(drag.side==0) {
            int64_t end;TimelineRangeEndFrame(range,&end);
            double first=drag.key?drag.track.keys[drag.key-1].frame+1:range.start_frame;
            double last=drag.key+1<drag.track.key_count?drag.track.keys[drag.key+1].frame-1:end;
            key->frame=(int64_t)llround(fmax(first,fmin(last,frame)));
            if(!strcmp(track.property_id,"light/path_progress")) {
                if(drag.key) value=fmax(value,drag.track.keys[drag.key-1].value.as.scalar);
                if(drag.key+1<drag.track.key_count) value=fmin(value,drag.track.keys[drag.key+1].value.as.scalar);
            }
            key->value.as.scalar=value;
            /* Fit handles for a valid preview as adjacent intervals shrink. */
            for(size_t i=0;i+1<drag.track.key_count;++i) {
                TimelineKeyframe *a=&drag.track.keys[i],*b=&drag.track.keys[i+1];
                double extent=a->outgoing_frame_offset-b->incoming_frame_offset,span=b->frame-a->frame;
                if(extent>span) {double scale=span/extent;a->outgoing_frame_offset*=scale;a->outgoing_value_offset*=scale;b->incoming_frame_offset*=scale;b->incoming_value_offset*=scale;}
            }
        } else if(drag.side<0) {
            TimelineKeyframe* previous=&drag.track.keys[drag.key-1];
            frame=fmax(previous->frame+previous->outgoing_frame_offset,fmin((double)key->frame,frame));
            if(!strcmp(track.property_id,"light/path_progress"))
                value=fmax(previous->value.as.scalar+previous->outgoing_value_offset,fmin(key->value.as.scalar,value));
            key->incoming_frame_offset=frame-key->frame;key->incoming_value_offset=value-key->value.as.scalar;
        } else {
            TimelineKeyframe* next=&drag.track.keys[drag.key+1];
            frame=fmax((double)key->frame,fmin(next->frame+next->incoming_frame_offset,frame));
            if(!strcmp(track.property_id,"light/path_progress"))
                value=fmax(key->value.as.scalar,fmin(next->value.as.scalar+next->incoming_value_offset,value));
            key->outgoing_frame_offset=frame-key->frame;key->outgoing_value_offset=value-key->value.as.scalar;
        }
        if(drag.side==0) TimelineTrackRecomputeTangents(&drag.track);
        return true;
    }
    if(event->type==SDL_MOUSEBUTTONUP && event->button.button==SDL_BUTTON_LEFT && drag.active) {
        TimelineKeyframe key=drag.track.keys[drag.key];drag.active=false;
        if(SceneEditorDocumentRevision()==drag.revision) {
            if(drag.side==0) {
                if(key.frame!=track.keys[drag.key].frame || key.value.as.scalar!=track.keys[drag.key].value.as.scalar)
                    SceneEditorTimelineMoveSelectedValue(key.frame,key.value.as.scalar);
            } else SceneEditorTimelineSelectedHandles(key.incoming_frame_offset,key.incoming_value_offset,key.outgoing_frame_offset,key.outgoing_value_offset);
        }
        return true;
    }
    return false;
}
void SceneEditorTimelineCurveRender(SDL_Renderer* renderer,SDL_Rect graph,const TimelineView* view) {
    TimelineTrack track;TimelineRate rate;TimelineRange range;TimelineSample sample;
    if(graph.w<10 || graph.h<10 || !SceneEditorTimelineSelectedTrack(&track,&rate,&range,&sample) || track.value_type!=TIMELINE_VALUE_SCALAR) return;
    SceneEditorTimelineKeySelection keys;bool have_keys=SceneEditorTimelineSelectionRead(&keys);
    double low,high;limits(&track,&low,&high);
    if(drag.active && drag.revision==SceneEditorDocumentRevision() && !strcmp(track.track_id,drag.track.track_id)) {
        track=drag.track;low=drag.minimum;high=drag.maximum;
    }
    SDL_Rect prior;SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&prior);SDL_RenderSetClipRect(renderer,&graph);
    SDL_SetRenderDrawColor(renderer,43,49,59,255);
    for(int tick=0;tick<=4;++tick) {
        int y=graph.y+tick*graph.h/4;
        SDL_RenderDrawLine(renderer,graph.x,y,graph.x+graph.w,y);
    }
    int previous_x=0,previous_y=0;bool previous=false;
    SDL_SetRenderDrawColor(renderer,105,196,239,255);
    for(int x=0;x<=graph.w;x+=2) {
        double frame=TimelineViewFrame(view,graph,graph.x+x),whole=floor(frame);
        TimelineSample at={(int64_t)whole,(uint32_t)((frame-whole)*1000000),1000000};
        TimelineEvaluationContext context;TimelineEvaluationResult result;
        if(TimelineEvaluationContextBuild(rate,range,at,&context)!=TIMELINE_STATUS_OK || TimelineTrackEvaluate(&track,&context,&result)!=TIMELINE_STATUS_OK || !MotionPlansRuntimeEvaluate(&context,&result)) {previous=false;continue;}
        int y=py(graph,low,high,result.value.as.scalar);
        if(previous) SDL_RenderDrawLine(renderer,previous_x,previous_y,graph.x+x,y);
        previous_x=graph.x+x;previous_y=y;previous=true;
    }
    for(size_t k=0;k<track.key_count;++k) {
        TimelineKeyframe* key=&track.keys[k];int x=TimelineViewX(view,graph,(double)key->frame),y=py(graph,low,high,key->value.as.scalar);
        SDL_Rect point={x-3,y-3,7,7};
        if(SceneEditorTimelineKeySelected(track.track_id,key->frame)) SDL_SetRenderDrawColor(renderer,255,194,91,255);
        else SDL_SetRenderDrawColor(renderer,105,196,239,255);
        SDL_RenderFillRect(renderer,&point);
        if(!have_keys || keys.count!=1 || key->frame!=keys.primary.frame) continue;
        for(int side=-1;side<=1;side+=2) if(handle_active(&track,k,side)) {
            double df=side<0?key->incoming_frame_offset:key->outgoing_frame_offset,dv=side<0?key->incoming_value_offset:key->outgoing_value_offset;
            int hx=TimelineViewX(view,graph,key->frame+df),hy=py(graph,low,high,key->value.as.scalar+dv);
            SDL_RenderDrawLine(renderer,x,y,hx,hy);SDL_Rect handle={hx-4,hy-4,9,9};SDL_RenderDrawRect(renderer,&handle);
        }
    }
    int playhead=TimelineViewX(view,graph,(double)sample.absolute_frame);
    SDL_SetRenderDrawColor(renderer,245,180,80,255);
    SDL_RenderDrawLine(renderer,playhead,graph.y,playhead,graph.y+graph.h);
    TTF_Font* font=ray_tracing_font_runtime_get_ui_regular(renderer,10,8);
    SDL_Color text={180,189,201,255};
    static char values[3][64];
    for(int tick=0;tick<3;++tick) {
        double fraction=tick*.5;
        snprintf(values[tick],sizeof(values[tick]),"%.4g %s",high-fraction*(high-low),TimelineUnitLabel(track.unit));
        ray_tracing_text_draw_utf8_at(renderer,font,values[tick],graph.x+8,
            graph.y+(int)(fraction*(graph.h-14)),text);
    }
    SDL_RenderSetClipRect(renderer,clipped?&prior:NULL);
}
