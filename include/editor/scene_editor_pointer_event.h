#ifndef SCENE_EDITOR_POINTER_EVENT_H
#define SCENE_EDITOR_POINTER_EVENT_H
#include <SDL2/SDL.h>

/* Queued wheel events retain their originating position. Every routing layer
   must use the same position as the zoom operation, even if the cursor moved. */
static inline void SceneEditorWheelPosition(const SDL_Event* event, int* x, int* y) {
#if SDL_VERSION_ATLEAST(2, 26, 0)
    *x = event->wheel.mouseX;
    *y = event->wheel.mouseY;
#else
    (void)event;
    SDL_GetMouseState(x, y);
#endif
}
#endif
