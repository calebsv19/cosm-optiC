#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_transform_panel.h"
#include "editor/scene_editor_shared_fields.h"
#include "editor/object_editor_selection_tracker.h"
#include "editor/scene_editor_shared_ui.h"
#include "editor/scene_editor_session_runtime.h"
#include "editor/scene_editor_shared_text.h"
#include "editor/scene_editor_workspace_layout.h"
#include "editor/scene_editor_workspace_profile.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_rename.h"
#include "editor/scene_editor_lifecycle.h"
#include "editor/scene_editor_scene_commands.h"
#include "editor/scene_editor_sidebar.h"
#include "editor/scene_editor_render_control.h"
#include "kit_ui_surface.h"
#include "kit_ui_window_sdl.h"
#include "kit_ui_interaction_sdl.h"
#include <string.h>
#include <limits.h>
#include <stdio.h>
#if USE_VULKAN
#include "vk_renderer.h"
#endif
static struct {
    SceneEditor *editor;KitUiSurface controls;KitUiWindowState observed;
    SDL_Rect rects[48];unsigned keys[48],count;
    unsigned modal,scope;int workspace;unsigned long long revision;
    char document_path[4096], selected_object[128];
    bool dispatching,ready,blocked_release,keyboard_focus;
} ui;
static void cancel(void) {
    kit_ui_interaction_reset(&ui.controls.interaction);ui.controls.activation_count=0;
    scene_editor_shared_fields_cancel();
    ui.blocked_release=true;ui.keyboard_focus=false;
}
void SceneEditorSharedUiReset(void) {memset(&ui,0,sizeof(ui));scene_editor_shared_fields_reset();}
static unsigned modal(void) {
    if(SceneEditorSceneDialogActive())return 5;
    if(SceneEditorLifecycleClosePending())return 3;
    if(SceneEditorRenameActive())return 4;
    if(SceneEditorWorkspaceProfileMenuOpen())return 2;
    if(SceneEditorRenderActive())return 6;
    return 0;
}
static void scope(void) {
    unsigned next=modal();int workspace=SceneEditorWorkspaceProfileGet();
    unsigned long long revision=SceneEditorDocumentRevision();
    const char *path=SceneEditorDocumentPath();if(!path)path="";
    const char *selected=ObjectEditorSelectionTrackerId();if(!selected)selected="";
    if(strcmp(selected,ui.selected_object) || next!=ui.modal || workspace!=ui.workspace || revision!=ui.revision || strcmp(path,ui.document_path)) {
        cancel();ui.ready=false;ui.modal=next;ui.workspace=workspace;ui.revision=revision;snprintf(ui.selected_object,sizeof(ui.selected_object),"%s",selected);snprintf(ui.document_path,sizeof(ui.document_path),"%s",path);
        kit_ui_surface_set_scope(&ui.controls,++ui.scope);
    }
}
static void map(SDL_Rect r,unsigned key) {if(r.w>0 && r.h>0 && ui.count<48){ui.rects[ui.count]=r;ui.keys[ui.count++]=key;}}
void SceneEditorSharedUiBegin(SceneEditor *editor) {
    if(ui.editor!=editor) {SceneEditorSharedUiReset();ui.editor=editor;}
    scope();ui.count=0;scene_editor_shared_fields_frame();
    uint32_t changes=0;
    if(editor->window && kit_ui_window_refresh_sdl(&ui.observed,editor->window,&changes).code!=CORE_OK)cancel();
    else if(changes)cancel();
    SceneEditorPaneLayout layout;SceneEditorWorkspaceChrome chrome;
    if(SceneEditorGetPaneLayout(&layout)) {
        SceneEditorWorkspaceLayoutChrome(&layout,&chrome);
        for(unsigned i=0;i<3;i++)map(chrome.menus[i],1+i);
        map(chrome.workspace,4);map(chrome.display_mode,5);
        for(unsigned i=0;i<2;i++)map(chrome.context_views[i],6+i);
        for(unsigned i=0;i<3;i++)map(chrome.transforms[i],10+i);
        map(chrome.transform_space,13);map(chrome.transform_snap,14);
        map(chrome.frame_all,15);map(chrome.frame_selected,16);map(chrome.undo,17);map(chrome.redo,18);
        for(unsigned i=0;i<SCENE_WORKSPACE_ACTION_COUNT;i++)map(chrome.actions[i],20+i);
        map(chrome.expand,30);map(chrome.restore,31);
    }
    kit_ui_surface_begin(&ui.controls,ui.scope);
}
void SceneEditorSharedUiEnd(void) {
    for(unsigned i=0;i<ui.controls.count;i++) if(ui.controls.controls[i].id==ui.controls.interaction.captured_id || ui.controls.controls[i].id==ui.controls.interaction.key_owner_id)
        for(unsigned j=0;j<ui.controls.next_count;j++) if(ui.controls.next_controls[j].id==ui.controls.controls[i].id && memcmp(&ui.controls.next_controls[j].bounds,&ui.controls.controls[i].bounds,sizeof(KitRenderRect)))cancel();
    ui.ready=kit_ui_surface_end(&ui.controls).code==CORE_OK;
    if(!ui.ready) {cancel();ui.controls.count=0;}
}
static Uint32 window_id(const SDL_Event *e) {
    switch(e->type) {
        case SDL_WINDOWEVENT:return e->window.windowID;case SDL_KEYDOWN:case SDL_KEYUP:return e->key.windowID;
        case SDL_MOUSEMOTION:return e->motion.windowID;case SDL_MOUSEBUTTONDOWN:case SDL_MOUSEBUTTONUP:return e->button.windowID;
        case SDL_MOUSEWHEEL:return e->wheel.windowID;case SDL_TEXTINPUT:return e->text.windowID;case SDL_TEXTEDITING:return e->edit.windowID;
        default:return 0;
    }
}
bool SceneEditorSharedUiFilter(SceneEditor *editor,const SDL_Event *e) {
    if(ui.dispatching)return false;
    if(ui.editor!=editor){SceneEditorSharedUiReset();ui.editor=editor;}
    Uint32 id=window_id(e);if(id && editor->window && id!=SDL_GetWindowID(editor->window))return true;
    uint32_t changes=0;
    if(editor->window && kit_ui_window_refresh_sdl(&ui.observed,editor->window,&changes).code!=CORE_OK)ui.observed.presentable=0;
    if(changes || kit_ui_window_event_invalidates_sdl(e)){cancel();ui.ready=false;}
    bool input=e->type==SDL_KEYDOWN || e->type==SDL_KEYUP || e->type==SDL_TEXTINPUT || e->type==SDL_TEXTEDITING ||
        e->type==SDL_MOUSEBUTTONDOWN || e->type==SDL_MOUSEBUTTONUP || e->type==SDL_MOUSEMOTION || e->type==SDL_MOUSEWHEEL;
    if(input && editor->window && !ui.observed.presentable)return true;
    if(e->type==SDL_WINDOWEVENT || e->type==SDL_QUIT)return false;
    if(e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT)ui.blocked_release=false;
    if(e->type==SDL_MOUSEBUTTONUP && ui.blocked_release)return true;
    return false;
}
bool SceneEditorSharedUiRoute(SceneEditor *editor,SDL_Event *e) {
    if(ui.dispatching)return false;scope();
    bool key=e->type==SDL_KEYDOWN || e->type==SDL_KEYUP;
    if(key && (SDL_IsTextInputActive() || SceneEditorRenameActive() || SceneEditorSceneDialogActive() || SceneEditorSidebarTextActive()))return false;
    if(SceneEditorSessionInteractionActive(editor) && !SceneEditorTransformPanelInteractionActive() && !ui.controls.interaction.pointer_owned)return false;
    if(e->type==SDL_MOUSEBUTTONDOWN && !ui.controls.interaction.pointer_owned)ui.keyboard_focus=false;
    if(e->type==SDL_MOUSEWHEEL){if(ui.controls.interaction.pointer_owned || ui.controls.interaction.key_owner_id)cancel();return false;}
    if(e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_F6 && !e->key.repeat && ui.ready) {
        KitUiInteractionEvent tab={.type=KIT_UI_INTERACTION_KEY_DOWN,.key=KIT_UI_INTERACTION_KEY_TAB};KitUiInteractionResult result;
        (void)kit_ui_surface_route(&ui.controls,&tab,&result);ui.keyboard_focus=true;return true;
    }
    /* F6 opts into chrome traversal; ordinary Tab retains workspace cycling. */
    if(key && !ui.keyboard_focus)return false;
    if(!ui.ready && e->type==SDL_MOUSEBUTTONDOWN) {
        SceneEditorPaneLayout layout;
        SDL_Point point={e->button.x,e->button.y};
        /* Do not fall through to the legacy down handler before a fresh paint
         * snapshot exists (first present, resize, document or modal switch). */
        if(ui.modal || (SceneEditorGetPaneLayout(&layout) &&
            (SDL_PointInRect(&point,&layout.workspace_header_rect) || SDL_PointInRect(&point,&layout.mode_router_rect))))return true;
    }
    KitUiInteractionEvent event;KitUiInteractionResult result;
    if(!ui.ready)return false;
    if(!kit_ui_interaction_event_from_sdl(e,&event)) {
        if(key && ui.controls.interaction.key_owner_id)cancel();
        return false;
    }
    if(kit_ui_surface_route(&ui.controls,&event,&result).code!=CORE_OK){cancel();return true;}
    if(result.activated_id && kit_ui_surface_take_activation(&ui.controls,result.activated_id)) {
        for(unsigned i=0;i<ui.controls.count;i++)if(ui.controls.controls[i].id==result.activated_id) {
            KitRenderRect r=ui.controls.controls[i].bounds;
            SDL_Event activation={0};activation.type=SDL_MOUSEBUTTONDOWN;activation.button.windowID=editor->window?SDL_GetWindowID(editor->window):0;
            activation.button.button=SDL_BUTTON_LEFT;activation.button.clicks=1;activation.button.x=r.x+r.width/2;activation.button.y=r.y+r.height/2;
            ui.dispatching=true;SceneEditorSessionRuntimeHandleEvent(editor,&activation);ui.dispatching=false;
            ui.controls.activation_count=0;ui.ready=false;
            scope();break;
        }
    }
    return result.consumed!=0;
}
static CoreThemeColor color(SDL_Color c){return (CoreThemeColor){c.r,c.g,c.b,c.a};}
void SceneEditorSharedUiModalButton(SDL_Renderer *renderer,unsigned domain,unsigned key,SDL_Rect r,const char *label,bool enabled,SDL_Color base,RayTracingThemePalette palette) {
    if(r.w<=0 || r.h<=0)return;
    bool eligible=!ui.modal || domain==ui.modal || (ui.modal==2 && domain==1 && key<=5);
    KitUiInteractionControl control={0};KitRenderRect bounds={r.x,r.y,r.w,r.h};
#if USE_VULKAN
    SDL_Rect clip;int clipped=vk_renderer_is_clip_enabled((VkRenderer *)renderer);vk_renderer_get_clip_rect((VkRenderer *)renderer,&clip);
#else
    SDL_Rect clip;int clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&clip);
#endif
    KitRenderRect clipping={clip.x,clip.y,clip.w,clip.h};
    KitUiButtonState state={.disabled=!enabled};
    if(eligible && ui.controls.collecting) {
        if(kit_ui_surface_register(&ui.controls,(KitUiSurfaceKey){domain,key},bounds,clipped?&clipping:NULL,enabled,&control).code!=CORE_OK)return;
        state=kit_ui_interaction_button_state(&ui.controls.interaction,&control,0);
    }
    KitUiButtonSpec spec={label,KIT_UI_BUTTON_VARIANT_DEFAULT,state};
    KitUiButtonTheme theme={color(base),color(base),color(palette.button_active_fill),color(base),color(palette.panel_border),color(palette.accent_primary),color(palette.button_text),color(palette.text_muted)};
    KitUiButtonStyle style;if(!kit_ui_button_style_resolve(&theme,&spec,&style))return;
    int border=spec.state.focused?2:1;SDL_Rect inner={r.x+border,r.y+border,r.w-2*border,r.h-2*border};
#if USE_VULKAN
    VkRenderer *v=(VkRenderer *)renderer;
    vk_renderer_set_draw_color(v,style.outline.r/255.f,style.outline.g/255.f,style.outline.b/255.f,style.outline.a/255.f);vk_renderer_fill_rect(v,&r);
    vk_renderer_set_draw_color(v,style.fill.r/255.f,style.fill.g/255.f,style.fill.b/255.f,style.fill.a/255.f);vk_renderer_fill_rect(v,&inner);
#else
    SDL_SetRenderDrawColor(renderer,style.outline.r,style.outline.g,style.outline.b,style.outline.a);SDL_RenderFillRect(renderer,&r);
    SDL_SetRenderDrawColor(renderer,style.fill.r,style.fill.g,style.fill.b,style.fill.a);SDL_RenderFillRect(renderer,&inner);
#endif
    SDL_Color fill={style.fill.r,style.fill.g,style.fill.b,style.fill.a};
    SDL_Color ink=spec.state.disabled ? palette.text_muted : ray_tracing_theme_choose_button_text(fill,palette);
    scene_editor_shared_label(renderer,inner,label,ink,13,0,1);
}
bool SceneEditorSharedUiButton(SDL_Renderer *renderer,SDL_Rect r,const char *label,bool enabled,SDL_Color base,RayTracingThemePalette palette) {
    if(!ui.controls.collecting)return false;
    for(unsigned i=0;i<ui.count;i++)if(!memcmp(&r,&ui.rects[i],sizeof(r))) {
        SceneEditorSharedUiModalButton(renderer,1,ui.keys[i],r,label,enabled,base,palette);return true;
    }
    return false;
}

void SceneEditorSharedUiExclude(SDL_Rect r) {
    for(unsigned i=0;i<ui.controls.next_count;) {
        KitRenderRect b=ui.controls.next_controls[i].bounds;
        if(b.x==r.x && b.y==r.y && b.width==r.w && b.height==r.h) {
            --ui.controls.next_count;
            memmove(ui.controls.next_controls+i,ui.controls.next_controls+i+1,(ui.controls.next_count-i)*sizeof(*ui.controls.next_controls));
            memmove(ui.controls.next_keys+i,ui.controls.next_keys+i+1,(ui.controls.next_count-i)*sizeof(*ui.controls.next_keys));
        }else ++i;
    }
}
void SceneEditorSharedUiPanelButton(SDL_Renderer *r,unsigned domain,unsigned key,SDL_Rect bounds,const char *label,bool selected,bool enabled) {
    SceneEditorSharedUiExclude(bounds);
    if(scene_editor_shared_field_paint(r,bounds,label))return;
    RayTracingThemePalette palette=SceneEditorChromeShellResolvePalette();
    SceneEditorSharedUiModalButton(r,domain,key,bounds,label,enabled,selected?palette.accent_primary:palette.button_fill,palette);
}
