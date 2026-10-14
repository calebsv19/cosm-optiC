#ifndef SCENE_EDITOR_SHARED_TEXT_H
#define SCENE_EDITOR_SHARED_TEXT_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "kit_ui_text_edit_sdl.h"
#include "kit_ui_text_presentation.h"
typedef struct SceneEditorSharedText {
    KitUiTextEdit edit;
    KitUiTextPresentation view;
    SDL_Renderer *renderer;
    TTF_Font *font;
    SDL_Rect bounds;
    int ready, dragging;
} SceneEditorSharedText;
void scene_editor_shared_text_bind(SceneEditorSharedText *,char *,size_t,unsigned);
KitUiTextEventResult scene_editor_shared_text_event(SceneEditorSharedText *,const SDL_Event *);
void scene_editor_shared_text_draw(SceneEditorSharedText *,SDL_Renderer *,SDL_Rect,SDL_Color);
int scene_editor_shared_label(SDL_Renderer *,SDL_Rect,const char *,SDL_Color,int,int,int);
void scene_editor_shared_label_at(SDL_Renderer *,const char *,int,int,SDL_Color,int);
#endif
