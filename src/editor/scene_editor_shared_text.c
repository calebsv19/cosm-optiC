#include "editor/scene_editor_shared_text.h"
#include "render/font_runtime.h"
#include "editor/scene_editor_chrome_shell.h"
#include "render/text_draw.h"
#include "config/config_manager.h"
#include <math.h>
#include <stdio.h>
#include "kit_ui_window_sdl.h"
#include <string.h>
#if USE_VULKAN
#include "vk_renderer.h"
#endif
static CoreResult measure(void *user,const char *text,float *width) {
    SceneEditorSharedText *s=user;int w=0;
    if (!ray_tracing_text_measure_utf8(s->renderer,s->font,text,&w,NULL)) return (CoreResult){CORE_ERR_INVALID_ARG,"editor font measurement failed"};
    *width=(float)w;return core_result_ok();
}
typedef struct Paint {SceneEditorSharedText *s;SDL_Color ink;} Paint;
static CoreResult text(void *user,KitRenderVec2 p,const char *run) {
    Paint *ctx=user;
    return ray_tracing_text_draw_utf8_at(ctx->s->renderer,ctx->s->font,run,(int)p.x,(int)p.y,ctx->ink)
        ? core_result_ok() : (CoreResult){CORE_ERR_IO,"editor text draw failed"};
}
static CoreResult rect(void *user,KitRenderRect r,KitRenderColor c) {
    Paint *ctx=user;SDL_Rect area={(int)r.x,(int)r.y,(int)ceilf(r.width),(int)ceilf(r.height)};
#if USE_VULKAN
    VkRenderer *v=(VkRenderer *)ctx->s->renderer;vk_renderer_set_draw_color(v,c.r/255.f,c.g/255.f,c.b/255.f,c.a/255.f);vk_renderer_fill_rect(v,&area);
#else
    SDL_SetRenderDrawColor(ctx->s->renderer,c.r,c.g,c.b,c.a);SDL_RenderFillRect(ctx->s->renderer,&area);
#endif
    return core_result_ok();
}
static void draw(SceneEditorSharedText *s,SDL_Color ink) {
    SDL_Rect old,clip=s->bounds;
#if USE_VULKAN
    VkRenderer *v=(VkRenderer *)s->renderer;int clipped=vk_renderer_is_clip_enabled(v);vk_renderer_get_clip_rect(v,&old);
#else
    int clipped=SDL_RenderIsClipEnabled(s->renderer);SDL_RenderGetClipRect(s->renderer,&old);
#endif
    if(clipped) SDL_IntersectRect(&old,&clip,&clip);
#if USE_VULKAN
    vk_renderer_set_clip_rect(v,&clip);
#else
    SDL_RenderSetClipRect(s->renderer,&clip);
#endif
    Paint ctx={s,ink};KitUiTextPainter painter={&ctx,rect,text};
    RayTracingThemePalette palette=SceneEditorChromeShellResolvePalette();
    KitUiTextPresentationColors colors={{palette.accent_primary.r,palette.accent_primary.g,palette.accent_primary.b,100},{ink.r,ink.g,ink.b,ink.a},{ink.r,ink.g,ink.b,ink.a}};
    (void)kit_ui_text_presentation_paint(&s->view,&colors,&painter);
#if USE_VULKAN
    vk_renderer_set_clip_rect(v,clipped ? &old : NULL);
#else
    SDL_RenderSetClipRect(s->renderer,clipped ? &old : NULL);
#endif
}
void scene_editor_shared_text_bind(SceneEditorSharedText *s,char *buffer,size_t capacity,unsigned flags) {
    memset(s,0,sizeof(*s));(void)kit_ui_text_bind(&s->edit,buffer,capacity,flags);
}
KitUiTextEventResult scene_editor_shared_text_event(SceneEditorSharedText *s,const SDL_Event *e) {
    if (e->type==SDL_WINDOWEVENT && kit_ui_window_event_invalidates_sdl(e)) {
        s->dragging=0;s->ready=0;kit_ui_text_cancel_composition(&s->edit);
    }
    if(s->ready && e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT && SDL_PointInRect(&(SDL_Point){e->button.x,e->button.y},&s->bounds)) {
        size_t at=0;if(kit_ui_text_presentation_hit(&s->view,e->button.x,e->button.y,measure,s,&at).code==CORE_OK) {
            (void)kit_ui_text_position(&s->edit,at,(SDL_GetModState()&KMOD_SHIFT)?s->edit.anchor:at);s->dragging=1;
        }
        return (KitUiTextEventResult){.consumed=1,.status=core_result_ok()};
    }
    if(s->dragging && (e->type==SDL_MOUSEMOTION || e->type==SDL_MOUSEBUTTONUP)) {
        size_t at=0;float x=e->type==SDL_MOUSEMOTION?e->motion.x:e->button.x,y=e->type==SDL_MOUSEMOTION?e->motion.y:e->button.y;
        if(s->ready && kit_ui_text_presentation_hit(&s->view,x,y,measure,s,&at).code==CORE_OK)(void)kit_ui_text_position(&s->edit,at,s->edit.anchor);
        if(e->type==SDL_MOUSEBUTTONUP)s->dragging=0;
        return (KitUiTextEventResult){.consumed=1,.status=core_result_ok()};
    }
    return kit_ui_text_event_sdl(&s->edit,e);
}
void scene_editor_shared_text_draw(SceneEditorSharedText *s,SDL_Renderer *renderer,SDL_Rect area,SDL_Color ink) {
    s->renderer=renderer;s->bounds=area;SDL_SetTextInputRect(&area);
    int point=animation_config_scale_text_point_size(&animSettings,13,10);
    s->font=ray_tracing_font_runtime_get_ui_regular(renderer,point,10);if(!s->font)return;
    int height=13;ray_tracing_text_line_height(renderer,s->font,&height);
    KitUiTextPresentationOptions options={{area.x+5,area.y+(area.h-height)/2,area.w-10,height},height,1,0,0,1,1};
    s->ready=kit_ui_text_presentation_build(&s->view,&s->edit,&options,measure,s).code==CORE_OK;
    if(s->ready)draw(s,ink);
}
int scene_editor_shared_label(SDL_Renderer *renderer,SDL_Rect area,const char *label,SDL_Color ink,int point,int wrap,int centered) {
    if(!renderer || !label || !*label || area.w<=0 || area.h<=0 || strlen(label)>=KIT_UI_TEXT_PRESENTATION_BYTES)return 0;
    SceneEditorSharedText s={0};char buffer[KIT_UI_TEXT_PRESENTATION_BYTES];snprintf(buffer,sizeof(buffer),"%s",label);
    (void)kit_ui_text_bind(&s.edit,buffer,sizeof(buffer),0);s.renderer=renderer;s.bounds=area;
    point=animation_config_scale_text_point_size(&animSettings,point,10);s.font=ray_tracing_font_runtime_get_ui_regular(renderer,point,10);if(!s.font)return 0;
    if(!wrap) {
        int width=0;
        while(point>10 && ray_tracing_text_measure_utf8(renderer,s.font,label,&width,NULL) && width>area.w) {
            s.font=ray_tracing_font_runtime_get_ui_regular(renderer,--point,10);if(!s.font)return 0;
        }
    }
    int height=13;ray_tracing_text_line_height(renderer,s.font,&height);
    KitUiTextPresentationOptions options={{area.x,area.y,area.w,area.h},height,1,0,wrap,0,0};
    if(kit_ui_text_presentation_build(&s.view,&s.edit,&options,measure,&s).code!=CORE_OK)return 0;
    if(centered) for(size_t i=0;i<s.view.count;i++) {s.view.rows[i].origin.x+=fmaxf(0,(area.w-s.view.rows[i].width)/2);s.view.rows[i].origin.y+=fmaxf(0,(area.h-s.view.content_height)/2);}
    draw(&s,ink);return (int)s.view.content_height;
}

void scene_editor_shared_label_at(SDL_Renderer *r,const char *label,int x,int y,SDL_Color ink,int point) {
    SDL_Rect clip;int w=0,h=0;
    if(SDL_RenderIsClipEnabled(r))SDL_RenderGetClipRect(r,&clip);
    else {SDL_GetRendererOutputSize(r,&w,&h);clip=(SDL_Rect){0,0,w,h};}
    SDL_Rect bounds={x,y,clip.x+clip.w-x,clip.y+clip.h-y};
    int height=point+6;if(bounds.h>height)bounds.h=height;
    scene_editor_shared_label(r,bounds,label,ink,point,0,0);
}
