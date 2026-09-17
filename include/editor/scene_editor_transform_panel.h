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
bool SceneEditorTransformPanelHistory(bool redo);
bool SceneEditorTransformPanelHandleEvent(SceneEditor* editor, const SDL_Event* event);
void SceneEditorTransformPanelOpenImport(void);
/* Release a draft before pane hit filtering so other panes remain reachable. */
void SceneEditorTransformPanelReleaseFocusForEvent(const SDL_Event* event);
bool SceneEditorTransformPanelInteractionActive(void);
bool SceneEditorTransformPanelPoll(void);
void SceneEditorTransformPanelReset(void);

#endif
