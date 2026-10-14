#include "ui/menu_shared_controls.h"
#include "ui/sdl_menu_state.h"
#include "ui/menu_batch_panel.h"
#include "kit_ui_interaction_sdl.h"
#include <string.h>

static bool editing(const MenuRuntimeState *s) {
    return s->numericEdit.active || s->editingBounce || s->editingFrame || s->editingStartFrame ||
        s->editingInputRoot || s->editingMeshAssetRoot || s->editingOutputRoot ||
        s->editingFrameDir || s->editingVideoOutputRoot;
}
void menu_shared_controls_cancel(MenuRuntimeState *s) {
    if (!s) return;
    KitUiInteractionEvent cancel={.type=KIT_UI_INTERACTION_CANCEL};
    KitUiInteractionResult result;
    (void)kit_ui_interaction_route(&s->sharedControls.surface.interaction,
        s->sharedControls.surface.controls,s->sharedControls.surface.count,&cancel,&result);
    s->sharedControls.surface.activation_count=0;
    s->sharedControls.release_blocked=true;
    memset(&s->sharedControls.press,0,sizeof(s->sharedControls.press));
}
static Uint32 event_window(const SDL_Event *e) {
    switch(e->type) {
        case SDL_WINDOWEVENT:return e->window.windowID;
        case SDL_KEYDOWN:case SDL_KEYUP:return e->key.windowID;
        case SDL_MOUSEMOTION:return e->motion.windowID;
        case SDL_MOUSEBUTTONDOWN:case SDL_MOUSEBUTTONUP:return e->button.windowID;
        case SDL_MOUSEWHEEL:return e->wheel.windowID;
        case SDL_TEXTINPUT:return e->text.windowID;
        case SDL_TEXTEDITING:return e->edit.windowID;
        default:return 0;
    }
}
bool menu_shared_controls_filter_window(MenuRuntimeState *s, SDL_Window *window, const SDL_Event *e) {
    MenuSharedControls *h=&s->sharedControls;
    Uint32 id=event_window(e);
    if (id && id!=SDL_GetWindowID(window)) return true;
    h->window=window;
    uint32_t changes=0;
    if (kit_ui_window_refresh_sdl(&h->observed,window,&changes).code!=CORE_OK) h->observed.presentable=0;
    if (changes || kit_ui_window_event_invalidates_sdl(e)) {
        menu_shared_controls_cancel(s);
        int count=0;const Uint8 *keys=SDL_GetKeyboardState(&count);
        memset(h->blocked_keys,0,sizeof(h->blocked_keys));
        if (count>SDL_NUM_SCANCODES) count=SDL_NUM_SCANCODES;
        if (keys && count>0) memcpy(h->blocked_keys,keys,(size_t)count);
        if (changes & ~KIT_UI_WINDOW_FOCUS) h->geometry_dirty=true;
        s->draggingSlider=false;s->selectedSlider=NULL;
        for (int i=0;i<MENU_SCROLL_COUNT;i++) s->scrolls[i].dragging=false;
        ray_tracing_menu_pane_host_end_splitter_drag(&s->menuPaneHost);
    }
    if (e->type==SDL_WINDOWEVENT || e->type==SDL_QUIT) return false;
    bool key=e->type==SDL_KEYDOWN || e->type==SDL_KEYUP;
    bool pointer=e->type==SDL_MOUSEMOTION || e->type==SDL_MOUSEBUTTONDOWN || e->type==SDL_MOUSEBUTTONUP || e->type==SDL_MOUSEWHEEL;
    if ((key || pointer || e->type==SDL_TEXTINPUT || e->type==SDL_TEXTEDITING) && !h->observed.presentable) return true;
    if (key && !(h->observed.flags & SDL_WINDOW_INPUT_FOCUS)) return true;
    if (key && e->key.keysym.scancode>=0 && e->key.keysym.scancode<SDL_NUM_SCANCODES) {
        int code=e->key.keysym.scancode;
        if (h->blocked_keys[code]) { if(e->type==SDL_KEYUP)h->blocked_keys[code]=0; return true; }
    }
    /* Editor retains its own window and shortcut policy in this bounded slice. */
    if (s->activeView!=MENU_VIEW_MAIN) {menu_shared_controls_cancel(s);return false;}
    if (kit_ui_window_fullscreen_key_sdl(window,e)) { menu_shared_controls_cancel(s);h->geometry_dirty=true;return true; }
    if ((pointer || (key && !editing(s))) && h->geometry_dirty) return true;
    if (e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT) h->release_blocked=false;
    if (e->type==SDL_MOUSEBUTTONUP && e->button.button==SDL_BUTTON_LEFT && h->release_blocked) return true;
    if (e->type==SDL_MOUSEMOTION && h->release_blocked && (e->motion.state & SDL_BUTTON_LMASK)) return true;
    return false;
}
void menu_shared_controls_begin(MenuRuntimeState *s) {
    uint32_t scope=1u+(uint32_t)s->menuWorkspaceHost.active_module*16u+(uint32_t)s->rendererControlsTab;
    if (s->folderPickerRequest.active) scope+=1024u;
    kit_ui_surface_begin(&s->sharedControls.surface,scope);
}
void menu_shared_controls_end(MenuRuntimeState *s) {
    KitUiSurface *surface=&s->sharedControls.surface;
    /* A moved/reclipped owner cannot receive a release intended for old geometry. */
    for (uint32_t i=0;i<surface->count;i++) {
        if (surface->controls[i].id!=surface->interaction.captured_id && surface->controls[i].id!=surface->interaction.key_owner_id) continue;
        for (uint32_t j=0;j<surface->next_count;j++) {
            if (surface->next_controls[j].id==surface->controls[i].id &&
                memcmp(&surface->next_controls[j].bounds,&surface->controls[i].bounds,sizeof(KitRenderRect))) menu_shared_controls_cancel(s);
        }
    }
    if (kit_ui_surface_end(surface).code!=CORE_OK) {
        menu_shared_controls_cancel(s);surface->count=0;
    }
    s->sharedControls.geometry_dirty=false;
}
bool menu_shared_controls_route(MenuRuntimeState *s, const SDL_Event *e, SDL_Event *activation) {
    memset(activation,0,sizeof(*activation));
    if (s->activeView!=MENU_VIEW_MAIN || s->folderPickerRequest.active) {menu_shared_controls_cancel(s);return false;}
    if ((e->type==SDL_KEYDOWN || e->type==SDL_KEYUP) && editing(s)) {
        menu_shared_controls_cancel(s);return false;
    }
    if (s->draggingSlider || ray_tracing_menu_pane_host_splitter_drag_active(&s->menuPaneHost)) return false;
    for (int i=0;i<MENU_SCROLL_COUNT;i++) if(s->scrolls[i].dragging)return false;
    if (e->type==SDL_MOUSEWHEEL) {
        if (s->sharedControls.surface.interaction.captured_id || s->sharedControls.surface.interaction.key_owner_id) menu_shared_controls_cancel(s);
        return false;
    }
    KitUiInteractionEvent event;KitUiInteractionResult result;
    if (!kit_ui_interaction_event_from_sdl(e,&event)) return false;
    KitUiSurface *surface=&s->sharedControls.surface;
    if (kit_ui_surface_route(surface,&event,&result).code!=CORE_OK) {menu_shared_controls_cancel(s);return true;}
    if (e->type==SDL_MOUSEBUTTONDOWN && result.consumed) s->sharedControls.press=*e;
    if (result.activated_id && kit_ui_surface_take_activation(surface,result.activated_id)) {
        for (uint32_t i=0;i<surface->count;i++) if(surface->controls[i].id==result.activated_id) {
            KitRenderRect rect=surface->controls[i].bounds;
            *activation=(SDL_Event){0};activation->type=SDL_MOUSEBUTTONDOWN;
            activation->button.windowID=e->type==SDL_MOUSEBUTTONUP ? e->button.windowID : e->key.windowID;
            activation->button.button=SDL_BUTTON_LEFT;activation->button.clicks=1;
            activation->button.x=(int)(rect.x+rect.width/2);activation->button.y=(int)(rect.y+rect.height/2);
            if (e->type==SDL_MOUSEBUTTONUP && s->sharedControls.press.type==SDL_MOUSEBUTTONDOWN) *activation=s->sharedControls.press;
            break;
        }
    }
    return result.consumed!=0;
}
