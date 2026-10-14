#ifndef SCENE_EDITOR_SHARED_FIELDS_H
#define SCENE_EDITOR_SHARED_FIELDS_H
#include "editor/scene_editor_shared_text.h"
void scene_editor_shared_field_begin(char *,size_t,int);
void scene_editor_shared_field_end(char *);
int scene_editor_shared_field_event(char *,size_t,const SDL_Event *);
void scene_editor_shared_field_draw(SDL_Renderer *,char *,size_t,SDL_Rect,const char *);
int scene_editor_shared_field_paint(SDL_Renderer *,SDL_Rect,const char *);
void scene_editor_shared_fields_cancel(void);
void scene_editor_shared_fields_reset(void);
void scene_editor_shared_fields_frame(void);
#endif
