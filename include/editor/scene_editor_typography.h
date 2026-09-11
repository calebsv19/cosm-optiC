#ifndef SCENE_EDITOR_TYPOGRAPHY_H
#define SCENE_EDITOR_TYPOGRAPHY_H

#include "render/render_helper.h"

/* Editor density is application policy. The existing font adapter still owns
   DPI and user text scaling; other application screens retain their sizing. */
static inline void SceneEditorButtonText(SDL_Renderer* r, SDL_Rect a,
                                         const char* t, SDL_Color c) {
    (void)RenderSizedText(r, a, t, c, 13, false, true);
}
static inline void SceneEditorLabel(SDL_Renderer* r, SDL_Rect a,
                                    const char* t, SDL_Color c) {
    (void)RenderSizedText(r, a, t, c, 13, false, true);
}
static inline int SceneEditorLabelLeft(SDL_Renderer* r, SDL_Rect a,
                                       const char* t, SDL_Color c) {
    return RenderSizedText(r, a, t, c, 13, false, false);
}
static inline int SceneEditorLabelWrapped(SDL_Renderer* r, SDL_Rect a,
                                          const char* t, SDL_Color c) {
    return RenderSizedText(r, a, t, c, 13, true, false);
}
#endif
