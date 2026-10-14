#include "ui/menu_shared_controls.h"
#include "ui/sdl_menu_state.h"
#include "ui/sdl_menu_render.h"
#include "ui/menu/sdl_menu_render_internal.h"
#include "ui/shared_theme_font_adapter.h"
#include "config/config_manager.h"
#include "engine/Render/render_pipeline.h"
#include "render/text_draw.h"
#include <string.h>
#if USE_VULKAN
#include "vk_renderer.h"
#endif
static CoreThemeColor color(SDL_Color c) {return (CoreThemeColor){c.r,c.g,c.b,c.a};}
static void fill(SDL_Renderer *renderer, SDL_Rect r, float radius, CoreThemeColor c) {
#if USE_VULKAN
    VkRenderer *vk=(VkRenderer *)renderer;
    vk_renderer_set_draw_color(vk,c.r/255.f,c.g/255.f,c.b/255.f,c.a/255.f);
    SDL_FRect rect={(float)r.x,(float)r.y,(float)r.w,(float)r.h};
    vk_renderer_fill_rounded_rect(vk,&rect,radius);
#else
    kit_ui_sdl_fill_rounded_rect(renderer,&r,(int)radius,(KitRenderColor){c.r,c.g,c.b,c.a});
#endif
}
void menu_shared_controls_button(MenuRuntimeState *s, unsigned key, SDL_Renderer *renderer,
    TTF_Font *font, const SDL_Rect *rect, const char *label, bool selected) {
    if (!s || !renderer || !font || !rect || rect->w<=0 || rect->h<=0) return;
    bool enabled=!s->folderPickerRequest.active;
    if (key==MENU_CONTROL_asyncDeepRenderRect && animation_config_space_mode_clamp(animSettings.spaceMode)==SPACE_MODE_3D) enabled=false;
    if (!animSettings.deepRenderMode && (key==MENU_CONTROL_bounceRect || key==MENU_CONTROL_autoMp4Rect ||
        key==MENU_CONTROL_asyncDeepRenderRect || key==MENU_CONTROL_sceneEditorRect)) enabled=false;
    KitUiInteractionControl control={0};
    KitRenderRect bounds={(float)rect->x,(float)rect->y,(float)rect->w,(float)rect->h};
#if USE_VULKAN
    SDL_Rect clip;vk_renderer_get_clip_rect((VkRenderer *)renderer,&clip);
    bool clipped=vk_renderer_is_clip_enabled((VkRenderer *)renderer)!=0;
#else
    SDL_Rect clip;SDL_RenderGetClipRect(renderer,&clip);
    bool clipped=SDL_RenderIsClipEnabled(renderer)!=0;
#endif
    KitRenderRect clip_bounds={(float)clip.x,(float)clip.y,(float)clip.w,(float)clip.h};
    if (kit_ui_surface_register(&s->sharedControls.surface,(KitUiSurfaceKey){1,key},bounds,
            clipped ? &clip_bounds : NULL,enabled,&control).code!=CORE_OK) return;
    KitUiButtonState state=kit_ui_interaction_button_state(&s->sharedControls.surface.interaction,&control,selected);
    RayTracingThemePalette palette={0};
    if (!ray_tracing_shared_theme_resolve_palette(&palette)) {
        palette.button_fill=(SDL_Color){100,100,100,255};palette.button_active_fill=(SDL_Color){0,255,0,255};
        palette.button_text=(SDL_Color){255,255,255,255};palette.accent_primary=(SDL_Color){0,200,80,255};
        palette.panel_border=(SDL_Color){35,42,50,255};palette.text_muted=(SDL_Color){164,167,164,255};
    }
    SDL_Color selected_fill=menu_render_ensure_highlight_fill_contrast(palette.button_active_fill,palette.button_text,palette.panel_fill);
    KitUiButtonTheme theme={color(palette.button_fill),color(selected_fill),color(palette.button_fill),color(selected_fill),
        color(palette.panel_border),color(palette.accent_primary),color(palette.button_text),color(palette.text_muted)};
    theme.hover_fill.r=(Uint8)(theme.hover_fill.r>243 ? 255 : theme.hover_fill.r+12);
    theme.hover_fill.g=(Uint8)(theme.hover_fill.g>243 ? 255 : theme.hover_fill.g+12);
    theme.hover_fill.b=(Uint8)(theme.hover_fill.b>243 ? 255 : theme.hover_fill.b+12);
    KitUiButtonSpec spec={label,KIT_UI_BUTTON_VARIANT_DEFAULT,state};
    bool primary=key==MENU_CONTROL_startRect;
    if (primary) {spec.state.selected=1;spec.variant=KIT_UI_BUTTON_VARIANT_PRIMARY;theme.selected_fill=color(palette.accent_primary);}
    KitUiButtonStyle style;
    if (!kit_ui_button_style_resolve(&theme,&spec,&style)) return;
    float radius=primary ? 0 : 4; /* Retain the launch rectangle and existing compact corners. */
    int border=state.focused ? 2 : primary ? 1 : 0;
    if (border) fill(renderer,*rect,radius,style.outline);
    SDL_Rect inner={rect->x+border,rect->y+border,rect->w-2*border,rect->h-2*border};
    fill(renderer,inner,radius>border ? radius-border : 0,style.fill);
    SDL_Color text=menu_render_choose_readable_text((SDL_Color){style.fill.r,style.fill.g,style.fill.b,style.fill.a},
        (SDL_Color){style.text.r,style.text.g,style.text.b,style.text.a});
    int decoration=(key==MENU_CONTROL_environmentBackgroundModeRect && rect->w>=48 && rect->h>=18) ? 44 : 0;
    char fitted[256];menu_render_fit_text_to_width(font,label,rect->w-16-decoration,fitted,sizeof(fitted));
    int width=0,height=0;
    if (ray_tracing_text_measure_utf8(renderer,font,fitted,&width,&height)) {
        SDL_Rect text_clip={rect->x+8,rect->y+3,rect->w-16-decoration,rect->h-6};
        if (clipped) SDL_IntersectRect(&clip,&text_clip,&text_clip);
#if USE_VULKAN
        vk_renderer_set_clip_rect((VkRenderer *)renderer,&text_clip);
#else
        SDL_RenderSetClipRect(renderer,&text_clip);
#endif
        (void)ray_tracing_text_draw_utf8_at(renderer,font,fitted,rect->x+(rect->w-decoration-width)/2,rect->y+(rect->h-height)/2,text);
#if USE_VULKAN
        vk_renderer_set_clip_rect((VkRenderer *)renderer,clipped ? &clip : NULL);
#else
        SDL_RenderSetClipRect(renderer,clipped ? &clip : NULL);
#endif
    }
}
