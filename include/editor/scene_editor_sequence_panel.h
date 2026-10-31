#ifndef SCENE_EDITOR_SEQUENCE_PANEL_H
#define SCENE_EDITOR_SEQUENCE_PANEL_H
#include <SDL.h>
#include <stdbool.h>
int SceneEditorSequencePanelDraw(SDL_Renderer *r, SDL_Rect pane, int y);
bool SceneEditorSequencePanelEvent(SDL_Event *event);
bool SceneEditorSequencePanelActive(void);
void SceneEditorSequencePanelReset(void);
bool SceneEditorSequencePanelControl(const char *name, SDL_Rect *out);
#endif
