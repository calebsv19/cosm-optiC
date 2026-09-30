#ifndef SCENE_EDITOR_LIGHT_PATH_PANEL_H
#define SCENE_EDITOR_LIGHT_PATH_PANEL_H
#include "editor/scene_editor_motion_paths.h"
void SceneEditorLightPathPanelReset(void);
int SceneEditorLightPathPanelDraw(SDL_Renderer *renderer, const MotionPaths *paths,
    const MotionPath *path, SDL_Rect clip, int x, int y, int width);
bool SceneEditorLightPathPanelEvent(SDL_Event *event, const MotionPath *path,
    char *message, size_t size);
bool SceneEditorLightPathPanelControl(const char *name, SDL_Rect *out);
#endif
