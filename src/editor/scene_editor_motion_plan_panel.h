#ifndef SCENE_EDITOR_MOTION_PLAN_PANEL_H
#define SCENE_EDITOR_MOTION_PLAN_PANEL_H
#include "editor/scene_editor_motion_paths.h"
void SceneEditorMotionPlanPanelReset(void);
bool SceneEditorMotionPlanPanelOpen(void);
bool SceneEditorMotionPlanPanelOpenTarget(const char *path, const char *target);
const char *SceneEditorMotionPlanPanelTarget(void);
int SceneEditorMotionPlanPanelDraw(SDL_Renderer *, const MotionPaths *,
                                   const MotionPath *, SDL_Rect, int, int, int);
bool SceneEditorMotionPlanPanelEvent(SDL_Event *);
bool SceneEditorMotionPlanPanelControl(const char *, SDL_Rect *);
#endif
