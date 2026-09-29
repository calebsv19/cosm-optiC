#ifndef SCENE_EDITOR_MOTION_PATH_VIEWPORT_H
#define SCENE_EDITOR_MOTION_PATH_VIEWPORT_H
#include "editor/scene_editor.h"
#include "motion/scene_motion_paths.h"
bool MotionPathViewportFrame(const MotionPath *path, const SDL_Rect *viewport);
bool MotionPathViewportAppend(MotionPath *path, const double point[3]);
bool MotionPathViewportPlacement(
    const SceneEditorDigestOverlayProjector *projector, int x, int y,
    double authored_z, double out[3]);
void MotionPathViewportGuide(SDL_Renderer *renderer,
                             const SceneEditorDigestOverlayProjector *projector,
                             const MotionPath *path, double authored_z);
#endif
