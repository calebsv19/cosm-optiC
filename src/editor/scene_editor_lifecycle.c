#include "editor/scene_editor_lifecycle.h"
#include "editor/scene_editor_chrome_actions.h"
#include "editor/scene_editor_chrome_shell.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_internal.h"
#include "editor/scene_editor_transform_panel.h"
#include "editor/material_editor_authored_texture_binding.h"
#include "editor/scene_editor_typography.h"
#include "app/ray_tracing_deep_render_desktop_host.h"
#include <stdio.h>
#include <string.h>

static bool pending;
static int focus=2;
static char reason[64], error[128];
static SDL_Rect choices[3];
void SceneEditorLifecycleReset(void) { pending=false; error[0]=0; }
bool SceneEditorLifecycleClosePending(void) { return pending; }
void SceneEditorLifecycleRequestClose(SceneEditor* editor, const char* requested_reason) {
    if (!editor || pending) return;
    if (SceneEditorTransformPanelInteractionActive() ||
        MaterialEditorAuthoredTextureBindingPickerActive() ||
        RayTracingDeepRenderDesktopHost_HasActiveWork()) {
        SceneEditorChromeShellSetActionFeedback("Finish or cancel the active edit, import or render before closing",4000);
        return;
    }
    pending=true; error[0]=0; focus=2;
    snprintf(reason,sizeof(reason),"%s",requested_reason);
    fprintf(stderr,"[editor lifecycle] close requested: %s; document_dirty=%d\n",
        reason,SceneEditorDocumentIsDirty());
}
bool SceneEditorLifecycleHandleEvent(SceneEditor* editor, SDL_Event* event) {
    if (event->type==SDL_QUIT || (event->type==SDL_WINDOWEVENT &&
        event->window.event==SDL_WINDOWEVENT_CLOSE && editor->window &&
        event->window.windowID==SDL_GetWindowID(editor->window))) {
        SceneEditorLifecycleRequestClose(editor,event->type==SDL_QUIT ? "application quit" : "window close");
        return true;
    }
    if (!pending) return false;
    if (event->type==SDL_DROPFILE) { SDL_free(event->drop.file); event->drop.file=NULL; return true; }
    if (event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_ESCAPE) {
        pending=false; fprintf(stderr,"[editor lifecycle] close cancelled\n"); return true;
    }
    SDL_Event keyboard_click;
    if (event->type==SDL_KEYDOWN) {
        if (event->key.keysym.sym==SDLK_TAB || event->key.keysym.sym==SDLK_RIGHT) {
            focus=(focus+((event->key.keysym.mod & KMOD_SHIFT) ? 2 : 1))%3;
        } else if (event->key.keysym.sym==SDLK_LEFT) focus=(focus+2)%3;
        else if ((event->key.keysym.sym==SDLK_RETURN || event->key.keysym.sym==SDLK_KP_ENTER) && !event->key.repeat) {
            keyboard_click=(SDL_Event){0}; keyboard_click.type=SDL_MOUSEBUTTONDOWN;
            keyboard_click.button.button=SDL_BUTTON_LEFT;
            keyboard_click.button.x=choices[focus].x+choices[focus].w/2;
            keyboard_click.button.y=choices[focus].y+choices[focus].h/2;
            event=&keyboard_click;
        }
    }
    if (event->type==SDL_MOUSEBUTTONDOWN && event->button.button==SDL_BUTTON_LEFT) {
        SDL_Point point={event->button.x,event->button.y};
        for (int i=0;i<3;++i) if (SDL_PointInRect(&point,&choices[i])) {
            if (i==2) { pending=false; fprintf(stderr,"[editor lifecycle] close cancelled\n"); }
            else if (i==0 && !SceneEditorChromeActionsSaveAuthoring()) {
                snprintf(error,sizeof(error),"Save failed. The editor remains open; resolve the error or cancel.");
                fprintf(stderr,"[editor lifecycle] close blocked: save failed\n");
            } else {
                fprintf(stderr,"[editor lifecycle] leaving editor: %s; %s\n",reason,
                    i==0 ? "saved" : "without saving");
                pending=false; editor->running=false; sceneEditorExitFlag=true;
            }
            break;
        }
    }
    /* Keep window resize/focus notifications flowing; all editing input is modal. */
    return event->type!=SDL_WINDOWEVENT;
}
void SceneEditorLifecycleRender(SDL_Renderer* renderer) {
    if (!pending) return;
    SceneEditorPaneLayout layout;
    if (!SceneEditorGetPaneLayout(&layout)) return;
    RayTracingThemePalette p=SceneEditorChromeShellResolvePalette();
    SDL_Rect prior; SDL_bool clipped=SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer,&prior); SDL_RenderSetClipRect(renderer,NULL);
    int width=layout.workspace_header_rect.w-32; if (width>580) width=580;
    SDL_Rect box={layout.workspace_header_rect.x+(layout.workspace_header_rect.w-width)/2,110,width,190};
    SDL_SetRenderDrawColor(renderer,p.panel_fill.r,p.panel_fill.g,p.panel_fill.b,255);
    SDL_RenderFillRect(renderer,&box);
    SDL_SetRenderDrawColor(renderer,p.panel_border.r,p.panel_border.g,p.panel_border.b,255);
    SDL_RenderDrawRect(renderer,&box);
    SceneEditorLabelLeft(renderer,(SDL_Rect){box.x+16,box.y+12,box.w-32,24},"Leave scene editor?",p.text_primary);
    SceneEditorLabelWrapped(renderer,(SDL_Rect){box.x+16,box.y+44,box.w-32,58},
        error[0] ? error : "Save scene edits before leaving. Leaving without saving does not undo earlier saves or imports. Escape keeps editing.",p.text_primary);
    static const char* labels[]={"Save and leave","Leave without saving","Keep editing"};
    int w=(box.w-40)/3;
    for (int i=0;i<3;++i) {
        choices[i]=(SDL_Rect){box.x+12+i*(w+8),box.y+128,w,42};
        SDL_Color fill=i==focus ? p.button_active_fill : p.button_fill;
        SDL_SetRenderDrawColor(renderer,fill.r,fill.g,fill.b,255); SDL_RenderFillRect(renderer,&choices[i]);
        SceneEditorButtonText(renderer,choices[i],labels[i],ray_tracing_theme_choose_button_text(fill,p));
    }
    SDL_RenderSetClipRect(renderer,clipped ? &prior : NULL);
}
