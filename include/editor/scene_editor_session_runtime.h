#ifndef SCENE_EDITOR_SESSION_RUNTIME_H
#define SCENE_EDITOR_SESSION_RUNTIME_H

#include "editor/scene_editor.h"

void SceneEditorSessionRuntimeHandleEvent(SceneEditor* editor, SDL_Event* event);
void SceneEditorSessionRuntimeRender(SceneEditor* editor);
void SceneEditorSessionRuntimeRenderWithPostDraw(SceneEditor* editor,
                                                 SceneEditorSessionPostDrawFn post_draw,
                                                 void* context);
void SceneEditorSessionRuntimeLoop(SceneEditor* editor);

#endif
