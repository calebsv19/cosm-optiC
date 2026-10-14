#include "preview_shared_ui_native_test.h"
#include "app/preview_session.h"
#include "config/config_manager.h"
#include "app/preview_shared_controls.h"
#include "editor/scene_editor_document.h"
#include "editor/scene_editor_session_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct PreviewInjection { Uint32 window; int width; } PreviewInjection;
static void key(Uint32 window,SDL_Keycode code) {
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.windowID=window;e.key.keysym.sym=code;
    assert(SDL_PushEvent(&e)==1);e.type=SDL_KEYUP;assert(SDL_PushEvent(&e)==1);
}
static int inject(void *user) {
    PreviewInjection *p=user;
    SDL_Delay(240);key(p->window,SDLK_RIGHT);
    SDL_Delay(120);
    SDL_Rect close=PreviewSharedCloseRect(p->width);
    SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.windowID=p->window;
    e.button.button=SDL_BUTTON_LEFT;e.button.x=close.x+8;e.button.y=close.y+8;
    assert(SDL_PushEvent(&e)==1);
    e=(SDL_Event){0};e.type=SDL_WINDOWEVENT;e.window.windowID=p->window;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    assert(SDL_PushEvent(&e)==1);
    e=(SDL_Event){0};e.type=SDL_MOUSEBUTTONUP;e.button.windowID=p->window;
    e.button.button=SDL_BUTTON_LEFT;e.button.x=close.x+8;e.button.y=close.y+8;
    assert(SDL_PushEvent(&e)==1);
    SDL_Delay(240);key(p->window,SDLK_RIGHT);
    SDL_Delay(120);key(p->window,SDLK_ESCAPE);
    return 0;
}
void preview_shared_ui_native_acceptance(SceneEditor *editor) {
    if (animSettings.framesForTravel < 3) animSettings.framesForTravel=3;
    unsigned long long revision=SceneEditorDocumentRevision();
    char path[4096];snprintf(path,sizeof(path),"%s",SceneEditorDocumentPath());
    const int sizes[][2]={{800,600},{480,360}};
    for (unsigned i=0;i<2;++i) {
        SDL_SetWindowSize(editor->window,sizes[i][0],sizes[i][1]);
        SDL_RaiseWindow(editor->window);(void)SDL_SetWindowInputFocus(editor->window);
        for(int j=0;j<3;++j) { SDL_Delay(40);SDL_PumpEvents();SceneEditorSessionRuntimeRender(editor); }
        PreviewInjection p={SDL_GetWindowID(editor->window),sizes[i][0]};
        SDL_Thread *thread=SDL_CreateThread(inject,"preview-ui-input",&p);assert(thread);
        TimelineSample sample={.absolute_frame=0,.subframe_denominator=1};
        RunPreviewModeEmbeddedAtSample(editor->window,editor->renderer,&sample);
        SDL_WaitThread(thread,NULL);
        assert(sample.absolute_frame==2 && sample.subframe_denominator==1);
        assert(SceneEditorDocumentRevision()==revision && !strcmp(path,SceneEditorDocumentPath()));
        assert(!SDL_GetRelativeMouseMode());
        assert(!(SDL_GetWindowFlags(editor->window)&(SDL_WINDOW_HIDDEN|SDL_WINDOW_MINIMIZED)));
        SceneEditorSessionRuntimeRender(editor);
    }
    fprintf(stderr,"Preview native normal/compact, interrupted-close, exact stepping, document and host restore: PASS\n");
}
