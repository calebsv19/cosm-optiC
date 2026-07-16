#include "editor/scene_editor_viewport_nav.h"

#include <math.h>

#include "editor/editor_mode_router.h"
#include "editor/material_editor.h"
#include "editor/scene_editor_viewport3d_bridge.h"
#include "editor/scene_editor_viewport_nav_math.h"
#include "render/ray_tracing_mode_backend.h"

#define SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG (-35.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG (24.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM (0.00005)
#define SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM (240.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_FRAME_FIT_FACTOR (0.72)
#define SCENE_EDITOR_DIGEST_OVERLAY_DYNAMIC_MIN_FACTOR (0.10)
#define SCENE_EDITOR_DIGEST_OVERLAY_DYNAMIC_MAX_FACTOR (20.0)
#define SCENE_EDITOR_DIGEST_OVERLAY_MATERIAL_MIN_FACTOR (0.02)
#define SCENE_EDITOR_DIGEST_OVERLAY_MATERIAL_MAX_FACTOR (500.0)

static bool scene_editor_viewport_nav_resolve_digest(RuntimeSceneBridge3DDigestState* out_digest) {
    RayTracingRuntimeRoute route = RayTracingModeBackend_ResolveRoute();
    RuntimeSceneBridge3DDigestState digest = {0};
    if (!RayTracingModeBackend_IsControlled3D(&route)) {
        if (out_digest) *out_digest = digest;
        return false;
    }
    runtime_scene_bridge_get_last_3d_digest_state(&digest);
    if (out_digest) *out_digest = digest;
    return digest.valid;
}

static bool scene_editor_viewport_nav_target_is_focused_material(int active_mode,
                                                                 int selected_object_index) {
    return active_mode == EDITOR_MODE_MATERIAL &&
           selected_object_index >= 0 &&
           MaterialEditorGetViewMode() == MATERIAL_EDITOR_VIEW_FOCUSED_ORIGIN;
}

static bool scene_editor_viewport_nav_resolve_target_extents(
    const RuntimeSceneBridge3DDigestState* digest,
    bool use_selected_object,
    int selected_object_index,
    double* min_x,
    double* min_y,
    double* min_z,
    double* max_x,
    double* max_y,
    double* max_z,
    double* span_max) {
    if (use_selected_object && selected_object_index >= 0 &&
        SceneEditorDigestOverlayResolveObjectExtents(digest,
                                                     selected_object_index,
                                                     min_x,
                                                     min_y,
                                                     min_z,
                                                     max_x,
                                                     max_y,
                                                     max_z,
                                                     span_max)) {
        return true;
    }
    return SceneEditorDigestOverlayResolveExtents(digest,
                                                  min_x,
                                                  min_y,
                                                  min_z,
                                                  max_x,
                                                  max_y,
                                                  max_z,
                                                  span_max);
}

static bool scene_editor_viewport_nav_resolve_fit_zoom(SceneEditorDigestOverlayNavState* nav_state,
                                                       const SDL_Rect* viewport_rect,
                                                       bool reset_angles,
                                                       bool use_selected_object,
                                                       int selected_object_index,
                                                       double* out_fit_zoom,
                                                       SceneEditorViewportNavVec3* out_target,
                                                       bool* out_target_valid) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    double span_max = 0.0;
    double projected_min_x = 0.0;
    double projected_min_y = 0.0;
    double projected_max_x = 0.0;
    double projected_max_y = 0.0;
    double corners[8][3];
    double projected_w = 0.0;
    double projected_h = 0.0;
    double fit_zoom = 1.0;
    CoreViewport3DFitRequest fit_request = {0};
    int sx = 0;
    int sy = 0;
    int i = 0;
    if (out_target_valid) *out_target_valid = false;
    if (!nav_state || !viewport_rect || !out_fit_zoom) {
        return false;
    }
    if (!scene_editor_viewport_nav_resolve_digest(&digest)) {
        return false;
    }
    if (reset_angles) {
        nav_state->orbit_yaw_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG;
        nav_state->orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG;
    }
    if (!scene_editor_viewport_nav_resolve_target_extents(&digest,
                                                          use_selected_object,
                                                          selected_object_index,
                                                          &min_x,
                                                          &min_y,
                                                          &min_z,
                                                          &max_x,
                                                          &max_y,
                                                          &max_z,
                                                          &span_max)) {
        nav_state->orbit_yaw_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_YAW_DEG;
        nav_state->orbit_pitch_deg = SCENE_EDITOR_DIGEST_OVERLAY_DEFAULT_PITCH_DEG;
        nav_state->overlay_zoom = 1.0;
        if (out_target_valid) *out_target_valid = false;
        return true;
    }

    if (!SceneEditorDigestOverlayBuildProjectorWithView(&digest,
                                                        viewport_rect,
                                                        nav_state->orbit_yaw_deg,
                                                        nav_state->orbit_pitch_deg,
                                                        1.0,
                                                        &projector)) {
        return false;
    }
    projector.center_x = (min_x + max_x) * 0.5;
    projector.center_y = (min_y + max_y) * 0.5;
    projector.center_z = (min_z + max_z) * 0.5;

    corners[0][0] = min_x; corners[0][1] = min_y; corners[0][2] = min_z;
    corners[1][0] = min_x; corners[1][1] = min_y; corners[1][2] = max_z;
    corners[2][0] = min_x; corners[2][1] = max_y; corners[2][2] = min_z;
    corners[3][0] = min_x; corners[3][1] = max_y; corners[3][2] = max_z;
    corners[4][0] = max_x; corners[4][1] = min_y; corners[4][2] = min_z;
    corners[5][0] = max_x; corners[5][1] = min_y; corners[5][2] = max_z;
    corners[6][0] = max_x; corners[6][1] = max_y; corners[6][2] = min_z;
    corners[7][0] = max_x; corners[7][1] = max_y; corners[7][2] = max_z;

    for (i = 0; i < 8; ++i) {
        if (!SceneEditorDigestOverlayProjectPoint(&projector,
                                                  corners[i][0],
                                                  corners[i][1],
                                                  corners[i][2],
                                                  &sx,
                                                  &sy)) {
            continue;
        }
        if (i == 0) {
            projected_min_x = projected_max_x = (double)sx;
            projected_min_y = projected_max_y = (double)sy;
        } else {
            if ((double)sx < projected_min_x) projected_min_x = (double)sx;
            if ((double)sx > projected_max_x) projected_max_x = (double)sx;
            if ((double)sy < projected_min_y) projected_min_y = (double)sy;
            if ((double)sy > projected_max_y) projected_max_y = (double)sy;
        }
    }

    projected_w = fmax(1.0, projected_max_x - projected_min_x);
    projected_h = fmax(1.0, projected_max_y - projected_min_y);
    fit_request.viewport_width_px = (double)projector.viewport.w;
    fit_request.viewport_height_px = (double)projector.viewport.h;
    fit_request.projected_span_right_world = projected_w;
    fit_request.projected_span_down_world = projected_h;
    fit_request.fill_fraction = SCENE_EDITOR_DIGEST_OVERLAY_FRAME_FIT_FACTOR;
    fit_request.min_scale_px_per_world_unit = SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM;
    fit_request.max_scale_px_per_world_unit = SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM;
    if (core_viewport3d_resolve_fit_scale(&fit_request, &fit_zoom).code != CORE_OK) {
        return false;
    }
    *out_fit_zoom = fit_zoom;
    if (out_target) {
        *out_target = (SceneEditorViewportNavVec3){projector.center_x,
                                                   projector.center_y,
                                                   projector.center_z};
    }
    if (out_target_valid) *out_target_valid = true;
    return true;
}

static void scene_editor_viewport_nav_set_zoom_limits(
    SceneEditorDigestOverlayNavState* nav_state,
    double fit_zoom,
    bool material_focus) {
    const double min_factor = material_focus
                                  ? SCENE_EDITOR_DIGEST_OVERLAY_MATERIAL_MIN_FACTOR
                                  : SCENE_EDITOR_DIGEST_OVERLAY_DYNAMIC_MIN_FACTOR;
    const double max_factor = material_focus
                                  ? SCENE_EDITOR_DIGEST_OVERLAY_MATERIAL_MAX_FACTOR
                                  : SCENE_EDITOR_DIGEST_OVERLAY_DYNAMIC_MAX_FACTOR;
    if (!nav_state || !(fit_zoom > 0.0) || !isfinite(fit_zoom)) return;
    nav_state->zoom_min = fmax(SCENE_EDITOR_DIGEST_OVERLAY_MIN_ZOOM,
                               fit_zoom * min_factor);
    nav_state->zoom_max = fmin(SCENE_EDITOR_DIGEST_OVERLAY_MAX_ZOOM,
                               fit_zoom * max_factor);
    if (nav_state->zoom_max < nav_state->zoom_min) {
        nav_state->zoom_max = nav_state->zoom_min;
    }
    nav_state->zoom_limits_material_focus = material_focus;
    nav_state->zoom_limits_valid = isfinite(nav_state->zoom_min) &&
                                   isfinite(nav_state->zoom_max);
}

bool SceneEditorViewportNavFitDigestOverlayForTarget(SceneEditorDigestOverlayNavState* nav_state,
                                                     const SDL_Rect* viewport_rect,
                                                     bool reset_angles,
                                                     int active_mode,
                                                     int selected_object_index) {
    double fit_zoom = 1.0;
    SceneEditorViewportNavVec3 target = {0};
    bool target_valid = false;
    bool current_target_valid = false;
    CoreViewport3DVec3d current_target = {0};
    CoreViewport3DVec3d frame_target = {0};
    CoreViewport3DVec3d next_target = {0};
    double current_scale = 1.0;
    double next_scale = 1.0;
    const double degrees_to_radians = 3.14159265358979323846 / 180.0;
    const bool material_focus = scene_editor_viewport_nav_target_is_focused_material(
        active_mode,
        selected_object_index);
    SceneEditorDigestOverlayNavState candidate = {0};
    if (!nav_state) return false;
    candidate = *nav_state;
    if (!scene_editor_viewport_nav_resolve_fit_zoom(&candidate,
                                                   viewport_rect,
                                                   reset_angles,
                                                   selected_object_index >= 0,
                                                   selected_object_index,
                                                   &fit_zoom,
                                                   &target,
                                                   &target_valid)) {
        return false;
    }
    current_target_valid = candidate.target_valid &&
                           isfinite(candidate.target_x) &&
                           isfinite(candidate.target_y) &&
                           isfinite(candidate.target_z);
    scene_editor_viewport_nav_set_zoom_limits(&candidate, fit_zoom, material_focus);
    candidate.target_valid = target_valid;
    if (target_valid) {
        frame_target = (CoreViewport3DVec3d){target.x, target.y, target.z};
        current_target = current_target_valid
                             ? (CoreViewport3DVec3d){candidate.target_x,
                                                    candidate.target_y,
                                                    candidate.target_z}
                             : frame_target;
        current_scale = candidate.overlay_zoom;
        if (!isfinite(current_scale) || current_scale <= 0.0) current_scale = fit_zoom;
        if (current_scale < candidate.zoom_min) current_scale = candidate.zoom_min;
        if (current_scale > candidate.zoom_max) current_scale = candidate.zoom_max;
        if (!SceneEditorViewport3DBridgeApplyFrame(
                current_target,
                candidate.orbit_yaw_deg * degrees_to_radians,
                candidate.orbit_pitch_deg * degrees_to_radians,
                current_scale,
                candidate.zoom_min,
                candidate.zoom_max,
                frame_target,
                fit_zoom,
                &next_target,
                &next_scale)) {
            return false;
        }
        candidate.target_x = next_target.x;
        candidate.target_y = next_target.y;
        candidate.target_z = next_target.z;
        candidate.overlay_zoom = next_scale;
    } else {
        candidate.overlay_zoom = fit_zoom;
    }
    *nav_state = candidate;
    return true;
}

bool SceneEditorViewportNavApplyDigestResize(SceneEditorDigestOverlayNavState* nav_state) {
    SceneEditorDigestOverlayNavState candidate;
    CoreViewport3DVec3d target;
    CoreViewport3DVec3d next_target;
    double next_scale = 0.0;
    double min_scale = 0.0;
    double max_scale = 0.0;
    const double degrees_to_radians = 3.14159265358979323846 / 180.0;
    if (!nav_state || !nav_state->target_valid ||
        !isfinite(nav_state->target_x) ||
        !isfinite(nav_state->target_y) ||
        !isfinite(nav_state->target_z) ||
        !isfinite(nav_state->overlay_zoom) || nav_state->overlay_zoom <= 0.0) {
        return false;
    }
    candidate = *nav_state;
    min_scale = candidate.zoom_limits_valid ? candidate.zoom_min : candidate.overlay_zoom;
    max_scale = candidate.zoom_limits_valid ? candidate.zoom_max : candidate.overlay_zoom;
    target = (CoreViewport3DVec3d){candidate.target_x,
                                   candidate.target_y,
                                   candidate.target_z};
    if (!SceneEditorViewport3DBridgeApplyResize(
            target,
            candidate.orbit_yaw_deg * degrees_to_radians,
            candidate.orbit_pitch_deg * degrees_to_radians,
            candidate.overlay_zoom,
            min_scale,
            max_scale,
            &next_target,
            &next_scale)) {
        return false;
    }
    candidate.target_x = next_target.x;
    candidate.target_y = next_target.y;
    candidate.target_z = next_target.z;
    candidate.overlay_zoom = next_scale;
    *nav_state = candidate;
    return true;
}

static bool scene_editor_viewport_nav_build_active_projector(
    const RuntimeSceneBridge3DDigestState* digest,
    const SDL_Rect* viewport_rect,
    const SceneEditorDigestOverlayNavState* nav_state,
    int active_mode,
    int selected_object_index,
    SceneEditorDigestOverlayProjector* out_projector) {
    if (scene_editor_viewport_nav_target_is_focused_material(active_mode, selected_object_index)) {
        return SceneEditorDigestOverlayBuildObjectProjector(digest,
                                                            viewport_rect,
                                                            nav_state,
                                                            selected_object_index,
                                                            true,
                                                            out_projector);
    }
    return SceneEditorDigestOverlayBuildProjector(digest,
                                                  viewport_rect,
                                                  nav_state,
                                                  out_projector);
}

static bool scene_editor_viewport_nav_ensure_target(
    SceneEditorDigestOverlayNavState* nav_state,
    const SceneEditorDigestOverlayProjector* projector) {
    if (!nav_state || !projector) return false;
    if (nav_state->target_valid &&
        isfinite(nav_state->target_x) &&
        isfinite(nav_state->target_y) &&
        isfinite(nav_state->target_z)) {
        return true;
    }
    if (!isfinite(projector->center_x) ||
        !isfinite(projector->center_y) ||
        !isfinite(projector->center_z)) {
        return false;
    }
    nav_state->target_x = projector->center_x;
    nav_state->target_y = projector->center_y;
    nav_state->target_z = projector->center_z;
    nav_state->target_valid = true;
    return true;
}

bool SceneEditorViewportNavApplyDigestPan(SceneEditorDigestOverlayNavState* nav_state,
                                         const SDL_Rect* viewport_rect,
                                         int screen_dx,
                                         int screen_dy,
                                         int active_mode,
                                         int selected_object_index) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector projector = {0};
    CoreViewport3DVec3d target = {0};
    CoreViewport3DVec3d next_target = {0};
    SceneEditorDigestOverlayNavState candidate = {0};
    if (!nav_state || !viewport_rect) return false;
    candidate = *nav_state;
    if (!scene_editor_viewport_nav_resolve_digest(&digest)) return false;
    if (!scene_editor_viewport_nav_build_active_projector(&digest,
                                                          viewport_rect,
                                                          &candidate,
                                                          active_mode,
                                                          selected_object_index,
                                                          &projector)) {
        return false;
    }
    if (!scene_editor_viewport_nav_ensure_target(&candidate, &projector)) return false;
    if (screen_dx == 0 && screen_dy == 0) {
        *nav_state = candidate;
        return true;
    }
    target = (CoreViewport3DVec3d){candidate.target_x,
                                   candidate.target_y,
                                   candidate.target_z};
    if (!SceneEditorViewport3DBridgeApplyPan(target,
                                             projector.yaw_rad,
                                             projector.pitch_rad,
                                             projector.scale,
                                             (double)screen_dx,
                                             (double)screen_dy,
                                             &next_target)) {
        return false;
    }
    candidate.target_x = next_target.x;
    candidate.target_y = next_target.y;
    candidate.target_z = next_target.z;
    *nav_state = candidate;
    return true;
}

bool SceneEditorViewportNavApplyDigestWheelZoom(SceneEditorDigestOverlayNavState* nav_state,
                                                const SDL_Rect* viewport_rect,
                                                int screen_x,
                                                int screen_y,
                                                double wheel_delta,
                                                int active_mode,
                                                int selected_object_index) {
    RuntimeSceneBridge3DDigestState digest = {0};
    SceneEditorDigestOverlayProjector old_projector = {0};
    SceneEditorDigestOverlayProjector new_projector = {0};
    CoreViewport3DVec3d target = {0};
    CoreViewport3DVec3d next_target = {0};
    SceneEditorDigestOverlayNavState candidate = {0};
    double fit_zoom = 1.0;
    double bounded_delta = 0.0;
    bool material_focus = scene_editor_viewport_nav_target_is_focused_material(active_mode,
                                                                               selected_object_index);
    if (!nav_state || wheel_delta == 0.0 || !isfinite(wheel_delta)) {
        return false;
    }
    candidate = *nav_state;
    if (!scene_editor_viewport_nav_resolve_digest(&digest)) {
        return false;
    }
    if ((!candidate.zoom_limits_valid ||
         candidate.zoom_limits_material_focus != material_focus) &&
        viewport_rect &&
        scene_editor_viewport_nav_resolve_fit_zoom(&candidate,
                                                   viewport_rect,
                                                   false,
                                                   material_focus,
                                                   selected_object_index,
                                                   &fit_zoom,
                                                   NULL,
                                                   NULL)) {
        scene_editor_viewport_nav_set_zoom_limits(&candidate, fit_zoom, material_focus);
        if (!(candidate.overlay_zoom > 0.0) || !isfinite(candidate.overlay_zoom)) {
            candidate.overlay_zoom = fit_zoom;
        }
    }
    if (!viewport_rect ||
        !scene_editor_viewport_nav_build_active_projector(&digest,
                                                          viewport_rect,
                                                          &candidate,
                                                          active_mode,
                                                          selected_object_index,
                                                          &old_projector) ||
        !scene_editor_viewport_nav_ensure_target(&candidate, &old_projector)) {
        return false;
    }
    old_projector.center_x = candidate.target_x;
    old_projector.center_y = candidate.target_y;
    old_projector.center_z = candidate.target_z;
    bounded_delta = fmax(-8.0, fmin(8.0, wheel_delta));
    candidate.overlay_zoom *= pow(material_focus ? 1.25 : 1.12, bounded_delta);
    if (candidate.zoom_limits_valid) {
        if (candidate.overlay_zoom < candidate.zoom_min) {
            candidate.overlay_zoom = candidate.zoom_min;
        }
        if (candidate.overlay_zoom > candidate.zoom_max) {
            candidate.overlay_zoom = candidate.zoom_max;
        }
    }
    if (!scene_editor_viewport_nav_build_active_projector(&digest,
                                                          viewport_rect,
                                                          &candidate,
                                                          active_mode,
                                                          selected_object_index,
                                                          &new_projector)) {
        return false;
    }
    target = (CoreViewport3DVec3d){candidate.target_x,
                                   candidate.target_y,
                                   candidate.target_z};
    if (!SceneEditorViewport3DBridgePreserveAnchor(
            target,
            old_projector.yaw_rad,
            old_projector.pitch_rad,
            old_projector.scale,
            new_projector.scale,
            (double)screen_x - ((double)viewport_rect->x + (double)viewport_rect->w * 0.5),
            (double)screen_y - ((double)viewport_rect->y + (double)viewport_rect->h * 0.5),
            &next_target)) {
        return false;
    }
    candidate.target_x = next_target.x;
    candidate.target_y = next_target.y;
    candidate.target_z = next_target.z;
    *nav_state = candidate;
    return true;
}
