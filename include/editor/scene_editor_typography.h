#ifndef SCENE_EDITOR_TYPOGRAPHY_H
#define SCENE_EDITOR_TYPOGRAPHY_H

#include "render/render_helper.h"
#include "editor/scene_editor_shared_text.h"

/* Editor density is application policy. The existing font adapter still owns
   DPI and user text scaling; other application screens retain their sizing. */
static inline void SceneEditorButtonText(SDL_Renderer* r, SDL_Rect a,
                                         const char* t, SDL_Color c) {
    (void)scene_editor_shared_label(r,a,t,c,13,0,1);
}
static inline void SceneEditorLabel(SDL_Renderer* r, SDL_Rect a,
                                    const char* t, SDL_Color c) {
    (void)scene_editor_shared_label(r,a,t,c,13,0,1);
}
static inline int SceneEditorLabelLeft(SDL_Renderer* r, SDL_Rect a,
                                       const char* t, SDL_Color c) {
    return scene_editor_shared_label(r,a,t,c,13,0,0);
}
static inline int SceneEditorLabelWrapped(SDL_Renderer* r, SDL_Rect a,
                                          const char* t, SDL_Color c) {
    return scene_editor_shared_label(r,a,t,c,13,1,0);
}
#endif
