#ifndef SCENE_EDITOR_LIFECYCLE_H
#define SCENE_EDITOR_LIFECYCLE_H
#include "editor/scene_editor.h"
void SceneEditorLifecycleReset(void);
void SceneEditorLifecycleRequestClose(SceneEditor* editor, const char* reason);
bool SceneEditorLifecycleHandleEvent(SceneEditor* editor, SDL_Event* event);
void SceneEditorLifecycleRender(SDL_Renderer* renderer);
bool SceneEditorLifecycleClosePending(void);
#endif
