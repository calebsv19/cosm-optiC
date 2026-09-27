#include "editor/scene_editor_timeline_view.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    TimelineView v;TimelineRange range={0,218};TimelineViewFit(&v,range);
    assert(TimelineViewTick(&v,900)==20);
    assert(TimelineViewTick(&v,600)==50);
    assert(TimelineViewTick(&v,180)==100);
    SDL_Rect grid={210,58,900,160};
    assert(TimelineViewX(&v,grid,0)==210 && TimelineViewX(&v,grid,217)==1110);
    double anchor=TimelineViewFrame(&v,grid,510);TimelineViewZoom(&v,.5,1.0/3);
    assert(fabs(TimelineViewFrame(&v,grid,510)-anchor)<1e-9);
    for(int f=0;f<=217;++f) assert(fabs(TimelineViewFrame(&v,grid,TimelineViewX(&v,grid,f))-f)<.13);
    double start=v.first;TimelineViewPan(&v,17);assert(v.first==start+17);
    TimelineViewFit(&v,(TimelineRange){-35,1});assert(v.span==1 && v.first==-35);
    for(int width=820;width<=2560;width+=20) {
        TimelineLayout l=SceneEditorTimelineLayout((SDL_Rect){10,500,width-20,140});
        for(int i=0;i<TL_CONTROL_COUNT;++i) assert(l.controls[i].x+l.controls[i].w<=l.panel.x+l.panel.w);
        assert(l.body.y+l.body.h==l.footer.y && l.grid.w>0);
        assert(l.controls[TL_START].x==l.panel.x+4);
        assert(l.controls[TL_ZOOM_IN].x+l.controls[TL_ZOOM_IN].w==l.panel.x+l.panel.w-4);
        assert(l.controls[TL_ADD].x+l.controls[TL_ADD].w+8<l.controls[TL_KEYS].x);
        assert(l.controls[TL_CURVES].x+l.controls[TL_CURVES].w+8<l.controls[TL_FIT].x);
        if(width>=1000) assert(abs((l.controls[TL_KEYS].x+l.controls[TL_CURVES].x+l.controls[TL_CURVES].w)/2-(l.panel.x+l.panel.w/2))<=1);
    }
    puts("Timeline view PASS: nice ticks, cursor-anchored zoom, mapping roundtrip, pan, single-frame range, compact layout");
}
