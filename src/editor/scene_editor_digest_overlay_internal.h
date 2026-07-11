#ifndef SCENE_EDITOR_DIGEST_OVERLAY_INTERNAL_H
#define SCENE_EDITOR_DIGEST_OVERLAY_INTERNAL_H

#include "editor/scene_editor_digest_overlay.h"

#define SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX 18.0
#define SCENE_EDITOR_BEZIER_HANDLE_PICK_RADIUS_PX 16.0
#define SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX 18.0

void SceneEditorDigestOverlayDrawLine3(SDL_Renderer* renderer,
                                       const SceneEditorDigestOverlayProjector* projector,
                                       double ax,
                                       double ay,
                                       double az,
                                       double bx,
                                       double by,
                                       double bz,
                                       SDL_Color color);
bool SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(
    const SceneEditorDigestOverlayProjector* projector,
    double base_x,
    double base_y,
    double base_z,
    SceneEditorBezier3DGizmoAxis axis,
    double world_length,
    int* out_anchor_x,
    int* out_anchor_y,
    int* out_end_x,
    int* out_end_y,
    double* out_pixels_per_unit);

void SceneEditorDigestOverlayRenderBezierLayer(SDL_Renderer* renderer,
                                               const SceneEditorDigestOverlayProjector* projector,
                                               const RuntimeSceneBridge3DDigestState* digest,
                                               int mouse_x,
                                               int mouse_y,
                                               const SceneEditorBezier3DGizmoState* gizmo_state);
void SceneEditorDigestOverlayRenderCameraLayer(SDL_Renderer* renderer,
                                               const SceneEditorDigestOverlayProjector* projector,
                                               const RuntimeSceneBridge3DDigestState* digest,
                                               int mouse_x,
                                               int mouse_y,
                                               const SceneEditorCamera3DGizmoState* gizmo_state);
void SceneEditorDigestOverlayRenderObjectLayer(SDL_Renderer* renderer,
                                               const SceneEditorDigestOverlayProjector* projector,
                                               const RuntimeSceneBridge3DDigestState* digest,
                                               int active_mode,
                                               int selected_object_index,
                                               int hover_object_index);
void SceneEditorDigestOverlayRenderMotionLayer(SDL_Renderer* renderer,
                                               const SceneEditorDigestOverlayProjector* projector,
                                               int selected_object_index);

#endif
