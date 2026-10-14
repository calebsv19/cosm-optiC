#include "render/runtime_native_3d_progress_hud_paint.h"
#include "ui/shared_hud_text.h"
#include "render/text_draw.h"
#include "kit_ui.h"
#include <math.h>
void RuntimeNative3DProgressHUDPaint(SDL_Renderer *r,TTF_Font *font,int width,int height,const char *title,const char *frame,const char *passes,const char *tiles,const char *elapsed,const char *eta,double progress) {
    if(!r || !font || width<32 || height<32)return;
    KitUiHudStyle style;kit_ui_hud_style_dark_floating(&style);style.panel_corner_radius=0;
    int line=16;ray_tracing_text_line_height(r,font,&line);int padding=10,gap=4,bar_height=8;
    int available=width-24;bool stacked=available<480;int wanted=stacked?available:520;int panel_width=wanted<available?wanted:available;
    int rows=stacked?6:3;int panel_height=padding*2+rows*(line+gap)+bar_height+8;if(panel_height>height-24)panel_height=height-24;
    SDL_Rect panel={width-panel_width-12,height-panel_height-12,panel_width,panel_height};
    SDL_Rect prior,clip=panel;SDL_bool clipped=SDL_RenderIsClipEnabled(r);SDL_RenderGetClipRect(r,&prior);if(clipped && !SDL_IntersectRect(&prior,&panel,&clip))return;SDL_RenderSetClipRect(r,&clip);
    SDL_SetRenderDrawColor(r,style.panel_fill.r,style.panel_fill.g,style.panel_fill.b,235);SDL_RenderFillRect(r,&panel);SDL_SetRenderDrawColor(r,84,95,112,220);SDL_RenderDrawRect(r,&panel);
    SDL_Color ink={236,239,244,255},detail={180,190,205,255};int x=panel.x+padding,y=panel.y+padding,w=panel.w-2*padding;
    const char *strings[]={title,frame,passes,elapsed,tiles,eta};
    int first_width=stacked?w:(w-gap)/2;
    ray_tracing_shared_hud_text(r,font,(SDL_Rect){x,y,first_width,line},strings[0],ink);
    if(stacked){y+=line+gap;ray_tracing_shared_hud_text(r,font,(SDL_Rect){x,y,w,line},strings[1],detail);}
    else ray_tracing_shared_hud_text(r,font,(SDL_Rect){x+first_width+gap,y,w-first_width-gap,line},strings[1],detail);
    y+=line+gap;SDL_Rect bar={x,y,w>0?w:0,bar_height};SDL_SetRenderDrawColor(r,34,40,48,255);SDL_RenderFillRect(r,&bar);SDL_Rect fill=bar;fill.w=(int)(fmax(0,fmin(1,progress))*bar.w);SDL_SetRenderDrawColor(r,113,214,182,255);SDL_RenderFillRect(r,&fill);y+=bar_height+8;
    for(int i=2;i<6;i++) {
        int column=stacked?0:(i-2)%2;int row=stacked?i-2:(i-2)/2;
        ray_tracing_shared_hud_text(r,font,(SDL_Rect){x+column*(first_width+gap),y+row*(line+gap),stacked?w:column?w-first_width-gap:first_width,line},strings[i],detail);
    }
    SDL_RenderSetClipRect(r,clipped?&prior:NULL);
}
