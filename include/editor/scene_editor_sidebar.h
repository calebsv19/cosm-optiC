#ifndef SCENE_EDITOR_SIDEBAR_H
#define SCENE_EDITOR_SIDEBAR_H
#include "editor/scene_editor_control_surface.h"
#include "editor/scene_editor_pane_host.h"
void SceneEditorSidebarReset(void);
bool SceneEditorSidebarDiagnosticsVisible(void);
void SceneEditorSidebarRestoreDefaults(void);
bool SceneEditorSidebarLibraryActive(void);
bool SceneEditorSidebarTextActive(void);
bool SceneEditorSidebarHandleEvent(const SDL_Event* event);
bool SceneEditorSidebarInspectorEventVisible(const SDL_Event* event);
void SceneEditorSidebarRender(SDL_Renderer* renderer, const SceneEditorPaneLayout* layout,
    const SceneEditorControlSurfaceContract* contract, SDL_Color title, SDL_Color body);
#endif
