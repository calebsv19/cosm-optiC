#include "app/preview_shared_controls.h"
#include <assert.h>
#include <stdio.h>
KitRenderTransform kit_render_identity_transform(void) { return (KitRenderTransform){0}; }
CoreResult kit_render_push_rect(KitRenderFrame *f,const KitRenderRectCommand *c) {
    (void)f;(void)c;return core_result_ok();
}
static SDL_Event pointer(Uint32 type,int x,int y) {
    SDL_Event e={0};e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;return e;
}
int main(void) {
    PreviewSharedControls c={0};
    PreviewWorkspace w={.layout={.panel={0,512,800,88},
        .play_pause_button={12,532,76,34},.loop_button={98,532,64,34},.bounce_button={170,532,76,34}}};
    PreviewSharedAction a;
    SDL_Event e=pointer(SDL_MOUSEBUTTONDOWN,20,540);
    assert(!PreviewSharedControlsRoute(&c,&e,&a) && a==PREVIEW_SHARED_NONE);
    PreviewSharedControlsPublish(&c,&w);
    assert(PreviewSharedControlsRoute(&c,&e,&a) && a==PREVIEW_SHARED_NONE);
    e=pointer(SDL_MOUSEBUTTONUP,20,540);
    assert(PreviewSharedControlsRoute(&c,&e,&a) && a==PREVIEW_SHARED_PLAY);
    assert(!PreviewSharedControlsRoute(&c,&e,&a) && a==PREVIEW_SHARED_NONE);
    e=pointer(SDL_MOUSEBUTTONDOWN,110,540);assert(PreviewSharedControlsRoute(&c,&e,&a));
    e=pointer(SDL_MOUSEBUTTONUP,180,540);assert(PreviewSharedControlsRoute(&c,&e,&a) && !a);
    e=pointer(SDL_MOUSEBUTTONDOWN,180,540);assert(PreviewSharedControlsRoute(&c,&e,&a));
    PreviewSharedControlsCancel(&c);PreviewSharedControlsPublish(&c,&w);
    e=pointer(SDL_MOUSEBUTTONUP,180,540);assert(PreviewSharedControlsRoute(&c,&e,&a) && !a);
    e=pointer(SDL_MOUSEBUTTONDOWN,180,540);assert(PreviewSharedControlsRoute(&c,&e,&a));
    w.layout.bounce_button.x=300;PreviewSharedControlsPublish(&c,&w);
    e=pointer(SDL_MOUSEBUTTONUP,310,540);assert(PreviewSharedControlsRoute(&c,&e,&a) && !a);
    e=pointer(SDL_MOUSEBUTTONDOWN,310,540);assert(PreviewSharedControlsRoute(&c,&e,&a));
    e=pointer(SDL_MOUSEBUTTONUP,310,540);assert(PreviewSharedControlsRoute(&c,&e,&a) && a==PREVIEW_SHARED_BOUNCE);
    e=(SDL_Event){0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_SPACE;
    assert(!PreviewSharedControlsRoute(&c,&e,&a)); /* host Space shortcut */
    PreviewSharedControlsCancel(&c);PreviewSharedControlsPublish(&c,&w);
    e.key.keysym.sym=SDLK_F6;assert(PreviewSharedControlsRoute(&c,&e,&a));
    e.key.keysym.sym=SDLK_RETURN;assert(PreviewSharedControlsRoute(&c,&e,&a) && !a);
    e.key.repeat=1;assert(PreviewSharedControlsRoute(&c,&e,&a) && !a);
    e.type=SDL_KEYUP;e.key.repeat=0;assert(PreviewSharedControlsRoute(&c,&e,&a) && a==PREVIEW_SHARED_PLAY);
    e.type=SDL_KEYDOWN;assert(PreviewSharedControlsRoute(&c,&e,&a));
    PreviewSharedControlsCancel(&c);PreviewSharedControlsPublish(&c,&w);
    e.type=SDL_KEYUP;assert(!PreviewSharedControlsRoute(&c,&e,&a) && !a);
    SDL_Rect close=PreviewSharedCloseRect(800);
    e=pointer(SDL_MOUSEBUTTONDOWN,close.x+5,close.y+5);assert(PreviewSharedControlsRoute(&c,&e,&a) && !a);
    e.type=SDL_MOUSEBUTTONUP;assert(PreviewSharedControlsRoute(&c,&e,&a) && a==PREVIEW_SHARED_CLOSE);
    puts("preview shared controls: release, cancellation, geometry, repeat, keyboard and close PASS");
    return 0;
}
