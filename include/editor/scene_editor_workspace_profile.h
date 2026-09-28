#ifndef SCENE_EDITOR_WORKSPACE_PROFILE_H
#define SCENE_EDITOR_WORKSPACE_PROFILE_H
#include "editor/scene_editor.h"
typedef enum SceneEditorWorkspaceProfile { SCENE_WORKSPACE_SCENE, SCENE_WORKSPACE_MATERIALS,
    SCENE_WORKSPACE_SURFACE, SCENE_WORKSPACE_ENVIRONMENT, SCENE_WORKSPACE_RENDER,
    SCENE_WORKSPACE_PROFILE_COUNT } SceneEditorWorkspaceProfile;
SceneEditorWorkspaceProfile SceneEditorWorkspaceProfileGet(void);
void SceneEditorWorkspaceProfileSelect(SceneEditor* editor, SceneEditorWorkspaceProfile profile);
void SceneEditorWorkspaceProfileBegin(SceneEditor* editor);
void SceneEditorWorkspaceProfileCycle(SceneEditor* editor, bool reverse);
void SceneEditorWorkspaceProfileSelectMode(SceneEditor* editor, int mode);
void SceneEditorWorkspaceProfileLight(SceneEditor* editor, bool timing);
const char* SceneEditorWorkspaceProfileLabel(int profile);
bool SceneEditorWorkspaceProfileHandleEvent(SceneEditor* editor, const SDL_Event* event);
void SceneEditorWorkspaceProfileRenderOverlay(SDL_Renderer* renderer);
bool SceneEditorWorkspaceProfileMenuOpen(void);
void SceneEditorWorkspaceProfileReset(void);
void SceneEditorWorkspaceProfileSyncMode(int mode);
#endif
