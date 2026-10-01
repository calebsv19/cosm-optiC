#ifndef SCENE_EDITOR_MOTION_ORIENTATION_PANEL_H
#define SCENE_EDITOR_MOTION_ORIENTATION_PANEL_H
#include "editor/scene_editor_motion_paths.h"
void MotionOrientationPanelReset(void);
void MotionOrientationPanelBeginFrame(void);
int MotionOrientationPanelDraw(SDL_Renderer *r, const MotionPaths *paths,
                               const char *target, SDL_Rect clip, int x, int y,
                               int width);
bool MotionOrientationPanelEvent(SDL_Event *event, char *message, size_t size);
void MotionOrientationHandlesDraw(
    SDL_Renderer *r, const SceneEditorDigestOverlayProjector *projector,
    SDL_Rect viewport);
bool MotionOrientationPanelControl(const char *name, SDL_Rect *out);
#endif
