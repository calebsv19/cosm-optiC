#include "editor/scene_editor_digest_overlay_internal.h"

#include <math.h>

#include "config/config_manager.h"
#include "editor/camera_editor.h"

int SceneEditorDigestOverlayPickCameraPointIndex(
    const SceneEditorDigestOverlayProjector* projector,
    int screen_x,
    int screen_y) {
    int picked = -1;
    double best_dist2 = 0.0;
    int i = 0;
    if (!projector) return -1;
    for (i = 0; i < sceneSettings.cameraPath.numPoints; ++i) {
        int px = 0;
        int py = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  sceneSettings.cameraPath.points[i].x,
                                                  sceneSettings.cameraPath.points[i].y,
                                                  sceneSettings.cameraPath3D.point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        dx = (double)screen_x - (double)px;
        dy = (double)screen_y - (double)py;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX) {
            if (picked < 0 || dist2 < best_dist2) {
                picked = i;
                best_dist2 = dist2;
            }
        }
    }
    return picked;
}

bool SceneEditorDigestOverlayPickCameraBezierHandle(
    const SceneEditorDigestOverlayProjector* projector,
    int screen_x,
    int screen_y,
    int* out_segment_index,
    int* out_handle_index) {
    int segment_index = 0;
    int picked_segment = -1;
    int picked_handle = -1;
    double best_dist2 = 0.0;
    if (!projector) return false;
    for (segment_index = 0; segment_index < sceneSettings.cameraPath.numPoints - 1; ++segment_index) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double handle_x = 0.0;
            double handle_y = 0.0;
            double handle_z = 0.0;
            double anchor_x = 0.0;
            double anchor_y = 0.0;
            double anchor_z = 0.0;
            int px = 0;
            int py = 0;
            double dx = 0.0;
            double dy = 0.0;
            double dist2 = 0.0;
            if (!CameraPath3D_GetHandleWorldPosition(&sceneSettings.cameraPath,
                                                     &sceneSettings.cameraPath3D,
                                                     segment_index,
                                                     handle_index,
                                                     &handle_x,
                                                     &handle_y,
                                                     &handle_z,
                                                     &anchor_x,
                                                     &anchor_y,
                                                     &anchor_z)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                      handle_x,
                                                      handle_y,
                                                      handle_z,
                                                      &px,
                                                      &py)) {
                continue;
            }
            dx = (double)screen_x - (double)px;
            dy = (double)screen_y - (double)py;
            dist2 = dx * dx + dy * dy;
            if (dist2 <= POINT_HIT_RADIUS * POINT_HIT_RADIUS) {
                if (picked_segment < 0 || dist2 < best_dist2) {
                    picked_segment = segment_index;
                    picked_handle = handle_index;
                    best_dist2 = dist2;
                }
            }
        }
    }
    if (picked_segment < 0) return false;
    if (out_segment_index) *out_segment_index = picked_segment;
    if (out_handle_index) *out_handle_index = picked_handle;
    return true;
}

int SceneEditorDigestOverlayPickCameraRotationHandle(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    int picked = -1;
    double best_dist2 = 0.0;
    int i = 0;
    if (!projector || !digest) return -1;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    for (i = 0; i < sceneSettings.cameraPath.numPoints; ++i) {
        double rotation = CameraEditorGetPointRotation(i);
        double pitch = CameraEditorGetPointPitch(i);
        double draw_angle = rotation - (M_PI * 0.5);
        double direction_len = metrics.gizmo_world_length * 0.95;
        double horizontal_len = cos(pitch) * direction_len;
        double end_x = sceneSettings.cameraPath.points[i].x + cos(draw_angle) * horizontal_len;
        double end_y = sceneSettings.cameraPath.points[i].y + sin(draw_angle) * horizontal_len;
        double end_z = sceneSettings.cameraPath3D.point_z[i] + sin(pitch) * direction_len;
        int px = 0;
        int py = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectPoint(projector, end_x, end_y, end_z, &px, &py)) {
            continue;
        }
        dx = (double)screen_x - (double)px;
        dy = (double)screen_y - (double)py;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= POINT_HIT_RADIUS * POINT_HIT_RADIUS) {
            if (picked < 0 || dist2 < best_dist2) {
                picked = i;
                best_dist2 = dist2;
            }
        }
    }
    return picked;
}

bool SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    double* out_x,
    double* out_y,
    double* out_z) {
    CameraEditorSelectionKind selection_kind = CAMERA_EDITOR_SELECTION_NONE;
    int point_index = -1;
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double rotation = 0.0;
    double draw_angle = 0.0;
    double direction_len = 0.0;
    if (!projector || !digest) return false;
    selection_kind = CameraEditorGetSelectionKind();
    if (selection_kind != CAMERA_EDITOR_SELECTION_ROTATION_HANDLE) {
        return CameraEditorGetSelectedGizmoWorldPosition(out_x, out_y, out_z);
    }
    point_index = CameraEditorGetSelectedPointIndex();
    if (point_index < 0 || point_index >= sceneSettings.cameraPath.numPoints) {
        return false;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    rotation = CameraEditorGetPointRotation(point_index);
    {
        double pitch = CameraEditorGetPointPitch(point_index);
        double horizontal_len = cos(pitch) * (metrics.gizmo_world_length * 0.95);
        direction_len = metrics.gizmo_world_length * 0.95;
        draw_angle = rotation - (M_PI * 0.5);
        if (out_x) {
            *out_x = sceneSettings.cameraPath.points[point_index].x + cos(draw_angle) * horizontal_len;
        }
        if (out_y) {
            *out_y = sceneSettings.cameraPath.points[point_index].y + sin(draw_angle) * horizontal_len;
        }
        if (out_z) {
            *out_z = sceneSettings.cameraPath3D.point_z[point_index] + sin(pitch) * direction_len;
        }
    }
    return true;
}

SceneEditorBezier3DGizmoAxis SceneEditorDigestOverlayPickCameraGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    SceneEditorBezier3DGizmoAxis best_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    double best_dist2 = 0.0;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!projector || !digest) return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    if (!SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(projector,
                                                                         digest,
                                                                         &base_x,
                                                                         &base_y,
                                                                         &base_z)) {
        return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int gx = 0;
        int gy = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                                  base_x,
                                                                  base_y,
                                                                  base_z,
                                                                  axis,
                                                                  metrics.gizmo_world_length,
                                                                  NULL,
                                                                  NULL,
                                                                  &gx,
                                                                  &gy,
                                                                  NULL)) {
            continue;
        }
        dx = (double)screen_x - (double)gx;
        dy = (double)screen_y - (double)gy;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_GIZMO_PICK_RADIUS_PX) {
            if (best_axis == SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE || dist2 < best_dist2) {
                best_axis = axis;
                best_dist2 = dist2;
            }
        }
    }
    return best_axis;
}

static void SceneEditorDigestOverlayDrawCameraGizmo(SDL_Renderer* renderer,
                                                    const SceneEditorDigestOverlayProjector* projector,
                                                    const RuntimeSceneBridge3DDigestState* digest,
                                                    int mouse_x,
                                                    int mouse_y,
                                                    const SceneEditorCamera3DGizmoState* gizmo_state) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    int anchor_x = 0;
    int anchor_y = 0;
    SceneEditorBezier3DGizmoAxis hover_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!renderer || !projector || !digest || !gizmo_state) return;
    if (!SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(projector,
                                                                         digest,
                                                                         &base_x,
                                                                         &base_y,
                                                                         &base_z)) return;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    hover_axis = SceneEditorDigestOverlayPickCameraGizmoAxis(projector, digest, mouse_x, mouse_y);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int end_x = 0;
        int end_y = 0;
        SDL_Color axis_color = {0, 0, 0, 255};
        SDL_Rect knob = {0, 0, 0, 0};
        int knob_half = 5;
        bool active = (gizmo_state->dragging && gizmo_state->drag_axis == axis);
        bool hovered = (hover_axis == axis);
        switch (axis) {
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X: axis_color = (SDL_Color){230, 96, 96, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y: axis_color = (SDL_Color){102, 224, 132, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z: axis_color = (SDL_Color){110, 160, 255, 255}; break;
            default: break;
        }
        if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                                  base_x,
                                                                  base_y,
                                                                  base_z,
                                                                  axis,
                                                                  metrics.gizmo_world_length,
                                                                  &anchor_x,
                                                                  &anchor_y,
                                                                  &end_x,
                                                                  &end_y,
                                                                  NULL)) {
            continue;
        }
        if (active || hovered) {
            axis_color.a = 255;
        } else {
            axis_color.a = 220;
        }
        if (active) {
            knob_half = 7;
        } else if (hovered) {
            knob_half = 6;
        }
        SDL_SetRenderDrawColor(renderer, axis_color.r, axis_color.g, axis_color.b, axis_color.a);
        SDL_RenderDrawLine(renderer, anchor_x, anchor_y, end_x, end_y);
        knob = (SDL_Rect){end_x - knob_half, end_y - knob_half, knob_half * 2, knob_half * 2};
        SDL_RenderFillRect(renderer, &knob);
        SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
        SDL_RenderDrawRect(renderer, &knob);
    }
}

static void SceneEditorDigestOverlayDrawCamera3D(SDL_Renderer* renderer,
                                                 const SceneEditorDigestOverlayProjector* projector,
                                                 const RuntimeSceneBridge3DDigestState* digest,
                                                 int mouse_x,
                                                 int mouse_y,
                                                 const SceneEditorCamera3DGizmoState* gizmo_state) {
    SDL_Color curve_color = {210, 168, 255, 230};
    SDL_Color point_color = {212, 238, 255, 255};
    SDL_Color selected_color = {255, 170, 82, 255};
    SDL_Color hover_point_color = {255, 240, 176, 255};
    SDL_Color handle_color = {255, 186, 104, 200};
    SDL_Color direction_color = {200, 170, 255, 220};
    SDL_Color start_color = {0, 200, 0, 235};
    SDL_Color end_color = {220, 40, 40, 235};
    PathTraversalEndpoints endpoints = {0};
    bool have_endpoints = false;
    int selected_point = CameraEditorGetSelectedPointIndex();
    int hover_point = -1;
    int i = 0;
    if (!renderer || !projector || !digest || !gizmo_state) return;
    have_endpoints = PathResolveTraversalEndpoints(&sceneSettings.cameraPath,
                                                   &sceneSettings.cameraPath3D,
                                                   &endpoints);
    hover_point = SceneEditorDigestOverlayPickCameraPointIndex(projector, mouse_x, mouse_y);
    for (i = 0; i < sceneSettings.cameraPath.numPoints - 1; ++i) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double anchor_x = 0.0;
            double anchor_y = 0.0;
            double anchor_z = 0.0;
            double end_x = 0.0;
            double end_y = 0.0;
            double end_z = 0.0;
            int anchor_px = 0;
            int anchor_py = 0;
            int end_px = 0;
            int end_py = 0;
            int knob_half = 4;
            SDL_Rect knob = {0, 0, 0, 0};
            bool related_selected = (selected_point == i) || (selected_point == i + 1);
            SDL_Color draw_color = handle_color;
            if (!CameraPath3D_GetHandleWorldPosition(&sceneSettings.cameraPath,
                                                     &sceneSettings.cameraPath3D,
                                                     i,
                                                     handle_index,
                                                     &end_x,
                                                     &end_y,
                                                     &end_z,
                                                     &anchor_x,
                                                     &anchor_y,
                                                     &anchor_z)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                      anchor_x,
                                                      anchor_y,
                                                      anchor_z,
                                                      &anchor_px,
                                                      &anchor_py) ||
                !SceneEditorDigestOverlayProjectPoint(projector,
                                                      end_x,
                                                      end_y,
                                                      end_z,
                                                      &end_px,
                                                      &end_py)) {
                continue;
            }
            if (related_selected) {
                draw_color.a = 255;
                knob_half = 5;
            }
            SDL_SetRenderDrawColor(renderer, draw_color.r, draw_color.g, draw_color.b, draw_color.a);
            SDL_RenderDrawLine(renderer, anchor_px, anchor_py, end_px, end_py);
            knob = (SDL_Rect){end_px - knob_half, end_py - knob_half, knob_half * 2, knob_half * 2};
            SDL_RenderFillRect(renderer, &knob);
            SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
            SDL_RenderDrawRect(renderer, &knob);
        }
    }
    if (sceneSettings.cameraPath.numPoints >= 2) {
        Point previous_xy = GetPositionAlongPathNormalized(&sceneSettings.cameraPath, 0.0);
        double previous_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.cameraPath,
                                                                &sceneSettings.cameraPath3D,
                                                                0.0);
        for (i = 1; i <= 48; ++i) {
            double t = (double)i / 48.0;
            Point current_xy = GetPositionAlongPathNormalized(&sceneSettings.cameraPath, t);
            double current_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.cameraPath,
                                                                   &sceneSettings.cameraPath3D,
                                                                   t);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              projector,
                                              previous_xy.x,
                                              previous_xy.y,
                                              previous_z,
                                              current_xy.x,
                                              current_xy.y,
                                              current_z,
                                              curve_color);
            previous_xy = current_xy;
            previous_z = current_z;
        }
    }
    for (i = 0; i < sceneSettings.cameraPath.numPoints; ++i) {
        int px = 0;
        int py = 0;
        bool point_selected = (i == selected_point);
        bool point_hovered = (i == hover_point);
        int radius = point_selected ? 6 : (point_hovered ? 5 : 4);
        SDL_Rect marker = {0, 0, 0, 0};
        SDL_Color color = point_selected ? selected_color : (point_hovered ? hover_point_color : point_color);
        if (!point_selected && !point_hovered) {
            if (have_endpoints && i == endpoints.start_point_index) {
                color = start_color;
            } else if (have_endpoints && i == endpoints.end_point_index) {
                color = end_color;
            }
        }
        double rotation = CameraEditorGetPointRotation(i);
        double pitch = CameraEditorGetPointPitch(i);
        double draw_angle = rotation - (M_PI * 0.5);
        double direction_len = 0.0;
        double horizontal_len = 0.0;
        double dir_end_x = 0.0;
        double dir_end_y = 0.0;
        double dir_end_z = 0.0;
        int dir_end_px = 0;
        int dir_end_py = 0;
        SDL_Color dir_color = direction_color;
        SDL_Rect dir_knob = {0, 0, 0, 0};
        int dir_knob_half = point_selected ? 5 : 4;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  sceneSettings.cameraPath.points[i].x,
                                                  sceneSettings.cameraPath.points[i].y,
                                                  sceneSettings.cameraPath3D.point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        marker = (SDL_Rect){px - radius, py - radius, radius * 2, radius * 2};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &marker);
        SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
        SDL_RenderDrawRect(renderer, &marker);

        direction_len = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector).gizmo_world_length * 0.95;
        horizontal_len = cos(pitch) * direction_len;
        dir_end_x = sceneSettings.cameraPath.points[i].x + cos(draw_angle) * horizontal_len;
        dir_end_y = sceneSettings.cameraPath.points[i].y + sin(draw_angle) * horizontal_len;
        dir_end_z = sceneSettings.cameraPath3D.point_z[i] + sin(pitch) * direction_len;
        if (SceneEditorDigestOverlayProjectPoint(projector,
                                                 dir_end_x,
                                                 dir_end_y,
                                                 dir_end_z,
                                                 &dir_end_px,
                                                 &dir_end_py)) {
            if (point_selected) {
                dir_color.a = 255;
            }
            SDL_SetRenderDrawColor(renderer, dir_color.r, dir_color.g, dir_color.b, dir_color.a);
            SDL_RenderDrawLine(renderer, px, py, dir_end_px, dir_end_py);
            dir_knob = (SDL_Rect){dir_end_px - dir_knob_half,
                                  dir_end_py - dir_knob_half,
                                  dir_knob_half * 2,
                                  dir_knob_half * 2};
            SDL_RenderFillRect(renderer, &dir_knob);
            SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
            SDL_RenderDrawRect(renderer, &dir_knob);
        }
    }
    if (selected_point >= 0 && selected_point < sceneSettings.cameraPath.numPoints) {
        SceneEditorDigestOverlayDrawCameraGizmo(renderer, projector, digest, mouse_x, mouse_y, gizmo_state);
    }
}

bool SceneEditorDigestOverlayApplyCameraGizmoDrag(const SceneEditorDigestOverlayProjector* projector,
                                                  const RuntimeSceneBridge3DDigestState* digest,
                                                  const SceneEditorCamera3DGizmoState* gizmo_state,
                                                  int mouse_x,
                                                  int mouse_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    int axis_anchor_x = 0;
    int axis_anchor_y = 0;
    int axis_end_x = 0;
    int axis_end_y = 0;
    double pixels_per_unit = 0.0;
    double axis_dx = 0.0;
    double axis_dy = 0.0;
    double axis_len = 0.0;
    double mouse_dx = 0.0;
    double mouse_dy = 0.0;
    double projected_px = 0.0;
    double world_delta = 0.0;
    if (!projector || !digest || !gizmo_state || !gizmo_state->dragging) return false;
    if (!SceneEditorDigestOverlayResolveSelectedCameraGizmoWorldPosition(projector,
                                                                         digest,
                                                                         &base_x,
                                                                         &base_y,
                                                                         &base_z)) return false;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    if (!SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                              gizmo_state->drag_start_world_x,
                                                              gizmo_state->drag_start_world_y,
                                                              gizmo_state->drag_start_world_z,
                                                              gizmo_state->drag_axis,
                                                              metrics.gizmo_world_length,
                                                              &axis_anchor_x,
                                                              &axis_anchor_y,
                                                              &axis_end_x,
                                                              &axis_end_y,
                                                              &pixels_per_unit)) {
        return false;
    }
    axis_dx = (double)axis_end_x - (double)axis_anchor_x;
    axis_dy = (double)axis_end_y - (double)axis_anchor_y;
    axis_len = sqrt(axis_dx * axis_dx + axis_dy * axis_dy);
    if (axis_len <= 1e-6 || pixels_per_unit <= 1e-6) return false;
    mouse_dx = (double)mouse_x - (double)gizmo_state->drag_start_mouse_x;
    mouse_dy = (double)mouse_y - (double)gizmo_state->drag_start_mouse_y;
    projected_px = mouse_dx * (axis_dx / axis_len) + mouse_dy * (axis_dy / axis_len);
    world_delta = projected_px / pixels_per_unit;
    if (!gizmo_state->smooth_drag) {
        world_delta = SceneEditorDigestOverlayQuantizeWorldValue(world_delta, metrics.snap_step);
    }
    switch (gizmo_state->drag_axis) {
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X:
            return CameraEditorMoveSelectedGizmoTo(gizmo_state->drag_start_world_x + world_delta,
                                                   gizmo_state->drag_start_world_y,
                                                   gizmo_state->drag_start_world_z);
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y:
            return CameraEditorMoveSelectedGizmoTo(gizmo_state->drag_start_world_x,
                                                   gizmo_state->drag_start_world_y + world_delta,
                                                   gizmo_state->drag_start_world_z);
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z:
            return CameraEditorMoveSelectedGizmoTo(gizmo_state->drag_start_world_x,
                                                   gizmo_state->drag_start_world_y,
                                                   gizmo_state->drag_start_world_z + world_delta);
        default:
            return false;
    }
}

void SceneEditorDigestOverlayRenderCameraLayer(SDL_Renderer* renderer,
                                               const SceneEditorDigestOverlayProjector* projector,
                                               const RuntimeSceneBridge3DDigestState* digest,
                                               int mouse_x,
                                               int mouse_y,
                                               const SceneEditorCamera3DGizmoState* gizmo_state) {
    SceneEditorDigestOverlayDrawCamera3D(renderer,
                                         projector,
                                         digest,
                                         mouse_x,
                                         mouse_y,
                                         gizmo_state);
}
