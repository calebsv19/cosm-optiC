#ifndef SCENE_EDITOR_TRANSFORM_PANEL_H
#define SCENE_EDITOR_TRANSFORM_PANEL_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "editor/scene_editor.h"

int SceneEditorTransformPanelRender(SDL_Renderer* renderer,
                                    SDL_Rect bounds,
                                    int top_y,
                                    int bottom_y);
bool SceneEditorTransformPanelImportSTL(const char* path);
bool SceneEditorTransformPanelStageImportSTL(const char* path);
bool SceneEditorTransformPanelImportApplyControl(SDL_Rect* out);
bool SceneEditorTransformPanelDeleteControl(SDL_Rect* out, bool* confirmation_pending);
bool SceneEditorTransformPanelImportControl(const char* name, SDL_Rect* out);
bool SceneEditorTransformPanelImportHandleEvent(const SDL_Event* event);
void SceneEditorTransformPanelRenderImportOverlay(SDL_Renderer* renderer, SDL_Rect viewport);
bool SceneEditorTransformPanelHistory(bool redo);
bool SceneEditorTransformPanelHandleEvent(SceneEditor* editor, const SDL_Event* event);
void SceneEditorTransformPanelOpenImport(void);
/* Release a draft before pane hit filtering so other panes remain reachable. */
void SceneEditorTransformPanelReleaseFocusForEvent(const SDL_Event* event);
bool SceneEditorTransformPanelInteractionActive(void);
bool SceneEditorTransformPanelPoll(void);
/* Called after a document revision rebuilds the recovered mesh preview. */
void SceneEditorTransformPanelFrameReadyImport(void);
void SceneEditorTransformPanelReset(void);
/* Read currently rendered material hit targets for native interaction clients. */
bool SceneEditorTransformPanelMaterialControl(const char* name,SDL_Rect* out);

#endif
