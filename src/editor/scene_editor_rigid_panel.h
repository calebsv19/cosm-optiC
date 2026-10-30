#pragma once
#include <SDL.h>
#include <stdbool.h>
void SceneEditorRigidPanelReset(void);
void SceneEditorRigidPanelClearControls(void);
bool SceneEditorRigidPanelEvent(SDL_Event *event);
int SceneEditorRigidPanelDraw(SDL_Renderer *renderer,SDL_Rect pane,int y);
bool SceneEditorRigidPanelControl(const char *name,SDL_Rect *out);

/* Transient UI readback for native host acceptance; never persisted. */
bool SceneEditorRigidPanelDetailsVisible(void);
const char *SceneEditorRigidPanelFeedback(void);
bool SceneEditorRigidPanelPickerActive(void);
