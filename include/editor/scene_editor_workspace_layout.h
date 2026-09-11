#ifndef SCENE_EDITOR_WORKSPACE_LAYOUT_H
#define SCENE_EDITOR_WORKSPACE_LAYOUT_H

#include "editor/scene_editor_pane_host.h"

enum { SCENE_WORKSPACE_ACTION_COUNT = 9, SCENE_WORKSPACE_MODE_COUNT = 5 };
typedef struct SceneEditorWorkspaceChrome {
    SDL_Rect modes[SCENE_WORKSPACE_MODE_COUNT];
    SDL_Rect actions[SCENE_WORKSPACE_ACTION_COUNT];
    SDL_Rect expand;
    SDL_Rect restore;
} SceneEditorWorkspaceChrome;

/* Presentation only. No selection, scene, history or config mutation. */
void SceneEditorWorkspaceLayoutChrome(const SceneEditorPaneLayout* layout,
                                     SceneEditorWorkspaceChrome* chrome);

#endif
