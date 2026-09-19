#ifndef SCENE_EDITOR_OBJECT_TRANSFORM_HANDLES_H
#define SCENE_EDITOR_OBJECT_TRANSFORM_HANDLES_H
#include "editor/scene_editor_object_move_gizmo.h"
typedef struct SceneEditorObjectTransformHandle {
    double cx,cy,ux,uy,vx,vy;
    int x,y;
    double pixels_per_unit;
} SceneEditorObjectTransformHandle;
bool SceneEditorObjectTransformHandleProject(const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest, const double position[3],
    SceneEditorObjectTransformMode mode, SceneEditorBezier3DGizmoAxis axis,
    SceneEditorObjectTransformHandle* handle);
bool SceneEditorObjectTransformHandlePick(const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest, const double position[3],
    SceneEditorObjectTransformMode mode, int x,int y,
    SceneEditorBezier3DGizmoAxis* axis, SceneEditorObjectTransformHandle* handle);
bool SceneEditorObjectTransformHandleAngle(const SceneEditorObjectTransformHandle* handle,
    int x,int y,double* angle);
void SceneEditorObjectTransformHandlesRender(SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest, const double position[3],
    SceneEditorObjectTransformMode mode,SceneEditorBezier3DGizmoAxis hover,
    SceneEditorBezier3DGizmoAxis active);
#endif
