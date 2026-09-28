#include "scene_editor_timeline_ui.h"
#include "scene_editor_timeline_curve.h"
#include "editor/scene_editor_timeline.h"
#include "editor/scene_editor_timeline_selection.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_render_authoring.h"
#include "editor/scene_editor_chrome_shell.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include <math.h>
#include <stdlib.h>
#include "kit_ui_sdl.h"
#include <stdio.h>
#include <string.h>
static void fill(SDL_Renderer* r,SDL_Rect box,SDL_Color c) {SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);SDL_RenderFillRect(r,&box);}
static void text(SDL_Renderer* r,const char* s,int x,int y,SDL_Color c) {ray_tracing_text_draw_utf8_at(r,ray_tracing_font_runtime_get_ui_regular(r,11,8),s,x,y,c);}
static void diamond(SDL_Renderer* r,int x,int y,SDL_Color c) {
    SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    for(int i=-4;i<=4;++i) SDL_RenderDrawLine(r,x-4+abs(i),y+i,x+4-abs(i),y+i);
}
static void grid(SDL_Renderer* r,const TimelineUI* u) {
    const TimelineLayout* l=&u->layout;
    double tick=TimelineViewTick(&u->view,l->grid.w),minor=tick>=5?tick/5:1;
    SDL_SetRenderDrawColor(r,37,43,52,255);
    for(double f=ceil(u->view.first/minor)*minor;f<=u->view.first+u->view.span;f+=minor) {
        int x=TimelineViewX(&u->view,l->grid,f);SDL_RenderDrawLine(r,x,l->ruler.y+18,x,l->body.y+l->body.h);
    }
    static char ticks[128][32];size_t i=0;
    for(double f=ceil(u->view.first/tick)*tick;f<=u->view.first+u->view.span && i<128;f+=tick) {
        int x=TimelineViewX(&u->view,l->grid,f);SDL_SetRenderDrawColor(r,59,65,76,255);SDL_RenderDrawLine(r,x,l->ruler.y+19,x,l->body.y+l->body.h);
        snprintf(ticks[i],sizeof(ticks[i]),"%.0f",f);text(r,ticks[i++],x+3,l->ruler.y+3,(SDL_Color){175,183,194,255});
    }
}
void SceneEditorTimelineDrawDock(SDL_Renderer* r,const TimelineUI* u,const TimelineDocument* d,const SceneTimelineSession* s,size_t selected) {
    const TimelineLayout* l=&u->layout;
    SDL_Rect prior;SDL_bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&prior);SDL_RenderSetClipRect(r,&l->panel);
    RayTracingThemePalette p=SceneEditorChromeShellResolvePalette();
    SDL_Color ink=p.text_primary,muted=p.text_muted,accent={105,196,239,255},gold={245,180,80,255};
    fill(r,l->panel,(SDL_Color){23,27,34,255});
    if(!d) {
        SceneEditorRenderButton(r,(SDL_Rect){l->panel.x+4,l->panel.y+3,230,32},"Set up scene animation",false,true);
        text(r,"Preserves camera and light paths. Undo is available.",l->panel.x+12,l->panel.y+48,ink);
        text(r,SceneEditorTimelineStatus(),l->panel.x+12,l->panel.y+72,gold);goto done;
    }
    fill(r,l->toolbar,p.panel_fill);fill(r,l->gutter,(SDL_Color){31,35,43,255});
    fill(r,(SDL_Rect){l->panel.x,l->ruler.y,l->gutter.w,l->ruler.h},(SDL_Color){42,47,56,255});
    fill(r,l->ruler,(SDL_Color){28,32,39,255});fill(r,l->footer,p.panel_fill);
    static char frame[64],value[96],context[256],groups[TL_ROW_CAPACITY][160];
    snprintf(frame,sizeof(frame),"Frame %lld",(long long)s->transport.sample.absolute_frame);
    if(u->numeric==1) snprintf(frame,sizeof(frame),"Frame %s_",u->draft);
    const char* labels[]={"|<","<",s->transport.playing?"Pause":"Play",">",">|",frame,"Add key","Keys","Curves","Fit","Fit channel","-","+"};
    for(int i=0;i<=TL_ZOOM_IN;++i) SceneEditorRenderButton(r,l->controls[i],labels[i],
        (i==TL_PLAY && s->transport.playing) || (i==TL_KEYS && !u->curves) || (i==TL_CURVES && u->curves),
        (i!=TL_ADD && i!=TL_FIT_CHANNEL) || selected<d->track_count);
    SDL_SetRenderDrawColor(r,66,73,85,255);
    int separators[]={l->controls[TL_KEYS].x-8,l->controls[TL_FIT].x-8};
    for(int i=0;i<2;++i) SDL_RenderDrawLine(r,separators[i],l->toolbar.y+8,separators[i],l->toolbar.y+26);
    text(r,"Zoom",l->controls[TL_ZOOM_OUT].x-38,l->toolbar.y+11,muted);
    text(r,"CHANNELS",l->panel.x+10,l->ruler.y+4,muted);
    SceneEditorTimelineKeySelection keys;bool have_keys=SceneEditorTimelineSelectionRead(&keys);
    SDL_RenderSetClipRect(r,&l->body);
    for(size_t row=u->row_offset;row<u->row_count;++row) {
        int y=l->body.y+(int)(row-u->row_offset)*l->row_height;if(y>=l->body.y+l->body.h) break;
        const TimelineRow* item=&u->rows[row];
        SDL_Rect band={l->body.x,y,u->curves?l->gutter.w:l->body.w,l->row_height};
        if(item->group) fill(r,band,(SDL_Color){39,44,53,255});
        else if(item->track==selected) fill(r,band,(SDL_Color){41,59,76,255});
        else if(row%2==0) fill(r,band,(SDL_Color){27,31,39,255});
        SDL_SetRenderDrawColor(r,47,53,63,255);SDL_RenderDrawLine(r,band.x,y+band.h-1,band.x+band.w,y+band.h-1);
    }
    SDL_RenderSetClipRect(r,&l->panel);grid(r,u);
    SDL_RenderSetClipRect(r,&l->gutter);
    for(size_t row=u->row_offset;row<u->row_count;++row) {
        int y=l->body.y+(int)(row-u->row_offset)*l->row_height;if(y>=l->body.y+l->body.h) break;
        const TimelineRow* item=&u->rows[row];
        if(item->group) {
            bool closed=false;for(size_t j=0;j<u->collapsed_count;++j) if(!strcmp(item->target,u->collapsed[j])) closed=true;
            SceneEditorDocumentObjectInfo object;
            const char* name=!strncmp(item->target,"camera/",7)?"Camera":item->target+6;
            if(!strncmp(item->target,"object/",7)) name=SceneEditorDocumentObjectById(item->target+7,&object)?object.name:item->target+7;
            snprintf(groups[row],sizeof(groups[row]),"%s  %s",closed?">":"v",name);
            text(r,groups[row],l->gutter.x+8,y+3,ink);
        } else text(r,TimelineChannelLabel(d->tracks[item->track].property_id),l->gutter.x+27,y+3,item->track==selected?accent:ink);
    }
    if(u->row_count>(size_t)(l->body.h/l->row_height)) {
        KitUiSdlScrollbarLayout scroll;
        kit_ui_sdl_scrollbar_layout(&l->gutter,(int)u->row_count*l->row_height,(int)u->row_offset*l->row_height,&scroll);
        kit_ui_sdl_draw_scrollbar(r,&scroll,(KitRenderColor){40,44,50,255},(KitRenderColor){130,140,155,255});
    }
    SDL_Rect canvas={l->ruler.x,l->body.y,l->ruler.w,l->body.h};SDL_RenderSetClipRect(r,&canvas);
    if(u->curves) SceneEditorTimelineCurveRender(r,l->grid,&u->view);
    else for(size_t row=u->row_offset;row<u->row_count;++row) {
        int y=l->body.y+(int)(row-u->row_offset)*l->row_height;if(y>=l->body.y+l->body.h) break;
        const TimelineRow* item=&u->rows[row];if(item->group) continue;
        const TimelineTrack* t=&d->tracks[item->track];
        for(size_t k=0;k<t->key_count;++k) diamond(r,TimelineViewX(&u->view,l->grid,t->keys[k].frame),y+l->row_height/2,
            SceneEditorTimelineKeySelected(t->track_id,t->keys[k].frame)?gold:accent);
        if(u->dragging && item->track==selected && have_keys) for(size_t k=0;k<keys.count;++k)
            diamond(r,TimelineViewX(&u->view,l->grid,keys.frames[k]+(u->drag_frame-u->drag_origin)),y+l->row_height/2,(SDL_Color){235,240,255,255});
    }
    SDL_RenderSetClipRect(r,&l->panel);
    SDL_SetRenderDrawColor(r,82,90,104,255);SDL_RenderDrawLine(r,l->ruler.x,l->ruler.y,l->ruler.x,l->footer.y);
    int64_t end;TimelineRangeEndFrame(d->range,&end);
    int edges[]={TimelineViewX(&u->view,l->grid,d->range.start_frame),TimelineViewX(&u->view,l->grid,end)};
    SDL_RenderSetClipRect(r,&canvas);SDL_SetRenderDrawColor(r,81,89,103,255);
    for(int i=0;i<2;++i) SDL_RenderDrawLine(r,edges[i],l->body.y,edges[i],l->footer.y);
    SDL_Rect time_clip={l->ruler.x,l->ruler.y,l->ruler.w,l->ruler.h+l->body.h};SDL_RenderSetClipRect(r,&time_clip);
    int x=TimelineViewX(&u->view,l->grid,s->transport.sample.absolute_frame);
    SDL_SetRenderDrawColor(r,gold.r,gold.g,gold.b,255);SDL_RenderDrawLine(r,x,l->ruler.y,x,l->footer.y);diamond(r,x,l->ruler.y+5,gold);
    if(u->dragging) {x=TimelineViewX(&u->view,l->grid,u->drag_frame);SDL_SetRenderDrawColor(r,230,240,255,255);SDL_RenderDrawLine(r,x,l->body.y,x,l->footer.y);}
    SDL_RenderSetClipRect(r,&l->panel);
    static char key_frame[64];
    snprintf(key_frame,sizeof(key_frame),have_keys?"Key F %lld":"Select a key",have_keys?(long long)keys.primary.frame:0);
    if(u->numeric==3) snprintf(key_frame,sizeof(key_frame),"Key F %s_",u->draft);
    SceneEditorRenderButton(r,l->controls[TL_KEY_FRAME],key_frame,u->numeric==3,have_keys);
    snprintf(value,sizeof(value),have_keys?"Value %.5g":"Value",have_keys?keys.primary.value.as.scalar:0);
    if(u->numeric==2) snprintf(value,sizeof(value),"Value %s_",u->draft);
    SceneEditorRenderButton(r,l->controls[TL_VALUE],value,u->numeric==2,have_keys);
    const char* mode="Interpolation";
    if(have_keys) {TimelineInterpolation m=keys.primary.interpolation_to_next;mode=m==TIMELINE_INTERPOLATION_STEP?"Hold v":m==TIMELINE_INTERPOLATION_LINEAR?"Linear v":"Bezier ease v";}
    if(have_keys && selected<d->track_count) for(size_t i=0;i<d->tracks[selected].key_count;++i) {
        const TimelineKeyframe* k=&d->tracks[selected].keys[i];
        if(SceneEditorTimelineKeySelected(d->tracks[selected].track_id,k->frame) && k->interpolation_to_next!=keys.primary.interpolation_to_next) mode="Mixed v";
    }
    SceneEditorRenderButton(r,l->controls[TL_INTERPOLATION],mode,u->menu,have_keys);
    SceneEditorRenderButton(r,l->controls[TL_DELETE],"Delete",false,have_keys && keys.count<d->tracks[selected].key_count);
    if(have_keys) snprintf(context,sizeof(context),"%zu selected | Shift-click: add keys | Arrows: scrub",keys.count);
    else snprintf(context,sizeof(context),"Click a diamond to edit it. Scrub on the ruler.");
    const char* error=SceneEditorTimelineSelectionStatus();
    bool rejected=error[0]!=0;
    text(r,u->feedback[0]?u->feedback:rejected?error:context,l->footer.x+444,l->footer.y+7,muted);
    if(u->menu) {
        const char* modes[]={"Hold","Linear","Bezier ease"};
        SDL_Rect box=l->controls[TL_INTERPOLATION];box.y-=72;box.h=24;
        for(int i=0;i<3;++i) {SceneEditorRenderButton(r,box,modes[i],false,true);box.y+=24;}
    }
    /* Hover help keeps the toolbar compact without making symbols ambiguous. */
    int mx,my;SDL_GetMouseState(&mx,&my);
    const char* help[]={"First frame (Home)","Previous frame (Left)","Play / pause (Space)","Next frame (Right)","Last frame (End)","Type frame, Enter applies","Key evaluated value at playhead","Keyframe view: drag diamonds to retime","Curve view: drag keys or Bezier handles","Fit animation range (F)","Fit selected channel's keys","Zoom out; Ctrl+wheel zooms at pointer","Zoom in; middle-drag or Shift+wheel pans","Edit selected values; the playhead stays put","Interpolation from selected keys to the next key","Delete selected keys; Undo restores the group","Retime selection; other selected keys keep their spacing"};
    for(int i=0;!u->feedback[0] && !rejected && i<TL_CONTROL_COUNT;++i) {SDL_Rect b=l->controls[i];if(mx>=b.x && mx<b.x+b.w && my>=b.y && my<b.y+b.h) {fill(r,(SDL_Rect){l->footer.x+440,l->footer.y,l->footer.w-440,l->footer.h},p.panel_fill);text(r,help[i],l->footer.x+444,l->footer.y+7,ink);break;}}
 done:SDL_RenderSetClipRect(r,clipped?&prior:NULL);
}
