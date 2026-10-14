#ifndef RAY_TRACING_PREVIEW_SHARED_CONTROLS_H
#define RAY_TRACING_PREVIEW_SHARED_CONTROLS_H
#include <SDL2/SDL.h>
#include "app/preview_workspace.h"
#include "kit_ui_surface.h"
typedef enum PreviewSharedAction {
    PREVIEW_SHARED_NONE, PREVIEW_SHARED_PLAY, PREVIEW_SHARED_LOOP,
    PREVIEW_SHARED_BOUNCE, PREVIEW_SHARED_CLOSE
} PreviewSharedAction;
typedef struct PreviewSharedControls {
    KitUiSurface surface;
    bool ready, keyboard_focus, blocked_release;
} PreviewSharedControls;
SDL_Rect PreviewSharedCloseRect(int width);
void PreviewSharedControlsCancel(PreviewSharedControls *controls);
void PreviewSharedControlsPublish(PreviewSharedControls *controls,
                                 const PreviewWorkspace *workspace);
bool PreviewSharedControlsRoute(PreviewSharedControls *controls,
                                const SDL_Event *event, PreviewSharedAction *action);
void PreviewSharedButtonRender(SDL_Renderer *renderer,
                              const PreviewSharedControls *controls,
                              PreviewSharedAction action, SDL_Rect bounds,
                              const char *label, bool selected);
#endif
