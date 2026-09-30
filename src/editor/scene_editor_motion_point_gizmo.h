#ifndef SCENE_EDITOR_MOTION_POINT_GIZMO_H
#define SCENE_EDITOR_MOTION_POINT_GIZMO_H
#include "editor/scene_editor_object_transform_handles.h"
#include "motion/scene_motion_paths.h"
void MotionPointGizmoDraw(SDL_Renderer *, const SceneEditorDigestOverlayProjector *,
    const MotionPathPoint *, int active_axis, SDL_Rect controls[3]);
int MotionPointGizmoPick(const SceneEditorDigestOverlayProjector *,
    const MotionPathPoint *, int x, int y, SceneEditorObjectTransformHandle *);
bool MotionPointGizmoMove(MotionPathPoint *, int axis, double initial,
    const SceneEditorObjectTransformHandle *, int dx, int dy);
#endif
