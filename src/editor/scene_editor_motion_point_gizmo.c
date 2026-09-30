/* Path-point adapter over the existing editor transform presentation/math.
 * The caller owns revision checks, preview lifetime and the retained command. */
#include "scene_editor_motion_point_gizmo.h"
#include "editor/scene_editor_document.h"
#include <math.h>
#include <string.h>
static bool resolve(const MotionPathPoint *point, RuntimeSceneBridge3DDigestState *digest, double position[3]) {
  if (!point || !SceneEditorDigestOverlayResolve(digest)) return false;
  double scale=SceneEditorDocumentWorldScale();
  if (!isfinite(scale) || scale<=0) return false;
  for(int k=0;k<3;++k) position[k]=point->position[k]*scale;
  return true;
}
void MotionPointGizmoDraw(SDL_Renderer *r,const SceneEditorDigestOverlayProjector *p,
    const MotionPathPoint *point,int active_axis,SDL_Rect controls[3]) {
  memset(controls,0,3*sizeof(*controls));
  RuntimeSceneBridge3DDigestState digest;double position[3];
  if(!resolve(point,&digest,position)) return;
  int x,y;SDL_GetMouseState(&x,&y);
  SceneEditorBezier3DGizmoAxis hover=SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
  SceneEditorObjectTransformHandle picked;
  SceneEditorObjectTransformHandlePick(p,&digest,position,SCENE_EDITOR_OBJECT_TRANSFORM_MOVE,x,y,&hover,&picked);
  SceneEditorObjectTransformHandlesRender(r,p,&digest,position,SCENE_EDITOR_OBJECT_TRANSFORM_MOVE,
      hover,(SceneEditorBezier3DGizmoAxis)active_axis);
  for(int k=0;k<3;++k) {
    SceneEditorObjectTransformHandle h;
    if(SceneEditorObjectTransformHandleProject(p,&digest,position,SCENE_EDITOR_OBJECT_TRANSFORM_MOVE,
        (SceneEditorBezier3DGizmoAxis)(k+1),&h)) controls[k]=(SDL_Rect){h.x-5,h.y-5,11,11};
  }
}
int MotionPointGizmoPick(const SceneEditorDigestOverlayProjector *p,
    const MotionPathPoint *point,int x,int y,SceneEditorObjectTransformHandle *handle) {
  RuntimeSceneBridge3DDigestState digest;double position[3];
  SceneEditorBezier3DGizmoAxis axis=SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
  if(resolve(point,&digest,position))
    SceneEditorObjectTransformHandlePick(p,&digest,position,SCENE_EDITOR_OBJECT_TRANSFORM_MOVE,x,y,&axis,handle);
  return (int)axis;
}
bool MotionPointGizmoMove(MotionPathPoint *point,int axis,double initial,
    const SceneEditorObjectTransformHandle *handle,int dx,int dy) {
  double scale=SceneEditorDocumentWorldScale();
  if(axis<1 || axis>3 || handle->pixels_per_unit<=0 || !isfinite(scale) || scale<=0) return false;
  double value=initial+(dx*handle->ux+dy*handle->uy)/(handle->pixels_per_unit*scale);
  if(!isfinite(value) || fabs(value)>1e12) return false;
  point->position[axis-1]=value;return true;
}
