#ifndef SCENE_EDITOR_CAMERA_INSPECTOR_H
#define SCENE_EDITOR_CAMERA_INSPECTOR_H
#include "editor/scene_editor_pane_host.h"
bool SceneEditorCameraInspectorEvent(SDL_Event* event,const SceneEditorPaneLayout* layout);
void SceneEditorCameraInspectorRender(SDL_Renderer* renderer,const SceneEditorPaneLayout* layout);
void SceneEditorCameraInspectorReset(void);
#endif
