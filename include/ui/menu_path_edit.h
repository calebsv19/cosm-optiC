#ifndef MENU_PATH_EDIT_H
#define MENU_PATH_EDIT_H
#include "ui/sdl_menu_state.h"
void menu_path_edit_begin(MenuRuntimeState* state);
void menu_path_edit_end(MenuRuntimeState* state);
bool menu_path_edit_key(MenuRuntimeState* state,const SDL_Event* event);
bool menu_path_edit_insert(MenuRuntimeState* state,const char* text);
bool menu_path_edit_apply_output(MenuRuntimeState* state,const char* path,bool frames);
#endif
