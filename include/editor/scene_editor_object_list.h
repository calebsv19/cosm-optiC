#ifndef SCENE_EDITOR_OBJECT_LIST_H
#define SCENE_EDITOR_OBJECT_LIST_H

#include <SDL2/SDL.h>
#include <stdbool.h>

int SceneEditorObjectListRender(SDL_Renderer* renderer,
                                SDL_Rect bounds,
                                int cursor_y,
                                int bottom_y,
                                int selected_index,
                                SDL_Color title_color,
                                SDL_Color body_color);
bool SceneEditorObjectListContainsPoint(int x, int y);
bool SceneEditorObjectListHandleWheel(int x, int y, float wheel_delta_y);
float SceneEditorObjectListScrollOffset(void);
void SceneEditorObjectListReset(void);
void SceneEditorObjectListSetFilter(const char* text);

#endif
