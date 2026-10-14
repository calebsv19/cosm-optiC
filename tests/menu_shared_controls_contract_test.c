#include "ui/menu_shared_controls.h"
#include "ui/sdl_menu_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void ray_tracing_menu_pane_host_end_splitter_drag(RayTracingMenuPaneHost *h) { (void)h; }
bool ray_tracing_menu_pane_host_splitter_drag_active(const RayTracingMenuPaneHost *h) { (void)h;return false; }
KitRenderTransform kit_render_identity_transform(void) {return (KitRenderTransform){0};}
CoreResult kit_render_push_rect(KitRenderFrame *f,const KitRenderRectCommand *c) {(void)f;(void)c;return core_result_ok();}
static MenuRuntimeState state;
static void publish(float x,int enabled) {
    menu_shared_controls_begin(&state);
    assert(kit_ui_surface_register(&state.sharedControls.surface,(KitUiSurfaceKey){1,MENU_CONTROL_saveRect},
        (KitRenderRect){x,20,100,40},NULL,enabled,NULL).code==CORE_OK);
    menu_shared_controls_end(&state);
}
static SDL_Event pointer(Uint32 type,int x) {
    SDL_Event e={0};e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=30;e.button.clicks=2;return e;
}
int main(void) {
    SDL_Event a,e;
    publish(10,1);
    e=pointer(SDL_MOUSEBUTTONDOWN,25);assert(menu_shared_controls_route(&state,&e,&a) && !a.type);
    e=pointer(SDL_MOUSEBUTTONUP,200);assert(menu_shared_controls_route(&state,&e,&a) && !a.type);
    e=pointer(SDL_MOUSEBUTTONDOWN,25);assert(menu_shared_controls_route(&state,&e,&a));
    e=pointer(SDL_MOUSEBUTTONUP,25);assert(menu_shared_controls_route(&state,&e,&a));assert(a.type==SDL_MOUSEBUTTONDOWN && a.button.clicks==2);
    assert(!menu_shared_controls_route(&state,&e,&a) && !a.type);
    e=pointer(SDL_MOUSEBUTTONDOWN,25);assert(menu_shared_controls_route(&state,&e,&a));publish(200,1);
    e=pointer(SDL_MOUSEBUTTONUP,220);assert(!menu_shared_controls_route(&state,&e,&a) && !a.type);
    publish(10,1);e=pointer(SDL_MOUSEBUTTONDOWN,25);assert(menu_shared_controls_route(&state,&e,&a));
    e.type=SDL_MOUSEWHEEL;assert(!menu_shared_controls_route(&state,&e,&a));
    e=pointer(SDL_MOUSEBUTTONUP,25);assert(!menu_shared_controls_route(&state,&e,&a) && !a.type);
    publish(10,0);e=pointer(SDL_MOUSEBUTTONDOWN,25);assert(menu_shared_controls_route(&state,&e,&a));
    e=pointer(SDL_MOUSEBUTTONUP,25);assert(menu_shared_controls_route(&state,&e,&a) && !a.type);
    publish(10,1);memset(&e,0,sizeof(e));e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_TAB;
    assert(menu_shared_controls_route(&state,&e,&a));assert(state.sharedControls.surface.interaction.focused_id);
    e.key.keysym.sym=SDLK_RETURN;assert(menu_shared_controls_route(&state,&e,&a) && !a.type);
    e.key.repeat=1;assert(menu_shared_controls_route(&state,&e,&a) && !a.type);
    e.type=SDL_KEYUP;e.key.repeat=0;assert(menu_shared_controls_route(&state,&e,&a) && a.type==SDL_MOUSEBUTTONDOWN);
    state.editingOutputRoot=true;e.type=SDL_KEYDOWN;assert(!menu_shared_controls_route(&state,&e,&a));
    state.editingOutputRoot=false;state.folderPickerRequest.active=true;e=pointer(SDL_MOUSEBUTTONDOWN,25);assert(!menu_shared_controls_route(&state,&e,&a));
    state.folderPickerRequest.active=false;publish(10,1);e=pointer(SDL_MOUSEBUTTONDOWN,25);assert(menu_shared_controls_route(&state,&e,&a));
    state.menuWorkspaceHost.active_module=MENU_WORKSPACE_OUTPUT;publish(10,1);e=pointer(SDL_MOUSEBUTTONUP,25);assert(menu_shared_controls_route(&state,&e,&a) && !a.type);
    puts("shared menu activation, release cancellation, geometry/scope, keyboard repeat, disabled/modal and draft ownership: PASS");return 0;
}
