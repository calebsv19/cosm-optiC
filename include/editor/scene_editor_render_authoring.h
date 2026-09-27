#ifndef SCENE_EDITOR_RENDER_AUTHORING_H
#define SCENE_EDITOR_RENDER_AUTHORING_H
#include "editor/scene_editor.h"
bool SceneEditorRenderAuthoringEvent(SceneEditor* editor, SDL_Event* event);
void SceneEditorRenderAuthoringDraw(SceneEditor* editor,const SceneEditorPaneLayout* layout);
void SceneEditorRenderAuthoringSelect(SceneEditor* editor,bool camera);
bool SceneEditorRenderAuthoringTiming(void);
void SceneEditorRenderAuthoringReset(void);
void SceneEditorRenderButton(SDL_Renderer* renderer,SDL_Rect rect,const char* label,bool selected,bool enabled);
bool SceneEditorRenderAuthoringControl(const char* name,SDL_Rect* out);
void SceneEditorRenderAuthoringSetTiming(bool enabled);
#endif
