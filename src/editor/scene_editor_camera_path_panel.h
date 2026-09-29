#ifndef SCENE_EDITOR_CAMERA_PATH_PANEL_H
#define SCENE_EDITOR_CAMERA_PATH_PANEL_H
#include "editor/scene_editor_motion_paths.h"
void SceneEditorCameraPathPanelReset(void);
int SceneEditorCameraPathPanelDraw(SDL_Renderer *renderer, const MotionPaths *paths,
    const MotionPath *path, SDL_Rect clip, int x, int y, int width);
bool SceneEditorCameraPathPanelEvent(SDL_Event *event, const MotionPath *path,
    char *message, size_t size);
bool SceneEditorCameraPathPanelControl(const char *name, SDL_Rect *out);
#endif
