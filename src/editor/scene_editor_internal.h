#ifndef SCENE_EDITOR_INTERNAL_H
#define SCENE_EDITOR_INTERNAL_H

#include "editor/scene_editor.h"
#include "editor/scene_editor_input_router.h"

void SceneEditorSyncWindowSize(SceneEditor* editor);
void SceneEditorRefreshPaneSplitterHover(SceneEditor* editor);
bool SceneEditorHandlePaneSplitterEvent(SceneEditor* editor, SDL_Event* event);
void RenderSceneButtons(SDL_Renderer* renderer);
void RenderSceneDigestOverlay(SDL_Renderer* renderer);
SceneEditorInputRouterCallbacks SceneEditorBuildInputRouterCallbacks(SceneEditor* editor);

#endif
