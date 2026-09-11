#ifndef SCENE_EDITOR_WORKSPACE_PROFILE_H
#define SCENE_EDITOR_WORKSPACE_PROFILE_H
#include "editor/scene_editor.h"
typedef enum SceneEditorWorkspaceProfile { SCENE_WORKSPACE_SCENE, SCENE_WORKSPACE_MATERIALS,
    SCENE_WORKSPACE_SURFACE, SCENE_WORKSPACE_ENVIRONMENT, SCENE_WORKSPACE_RENDER,
    SCENE_WORKSPACE_PROFILE_COUNT } SceneEditorWorkspaceProfile;
SceneEditorWorkspaceProfile SceneEditorWorkspaceProfileGet(void);
void SceneEditorWorkspaceProfileSelect(SceneEditor* editor, SceneEditorWorkspaceProfile profile);
const char* SceneEditorWorkspaceProfileLabel(int profile);
bool SceneEditorWorkspaceProfileHandleEvent(SceneEditor* editor, const SDL_Event* event);
void SceneEditorWorkspaceProfileReset(void);
void SceneEditorWorkspaceProfileSyncMode(int mode);
#endif
