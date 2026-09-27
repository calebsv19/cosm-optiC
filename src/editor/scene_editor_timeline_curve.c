#include "scene_editor_timeline_curve.h"
#include "animation/timeline_property_registry.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_document.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct {
    bool active;
    int side;
    size_t key;
    unsigned long long revision;
    TimelineTrack track;
    double minimum,maximum;
} drag;
void SceneEditorTimelineCurveCancel(void) {drag.active=false;}
static bool inside(SDL_Rect r,int x,int y) {return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;}
static void limits(const TimelineTrack* track,double* minimum,double* maximum) {
    *minimum=INFINITY;*maximum=-INFINITY;
    for(size_t i=0;i<track->key_count;++i) {
        const TimelineKeyframe* k=&track->keys[i];
        double values[]={k->value.as.scalar,k->value.as.scalar+k->incoming_value_offset,k->value.as.scalar+k->outgoing_value_offset};
        for(size_t j=0;j<3;++j) {*minimum=fmin(*minimum,values[j]);*maximum=fmax(*maximum,values[j]);}
    }
    double pad=fmax((*maximum-*minimum)*.15,.05);
    *minimum-=pad;*maximum+=pad;
}
static int px(SDL_Rect g,TimelineRange range,double frame) {
    double span=range.frame_count>1?(double)(range.frame_count-1):1;
    return g.x+(int)llround((frame-range.start_frame)/span*g.w);
}
static int py(SDL_Rect g,double low,double high,double value) {
    return g.y+g.h-(int)llround((value-low)/(high-low)*g.h);
}
static bool handle_active(const TimelineTrack* t,size_t k,int side) {
    return side<0 ? k>0 && t->keys[k-1].interpolation_to_next==TIMELINE_INTERPOLATION_CUBIC_BEZIER
        : k+1<t->key_count && t->keys[k].interpolation_to_next==TIMELINE_INTERPOLATION_CUBIC_BEZIER;
}
bool SceneEditorTimelineCurveEvent(SDL_Event* event,SDL_Rect graph) {
    TimelineTrack track;TimelineRate rate;TimelineRange range;TimelineSample sample;
    if(!event || !SceneEditorTimelineSelectedTrack(&track,&rate,&range,&sample) || track.value_type!=TIMELINE_VALUE_SCALAR) {drag.active=false;return false;}
    if(drag.active && (drag.revision!=SceneEditorDocumentRevision() || strcmp(track.track_id,drag.track.track_id))) drag.active=false;
    if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_ESCAPE && drag.active) {drag.active=false;return true;}
    double low,high;limits(&track,&low,&high);
    if(event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT && inside(graph,event->button.x,event->button.y)) {
        for(size_t k=0;k<track.key_count;++k) {
            const TimelineKeyframe* key=&track.keys[k];
            if(key->frame!=sample.absolute_frame) continue;
            for(int side=-1;side<=1;side+=2) {
                if(!handle_active(&track,k,side)) continue;
                double df=side<0?key->incoming_frame_offset:key->outgoing_frame_offset;
                double dv=side<0?key->incoming_value_offset:key->outgoing_value_offset;
                if(hypot(event->button.x-px(graph,range,(double)key->frame+df),event->button.y-py(graph,low,high,key->value.as.scalar+dv))<=10) {
                    drag.active=true;drag.side=side;drag.key=k;drag.revision=SceneEditorDocumentRevision();
                    drag.track=track;drag.minimum=low;drag.maximum=high;SceneEditorTimelinePause();return true;
                }
            }
        }
        for(size_t k=0;k<track.key_count;++k)
            if(hypot(event->button.x-px(graph,range,(double)track.keys[k].frame),event->button.y-py(graph,low,high,track.keys[k].value.as.scalar))<=10) {
                SceneEditorTimelinePause();SceneEditorTimelineSeek(track.keys[k].frame);return true;
            }
        return true;
    }
    if(event->type==SDL_MOUSEMOTION && drag.active) {
        TimelineKeyframe* key=&drag.track.keys[drag.key];
        double t=fmax(0,fmin(1,(double)(event->motion.x-graph.x)/graph.w));
        double frame=range.start_frame+t*(range.frame_count-1);
        double value=drag.minimum+(1-fmax(0,fmin(1,(double)(event->motion.y-graph.y)/graph.h)))*(drag.maximum-drag.minimum);
        TimelinePropertyRegistry registry;
        const TimelinePropertyDescriptor* descriptor=NULL;
        if(TimelinePropertyRegistryInitFoundationDefaults(&registry)==TIMELINE_STATUS_OK &&
            TimelinePropertyRegistryFind(&registry,track.property_id,&descriptor)==TIMELINE_STATUS_OK) {
            if(descriptor->has_minimum) value=fmax(value,descriptor->minimum.as.scalar);
            if(descriptor->has_maximum) value=fmin(value,descriptor->maximum.as.scalar);
        }
        if(drag.side<0) {
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
        return true;
    }
    if(event->type==SDL_MOUSEBUTTONUP && event->button.button==SDL_BUTTON_LEFT && drag.active) {
        TimelineKeyframe key=drag.track.keys[drag.key];drag.active=false;
        if(SceneEditorDocumentRevision()==drag.revision)
            SceneEditorTimelineSetHandles(key.incoming_frame_offset,key.incoming_value_offset,key.outgoing_frame_offset,key.outgoing_value_offset);
        return true;
    }
    return false;
}
void SceneEditorTimelineCurveRender(SDL_Renderer* renderer,SDL_Rect graph) {
    TimelineTrack track;TimelineRate rate;TimelineRange range;TimelineSample sample;
    if(graph.w<10 || graph.h<10 || !SceneEditorTimelineSelectedTrack(&track,&rate,&range,&sample) || track.value_type!=TIMELINE_VALUE_SCALAR) return;
    double low,high;limits(&track,&low,&high);
    if(drag.active && drag.revision==SceneEditorDocumentRevision() && !strcmp(track.track_id,drag.track.track_id)) {
        track=drag.track;low=drag.minimum;high=drag.maximum;
    }
    SDL_Rect prior;SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&prior);SDL_RenderSetClipRect(renderer,&graph);
    SDL_SetRenderDrawColor(renderer,20,24,31,255);SDL_RenderFillRect(renderer,&graph);
    SDL_SetRenderDrawColor(renderer,43,49,59,255);
    for(int tick=0;tick<=4;++tick) {
        int x=graph.x+tick*graph.w/4,y=graph.y+tick*graph.h/4;
        SDL_RenderDrawLine(renderer,x,graph.y,x,graph.y+graph.h);
        SDL_RenderDrawLine(renderer,graph.x,y,graph.x+graph.w,y);
    }
    int previous_x=0,previous_y=0;bool previous=false;
    SDL_SetRenderDrawColor(renderer,105,196,239,255);
    for(int x=0;x<=graph.w;x+=2) {
        double frame=range.start_frame+(double)x/graph.w*(range.frame_count-1),whole=floor(frame);
        TimelineSample at={(int64_t)whole,(uint32_t)((frame-whole)*1000000),1000000};
        TimelineEvaluationContext context;TimelineEvaluationResult result;
        if(TimelineEvaluationContextBuild(rate,range,at,&context)!=TIMELINE_STATUS_OK || TimelineTrackEvaluate(&track,&context,&result)!=TIMELINE_STATUS_OK) {previous=false;continue;}
        int y=py(graph,low,high,result.value.as.scalar);
        if(previous) SDL_RenderDrawLine(renderer,previous_x,previous_y,graph.x+x,y);
        previous_x=graph.x+x;previous_y=y;previous=true;
    }
    for(size_t k=0;k<track.key_count;++k) {
        TimelineKeyframe* key=&track.keys[k];int x=px(graph,range,(double)key->frame),y=py(graph,low,high,key->value.as.scalar);
        SDL_Rect point={x-3,y-3,7,7};SDL_SetRenderDrawColor(renderer,255,194,91,255);SDL_RenderFillRect(renderer,&point);
        if(key->frame!=sample.absolute_frame) continue;
        for(int side=-1;side<=1;side+=2) if(handle_active(&track,k,side)) {
            double df=side<0?key->incoming_frame_offset:key->outgoing_frame_offset,dv=side<0?key->incoming_value_offset:key->outgoing_value_offset;
            int hx=px(graph,range,key->frame+df),hy=py(graph,low,high,key->value.as.scalar+dv);
            SDL_RenderDrawLine(renderer,x,y,hx,hy);SDL_Rect handle={hx-4,hy-4,9,9};SDL_RenderDrawRect(renderer,&handle);
        }
    }
    int playhead=px(graph,range,(double)sample.absolute_frame);
    SDL_SetRenderDrawColor(renderer,245,180,80,255);
    SDL_RenderDrawLine(renderer,playhead,graph.y,playhead,graph.y+graph.h);
    SDL_RenderSetClipRect(renderer,clipped?&prior:NULL);
    TTF_Font* font=ray_tracing_font_runtime_get_ui_regular(renderer,10,8);
    SDL_Color text={180,189,201,255};
    /* Queued text needs distinct storage for every label until frame submission. */
    static char values[3][40],frames[3][40];
    for(int tick=0;tick<3;++tick) {
        double fraction=tick*.5;
        snprintf(values[tick],sizeof(values[tick]),"%.4g",high-fraction*(high-low));
        uint64_t offset=tick==0?0:tick==1?(range.frame_count-1)/2:range.frame_count-1;
        snprintf(frames[tick],sizeof(frames[tick]),"F %lld",(long long)(range.start_frame+(int64_t)offset));
        ray_tracing_text_draw_utf8_at(renderer,font,values[tick],graph.x-62,
            graph.y+(int)(fraction*(graph.h-12)),text);
        int x=graph.x+(int)(fraction*graph.w)-(tick==2?70:0);
        ray_tracing_text_draw_utf8_at(renderer,font,frames[tick],x,graph.y+graph.h+2,text);
    }
    const char* unit=track.unit==TIMELINE_UNIT_RADIANS?"Radians":
        track.unit==TIMELINE_UNIT_DEGREES?"Degrees":
        track.unit==TIMELINE_UNIT_RELATIVE_INTENSITY?"Relative intensity":
        track.unit==TIMELINE_UNIT_WORLD_DISTANCE?"World distance":"Progress";
    ray_tracing_text_draw_utf8_at(renderer,font,unit,graph.x-230,graph.y+8,text);
}
