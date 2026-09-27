#include "scene_editor_timeline_view.h"
#include <math.h>
#include <string.h>
TimelineLayout SceneEditorTimelineLayout(SDL_Rect r) {
    TimelineLayout l={0};l.panel=r;l.row_height=22;
    int gutter=r.w<900?200:240;
    l.toolbar=(SDL_Rect){r.x,r.y,r.w,34};
    l.ruler=(SDL_Rect){r.x+gutter,r.y+34,r.w-gutter,24};
    l.body=(SDL_Rect){r.x,r.y+58,r.w,r.h-84};
    if(l.body.h<0) l.body.h=0;
    l.gutter=(SDL_Rect){r.x,l.body.y,gutter,l.body.h};
    /* Horizontal inset lets endpoint diamonds remain fully visible. */
    l.grid=(SDL_Rect){l.ruler.x+10,l.body.y,l.ruler.w-20,l.body.h};
    l.footer=(SDL_Rect){r.x,r.y+r.h-26,r.w,26};
    const int widths[]={26,26,44,26,26,104,70,46,56,42,82,26,26};
    /* Three stable groups: author/transport left, display mode center,
     * navigation right. On narrow windows the middle group yields enough
     * space to the authoring controls rather than overlapping them. */
    int x=r.x+4;
    for(int i=0;i<=TL_ADD;++i) {
        l.controls[i]=(SDL_Rect){x,r.y+3,widths[i],28};x+=widths[i]+3;
        if(i==TL_END) x+=6;
    }
    int middle=r.x+(r.w-widths[TL_KEYS]-3-widths[TL_CURVES])/2;
    if(middle<x+12) middle=x+12;
    l.controls[TL_KEYS]=(SDL_Rect){middle,r.y+3,widths[TL_KEYS],28};
    l.controls[TL_CURVES]=(SDL_Rect){middle+widths[TL_KEYS]+3,r.y+3,widths[TL_CURVES],28};
    int right_width=widths[TL_FIT]+widths[TL_FIT_CHANNEL]+widths[TL_ZOOM_OUT]+widths[TL_ZOOM_IN]+9+42;
    x=r.x+r.w-4-right_width;
    for(int i=TL_FIT;i<=TL_ZOOM_IN;++i) {
        l.controls[i]=(SDL_Rect){x,r.y+3,widths[i],28};x+=widths[i]+3;
        if(i==TL_FIT_CHANNEL) x+=42;
    }
    l.controls[TL_VALUE]=(SDL_Rect){r.x+4,l.footer.y+1,150,24};
    l.controls[TL_INTERPOLATION]=(SDL_Rect){r.x+160,l.footer.y+1,112,24};
    l.controls[TL_DELETE]=(SDL_Rect){r.x+278,l.footer.y+1,60,24};
    return l;
}
void TimelineViewFit(TimelineView* v,TimelineRange r) {
    v->first=(double)r.start_frame;v->span=r.frame_count>1?(double)(r.frame_count-1):1;v->valid=true;
}
void TimelineViewZoom(TimelineView* v,double factor,double anchor) {
    if(!v || !v->valid || !isfinite(factor) || factor<=0) return;
    anchor=fmax(0,fmin(1,anchor));double frame=v->first+anchor*v->span;
    v->span=fmax(1,fmin(1e12,v->span*factor));v->first=frame-anchor*v->span;
    v->first=fmax(-1e12,fmin(1e12,v->first));
}
void TimelineViewPan(TimelineView* v,double frames) {
    if(v && v->valid && isfinite(frames)) v->first=fmax(-1e12,fmin(1e12,v->first+frames));
}
double TimelineViewFrame(const TimelineView* v,SDL_Rect grid,int x) {
    return v->first+(double)(x-grid.x)/fmax(1,grid.w)*v->span;
}
int TimelineViewX(const TimelineView* v,SDL_Rect grid,double frame) {
    double x=grid.x+(frame-v->first)/fmax(1,v->span)*grid.w;
    return (int)llround(fmax(-1000000,fmin(1000000,x)));
}
double TimelineViewTick(const TimelineView* v,int width) {
    double wanted=fmax(1,v->span*80/fmax(1,width)),power=pow(10,floor(log10(wanted)));
    double n=wanted/power;return power*(n<=1?1:n<=2?2:n<=5?5:10);
}
const char* TimelineChannelLabel(const char* p) {
    if(strstr(p,"path_progress")) return "Path progress";
    if(strstr(p,"fov_y")) return "Field of view";
    if(strstr(p,"intensity")) return "Intensity";
    if(strstr(p,"yaw")) return "Yaw";
    if(strstr(p,"pitch")) return "Pitch";
    const char* slash=strchr(p,'/');return slash?slash+1:p;
}
