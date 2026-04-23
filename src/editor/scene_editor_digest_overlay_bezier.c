#include "editor/scene_editor_digest_overlay_internal.h"

#include <math.h>

#include "config/config_manager.h"
#include "editor/bezier_editor.h"

int SceneEditorDigestOverlayPickBezierPointIndex(const SceneEditorDigestOverlayProjector* projector,
                                                 const Path* path,
                                                 const CameraPath3D* path3d,
                                                 double plane_z,
                                                 int screen_x,
                                                 int screen_y) {
    int pick_index = -1;
    double best_dist2 = 0.0;
    int i = 0;
    if (!projector || !path) return -1;
    for (i = 0; i < path->numPoints; ++i) {
        int px = 0;
        int py = 0;
        double dx = 0.0;
        double dy = 0.0;
        double dist2 = 0.0;
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  path->points[i].x,
                                                  path->points[i].y,
                                                  path3d ? path3d->point_z[i] : plane_z,
                                                  &px,
                                                  &py)) {
            continue;
        }
        dx = (double)screen_x - (double)px;
        dy = (double)screen_y - (double)py;
        dist2 = dx * dx + dy * dy;
        if (dist2 <= SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_POINT_PICK_RADIUS_PX) {
            if (pick_index < 0 || dist2 < best_dist2) {
                pick_index = i;
                best_dist2 = dist2;
            }
        }
    }
    return pick_index;
}

static bool SceneEditorDigestOverlayBezierHandleWorldPosition(const Path* path,
                                                              const CameraPath3D* path3d,
                                                              int segment_index,
                                                              int handle_index,
                                                              double* out_x,
                                                              double* out_y,
                                                              double* out_z,
                                                              double* out_anchor_x,
                                                              double* out_anchor_y,
                                                              double* out_anchor_z,
                                                              double plane_z) {
    int point_index = 0;
    (void)plane_z;
    if (!path || !path3d) return false;
    if (segment_index < 0 || segment_index >= path->numPoints - 1) return false;
    if (!(handle_index == 0 || handle_index == 1)) return false;
    point_index = (handle_index == 0) ? segment_index : (segment_index + 1);
    if (out_anchor_x) *out_anchor_x = path->points[point_index].x;
    if (out_anchor_y) *out_anchor_y = path->points[point_index].y;
    if (out_anchor_z) *out_anchor_z = path3d->point_z[point_index];
    if (out_x) *out_x = path->points[point_index].x + path->handles[segment_index][handle_index].vx;
    if (out_y) *out_y = path->points[point_index].y + path->handles[segment_index][handle_index].vy;
    if (out_z) *out_z = path3d->point_z[point_index] + path3d->handles_vz[segment_index][handle_index];
    return true;
}

static bool SceneEditorDigestOverlayGetBezierSelectionWorldPosition(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    double* out_x,
    double* out_y,
    double* out_z) {
    double world_x = 0.0;
    double world_y = 0.0;
    double world_z = 0.0;
    if (!projector || !digest) return false;
    if (!BezierEditorGetSelectionWorldPosition3D(&world_x, &world_y, &world_z)) return false;
    if (out_x) *out_x = world_x;
    if (out_y) *out_y = world_y;
    if (out_z) *out_z = world_z;
    return true;
}

int SceneEditorDigestOverlayPickBezierHandle(const SceneEditorDigestOverlayProjector* projector,
                                             const Path* path,
                                             const CameraPath3D* path3d,
                                             double plane_z,
                                             int screen_x,
                                             int screen_y,
                                             int* out_segment_index,
                                             int* out_handle_index) {
    int picked_segment = -1;
    int picked_handle = -1;
    double best_dist2 = 0.0;
    int segment = 0;
    if (!projector || !path || !path3d) return -1;
    for (segment = 0; segment < path->numPoints - 1; ++segment) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double hx = 0.0;
            double hy = 0.0;
            double hz = 0.0;
            int px = 0;
            int py = 0;
            double dx = 0.0;
            double dy = 0.0;
            double dist2 = 0.0;
            if (!SceneEditorDigestOverlayBezierHandleWorldPosition(path,
                                                                   path3d,
                                                                   segment,
                                                                   handle_index,
                                                                   &hx,
                                                                   &hy,
                                                                   &hz,
                                                                   NULL,
                                                                   NULL,
                                                                   NULL,
                                                                   plane_z)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector, hx, hy, hz, &px, &py)) {
                continue;
            }
            dx = (double)screen_x - (double)px;
            dy = (double)screen_y - (double)py;
            dist2 = dx * dx + dy * dy;
            if (dist2 <= SCENE_EDITOR_BEZIER_HANDLE_PICK_RADIUS_PX * SCENE_EDITOR_BEZIER_HANDLE_PICK_RADIUS_PX) {
                if (picked_segment < 0 || dist2 < best_dist2) {
                    picked_segment = segment;
                    picked_handle = handle_index;
                    best_dist2 = dist2;
                }
            }
        }
    }
    if (picked_segment >= 0) {
        if (out_segment_index) *out_segment_index = picked_segment;
        if (out_handle_index) *out_handle_index = picked_handle;
    }
    return picked_segment;
}

static bool SceneEditorDigestOverlayProjectBezierGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    SceneEditorBezier3DGizmoAxis axis,
    double world_length,
    int* out_anchor_x,
    int* out_anchor_y,
    int* out_end_x,
    int* out_end_y,
    double* out_pixels_per_unit) {
    double base_x = 0.0;
    double base_y = 0.0;
    double base_z = 0.0;
    if (!projector) return false;
    if (!SceneEditorDigestOverlayGetBezierSelectionWorldPosition(projector,
                                                                 digest,
                                                                 &base_x,
                                                                 &base_y,
                                                                 &base_z)) {
        return false;
    }
    return SceneEditorDigestOverlayProjectGizmoAxisAtWorldPoint(projector,
                                                                base_x,
                                                                base_y,
                                                                base_z,
                                                                axis,
                                                                world_length,
                                                                out_anchor_x,
                                                                out_anchor_y,
                                                                out_end_x,
                                                                out_end_y,
                                                                out_pixels_per_unit);
}

SceneEditorBezier3DGizmoAxis SceneEditorDigestOverlayPickBezierGizmoAxis(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridge3DDigestState* digest,
    int screen_x,
    int screen_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    SceneEditorBezier3DGizmoAxis best_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    double best_dist2 = 0.0;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!projector || !digest) return SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    if (BezierEditorGetSelectionKind() == BEZIER_EDITOR_SELECTION_NONE) {
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
        if (SceneEditorDigestOverlayBezierGizmoAxisLocked(axis)) {
            continue;
        }
        if (!SceneEditorDigestOverlayProjectBezierGizmoAxis(projector,
                                                            digest,
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

bool SceneEditorDigestOverlayBezierGizmoAxisLocked(SceneEditorBezier3DGizmoAxis axis) {
    (void)axis;
    return false;
}

static void SceneEditorDigestOverlayDrawBezierGizmo(SDL_Renderer* renderer,
                                                    const SceneEditorDigestOverlayProjector* projector,
                                                    const RuntimeSceneBridge3DDigestState* digest,
                                                    int mouse_x,
                                                    int mouse_y,
                                                    const SceneEditorBezier3DGizmoState* gizmo_state) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
    int anchor_x = 0;
    int anchor_y = 0;
    SceneEditorBezier3DGizmoAxis hover_axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_NONE;
    SceneEditorBezier3DGizmoAxis axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
    if (!renderer || !projector || !digest || !gizmo_state) return;
    if (!SceneEditorDigestOverlayGetBezierSelectionWorldPosition(projector,
                                                                 digest,
                                                                 NULL,
                                                                 NULL,
                                                                 NULL)) {
        return;
    }
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    hover_axis = SceneEditorDigestOverlayPickBezierGizmoAxis(projector, digest, mouse_x, mouse_y);
    for (axis = SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X;
         axis <= SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z;
         axis = (SceneEditorBezier3DGizmoAxis)(axis + 1)) {
        int end_x = 0;
        int end_y = 0;
        SDL_Color axis_color = {0, 0, 0, 255};
        SDL_Rect knob = {0, 0, 0, 0};
        int knob_half = 5;
        bool locked = SceneEditorDigestOverlayBezierGizmoAxisLocked(axis);
        bool active = (gizmo_state->dragging && gizmo_state->drag_axis == axis);
        bool hovered = (hover_axis == axis);
        switch (axis) {
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_X: axis_color = (SDL_Color){230, 96, 96, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y: axis_color = (SDL_Color){102, 224, 132, 255}; break;
            case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z: axis_color = (SDL_Color){110, 160, 255, 255}; break;
            default: break;
        }
        if (!SceneEditorDigestOverlayProjectBezierGizmoAxis(projector,
                                                            digest,
                                                            axis,
                                                            metrics.gizmo_world_length,
                                                            &anchor_x,
                                                            &anchor_y,
                                                            &end_x,
                                                            &end_y,
                                                            NULL)) {
            continue;
        }
        if (locked) {
            axis_color.a = 110;
        } else if (active || hovered) {
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
        if (locked) {
            SDL_RenderDrawRect(renderer, &knob);
        } else {
            SDL_RenderFillRect(renderer, &knob);
        }
        SDL_SetRenderDrawColor(renderer, 18, 22, 28, 255);
        SDL_RenderDrawRect(renderer, &knob);
    }
}

static void SceneEditorDigestOverlayDrawBezier3D(SDL_Renderer* renderer,
                                                 const SceneEditorDigestOverlayProjector* projector,
                                                 const RuntimeSceneBridge3DDigestState* digest,
                                                 int mouse_x,
                                                 int mouse_y,
                                                 const SceneEditorBezier3DGizmoState* gizmo_state) {
    const double plane_z = SceneEditorDigestOverlayResolveEditPlaneZ(digest, projector);
    SDL_Color curve_color = {128, 214, 255, 235};
    SDL_Color point_color = {218, 232, 246, 255};
    SDL_Color selected_color = {255, 170, 82, 255};
    SDL_Color handle_color = {236, 120, 120, 210};
    SDL_Color selected_handle_color = {255, 196, 98, 255};
    SDL_Color hover_handle_color = {255, 226, 138, 255};
    SDL_Color hover_point_color = {255, 240, 176, 255};
    SDL_Color start_color = {0, 200, 0, 235};
    SDL_Color end_color = {220, 40, 40, 235};
    PathTraversalEndpoints endpoints = {0};
    bool have_endpoints = false;
    int selected_segment = -1;
    int selected_handle = -1;
    int hover_segment = -1;
    int hover_handle = -1;
    int hover_point = -1;
    BezierEditorSelectionKind selection_kind = BezierEditorGetSelectionKind();
    int i = 0;
    if (!renderer || !projector || !gizmo_state) return;
    have_endpoints = PathResolveTraversalEndpoints(&sceneSettings.bezierPath,
                                                   &sceneSettings.bezierPath3D,
                                                   &endpoints);
    hover_point = SceneEditorDigestOverlayPickBezierPointIndex(projector,
                                                               &sceneSettings.bezierPath,
                                                               &sceneSettings.bezierPath3D,
                                                               plane_z,
                                                               mouse_x,
                                                               mouse_y);
    if (SceneEditorDigestOverlayPickBezierHandle(projector,
                                                 &sceneSettings.bezierPath,
                                                 &sceneSettings.bezierPath3D,
                                                 plane_z,
                                                 mouse_x,
                                                 mouse_y,
                                                 &hover_segment,
                                                 &hover_handle) >= 0) {
        hover_point = -1;
    } else {
        hover_segment = -1;
        hover_handle = -1;
    }
    if (sceneSettings.bezierPath.numPoints >= 2) {
        Point previous = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, 0.0);
        double previous_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath,
                                                                &sceneSettings.bezierPath3D,
                                                                0.0);
        for (i = 1; i <= 48; ++i) {
            double t = (double)i / 48.0;
            Point current = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, t);
            double current_z = CameraPath3D_GetPositionZNormalized(&sceneSettings.bezierPath,
                                                                   &sceneSettings.bezierPath3D,
                                                                   t);
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              projector,
                                              previous.x,
                                              previous.y,
                                              previous_z,
                                              current.x,
                                              current.y,
                                              current_z,
                                              curve_color);
            previous = current;
            previous_z = current_z;
        }
    }
    if (BezierEditorGetSelectedHandle(&selected_segment, &selected_handle)) {
        selection_kind = BEZIER_EDITOR_SELECTION_HANDLE;
    }
    for (i = 0; i < sceneSettings.bezierPath.numPoints - 1; ++i) {
        int handle_index = 0;
        for (handle_index = 0; handle_index < 2; ++handle_index) {
            double hx = 0.0;
            double hy = 0.0;
            double hz = 0.0;
            double anchor_x = 0.0;
            double anchor_y = 0.0;
            double anchor_z = 0.0;
            int px = 0;
            int py = 0;
            int ax = 0;
            int ay = 0;
            SDL_Rect knob = {0, 0, 0, 0};
            SDL_Color knob_color = handle_color;
            int knob_half = 5;
            bool selected = (selection_kind == BEZIER_EDITOR_SELECTION_HANDLE &&
                             i == selected_segment &&
                             handle_index == selected_handle);
            bool hovered = (i == hover_segment && handle_index == hover_handle);
            if (selected) {
                knob_color = selected_handle_color;
                knob_half = 6;
            } else if (hovered) {
                knob_color = hover_handle_color;
                knob_half = 6;
            }
            if (!SceneEditorDigestOverlayBezierHandleWorldPosition(&sceneSettings.bezierPath,
                                                                   &sceneSettings.bezierPath3D,
                                                                   i,
                                                                   handle_index,
                                                                   &hx,
                                                                   &hy,
                                                                   &hz,
                                                                   &anchor_x,
                                                                   &anchor_y,
                                                                   &anchor_z,
                                                                   plane_z)) {
                continue;
            }
            SceneEditorDigestOverlayDrawLine3(renderer,
                                              projector,
                                              anchor_x,
                                              anchor_y,
                                              anchor_z,
                                              hx,
                                              hy,
                                              hz,
                                              (SDL_Color){handle_color.r, handle_color.g, handle_color.b, 120});
            if (!SceneEditorDigestOverlayProjectPoint(projector, hx, hy, hz, &px, &py)) {
                continue;
            }
            if (!SceneEditorDigestOverlayProjectPoint(projector, anchor_x, anchor_y, anchor_z, &ax, &ay)) {
                continue;
            }
            knob = (SDL_Rect){px - knob_half, py - knob_half, knob_half * 2, knob_half * 2};
            SDL_SetRenderDrawColor(renderer, knob_color.r, knob_color.g, knob_color.b, knob_color.a);
            SDL_RenderFillRect(renderer, &knob);
            SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
            SDL_RenderDrawRect(renderer, &knob);
        }
    }
    for (i = 0; i < sceneSettings.bezierPath.numPoints; ++i) {
        int px = 0;
        int py = 0;
        bool point_selected = (selection_kind == BEZIER_EDITOR_SELECTION_POINT &&
                               i == BezierEditorGetSelectedPointIndex());
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
        if (!SceneEditorDigestOverlayProjectPoint(projector,
                                                  sceneSettings.bezierPath.points[i].x,
                                                  sceneSettings.bezierPath.points[i].y,
                                                  sceneSettings.bezierPath3D.point_z[i],
                                                  &px,
                                                  &py)) {
            continue;
        }
        marker = (SDL_Rect){px - radius, py - radius, radius * 2, radius * 2};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &marker);
        SDL_SetRenderDrawColor(renderer, 20, 24, 32, 255);
        SDL_RenderDrawRect(renderer, &marker);
    }
    if (selection_kind != BEZIER_EDITOR_SELECTION_NONE) {
        SceneEditorDigestOverlayDrawBezierGizmo(renderer, projector, digest, mouse_x, mouse_y, gizmo_state);
    }
}

bool SceneEditorDigestOverlayApplyBezierGizmoDrag(const SceneEditorDigestOverlayProjector* projector,
                                                  const RuntimeSceneBridge3DDigestState* digest,
                                                  const SceneEditorBezier3DGizmoState* gizmo_state,
                                                  int mouse_x,
                                                  int mouse_y) {
    SceneEditorBezier3DInteractionMetrics metrics = {0};
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
    double new_x = 0.0;
    double new_y = 0.0;
    double new_z = 0.0;
    if (!projector || !digest || !gizmo_state) return false;
    if (!gizmo_state->dragging) return false;
    new_x = gizmo_state->drag_start_world_x;
    new_y = gizmo_state->drag_start_world_y;
    new_z = gizmo_state->drag_start_world_z;
    metrics = SceneEditorDigestOverlayResolveBezierMetrics(digest, projector);
    if (!SceneEditorDigestOverlayProjectBezierGizmoAxis(projector,
                                                        digest,
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
            new_x += world_delta;
            break;
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Y:
            new_y += world_delta;
            break;
        case SCENE_EDITOR_BEZIER_3D_GIZMO_AXIS_Z:
            new_z += world_delta;
            break;
        default:
            return false;
    }
    return BezierEditorMoveSelectionTo3D(new_x, new_y, new_z);
}

void SceneEditorDigestOverlayRenderBezierLayer(SDL_Renderer* renderer,
                                               const SceneEditorDigestOverlayProjector* projector,
                                               const RuntimeSceneBridge3DDigestState* digest,
                                               int mouse_x,
                                               int mouse_y,
                                               const SceneEditorBezier3DGizmoState* gizmo_state) {
    SceneEditorDigestOverlayDrawBezier3D(renderer,
                                         projector,
                                         digest,
                                         mouse_x,
                                         mouse_y,
                                         gizmo_state);
}
